cmake_minimum_required(VERSION 2.8.4)
include(ExternalProject)

option(DEBUG "If debug build" OFF)

list(APPEND CONFIGURE_OPTIONS --disable-shared --enable-static --enable-pic
    --disable-cli --prefix=${CMAKE_CURRENT_SOURCE_DIR}/install)
if (${DEBUG})
  list(APPEND CONFIGURE_OPTIONS --enable-debug --disable-asm)
endif(${DEBUG})

ExternalProject_Add(x264_local
    URL http://download.videolan.org/pub/videolan/x264/snapshots/x264-snapshot-20130217-2245-stable.tar.bz2
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ./configure ${CONFIGURE_OPTIONS}
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    )
