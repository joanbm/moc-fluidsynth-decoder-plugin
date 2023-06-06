# MOC (Music on Console) FluidSynth plugin

[![Build and push Docker image](https://github.com/joanbm/moc-fluidsynth-plugin/actions/workflows/docker_build_push.yml/badge.svg)](https://github.com/joanbm/moc-fluidsynth-plugin/actions/workflows/docker_build_push.yml)

A plugin to add support for playing [MIDI](https://en.wikipedia.org/wiki/MIDI) files using [FluidSynth](https://www.fluidsynth.org/) to the [MOC (Music on Console)](https://moc.daper.net/) player.

## Why?

MOC already supports playing MIDI files using the [libTiMidity](https://sourceforge.net/projects/libtimidity/) library. However, this library is relatively unpopular and does not support [SoundFont (.sf2) files](https://en.wikipedia.org/wiki/SoundFont).

In contrast, FluidSynth is available on all major Linux distributions and supports SoundFont (.sf2) files.

## Dependencies

* FluidSynth >= 2.0.0
* libsmf >= 1.3 (optional: support for duration and seeking)

To compile the plugin, you will need the development versions of those libraries along with basic C build tools (`binutils`, `gcc`, etc.), `pkg-config` and `wget`.

For Debian/Ubuntu, the packages you need are called `build-essential`, `pkgconf`, `wget`, `libfluidsynth-dev` and `libsmf-dev`.
For Arch Linux, the packages you need are: `base-devel`, `wget`, `fluidsynth` and `libsmf`.

## Build and installation instructions

You can either compile and install the plugin separately from MOC, or build it alongside the rest of MOC.

### As a standalone plugin

This is the easiest way to build and install the plugin. Simply run:

```sh
make
sudo make install
```

And the plugin should have been compiled and installed.

Unfortunately, this approach has a (relatively minor) con, which stems from the fact that MOC does not have great support for plugins built separately from the MOC source code.
In particular, you will not be able to set the SoundFont in the MOC configuration file, but rather you will need to use an environment variable.

### As part of MOC

If you are building MOC yourself, you can include this plugin as part of the MOC source code tree, so that it gets compiled alongside the rest of the source code.

For instructions on how to build MOC, refer to the [official README](https://moc.daper.net/node/87).
Additionally, consider examining how your distribution builds MOC (e.g. the [PKGBUILD](https://gitlab.archlinux.org/archlinux/packaging/packages/moc/-/blob/main/PKGBUILD) for Arch Linux) in order to see which packages need to be installed, which plugins are included by default, any additional workarounds/patches that need to be used, etc.

Once you know how to build MOC, you need to do the following steps after downloading the source code and before running `./configure`:

1. Add the plugin by applying the patch over the MOC code base (tested with MOC 2.5.2, but any future version should work):

  ```sh
  foo@bar ~/moc-2.5.2$ patch -Np1 -i ~/moc-fluidsynth-plugin/0001-Add-FluidSynth-decoder-plugin.patch
  ```

2. Regenerate the Autotools build files:

  ```sh
  foo@bar ~/moc-2.5.2$ autoreconf -fiv
  ```

3. When running `./configure`, include the `--with-fluidsynth` option:

  ```sh
  foo@bar ~/moc-2.5.2$ ./configure --with-fluidsynth `# ...add your specific options here`
  ```

This example shows what the entire build process should look like:

```sh
foo@bar ~$ wget http://ftp.daper.net/pub/soft/moc/stable/moc-2.5.2.tar.bz2
foo@bar ~$ echo "f3a68115602a4788b7cfa9bbe9397a9d5e24c68cb61a57695d1c2c3ecf49db08  moc-2.5.2.tar.bz2" | sha256sum -c
foo@bar ~$ tar xf moc-2.5.2.tar.bz2
foo@bar ~$ cd moc-2.5.2/
foo@bar ~/moc-2.5.2$ patch -Np1 -i ~/moc-fluidsynth-plugin/0001-Add-FluidSynth-decoder-plugin.patch
foo@bar ~/moc-2.5.2$ autoreconf -fiv
foo@bar ~/moc-2.5.2$ ./configure --with-fluidsynth --prefix=$HOME/my-moc-install --disable-cache --without-ffmpeg
foo@bar ~/moc-2.5.2$ make -j"$(nproc)"
foo@bar ~/moc-2.5.2$ make install
foo@bar ~/moc-2.5.2$ ~/my-moc-install/bin/mocp
```

## Usage instructions

By default, the plugin will try to use the system's default SoundFont.
For example, `/usr/share/sounds/sf2/default-GM.sf2` on Debian/Ubuntu, or `/usr/share/soundfonts/default.sf2` in Arch Linux.

To use a different SoundFont:

- If you have built the plugin as a standalone plugin, define the `MOC_FLUIDSYNTH_SOUNDFONT` environment variable.

  For example, launch MOC like this:

  ```sh
  MOC_FLUIDSYNTH_SOUNDFONT=/usr/share/soundfonts/FluidR3_GM.sf2 mocp
  ```

  You can add the variable to your `~/.profile` file so that you do not have to define it every time.

- If you have built the plugin as part of MOC, set the `FluidSynth_SoundFont` configuration option to the path of your SoundFont in `~/.moc/config`, for example:

  ```
  FluidSynth_SoundFont=/usr/share/soundfonts/FluidR3_GM.sf2
  ```

Other than this, you should be able to just browse to a folder containing MIDI files and play them.

NOTE: If you have built MOC with both the `libTiMidity` and `FluidSynth` plugins (which I don't recommend), you should be able to pick which one to use with the `PreferredDecoders` configuration option.

## With Docker or Podman

A `Dockerfile` containing MOC and the plugin is provided to ease both testing and also simple usage.

On a typical Linux system with Docker and PulseAudio installed, the following should start an instance of MOC and you should be able to play a sample MIDI file:

```sh
docker build . -t moc-fluidsynth-plugin
docker run --rm -it \
    -v "$XDG_RUNTIME_DIR"/pulse/native:/pulse-native \
    -e PULSE_SERVER=unix:/pulse-native \
    moc-fluidsynth-plugin
```

You can mount your SoundFont and music inside the container as follows:

```sh
docker build . -t moc-fluidsynth-plugin
podman run --rm -it \
    -v "$XDG_RUNTIME_DIR"/pulse/native:/pulse-native \
    -e PULSE_SERVER=unix:/pulse-native \
    -v "/path/to/my/music/folder":/app/music:ro \
    -v "/path/to/my/soundfont.sf2":/usr/share/sounds/sf2/default-GM.sf2:ro \
    moc-fluidsynth-plugin
```

If you run into trouble forwarding the audio inside the container, take a look at [this article](https://github.com/mviereck/x11docker/wiki/Container-sound:-ALSA-or-Pulseaudio) and [this article](https://joonas.fi/2020/12/audio-in-docker-containers-linux-audio-subsystems-spotifyd/).
If you are using SELinux (e.g. on Fedora), also take a look at [this article](https://github.com/mviereck/x11docker/wiki/SELinux).

## TODO List

* Implement support for duration and seeking with just FluidSynth, without requiring libsmf.
  This is currently not possible due to various limitations and quirks of FluidSynth.
  See: https://github.com/libsdl-org/SDL_mixer/issues/519, https://github.com/FluidSynth/fluidsynth/issues/1151, https://github.com/FluidSynth/fluidsynth/issues/648.
* Submit this plugin upstream
