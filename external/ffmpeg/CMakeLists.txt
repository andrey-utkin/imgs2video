cmake_minimum_required(VERSION 2.8.4)
include(ExternalProject)

#project(ffmpeg-local)

option(DEBUG "If debug build" OFF)

set(FFMPEG_EXTRA_CFLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/../x264/install/include ")

if (${DEBUG})
  message("Debug build of ffmpeg")
  list(APPEND FFMPEG_OPTIM_OPTIONS --enable-debug --disable-optimizations --disable-yasm --disable-asm)
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
--disable-avdevice
--disable-indevs
--disable-outdevs
--disable-filters
--enable-filter=hqdn3d
--enable-filter=scale
--disable-muxers
--enable-muxer=flv
--disable-demuxers
--enable-demuxer=image2
--enable-demuxer=flv
--disable-encoders
--enable-encoder=libx264
--disable-decoders
--enable-decoder=mjpeg
--enable-decoder=h264
--disable-parsers
--enable-static
--disable-bsfs
--disable-swresample
--disable-stripping
--disable-postproc
--disable-network
--disable-doc
--disable-libgsm
--disable-zlib
--enable-libx264
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
  URL http://ffmpeg.org/releases/ffmpeg-1.1.2.tar.gz
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND ./configure ${FFMPEG_CONFIGURE_OPTIONS}
  )
