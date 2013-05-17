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

struct concatenator {
    char **chunks;
    unsigned int n_chunks;
    AVFormatContext *out;
    unsigned int frame_rate;
    unsigned int pts_step;
    unsigned int frames_out;
    enum CodecID video_codec;
    unsigned int width;
    unsigned int height;
    unsigned int extradata_size;
    void *extradata;
};
typedef struct concatenator Concatenator;


static int global_init(void);
static int probe(Concatenator *cat);
static int open_out(Concatenator *cat, char *filename);
static int cat_process_chunk(Concatenator *cat, unsigned int i);

int main(int argc, char **argv) {
    int r;
    Concatenator *cat;
    unsigned int i;

    if (argc < 4) {
        av_log(NULL, AV_LOG_INFO, "usage: %s <output_file> -- <input1> [input2] [...] [inputN]\n"
               "By Andrey Utkin <andrey.krieger.utkin@gmail.com>\n"
               "\n", argv[0]);
        return 1;
    }

    r = global_init();
    assert(!r);

    cat = calloc(1, sizeof(*cat));
    assert(cat);

    cat->chunks = argv + 3;
    cat->n_chunks = argc - 3;

    r = probe(cat);
    if (r) {
        av_log(NULL, AV_LOG_ERROR, "Probing fail\n");
        return 1;
    }

    r = open_out(cat, argv[1]);
    if (r) {
        av_log(NULL, AV_LOG_ERROR, "Probing fail\n");
        return 1;
    }

    for(i = 0; i < cat->n_chunks; i++) {
        r = cat_process_chunk(cat, i);
        if (r < 0) {
            av_log(NULL, AV_LOG_ERROR, "Fatal error processing chunk, aborting\n");
            break;
        }
        if (r) {
            av_log(NULL, AV_LOG_ERROR, "Processing chunk %d (%s) failed, throw away and proceed\n", i, cat->chunks[i]);
        }
    }
    av_write_trailer(cat->out);
    av_dump_format(cat->out, 0, cat->out->filename, 1);

    avio_close(cat->out->pb);
    avformat_free_context(cat->out);

    return 0;
}

static int global_init(void) {
    av_register_all();
    return 0;
}

static int probe(Concatenator *cat) {
    AVFormatContext *pFormatCtx = NULL;
    AVCodecContext *pCodecCtx;
    int r;

    if(avformat_open_input(&pFormatCtx, cat->chunks[0], NULL, NULL)) {
        av_log(NULL, AV_LOG_INFO, "Can't open image file '%s'\n", cat->chunks[0]);
        return 1;
    }
    r = avformat_find_stream_info(pFormatCtx, NULL);
    if (r < 0) {
        av_log(NULL, AV_LOG_ERROR, "Probing %s fail (ret %d)\n", cat->chunks[0], r);
        return 1;
    }

    av_dump_format(pFormatCtx, 0, cat->chunks[0], 0);
    pCodecCtx = pFormatCtx->streams[0]->codec;

    cat->width = pCodecCtx->width;
    cat->height = pCodecCtx->height;
    cat->frame_rate = pFormatCtx->streams[0]->r_frame_rate.num;
    cat->pts_step = FMT_TIMEBASE_DEN / cat->frame_rate;
    cat->video_codec = pCodecCtx->codec_id;
    cat->extradata_size = pCodecCtx->extradata_size;
    if (cat->extradata_size) {
        cat->extradata = malloc(cat->extradata_size);
        assert(cat->extradata);
        memcpy(cat->extradata, pCodecCtx->extradata, cat->extradata_size);
    }

    av_log(NULL, AV_LOG_INFO, "File '%s' has width %d, height %d, framerate %d assuming each input video has same\n", cat->chunks[0], cat->width, cat->height, cat->frame_rate);

    avformat_close_input(&pFormatCtx);
    return 0;
}

static int open_out(Concatenator *cat, char *filename) {
    int r;
    AVOutputFormat *fmt;
    fmt = av_guess_format(NULL, filename, NULL);
    if (!fmt) {
        av_log(NULL, AV_LOG_ERROR, "Could not find suitable output format\n");
        return 1;
    }
    if (fmt->video_codec == CODEC_ID_NONE) {
        av_log(NULL, AV_LOG_ERROR, "guessed format %s (%s) doesnt assume video\n", fmt->name, fmt->long_name);
        return 1;
    }
    /* allocate the output media context */
    cat->out = avformat_alloc_context();
    if (!cat->out) {
        av_log(NULL, AV_LOG_ERROR, "Memory error\n");
        return 1;
    }
    cat->out->oformat = fmt;
    snprintf(cat->out->filename, sizeof(cat->out->filename), "%s", filename);

    fmt->video_codec = cat->video_codec;


    AVCodecContext *c;
    AVStream *st;

    st = avformat_new_stream(cat->out, avcodec_find_encoder(cat->video_codec));
    if (!st) {
        av_log(NULL, AV_LOG_ERROR, "Could not alloc stream\n");
        return 1;
    }
    // TODO check which settings are necessary
    st->r_frame_rate.den = 1;
    st->r_frame_rate.num = cat->frame_rate;

    st->time_base.den = FMT_TIMEBASE_DEN;
    st->time_base.num = 1;

    c = st->codec;
    c->codec_id = cat->video_codec;
    c->codec_type = AVMEDIA_TYPE_VIDEO;

    /* resolution must be a multiple of two */
    c->width = cat->width;
    c->height = cat->height;
    c->time_base.den = cat->frame_rate;
    c->time_base.num = 1;

    c->extradata_size = cat->extradata_size;
    if (cat->extradata_size)
        c->extradata = cat->extradata;

    if (avio_open(&cat->out->pb, filename, AVIO_FLAG_WRITE) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open '%s'\n", filename);
        return 1;
    }
    r = avformat_write_header(cat->out, NULL);
    if (r) {
        av_log(NULL, AV_LOG_ERROR, "avformat_write_header fail %d\n", r);
        return r;
    }
    av_dump_format(cat->out, 0, filename, 1);
    return 0;
}

static int cat_process_chunk(Concatenator *cat, unsigned int i) {
    char *filename = cat->chunks[i];
    AVFormatContext *pFormatCtx = NULL;
    AVCodecContext *pCodecCtx;
    AVPacket packet;
    int r;

    if(avformat_open_input(&pFormatCtx, filename, NULL, NULL)) {
        av_log(NULL, AV_LOG_INFO, "Can't open image file '%s'\n", filename);
        return 1;
    }

    r = avformat_find_stream_info(pFormatCtx, NULL);
    if (r < 0) {
        av_log(NULL, AV_LOG_ERROR, "avformat_find_stream_info fail %d\n", r);
        return 1;
    }

    av_dump_format(pFormatCtx, 0, filename, 0);

    pCodecCtx = pFormatCtx->streams[0]->codec;

    if ((int)cat->width != pCodecCtx->width ||
            (int)cat->height != pCodecCtx->height) {
        av_log(NULL, AV_LOG_INFO, "Dimensions of '%s' %dx%d do not match previous ones %dx%d\n",
                filename,
                pCodecCtx->width,
                pCodecCtx->height,
                cat->width,
                cat->height);
        return 1;
    }

    while (1) {
        r = av_read_frame(pFormatCtx, &packet);
        if (r < 0) {
            av_log(NULL, AV_LOG_INFO, "Frames exhausted\n");
            break;
        }
        if (packet.stream_index != 0)
            continue;

        packet.pts = cat->frames_out++ * cat->pts_step;
        packet.dts = packet.pts;
        av_log(NULL, AV_LOG_INFO, "frame pts: %"PRId64"\n", packet.pts);

        r = av_interleaved_write_frame(cat->out, &packet);
        if (r != 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while writing video frame\n");
            return 1;
        }
        av_free_packet(&packet);
    }

    avformat_close_input(&pFormatCtx);

    return 0;
}

