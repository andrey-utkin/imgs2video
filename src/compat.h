#ifndef COMPAT_H
#define COMPAT_H

#ifdef LIBAV
#include <libavutil/mem.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/avfiltergraph.h>
// add missing proc for compatibility
AVFilterInOut *avfilter_inout_alloc(void);
int avfilter_fill_frame_from_video_buffer_ref(AVFrame *frame,
        const AVFilterBufferRef *picref);
#else
#include <libavfilter/avcodec.h>
#endif

#endif // COMPAT_H
