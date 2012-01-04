#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/vsrc_buffer.h>
#include <libavutil/avutil.h>
#include "imgs2video_cmdline.h"
#include "compat.h"

#define FMT_TIMEBASE_DEN 1000

struct img {
    char *filename;
    time_t ts;
    unsigned int duration;
};

struct transcoder {
    struct args args;
    struct img *files;
    unsigned int n_files;
    struct img *frames;
    unsigned int n_frames;
    AVFormatContext *out;
    AVCodecContext *enc;
    AVFilterContext *filter_src;
    AVFilterContext *filter_sink;
    AVFilterGraph *filter_graph;
    uint8_t *video_outbuf;
    int video_outbuf_size;
    unsigned int frames_out;
    unsigned int pts_step;
    unsigned int width;
    unsigned int height;
};
typedef struct transcoder Transcoder;

static int write_video_frame(Transcoder *tc, AVFilterBufferRef *picref);

int match_postfix_jpg(const char *filename) {
    char *p = strcasestr(filename, ".jpg");
    if (!p)
        return 0;
    if (!strcasecmp(p, ".jpg"))
        return 1; // exact postfix match
    else
        return 0;
}

int filter_jpg(const struct dirent *a) {
    return match_postfix_jpg(a->d_name);
}

int compare_mod_dates(const struct dirent **a, const struct dirent **b) {
    struct stat a_stat, b_stat;
    int r;
    r = stat((*a)->d_name, &a_stat);
    if (r != 0) {
        printf("stat for '%s' failed: ret %d, errno %d '%s'\n",
                (*a)->d_name, r, errno, strerror(errno));
        exit(1);
    }
    r = stat((*b)->d_name, &b_stat);
    if (r != 0) {
        printf("stat for '%s' failed: ret %d, errno %d '%s'\n",
                (*b)->d_name, r, errno, strerror(errno));
        exit(1);
    }

    return a_stat.st_mtime - b_stat.st_mtime;
}

int transform_frames_chain(Transcoder *tc, struct img *array, unsigned int n, struct img **arg) {
    // TODO better algo?
    unsigned idx(struct img *frames, unsigned n_frames, unsigned timestamp) {
        unsigned best_i = 0;
        int i;
        for (i = 0; i < n_frames; i++) {
            if (FFABS((int64_t)frames[i].ts - (int64_t)timestamp) <
                    FFABS((int64_t)frames[best_i].ts - (int64_t)timestamp) )
                best_i = i;
        }
        return best_i;
    }
    unsigned int realtime_duration = array[n-1].ts - array[0].ts;
    unsigned int n_frames = realtime_duration * tc->args.frame_rate_arg / tc->args.speedup_coef_arg;
    struct img *frames = calloc(n_frames, sizeof(struct img));
    int i, j;

    for (i = 0; i < n_frames; i++) {
        frames[i].ts = i * tc->pts_step;
        //printf("frame[%d].ts := %d\n", i, i * pts_step);
        frames[i].duration = tc->pts_step;
    }

    frames[0].filename = array[0].filename;
    frames[n_frames-1].filename = array[n-1].filename;

    for (i = 0; i < n-1; i++) {
        /* for each image:
         * set the stop point at the cell in frames[] which is for next image
         * fill all frames[] from which is for it, up to which is for next
         */
        unsigned entry_pos = idx(frames, n_frames, (array[i].ts-array[0].ts) * FMT_TIMEBASE_DEN / tc->args.speedup_coef_arg);
        unsigned next_entry_pos = idx(frames, n_frames, (array[i+1].ts-array[0].ts) * FMT_TIMEBASE_DEN / tc->args.speedup_coef_arg);
        //printf("img %d, ts %"PRId64", position %d, of next is %d\n", i, (array[i].ts-array[0].ts) * FMT_TIMEBASE_DEN / args.speedup_coef_arg, entry_pos, next_entry_pos);
        for (j = entry_pos; j < next_entry_pos; j++)
            frames[j].filename = array[i].filename;
    }

    for (i = 0; i < n_frames; i++)
        assert(frames[i].filename);

    *arg = frames;
    return n_frames;
}

int imgs_names_durations(Transcoder *tc) {
    const char *dir = tc->args.images_dir_arg;

    /*
     * 1. List directory files
     * 2. Sort by mod date, ascending
     * 3. Calc intervals
     */

    struct dirent **namelist;
    struct stat st;
    char *cwd;
    int i;
    int r;

    cwd = getcwd(NULL, 0);
    assert(cwd);
    r = chdir(dir);
    if (r) {
        fprintf(stderr, "Failed to chdir to '%s': errno %d\n", dir, errno);
        exit(1);
    }

    tc->n_files = scandir(".", &namelist, filter_jpg, compare_mod_dates);
    if (tc->n_files == 0) {
        fprintf(stderr, "source dir contains no suitable files\n");
        return 1;
    }

    r = chdir(cwd);
    assert(!r);

    tc->files = calloc(tc->n_files, sizeof(struct img));

    for (i = 0; i < tc->n_files; i++) {
        asprintf(&tc->files[i].filename, "%s/%s", dir, namelist[i]->d_name);
        free(namelist[i]);
        r = stat(tc->files[i].filename, &st);
        if (r != 0) {
            printf("stat for '%s' failed: ret %d, errno %d '%s'\n",
                    tc->files[i].filename, r, errno, strerror(errno));
            return 1;
        }
        tc->files[i].ts = st.st_mtime;
        if (i > 0)
            tc->files[i].duration = tc->files[i].ts - tc->files[i-1].ts;
        //printf("%s %u\n", tc->files[i].filename, tc->files[i].duration);
    }
    free(namelist);
    printf("%d images\n", tc->n_files);
    tc->n_frames = transform_frames_chain(tc, tc->files, tc->n_files, &tc->frames);
    printf("%d frames\n", tc->n_frames);
    return 0;
}

/* http://stackoverflow.com/questions/3527584/ffmpeg-jpeg-file-to-avframe */
int open_image_and_push_video_frame(struct img *img, Transcoder *tc) {
    AVFormatContext *pFormatCtx = NULL;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame;
    AVPacket packet;
    int r;
    int frameFinished;

    if(avformat_open_input(&pFormatCtx, img->filename, NULL, 0)) {
        printf("Can't open image file '%s'\n", img->filename);
        goto fail_open_file;
    }
    //dump_format(pFormatCtx, 0, img->filename, 0);
    pCodecCtx = pFormatCtx->streams[0]->codec;

    // Find the decoder for the video stream
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (!pCodec) {
        printf("Codec not found\n");
        goto fail_find_decoder;
    }

    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
        printf("Could not open codec\n");
        goto fail_avcodec_open;
    }

    pCodecCtx->width = tc->width;
    pCodecCtx->height = tc->height;
    pCodecCtx->pix_fmt = PIX_FMT_YUV420P;

    pFrame = avcodec_alloc_frame();
    if (!pFrame) {
        printf("Can't allocate memory for AVFrame\n");
        goto fail_alloc_frame;
    }

    r = av_read_frame(pFormatCtx, &packet);
    if (r < 0) {
        printf("Failed to read frame\n");
        goto fail_read_frame;
    }

    r = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
    if (r <= 0) {
        printf("Failed to decode image\n");
        goto fail_decode;
    }
    pFrame->quality = 1;
    pFrame->pts = img->ts / tc->pts_step; // for encoding, pts step must be exactly 1
    pFrame->pict_type = 0; /* let codec choose */

    /* push the decoded frame into the filtergraph */
#ifdef LIBAV
    r = av_vsrc_buffer_add_frame(tc->filter_src, pFrame, pFrame->pts, (AVRational){1, 1});
#else
    r = av_vsrc_buffer_add_frame(tc->filter_src, pFrame, 0);
#endif
    assert(r >= 0);

    /* pull filtered pictures from the filtergraph */
    AVFilterBufferRef *picref = NULL;
    assert(avfilter_poll_frame(tc->filter_sink->inputs[0]));
    r = avfilter_request_frame(tc->filter_sink->inputs[0]);
    if (r == 0)
        picref = tc->filter_sink->inputs[0]->cur_buf;

    if (picref) {
        write_video_frame(tc, picref);
        avfilter_unref_buffer(picref);
    }

    av_free(pFrame);
    av_free_packet(&packet);
    avcodec_close(pCodecCtx);
    av_close_input_file(pFormatCtx);

    return 0;
fail_decode:
    av_free_packet(&packet);
fail_read_frame:
    av_free(pFrame);
fail_alloc_frame:
    avcodec_close(pCodecCtx);
fail_avcodec_open:
    ;
fail_find_decoder:
    av_close_input_file(pFormatCtx);
fail_open_file:
    return 1;
}

char *get_some_pic(const char *dirname) {
    int r;
    char *ret;
    DIR *dir = opendir(dirname);
    if (!dir) {
        fprintf(stderr, "dir '%s' not found", dirname);
        return NULL;
    }
    struct dirent *entry;
    do {
        entry = readdir(dir);
        if (!entry) {
            fprintf(stderr, "no matching files in '%s'!\n", dirname);
            exit(1);
        }
    } while(!match_postfix_jpg(entry->d_name));
    r = asprintf(&ret, "%s/%s", dirname, entry->d_name);
    assert(r != -1);
    closedir(dir);

    return ret;
}

static int init_sizes(Transcoder *tc, const char* imageFileName) {
    AVFormatContext *pFormatCtx = NULL;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame;
    AVPacket packet;
    int r;
    int frameFinished;

    if(avformat_open_input(&pFormatCtx, imageFileName, NULL, 0)) {
        printf("Can't open image file '%s'\n", imageFileName);
        return 1;
    }
    //dump_format(pFormatCtx, 0, imageFileName, 0);
    pCodecCtx = pFormatCtx->streams[0]->codec;

    // Find the decoder for the video stream
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (!pCodec) {
        printf("Codec not found\n");
        return 1;
    }

    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
        printf("Could not open codec\n");
        return 1;
    }

    pFrame = avcodec_alloc_frame();
    if (!pFrame) {
        printf("Can't allocate memory for AVFrame\n");
        return 1;
    }

    r = av_read_frame(pFormatCtx, &packet);
    assert(r >= 0);

    r = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
    assert(r > 0);
    printf("File '%s' has width %d, height %d, assuming each pic has same\n", imageFileName, pCodecCtx->width, pCodecCtx->height);
    tc->width = pCodecCtx->width;
    tc->height = pCodecCtx->height;

    avcodec_close(pCodecCtx);
    av_close_input_file(pFormatCtx);
    return 0;
}

static int write_video_frame(Transcoder *tc, AVFilterBufferRef *picref)
{
    int out_size, ret;

    AVFrame *picture = avcodec_alloc_frame();
    assert(picture);
    ret = avfilter_fill_frame_from_video_buffer_ref(picture, picref);
    assert(ret == 0);
    picture->pts = picref->pts;
    printf("encoding with pts %"PRId64", pict_type %d\n", picture->pts, picture->pict_type);
    /* encode the image */
    out_size = avcodec_encode_video(tc->enc, tc->video_outbuf, tc->video_outbuf_size, picture);

    /* if zero size, it means the image was buffered */
    if (out_size > 0) {
        AVPacket pkt;
        av_init_packet(&pkt);

        pkt.pts = tc->frames_out++ * tc->pts_step;
        pkt.dts = pkt.pts;
        pkt.duration = tc->pts_step;
        if(tc->enc->coded_frame->key_frame)
            pkt.flags |= AV_PKT_FLAG_KEY;
        pkt.data= tc->video_outbuf;
        pkt.size= out_size;

        /* write the compressed frame in the media file */
        ret = av_interleaved_write_frame(tc->out, &pkt);
        av_free_packet(&pkt);
    } else {
        //printf("out_size=%d\n", out_size);
        ret = 0;
        //printf("should this ever happen?\n");
    }
    if (ret != 0) {
        fprintf(stderr, "Error while writing video frame\n");
        return 1;
    }
    return 0;
}

int global_init(void) {
    av_log_set_level(AV_LOG_VERBOSE);
    av_register_all();
    avfilter_register_all();
    return 0;
}

int tc_build_frames_table(Transcoder *tc) {
    int r;
    tc->pts_step = FMT_TIMEBASE_DEN / tc->args.frame_rate_arg;
    r = imgs_names_durations(tc);
    if (r)
        return r;
    return 0;
}

int probe(Transcoder *tc) {
    char *tmp;
    int r;
    tmp = get_some_pic(tc->args.images_dir_arg);
    if (!tmp)
        return 1;
    r = init_sizes(tc, tmp);
    if (r)
        return r;
    free(tmp);
    return 0;
}

int open_out(Transcoder *tc) {
    /* auto detect the output format from the name. default is
       mpeg. */
    AVOutputFormat *fmt;
    fmt = av_guess_format(NULL, tc->args.output_file_arg, NULL);
    if (!fmt) {
        fprintf(stderr, "Could not find suitable output format\n");
        return 1;
    }

    /* allocate the output media context */
    tc->out = avformat_alloc_context();
    if (!tc->out) {
        fprintf(stderr, "Memory error\n");
        return 1;
    }
    tc->out->oformat = fmt;
    snprintf(tc->out->filename, sizeof(tc->out->filename), "%s", tc->args.output_file_arg);

    if (avio_open(&tc->out->pb, tc->out->filename, URL_WRONLY) < 0) {
        fprintf(stderr, "Could not open '%s'\n", tc->out->filename);
        return 1;
    }
    return 0;
}

int open_encoder(Transcoder *tc) {
    if (tc->out->oformat->video_codec == CODEC_ID_NONE) {
        printf("guessed format doesnt assume video?\n");
        return 1;
    }

    AVCodec *codec = avcodec_find_encoder_by_name(tc->args.vcodec_arg);
    if (!codec) {
        fprintf(stderr, "Encoder %s not found\n", tc->args.vcodec_arg);
        return 1;
    }
    AVStream *st;
    st = avformat_new_stream(tc->out, codec);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        return 1;
    }

    // for flv format, timebase is 1kHz
    st->time_base.den = FMT_TIMEBASE_DEN; // FIXME hardcode
    st->time_base.num = 1;
    st->sample_aspect_ratio.den = 1;
    st->sample_aspect_ratio.num = 1;

    tc->enc = st->codec;
    /* resolution must be a multiple of two */
    tc->enc->width = tc->width;
    tc->enc->height = tc->height;

    // for video codecs, at last libx264, timebase must be <framerate> Hz
    tc->enc->time_base.den = tc->args.frame_rate_arg;
    tc->enc->time_base.num = 1;
    tc->enc->pix_fmt = PIX_FMT_YUV420P;

    if(tc->out->oformat->flags & AVFMT_GLOBALHEADER)
        tc->enc->flags |= CODEC_FLAG_GLOBAL_HEADER;

    tc->enc->sample_aspect_ratio.den = 1;
    tc->enc->sample_aspect_ratio.num = 1;

    tc->enc->bit_rate = tc->args.bitrate_arg;
    tc->enc->bit_rate_tolerance = tc->enc->bit_rate / 5;
    tc->enc->thread_count = 0; // use several threads for encoding

    AVDictionary *opts = NULL;
    if (tc->args.bitrate_arg != 0) // profiles don't support lossless
        av_dict_set(&opts, "profile", tc->args.profile_arg, 0);
    else
        av_dict_set(&opts, "qp", "0", 0); // set lossless mode
    av_dict_set(&opts, "preset", tc->args.preset_arg, 0);
    /* open the codec */
    if (avcodec_open2(tc->enc, codec, &opts) < 0) {
        fprintf(stderr, "could not open codec\n");
        return 1;
    }

    /* allocate output buffer */
    tc->video_outbuf_size = 10000000; // FIXME hardcode
    tc->video_outbuf = av_malloc(tc->video_outbuf_size);
    if (!tc->video_outbuf) {
        fprintf(stderr, "Alloc outbuf fail\n");
        return 1;
    }
    av_dump_format(tc->out, 0, tc->args.output_file_arg, 1);
    return 0;
}

int setup_filters(Transcoder *tc) {
    int r;
    AVFilter *src  = avfilter_get_by_name("buffer");
    assert(src);
    AVFilter *sink = avfilter_get_by_name("nullsink");
    assert(sink);
    AVFilterInOut *outputs = avfilter_inout_alloc();
    assert(outputs);
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    assert(inputs);
    tc->filter_graph = avfilter_graph_alloc();
    assert(tc->filter_graph);

    char filter_args[50];
    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(filter_args, sizeof(filter_args), "%d:%d:%d:%d:%d:%d:%d",
            tc->width, tc->height, tc->enc->pix_fmt,
            tc->enc->time_base.num, tc->enc->time_base.den,
            tc->enc->sample_aspect_ratio.num, tc->enc->sample_aspect_ratio.den);
    r = avfilter_graph_create_filter(&tc->filter_src, src, "in",
            filter_args, NULL, tc->filter_graph);
    if (r < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        return r;
    }

    /* buffer video sink: to terminate the filter chain. */
    enum PixelFormat pix_fmts[] = { PIX_FMT_YUV420P, PIX_FMT_NONE };
    r = avfilter_graph_create_filter(&tc->filter_sink, sink, "out",
            NULL, pix_fmts, tc->filter_graph);
    if (r < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        return r;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = tc->filter_src;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = tc->filter_sink;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    char *filter_descr = tc->args.filter_arg;

    r = avfilter_graph_parse(tc->filter_graph, filter_descr,
#ifdef LIBAV
            inputs, outputs,
#else
            &inputs, &outputs,
#endif
            NULL);

    if (r < 0)
        return r;

    if ((r = avfilter_graph_config(tc->filter_graph, NULL)) < 0)
        return r;

    return 0;
}

int tc_process_frame(Transcoder *tc, unsigned int i) {
    return open_image_and_push_video_frame(&tc->frames[i], tc);
}

int tc_flush_encoder(Transcoder *tc) {
    int r;
    while (1) {
        /* flush buffered remainings */
        r = avcodec_encode_video(tc->enc, tc->video_outbuf, tc->video_outbuf_size, NULL);
        if (r <= 0)
            break;
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.pts = tc->frames_out++ * tc->pts_step;
        pkt.dts = pkt.pts;
        pkt.duration = tc->pts_step;
        printf("pkt.pts %"PRId64"\n", pkt.pts);
        if(tc->enc->coded_frame->key_frame)
            pkt.flags |= AV_PKT_FLAG_KEY;
        pkt.stream_index= 0;
        pkt.data= tc->video_outbuf;
        pkt.size= r;

        /* write the compressed frame in the media file */
        r = av_interleaved_write_frame(tc->out, &pkt);
        assert(r == 0);
        av_free_packet(&pkt);
    }
    return 0;
}

int main(int argc, char **argv) {
    int r;
    Transcoder *tc;
    unsigned int i;

    r = global_init();
    assert(!r);

    tc = calloc(1, sizeof(*tc));
    assert(tc);


    r = cmdline_parser(argc, argv, &tc->args);
    if (r) {
        cmdline_parser_print_help();
        return r;
    }

    r = tc_build_frames_table(tc);
    if (r)
        return r;

    // recognize dimensions
    r = probe(tc);
    if (r) {
        fprintf(stderr, "Probing fail\n");
        return r;
    }

    r = open_out(tc);
    if (r) {
        fprintf(stderr, "Open out file fail\n");
        return r;
    }

    r = open_encoder(tc);
    if (r) {
        fprintf(stderr, "Encoder open fail\n");
        return r;
    }

    r = setup_filters(tc);
    if (r) {
        fprintf(stderr, "Filters setup fail\n");
        return r;
    }

    r = avformat_write_header(tc->out, NULL);
    if (r) {
        fprintf(stderr, "write out file fail\n");
        return r;
    }

    for(i = 0; i < tc->n_frames; i++) {
        printf("processing frame %d/%d\n", i, tc->n_frames);
        if (i > 0)
            assert(tc->frames[i].ts > tc->frames[i-1].ts);
        r = tc_process_frame(tc, i);
        if (r < 0) {
            fprintf(stderr, "Fatal error processing frame, aborting\n");
            break;
        }
        if (r) {
            fprintf(stderr, "Processing file %s/%s failed, throw away and proceed\n", tc->args.images_dir_arg, tc->frames[i].filename);
        }
    }
    tc_flush_encoder(tc);

    av_write_trailer(tc->out);
    av_dump_format(tc->out, 0, tc->args.output_file_arg, 1);

    avcodec_close(tc->enc);
    av_free(tc->video_outbuf);
    avio_close(tc->out->pb);
    avformat_free_context(tc->out);
    free(tc);

    return 0;
}
