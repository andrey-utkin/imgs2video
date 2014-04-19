#/bin/bash
set -e
pushd external/ffmpeg
cmake .
make
popd
cp external/ffmpeg/install/bin/{ffmpeg,ffprobe} .
