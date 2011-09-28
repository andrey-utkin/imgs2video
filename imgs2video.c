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

#define FRAME_RATE 25 // TODO app parameter
#define FMT_TIMEBASE_DEN 1000
#define ENC_TIMEBASE_DEN FRAME_RATE
#define PTS_STEP ( FMT_TIMEBASE_DEN / FRAME_RATE )
#define DEFAULT_SPEEDUP_COEF 240
#define DEFAULT_QMIN 2
#define DEFAULT_QMAX 30
#define DEFAULT_CODEC CODEC_ID_FLV1

struct img {
    char *filename;
    time_t ts;
    unsigned int duration;
};


unsigned int speedup_coef = DEFAULT_SPEEDUP_COEF;
unsigned int global_qmin = DEFAULT_QMIN;
unsigned int global_qmax = DEFAULT_QMAX;
enum CodecID codec_id = DEFAULT_CODEC;

uint8_t *video_outbuf;
int video_outbuf_size;
int64_t pts = 0;
unsigned int global_width;
unsigned int global_height;

static void write_video_frame(AVFormatContext *oc, AVFrame *picture, unsigned int duration);

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
    unsigned int n_frames = realtime_duration * FRAME_RATE / speedup_coef;
    struct img *frames = calloc(n_frames, sizeof(struct img));
    int i, j;

    for (i = 0; i < n_frames; i++) {
        frames[i].ts = i * PTS_STEP;
        //printf("frame[%d].ts := %d\n", i, i * PTS_STEP);
        frames[i].duration = PTS_STEP;
    }

    frames[0].filename = array[0].filename;
    frames[n_frames-1].filename = array[n-1].filename;

    for (i = 0; i < n-1; i++) {
        /* for each image:
         * set the stop point at the cell in frames[] which is for next image
         * fill all frames[] from which is for it, up to which is for next
         */
        unsigned entry_pos = idx(frames, n_frames, (array[i].ts-array[0].ts) * FMT_TIMEBASE_DEN / speedup_coef);
        unsigned next_entry_pos = idx(frames, n_frames, (array[i+1].ts-array[0].ts) * FMT_TIMEBASE_DEN / speedup_coef);
        //printf("img %d, ts %"PRId64", position %d, of next is %d\n", i, (array[i].ts-array[0].ts) * FMT_TIMEBASE_DEN / speedup_coef, entry_pos, next_entry_pos);
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
    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame;
    AVPacket packet;
    int r;
    int frameFinished;

    if(av_open_input_file(&pFormatCtx, img->filename, NULL, 0, NULL)) {
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
    if(avcodec_open(pCodecCtx, pCodec)<0) {
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

    //printf("given frame ts: %u duration: %d\n", img->ts, img->duration);
    write_video_frame(video_output, pFrame, img->duration);
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
    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame;
    AVPacket packet;
    int r;
    int frameFinished;

    if(av_open_input_file(&pFormatCtx, imageFileName, NULL, 0, NULL)) {
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
    if(avcodec_open(pCodecCtx, pCodec)<0) {
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

    st = av_new_stream(oc, 0);
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
    st->r_frame_rate.num = FRAME_RATE;
    st->avg_frame_rate.den = 1;
    st->avg_frame_rate.num = FRAME_RATE;

    /* put sample parameters */
    c->bit_rate = 500000; // TODO app parameter
    /* resolution must be a multiple of two */
    c->width = global_width;
    c->height = global_height;
    /* time base: this is the fundamental unit of time (in seconds) in terms
       of which frame timestamps are represented. for fixed-fps content,
       timebase should be 1/framerate and timestamp increments should be
       identically 1. */

    c->time_base.den = ENC_TIMEBASE_DEN;

    c->time_base.num = 1;
    c->gop_size = 12; /* emit one intra frame every twelve frames at most */
    c->pix_fmt = PIX_FMT_YUV420P;
    if (c->codec_id == CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B frames */
        c->max_b_frames = 2;
    }
    if (c->codec_id == CODEC_ID_MPEG1VIDEO){
        /* Needed to avoid using macroblocks in which some coeffs overflow.
           This does not happen with normal video, it just happens here as
           the motion of the chroma plane does not match the luma plane. */
        c->mb_decision=2;
    }
    // some formats want stream headers to be separate
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    c->qmin = global_qmin;
    c->qmax = global_qmax;

    c->noise_reduction = 50;
    c->quantizer_noise_shaping = 50;

    if (c->codec_id == CODEC_ID_H264) {
        /* we must override 'broken defaults'.
         * Exactly this set is not considered very intelligent.
         * Just what has been found.
         * Feel free to tweak.
         */
        c->bit_rate = 500*1000;
        c->bit_rate_tolerance = c->bit_rate;
        c->rc_max_rate = 0;
        c->rc_buffer_size = 0;
        c->gop_size = 40;
        c->max_b_frames = 3;
        c->b_frame_strategy = 1;
        c->coder_type = 1;
        c->me_cmp = 1;
        c->me_range = 16;
        c->scenechange_threshold = 100500;
        c->flags |= CODEC_FLAG_LOOP_FILTER;
        c->me_method = ME_HEX;
        c->me_subpel_quality = 9;
        c->i_quant_factor = 0.71;
        c->qcompress = 0.6;
        c->max_qdiff = 4;
        c->directpred = 1;
        c->flags2 |= CODEC_FLAG2_FASTPSKIP;
    }

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
    if (avcodec_open(c, codec) < 0) {
        fprintf(stderr, "could not open codec\n");
        exit(1);
    }

    /* allocate output buffer */
    video_outbuf_size = 900000;
    video_outbuf = av_malloc(video_outbuf_size);
}

static void write_video_frame(AVFormatContext *oc, AVFrame *picture, unsigned int duration)
{
    AVStream *st = oc->streams[0];
    int out_size, ret;
    AVCodecContext *c = st->codec;

    //printf("picture.pts %"PRId64"\n", picture->pts);

    /* encode the image */
    out_size = avcodec_encode_video(c, video_outbuf, video_outbuf_size, picture);

    /* if zero size, it means the image was buffered */
    if (out_size > 0) {
        AVPacket pkt;
        av_init_packet(&pkt);

        pkt.pts = picture->pts;
        pkt.dts = pkt.pts;
        //printf("pkt.pts %"PRId64"\n", pkt.pts);
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
    const char *filename, *img_dir;
    char *tmp;
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVStream *video_st = NULL;
    int i;
    int n;

    av_register_all();

    if (argc < 2) {
        printf("usage: %s <output_file> [img_dir=./] [speedup_coef=240] [qmin=2] [qmax=30]\n"
               "Make video from series of pics, with linear time transform.\n"
               "The output format is automatically guessed according to the file extension.\n"
               "qmin, qmax (both unsigned int) are libavcodec encoding quantization params.\n"
               "Default qmin=2, qmax=30. For perfect and incredibly heavy video, qmin=1,qmax=1\n"
               "By Andrey Utkin <andrey.krieger.utkin@gmail.com>\n"
               "\n", argv[0]);
        exit(1);
    }
    filename = argv[1];
    img_dir = (argc >= 3) ? argv[2] : ".";
    if (argc > 3) {
        r = atoi(argv[3]);
        if (r > 0)
            speedup_coef = r;
        else {
            fprintf(stderr, "speedup_coef invalid\n");
            exit(1);
        }
    }
    if (argc > 4) {
        r = atoi(argv[4]);
        if (r > 0)
            global_qmin = r;
        else {
            fprintf(stderr, "qmin invalid\n");
            exit(1);
        }
    }
    if (argc > 5) {
        r = atoi(argv[5]);
        if (r > 0)
            global_qmax = r;
        else {
            fprintf(stderr, "qmax invalid\n");
            exit(1);
        }
    }
    printf("gonna work with output_file='%s', img_dir='%s', speedup_coef=%u, qmin=%u, qmax=%u\n",
            filename, img_dir, speedup_coef, global_qmin, global_qmax);

    tmp = get_some_pic(img_dir);
    init_sizes(tmp);
    free(tmp);

    /* auto detect the output format from the name. default is
       mpeg. */
    fmt = av_guess_format(NULL, filename, NULL);
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
    snprintf(oc->filename, sizeof(oc->filename), "%s", filename);

    /* add the audio and video streams using the default format codecs
       and initialize the codecs */
    if (fmt->video_codec == CODEC_ID_NONE) {
        printf("guessed format doesnt assume video?\n");
        exit(1);
    }

    fmt->video_codec = codec_id;
    video_st = add_video_stream(oc, fmt->video_codec);

    /* set the output parameters (must be done even if no
       parameters). */
    if (av_set_parameters(oc, NULL) < 0) {
        fprintf(stderr, "Invalid output format parameters\n");
        exit(1);
    }

    dump_format(oc, 0, filename, 1);

    open_video(oc, video_st);
    if (url_fopen(&oc->pb, filename, URL_WRONLY) < 0) {
        fprintf(stderr, "Could not open '%s'\n", filename);
        exit(1);
    }
    n = imgs_names_durations(img_dir, &array);
    assert(n > 0);

    av_write_header(oc);

    unsigned short percent = 0, prev_percent = 0;
    printf("0%% done");
    for(i = 0; i < n; i++) {
        printf("processing frame %d\n", i);
        if ((i > 0) && (array[i].ts == array[i-1].ts))
            continue; // avoid monotonity problems
        r = open_image_and_push_video_frame(&array[i], oc);
        if (r) {
            printf("Processing file %s/%s failed, throw away and proceed\n", img_dir, array[i].filename);
        }
        percent = 100 * i / n;
        if (percent - prev_percent > 0/*threshold*/) {
            prev_percent = percent;
            printf("\r%d%% done", percent);
            fflush(stdout);
        }
    }
    av_write_trailer(oc);

    avcodec_close(video_st->codec);
    av_free(video_outbuf);
    av_freep(&oc->streams[0]->codec);
    av_freep(&oc->streams[0]);
    url_fclose(oc->pb);
    av_free(oc);

    return 0;
}
