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

#define FMT_TIMEBASE_DEN 1000 // FIXME: hardcode FLV container value

unsigned int frame_rate;
unsigned int pts_step;
unsigned int frames_out = 0;
unsigned int global_video_codec;
unsigned int global_width;
unsigned int global_height;

unsigned int extradata_size;
void *extradata;

int open_video_and_append(char *filename, AVFormatContext *video_output) {
    AVFormatContext *pFormatCtx = NULL;
    AVCodecContext *pCodecCtx;
    AVPacket packet;
    int r;

    if(avformat_open_input(&pFormatCtx, filename, NULL, NULL)) {
        printf("Can't open image file '%s'\n", filename);
        goto fail_open_file;
    }

    r = avformat_find_stream_info(pFormatCtx, NULL);
    assert(r >= 0);

    av_dump_format(pFormatCtx, 0, filename, 0);

    pCodecCtx = pFormatCtx->streams[0]->codec;

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

    while (1) {
        r = av_read_frame(pFormatCtx, &packet);
        if (r < 0) {
            printf("Frames exhausted\n");
            break;
        }
        if (packet.stream_index != 0)
            continue;

        packet.pts = frames_out++ * pts_step;
        packet.dts = packet.pts;
        printf("frame pts: %"PRId64"\n", packet.pts);

        r = av_interleaved_write_frame(video_output, &packet);
        if (r != 0) {
            fprintf(stderr, "Error while writing video frame\n");
            exit(1);
        }
        av_free_packet(&packet);
    }

    avcodec_close(pCodecCtx);
    av_close_input_file(pFormatCtx);

    return 0;

fail_check_video:
    avcodec_close(pCodecCtx);
    av_close_input_file(pFormatCtx);
fail_open_file:
    return 1;
}

void populate_video_params(const char* filename) {
    AVFormatContext *pFormatCtx = NULL;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVPacket packet;
    int r;

    if(avformat_open_input(&pFormatCtx, filename, NULL, NULL)) {
        printf("Can't open image file '%s'\n", filename);
        exit(1);
    }
    r = avformat_find_stream_info(pFormatCtx, NULL);
    assert(r >= 0);

    av_dump_format(pFormatCtx, 0, filename, 0);
    pCodecCtx = pFormatCtx->streams[0]->codec;

    // Find the decoder for the video stream
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (!pCodec) {
        printf("Codec not found %d\n", pCodecCtx->codec_id);
        exit(1);
    }

    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
        printf("Could not open codec\n");
        exit(1);
    }

    r = av_read_frame(pFormatCtx, &packet);
    assert(r >= 0);

    global_width = pCodecCtx->width;
    global_height = pCodecCtx->height;
    frame_rate = pFormatCtx->streams[0]->r_frame_rate.num;
    pts_step = FMT_TIMEBASE_DEN / frame_rate;
    global_video_codec = pCodecCtx->codec_id;
    extradata_size = pCodecCtx->extradata_size;
    if (extradata_size) {
        extradata = malloc(extradata_size);
        assert(extradata);
        memcpy(extradata, pCodecCtx->extradata, extradata_size);
    }

    printf("File '%s' has width %d, height %d, framerate %d assuming each input video has same\n", filename, global_width, global_height, frame_rate);

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
    st->r_frame_rate.den = 1;
    st->r_frame_rate.num = frame_rate;
    st->avg_frame_rate.den = 1;
    st->avg_frame_rate.num = frame_rate;

    st->time_base.den = FMT_TIMEBASE_DEN;
    st->time_base.num = 1;

    st->sample_aspect_ratio.num = 1;
    st->sample_aspect_ratio.den = 1;
    st->codec->sample_aspect_ratio.num = 1;
    st->codec->sample_aspect_ratio.den = 1;

    c = st->codec;
    c->codec_id = codec_id;
    c->codec_type = AVMEDIA_TYPE_VIDEO;

    /* resolution must be a multiple of two */
    c->width = global_width;
    c->height = global_height;
    c->time_base.den = frame_rate;
    c->time_base.num = 1;

    c->extradata_size = extradata_size;
    if (extradata_size)
        c->extradata = extradata;
//    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return st;
}

int main(int argc, char **argv) {
    int r;
    const char *filename;
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    AVStream *video_st = NULL;
    int i;

    av_register_all();

    if (argc < 4) {
        printf("usage: %s <output_file> -- <input1> [input2] [...] [inputN]\n"
               "By Andrey Utkin <andrey.krieger.utkin@gmail.com>\n"
               "\n", argv[0]);
        exit(1);
    }
    filename = argv[1];

    fmt = av_guess_format(NULL, filename, NULL);
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

    if (fmt->video_codec == CODEC_ID_NONE) {
        printf("guessed format doesnt assume video?\n");
        exit(1);
    }

    populate_video_params(argv[3]);

    fmt->video_codec = global_video_codec;
    video_st = add_video_stream(oc, fmt->video_codec);

    /* set the output parameters (must be done even if no
       parameters). */
    av_dump_format(oc, 0, filename, 1);

    if (avio_open(&oc->pb, filename, URL_WRONLY) < 0) {
        fprintf(stderr, "Could not open '%s'\n", filename);
        exit(1);
    }
    avformat_write_header(oc, NULL);

    char **array = &argv[3];
    for(i = 0; i < argc - 3; i++) {
        r = open_video_and_append(array[i], oc);
        if (r) {
            printf("Processing file %s failed, throw away and proceed\n", array[i]);
        }
    }
    av_write_trailer(oc);
    av_dump_format(oc, 0, filename, 1);

    avcodec_close(video_st->codec);
    av_freep(&oc->streams[0]->codec);
    av_freep(&oc->streams[0]);
    avio_close(oc->pb);
    av_free(oc);

    return 0;
}
