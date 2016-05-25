# imgs2video

This toolkit is dedicated to continuous generation of timelapse video.

https://github.com/andrey-utkin/imgs2video

For any inquiry, feel free to file a ticket on Github.

## How to install

First, install prerequisites. For DEB-based distro, this is:

```
apt-get install git yasm cmake gcc g++ build-essential libvpx3 libvpx-dev
```

Then, build the utilities used internally (ffmpeg and ffprobe):

```
./build.sh
```

Then, make your config from supplied template:

```
cp config-sample.i2v config.i2v
vim config.i2v
```

While editing the config, pay attention to the following:

* You probably want to update `BASEPATH` setting. This setting is not used by itself by application, but some below settings in config template use it.

* Edit `FFMPEG` and `FFPROBE` to point to binaries built at `./build.sh` stage. You may try to run with system-provided binaries, but they may be crippled or outdated.

* Uncomment `NOCAT=yes` if you don't want daily videos concatenation to be performed by daemon (e.g. you want to do it elsewhere).

* Set `URL` to single-jpeg returning URL of your IP camera.

* If your camera is local and returns the image very fast, you probably want to slow down the retrieval process by setting `AFTER_GET_IMAGE_HOOK` to something like `sleep 5`

* As explained in config, uncomment `HV_AND_LOG_SYNC=yes` and edit that config section accordingly to enable automatic syncing of videos and logs onto another server by means of rsync over SSH. Don't forget to set up passphrase-less SSH login by public key from the local server to the remote one.
