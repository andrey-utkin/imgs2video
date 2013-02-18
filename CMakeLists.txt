cmake_minimum_required(VERSION 2.6)

project(imgs2video)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

if(CMAKE_BUILD_TYPE MATCHES Debug)
  add_definitions(-DDEBUG)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -ggdb -O0 -fno-inline")
endif(CMAKE_BUILD_TYPE MATCHES Debug)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/ffmpeg/install/lib)
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/external/ffmpeg/install/include
)

set_source_files_properties(version.h PROPERTIES GENERATED true)
add_custom_command(OUTPUT version.h COMMAND ./version.sh . version.h)

add_executable(imgs2video
  version.h
  src/imgs2video.c
  src/imgs2video_cmdline.c
)

add_executable(cat
  version.h
  src/cat.c
)

add_subdirectory(external/ffmpeg)
add_dependencies(imgs2video ffmpeg-local)
add_dependencies(cat ffmpeg-local)

set(FFMPEG_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/ffmpeg/install/lib)
set(X264_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/x264/install/lib)

add_library(avformat STATIC IMPORTED)
set_property(TARGET avformat PROPERTY IMPORTED_LOCATION "${FFMPEG_LIB_PATH}/libavformat.a")
add_library(avcodec STATIC IMPORTED)
set_property(TARGET avcodec PROPERTY IMPORTED_LOCATION "${FFMPEG_LIB_PATH}/libavcodec.a")
add_library(avfilter STATIC IMPORTED)
set_property(TARGET avfilter PROPERTY IMPORTED_LOCATION "${FFMPEG_LIB_PATH}/libavfilter.a")
add_library(avutil STATIC IMPORTED)
set_property(TARGET avutil PROPERTY IMPORTED_LOCATION "${FFMPEG_LIB_PATH}/libavutil.a")
add_library(swscale STATIC IMPORTED)
set_property(TARGET swscale PROPERTY IMPORTED_LOCATION "${FFMPEG_LIB_PATH}/libswscale.a")
add_library(swresample STATIC IMPORTED)
set_property(TARGET swresample PROPERTY IMPORTED_LOCATION "${FFMPEG_LIB_PATH}/libswresample.a")
add_library(x264 STATIC IMPORTED)
set_property(TARGET x264 PROPERTY IMPORTED_LOCATION "${X264_LIB_PATH}/libx264.a")

# The order is important here. Entries seem being processed from last to
# first. Place new entries BEFORE its dependencies.
target_link_libraries(imgs2video avfilter)
target_link_libraries(imgs2video swscale)
target_link_libraries(imgs2video swresample)
target_link_libraries(imgs2video avformat)
target_link_libraries(imgs2video avcodec)
target_link_libraries(imgs2video avutil)
target_link_libraries(imgs2video x264)
target_link_libraries(imgs2video pthread)  # taken from system, dynamic
target_link_libraries(imgs2video m)  # math lib, taken from system, dynamic

target_link_libraries(cat avfilter)
target_link_libraries(cat swscale)
target_link_libraries(cat swresample)
target_link_libraries(cat avformat)
target_link_libraries(cat avcodec)
target_link_libraries(cat avutil)
target_link_libraries(cat x264)
target_link_libraries(cat pthread)  # taken from system, dynamic
target_link_libraries(cat m)  # math lib, taken from system, dynamic
