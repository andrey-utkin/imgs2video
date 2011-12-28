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

uint8_t *video_outbuf;
int video_outbuf_size;
unsigned int frames_out = 0;

struct args args;
unsigned int pts_step;
unsigned int global_width;
unsigned int global_height;

struct args args;
unsigned int pts_step;

AVFilterContext *buffersink_ctx;
AVFilterContext *buffersrc_ctx;
AVFilterGraph *filter_graph;

static void write_video_frame(AVFormatContext *oc, AVFilterBufferRef *picref);

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

int transform_frames_chain(struct img *array, unsigned int n, struct img **arg) {
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
    unsigned int n_frames = realtime_duration * args.frame_rate_arg / args.speedup_coef_arg;
    struct img *frames = calloc(n_frames, sizeof(struct img));
    int i, j;

    for (i = 0; i < n_frames; i++) {
        frames[i].ts = i * pts_step;
        //printf("frame[%d].ts := %d\n", i, i * pts_step);
        frames[i].duration = pts_step;
    }

    frames[0].filename = array[0].filename;
    frames[n_frames-1].filename = array[n-1].filename;

    for (i = 0; i < n-1; i++) {
        /* for each image:
         * set the stop point at the cell in frames[] which is for next image
         * fill all frames[] from which is for it, up to which is for next
         */
        unsigned entry_pos = idx(frames, n_frames, (array[i].ts-array[0].ts) * FMT_TIMEBASE_DEN / args.speedup_coef_arg);
        unsigned next_entry_pos = idx(frames, n_frames, (array[i+1].ts-array[0].ts) * FMT_TIMEBASE_DEN / args.speedup_coef_arg);
        //printf("img %d, ts %"PRId64", position %d, of next is %d\n", i, (array[i].ts-array[0].ts) * FMT_TIMEBASE_DEN / args.speedup_coef_arg, entry_pos, next_entry_pos);
        for (j = entry_pos; j < next_entry_pos; j++)
            frames[j].filename = array[i].filename;
    }

    for (i = 0; i < n_frames; i++)
        assert(frames[i].filename);

    *arg = frames;
    return n_frames;
}

int imgs_names_durations(const char *dir, struct img **arg) {

    /*
     * 1. List directory files
     * 2. Sort by mod date, ascending
     * 3. Calc intervals
     */

    struct dirent **namelist;
    struct stat st;
    struct img *array;
    char *cwd;
    int i;
    int r;
    int n;

    cwd = getcwd(NULL, 0);
    assert(cwd);
    r = chdir(dir);
    if (r) {
        fprintf(stderr, "Failed to chdir to '%s': errno %d\n", dir, errno);
        exit(1);
    }

    n = scandir(".", &namelist, filter_jpg, compare_mod_dates);
    assert(n >= 0);

    r = chdir(cwd);
    if (r) {
        fprintf(stderr, "Failed to chdir back to '%s': errno %d\n", cwd, errno);
        exit(1);
    }

    array = calloc(n, sizeof(struct img));

    for (i = 0; i < n; i++) {
        asprintf(&array[i].filename, "%s/%s", dir, namelist[i]->d_name);
        free(namelist[i]);
        r = stat(array[i].filename, &st);
        if (r != 0) {
            printf("stat for '%s' failed: ret %d, errno %d '%s'\n",
                    array[i].filename, r, errno, strerror(errno));
            exit(1);
        }
        array[i].ts = st.st_mtime;
        if (i > 0)
            array[i].duration = array[i].ts - array[i-1].ts;
        //printf("%s %u\n", array[i].filename, array[i].duration);
    }
    free(namelist);
    printf("%d images\n", n);
    n = transform_frames_chain(array, n, arg);
    printf("%d frames\n", n);
    return n;
}

/* http://stackoverflow.com/questions/3527584/ffmpeg-jpeg-file-to-avframe */
int open_image_and_push_video_frame(struct img *img, AVFormatContext *video_output) {
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

    pCodecCtx->width = global_width;
    pCodecCtx->height = global_height;
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
    pFrame->pts = img->ts;
    pFrame->pict_type = 0; /* let codec choose */

    /* push the decoded frame into the filtergraph */
#ifdef LIBAV
    r = av_vsrc_buffer_add_frame(buffersrc_ctx, pFrame, pFrame->pts, (AVRational){1, 1});
#else
    r = av_vsrc_buffer_add_frame(buffersrc_ctx, pFrame, 0);
#endif
    assert(r >= 0);

    /* pull filtered pictures from the filtergraph */
    AVFilterBufferRef *picref = NULL;
    while (avfilter_poll_frame(buffersink_ctx->inputs[0])) {
        r = avfilter_request_frame(buffersink_ctx->inputs[0]);
        if (r == 0)
            picref = buffersink_ctx->inputs[0]->cur_buf;

        if (picref) {
            write_video_frame(video_output, picref);
            avfilter_unref_buffer(picref);
        }
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

void init_sizes(const char* imageFileName) {
    AVFormatContext *pFormatCtx = NULL;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame;
    AVPacket packet;
    int r;
    int frameFinished;

    if(avformat_open_input(&pFormatCtx, imageFileName, NULL, 0)) {
        printf("Can't open image file '%s'\n", imageFileName);
        exit(1);
    }
    //dump_format(pFormatCtx, 0, imageFileName, 0);
    pCodecCtx = pFormatCtx->streams[0]->codec;

    // Find the decoder for the video stream
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (!pCodec) {
        printf("Codec not found\n");
        exit(1);
    }

    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
        printf("Could not open codec\n");
        exit(1);
    }

    pFrame = avcodec_alloc_frame();
    if (!pFrame) {
        printf("Can't allocate memory for AVFrame\n");
        exit(1);
    }

    r = av_read_frame(pFormatCtx, &packet);
    assert(r >= 0);

    r = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
    assert(r > 0);
    printf("File '%s' has width %d, height %d, assuming each pic has same\n", imageFileName, pCodecCtx->width, pCodecCtx->height);
    global_width = pCodecCtx->width;
    global_height = pCodecCtx->height;

    av_free(pFrame);
    av_free_packet(&packet);
    avcodec_close(pCodecCtx);
    av_close_input_file(pFormatCtx);
}

/* add a video output stream */
static AVStream *add_video_stream(AVFormatContext *oc, enum CodecID codec_id)
{
    AVCodecContext *c;
    AVStream *st;

    st = avformat_new_stream(oc, avcodec_find_encoder(codec_id));
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        exit(1);
    }

    st->time_base.den = FMT_TIMEBASE_DEN;
    st->time_base.num = 1;

    c = st->codec;
    c->codec_id = codec_id;
    c->codec_type = AVMEDIA_TYPE_VIDEO;

    st->r_frame_rate.den = 1;
    st->r_frame_rate.num = args.frame_rate_arg;
    st->avg_frame_rate.den = 1;
    st->avg_frame_rate.num = args.frame_rate_arg;

    /* resolution must be a multiple of two */
    c->width = global_width;
    c->height = global_height;
    /* time base: this is the fundamental unit of time (in seconds) in terms
       of which frame timestamps are represented. for fixed-fps content,
       timebase should be 1/framerate and timestamp increments should be
       identically 1. */

    c->time_base.den = args.frame_rate_arg;
    c->time_base.num = 1;
    c->pix_fmt = PIX_FMT_YUV420P;

    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    st->sample_aspect_ratio.den = 1;
    st->sample_aspect_ratio.num = 1;
    c->sample_aspect_ratio.den = 1;
    c->sample_aspect_ratio.num = 1;

    c->qmin = 0;
    c->qmax = 69;
    c->cqp = 0;
    c->thread_count = 0; // use several threads for encoding

    return st;
}

static void open_video(AVFormatContext *oc, AVStream *st)
{
    AVCodec *codec;
    AVCodecContext *c;

    c = st->codec;

    /* find the video encoder */
    codec = avcodec_find_encoder(c->codec_id);
    if (!codec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }

    /* open the codec */
    if (avcodec_open2(c, codec, NULL) < 0) {
        fprintf(stderr, "could not open codec\n");
        exit(1);
    }

    /* allocate output buffer */
    video_outbuf_size = 10000000;
    video_outbuf = av_malloc(video_outbuf_size);
}

static void write_video_frame(AVFormatContext *oc, AVFilterBufferRef *picref)
{
    AVStream *st = oc->streams[0];
    int out_size, ret;
    AVCodecContext *c = st->codec;

    AVFrame *picture = avcodec_alloc_frame();
    assert(picture);
    ret = avfilter_fill_frame_from_video_buffer_ref(picture, picref);
    assert(ret == 0);
    picture->pts = picref->pts;
    printf("encoding with pts %"PRId64", pict_type %d\n", picture->pts, picture->pict_type);
    /* encode the image */
    out_size = avcodec_encode_video(c, video_outbuf, video_outbuf_size, picture);

    /* if zero size, it means the image was buffered */
    if (out_size > 0) {
        AVPacket pkt;
        av_init_packet(&pkt);

        pkt.pts = frames_out++ * pts_step;
        pkt.dts = pkt.pts;
        pkt.duration = pts_step;
        printf("pkt.pts %"PRId64"\n", pkt.pts);
        if(c->coded_frame->key_frame)
            pkt.flags |= AV_PKT_FLAG_KEY;
        pkt.stream_index= st->index;
        pkt.data= video_outbuf;
        pkt.size= out_size;

        /* write the compressed frame in the media file */
        ret = av_interleaved_write_frame(oc, &pkt);
        av_free_packet(&pkt);
    } else {
        //printf("out_size=%d\n", out_size);
        ret = 0;
        //printf("should this ever happen?\n");
    }
    if (ret != 0) {
        fprintf(stderr, "Error while writing video frame\n");
        exit(1);
    }
}

int main(int argc, char **argv) {
    int r;
    struct img *array;
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVStream *video_st = NULL;
    int i;
    int n;

    av_log_set_level(AV_LOG_VERBOSE);
    av_register_all();
    avfilter_register_all();

    r = cmdline_parser(argc, argv, &args);

    char *tmp;
    tmp = get_some_pic(args.images_dir_arg);
    init_sizes(tmp);
    free(tmp);

    pts_step = FMT_TIMEBASE_DEN / args.frame_rate_arg;

    /* auto detect the output format from the name. default is
       mpeg. */
    fmt = av_guess_format(NULL, args.output_file_arg, NULL);
    if (!fmt) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        fmt = av_guess_format("mpeg", NULL, NULL);
    }
    if (!fmt) {
        fprintf(stderr, "Could not find suitable output format\n");
        exit(1);
    }

    /* allocate the output media context */
    oc = avformat_alloc_context();
    if (!oc) {
        fprintf(stderr, "Memory error\n");
        exit(1);
    }
    oc->oformat = fmt;
    snprintf(oc->filename, sizeof(oc->filename), "%s", args.output_file_arg);

    /* add the audio and video streams using the default format codecs
       and initialize the codecs */
    if (fmt->video_codec == CODEC_ID_NONE) {
        printf("guessed format doesnt assume video?\n");
        exit(1);
    }

    fmt->video_codec = (!strcasecmp(args.vcodec_arg, "h264") ? CODEC_ID_H264 : CODEC_ID_FLV1);
    video_st = add_video_stream(oc, fmt->video_codec);
    av_dump_format(oc, 0, args.output_file_arg, 1);

    open_video(oc, video_st);
    if (avio_open(&oc->pb, args.output_file_arg, URL_WRONLY) < 0) {
        fprintf(stderr, "Could not open '%s'\n", args.output_file_arg);
        exit(1);
    }


    AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    AVFilter *buffersink = avfilter_get_by_name("nullsink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    filter_graph = avfilter_graph_alloc();

    char filter_args[50];
    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(filter_args, sizeof(filter_args), "%d:%d:%d:%d:%d:%d:%d",
            global_width, global_height, video_st->codec->pix_fmt,
            video_st->codec->time_base.num, video_st->codec->time_base.den,
            video_st->codec->sample_aspect_ratio.num, video_st->codec->sample_aspect_ratio.den);
    r = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
            filter_args, NULL, filter_graph);
    if (r < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        return r;
    }

    /* buffer video sink: to terminate the filter chain. */
    enum PixelFormat pix_fmts[] = { PIX_FMT_YUV420P, PIX_FMT_NONE };
    r = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
            NULL, pix_fmts, filter_graph);
    if (r < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        return r;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    char *filter_descr = args.filter_arg;

    r = avfilter_graph_parse(filter_graph, filter_descr,
#ifdef LIBAV
            inputs, outputs,
#else
            &inputs, &outputs,
#endif
            NULL);

    if (r < 0)
        return r;

    if ((r = avfilter_graph_config(filter_graph, NULL)) < 0)
        return r;




    n = imgs_names_durations(args.images_dir_arg, &array);
    assert(n > 0);

    avformat_write_header(oc, NULL);

    unsigned short percent = 0, prev_percent = 0;
    printf("0%% done");
    for(i = 0; i < n; i++) {
        printf("processing frame %d\n", i);
        if ((i > 0) && (array[i].ts == array[i-1].ts))
            continue; // avoid monotonity problems
        r = open_image_and_push_video_frame(&array[i], oc);
        if (r) {
            printf("Processing file %s/%s failed, throw away and proceed\n", args.images_dir_arg, array[i].filename);
        }
        percent = 100 * i / n;
        if (percent - prev_percent > 0/*threshold*/) {
            prev_percent = percent;
            printf("\r%d%% done", percent);
            fflush(stdout);
        }
    }
    while (1) {
        /* flush buffered remainings */
        r = avcodec_encode_video(oc->streams[0]->codec, video_outbuf, video_outbuf_size, NULL);
        if (r <= 0)
            break;
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.pts = frames_out++ * pts_step;
        pkt.dts = pkt.pts;
        pkt.duration = pts_step;
        printf("pkt.pts %"PRId64"\n", pkt.pts);
        if(oc->streams[0]->codec->coded_frame->key_frame)
            pkt.flags |= AV_PKT_FLAG_KEY;
        pkt.stream_index= 0;
        pkt.data= video_outbuf;
        pkt.size= r;

        /* write the compressed frame in the media file */
        r = av_interleaved_write_frame(oc, &pkt);
        assert(r == 0);
        av_free_packet(&pkt);
    }
    av_write_trailer(oc);
    av_dump_format(oc, 0, args.output_file_arg, 1);

    avcodec_close(video_st->codec);
    av_free(video_outbuf);
    av_freep(&oc->streams[0]->codec);
    av_freep(&oc->streams[0]);
    avio_close(oc->pb);
    av_free(oc);

    return 0;
}
