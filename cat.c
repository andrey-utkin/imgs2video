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

#define FRAME_RATE 25
#define FMT_TIMEBASE_DEN 1000
#define PTS_STEP ( FMT_TIMEBASE_DEN / FRAME_RATE )

unsigned int frames_out = 0;
unsigned int global_width = 1280;
unsigned int global_height = 720; // FIXME hardcode

int open_video_and_append(char *filename, AVFormatContext *video_output) {
    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVPacket packet;
    int r;
    int frameFinished;

    if(av_open_input_file(&pFormatCtx, filename, NULL, 0, NULL)) {
        printf("Can't open image file '%s'\n", filename);
        goto fail_open_file;
    }

    r = av_find_stream_info(pFormatCtx);
    assert(r >= 0);

    dump_format(pFormatCtx, 0, filename, 0);

    pCodecCtx = pFormatCtx->streams[0]->codec;
//    // Find the decoder for the video stream
//    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
//    if (!pCodec) {
//        printf("Codec not found\n");
//        goto fail_find_decoder;
//    }
//    // Open codec
//    if(avcodec_open(pCodecCtx, pCodec)<0) {
//        printf("Could not open codec\n");
//        goto fail_avcodec_open;
//    }

    if (frames_out == 0)
        video_output->streams[0]->codec = pFormatCtx->streams[0]->codec;

    if (global_width == 0) {
        global_width = pCodecCtx->width;
        global_height = pCodecCtx->height;
        // FIXME: vvv
        //video_output->streams[0]->time_base = pFormatCtx->streams[0]->time_base;
        //video_output->streams[0]->codec->time_base = pFormatCtx->streams[0]->codec->time_base;

    } else {
        if (global_width != pCodecCtx->width ||
                global_height != pCodecCtx->height) {
            printf("Dimensions of '%s' %dx%d do not match previous ones %dx%d\n",
                    filename,
                    pCodecCtx->width,
                    pCodecCtx->height,
                    global_width,
                    global_height);
            goto fail_check_video;
        }
        printf("Dimensions match, proceed\n");
    }

    while (1) {
        r = av_read_frame(pFormatCtx, &packet);
        if (r < 0) {
            printf("Frames exhausted\n");
            break;
        }
        if (packet.stream_index != 0)
            continue;

        //r = avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
        //if (r <= 0) {
        //    printf("Failed to decode image\n");
        //    goto fail_decode;
        //}
        //pFrame->quality = 1;

        packet.pts = frames_out++ * PTS_STEP;
        packet.dts = packet.pts;
        printf("frame pts: %"PRId64"\n", packet.pts);

        r = av_interleaved_write_frame(video_output, &packet);
        if (r != 0) {
            fprintf(stderr, "Error while writing video frame\n");
            exit(1);
        }

        //av_free(pFrame);
        av_free_packet(&packet);
    }

    avcodec_close(pCodecCtx);
    av_close_input_file(pFormatCtx);

    return 0;
fail_decode:
    av_free_packet(&packet);
fail_read_frame:
    ;//av_free(pFrame);
fail_alloc_frame:
    ;
fail_check_video:
    avcodec_close(pCodecCtx);
fail_avcodec_open:
    ;
fail_find_decoder:
    av_close_input_file(pFormatCtx);
fail_open_file:
    return 1;
}
/*
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
        printf("Codec not found 1\n");
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
*/

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
    st->r_frame_rate.den = 1;
    st->r_frame_rate.num = FRAME_RATE;
    st->avg_frame_rate.den = 1;
    st->avg_frame_rate.num = FRAME_RATE;

    st->time_base.den = FMT_TIMEBASE_DEN;
    st->time_base.num = 1;

    c = st->codec;
    c->codec_id = codec_id;
    c->codec_type = AVMEDIA_TYPE_VIDEO;

    /* put sample parameters */
    c->bit_rate = 4000000;
    /* resolution must be a multiple of two */
    c->width = global_width;
    c->height = global_height;
    /* time base: this is the fundamental unit of time (in seconds) in terms
       of which frame timestamps are represented. for fixed-fps content,
       timebase should be 1/framerate and timestamp increments should be
       identically 1. */
    c->time_base.den = FRAME_RATE; // FIXME
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

    //c->qmin = global_qmin;
    //c->qmax = global_qmax;


    //c->noise_reduction = 50;
    //c->quantizer_noise_shaping = 50;

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

    st->sample_aspect_ratio.num = 1;
    st->sample_aspect_ratio.den = 1;
    st->codec->sample_aspect_ratio.num = 1;
    st->codec->sample_aspect_ratio.den = 1;

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
    //video_outbuf_size = 900000;
    //video_outbuf = av_malloc(video_outbuf_size);
}

int main(int argc, char **argv) {
    int r;
    const char *filename, *img_dir;
    char *tmp;
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVStream *video_st = NULL;
    int i;
    int n;

    av_register_all();

    if (argc < 4) {
        printf("usage: %s <output_file> -- <input1> [input2] [...] [inputN]\n"
               "By Andrey Utkin <andrey.krieger.utkin@gmail.com>\n"
               "\n", argv[0]);
        exit(1);
    }
    filename = argv[1];

    /*
    tmp = get_some_pic(img_dir);
    init_sizes(tmp);
    free(tmp);
    */

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

    fmt->video_codec = CODEC_ID_H264;
    video_st = add_video_stream(oc, fmt->video_codec);

    /* set the output parameters (must be done even if no
       parameters). */
    AVFormatParameters ap;
    ap.time_base.den = FRAME_RATE;//FMT_TIMEBASE_DEN;
    ap.time_base.num = 1;
    ap.width = global_width;
    ap.height = global_height;
    ap.pix_fmt = PIX_FMT_YUV420P;

    if (av_set_parameters(oc, &ap) < 0) {
        fprintf(stderr, "Invalid output format parameters\n");
        exit(1);
    }

    dump_format(oc, 0, filename, 1);

    //open_video(oc, video_st);
    if (url_fopen(&oc->pb, filename, URL_WRONLY) < 0) {
        fprintf(stderr, "Could not open '%s'\n", filename);
        exit(1);
    }
    //n = imgs_names_durations(img_dir, &array);
    //assert(n > 0);

    av_write_header(oc);

    unsigned short percent = 0, prev_percent = 0;
    //printf("0%% done");

    img_dir = ".";

    char **array = &argv[3];
    for(i = 0; i < argc - 3; i++) {
      //  printf("processing frame %d\n", i);
        //printf("given img %p, filename %s, ts %u, dur %u\n", &array[i],
        //        array[i].filename, array[i].ts, array[i].duration);
        r = open_video_and_append(array[i], oc);
        if (r) {
            printf("Processing file %s/%s failed, throw away and proceed\n", img_dir, array[i]);
        }
        //percent = 100 * i / n;
        //if (percent - prev_percent > 0/*threshold*/) {
          //  prev_percent = percent;
           // printf("\r%d%% done", percent);
            //fflush(stdout);
        //}
    }
    av_write_trailer(oc);
    dump_format(oc, 0, filename, 1);

    avcodec_close(video_st->codec);
    //av_free(video_outbuf);
    av_freep(&oc->streams[0]->codec);
    av_freep(&oc->streams[0]);
    url_fclose(oc->pb);
    av_free(oc);

    return 0;
}
