#ifdef LIBAV

#include "compat.h"

// add missing proc for compatibility
AVFilterInOut *avfilter_inout_alloc(void)
{
    return av_mallocz(sizeof(AVFilterInOut));
}

int avfilter_fill_frame_from_video_buffer_ref(AVFrame *frame,
        const AVFilterBufferRef *picref)
{
    if (!picref || !picref->video || !frame)
        return AVERROR(EINVAL);

    memcpy(frame->data,     picref->data,     sizeof(frame->data));
    memcpy(frame->linesize, picref->linesize, sizeof(frame->linesize));
    //frame->pkt_pos          = picref->pos;
    frame->interlaced_frame = picref->video->interlaced;
    frame->top_field_first  = picref->video->top_field_first;
    frame->key_frame        = picref->video->key_frame;
    frame->pict_type        = picref->video->pict_type;
    //frame->sample_aspect_ratio = picref->video->sample_aspect_ratio;

    return 0;
}
#endif

