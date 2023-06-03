# MOC (Music on Console) FluidSynth decoder plugin

A plugin to add support for playing [MIDI](https://en.wikipedia.org/wiki/MIDI) files using [FluidSynth](https://www.fluidsynth.org/) to the [MOC (Music on Console)](https://moc.daper.net/) player.

## Why?

MOC already supports playing MIDI files using the [libTiMidity](https://sourceforge.net/projects/libtimidity/) library. However, this library is relatively unpopular and does not support [SoundFont (.sf2) files](https://en.wikipedia.org/wiki/SoundFont).

On the other hand, FluidSynth is available on all major Linux distributions and supports SoundFont (.sf2) files.

## Dependencies

FluidSynth >= 2.0.0, plus all dependencies required to build MOC (see below).

## Installation instructions

Unfortunately, MOC plugins are not designed to be compiled separately from the main code, so you will have to rebuild MOC to include this plugin.

For instructions on how to build MOC, refer to the [official README](https://moc.daper.net/node/87).
It is also worth taking a look into how your distribution builds MOC (e.g. the [PKGBUILD](https://gitlab.archlinux.org/archlinux/packaging/packages/moc/-/blob/main/PKGBUILD) for Arch Linux) in order to see which packages need to be installed, which plugins are included by default, any additional workarounds/patched that need to be used, etc.

Once you know how to build MOC, you need to do the following steps after downloading the source code and before running `./configure`:

1. Add the plugin by applying the patch over the MOC code base (tested with MOC 2.5.2, but any future version should work):

  ```sh
  foo@bar ~/moc-2.5.2$ patch -Np1 -i ~/moc-fluidsynth-decoder-plugin/0001-Add-FluidSynth-decoder-plugin.patch
  ```

2. Regenerate the Autotools build files:

  ```sh
  foo@bar ~/moc-2.5.2$ autoreconf -fiv
  ```

3. When running `./configure`, include the `--with-fluidsynth` option:

  ```sh
  foo@bar ~/moc-2.5.2$ ./configure --with-fluidsynth `# ...add your specific options here`
  ```

This example shows how the entire build process should look like:

```sh
foo@bar ~$ wget http://ftp.daper.net/pub/soft/moc/stable/moc-2.5.2.tar.bz2
foo@bar ~$ tar xf moc-2.5.2.tar.bz2
foo@bar ~$ cd moc-2.5.2/
foo@bar ~/moc-2.5.2$ patch -Np1 -i ~/moc-fluidsynth-decoder-plugin/0001-Add-FluidSynth-decoder-plugin.patch
foo@bar ~/moc-2.5.2$ autoreconf -fiv
foo@bar ~/moc-2.5.2$ ./configure --with-fluidsynth --prefix=$HOME/my-moc-install --disable-cache --without-ffmpeg
foo@bar ~/moc-2.5.2$ make -j(nproc)
foo@bar ~/moc-2.5.2$ make install
foo@bar ~/moc-2.5.2$ ~/my-moc-install/bin/mocp
```

## Usage instructions

By default, the plugin will try to use the SoundFont you have in `/usr/share/soundfonts/default.sf2`.

To use a different SoundFont, set the `FluidSynth_SoundFont` configuration option to the path of your SoundFont in `.moc/config`, e.g.:

```
FluidSynth_SoundFont=/usr/share/soundfonts/FluidR3_GM.sf2
```

Other than this, you should be able to just browse to a folder containing MIDI files and play them.

NOTE: If you have built MOC with both the `libTiMidity` and `FluidSynth` decoder plugins (which I don't recommend), you should be able to pick which one to use with the `PreferredDecoders` configuration option.

## Known bugs and limitations

* Due to FluidSynth limitations, the length/duration shown for MIDI files will likely be incorrect, and seeking will also not work properly.
  See https://github.com/FluidSynth/fluidsynth/issues/1151 for more detail.

## TODO List

* Submit this plugin upstream
