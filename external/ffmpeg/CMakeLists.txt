cmake_minimum_required(VERSION 2.8.4)
include(ExternalProject)

#project(ffmpeg-local)

option(DEBUG "If debug build" OFF)

set(FFMPEG_EXTRA_CFLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/../x264/install/include ")

if (${DEBUG})
  message("Debug build of ffmpeg")
  list(APPEND FFMPEG_OPTIM_OPTIONS --enable-debug --disable-optimizations --disable-yasm --disable-asm --disable-stripping)
  set(FFMPEG_EXTRA_CFLAGS "${FFMPEG_EXTRA_CFLAGS} -O0 -g -ggdb ")
else (${DEBUG})
  message("Optimized build of ffmpeg")
  list(APPEND FFMPEG_OPTIM_OPTIONS --disable-debug --enable-optimizations --enable-yasm --enable-asm)
endif (${DEBUG})

set(FFMPEG_EXTRA_LIBS
    "${CMAKE_CURRENT_SOURCE_DIR}/../x264/install/lib/libx264.a")
set(FFMPEG_EXTRA_LDFLAGS "-L${CMAKE_CURRENT_SOURCE_DIR}/../x264/install/lib ")

list (APPEND FFMPEG_CONFIGURE_OPTIONS
--enable-ffmpeg
--disable-protocols
--enable-protocol=file
--enable-protocol=pipe
--enable-protocol=rtp
--enable-protocol=udp
--disable-avdevice
--disable-indevs
--disable-outdevs
--disable-filters
--enable-filter=hqdn3d
--enable-filter=scale
--enable-filter=settb
--enable-filter=setpts
--enable-filter=fps
--disable-muxers
--enable-muxer=flv
--enable-muxer=mp4
--enable-muxer=webm
--enable-muxer=mpegts
--disable-demuxers
--enable-demuxer=image2
--enable-demuxer=concat
--enable-demuxer=flv
--enable-demuxer=mov
--enable-demuxer=rtp
--enable-demuxer=rtsp
--enable-demuxer=matroska
--enable-demuxer=mpegts
--disable-encoders
--enable-encoder=libx264
--enable-encoder=libvpx_vp8
--disable-decoders
--enable-decoder=mjpeg
--enable-decoder=h264
--enable-decoder=vp8
--disable-parsers
--enable-parser=h264
--enable-parser=mpeg4video
--enable-parser=mjpeg
--enable-static
--disable-bsfs
--enable-bsf=h264_mp4toannexb
--disable-stripping
--disable-postproc
--disable-doc
--disable-libgsm
--disable-zlib
--enable-libx264
--enable-libvpx
--enable-gpl
--extra-libs=${FFMPEG_EXTRA_LIBS}
--extra-cflags=${FFMPEG_EXTRA_CFLAGS}
--extra-ldflags=${FFMPEG_EXTRA_LDFLAGS}
${FFMPEG_OPTIM_OPTIONS}
--prefix=${CMAKE_CURRENT_SOURCE_DIR}/install
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../x264 ${CMAKE_CURRENT_SOURCE_DIR}/../x264)

ExternalProject_Add(ffmpeg-local
  DEPENDS x264_local
  GIT_REPOSITORY https://github.com/krieger-od/FFmpeg.git
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND ./configure ${FFMPEG_CONFIGURE_OPTIONS}
  )
