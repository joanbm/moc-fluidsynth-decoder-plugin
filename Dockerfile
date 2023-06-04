FROM debian:bullseye-slim AS builder

WORKDIR /app

# Download dependencies, MOC sources and copy the patch inside
RUN sed -i '/^deb / {p; s/deb /deb-src /}' /etc/apt/sources.list \
 && apt-get update \
 && apt-get build-dep --no-install-recommends -y moc \
 && apt-get install --no-install-recommends -y devscripts quilt libsmf-dev libfluidsynth-dev \
 && apt-get source moc \
 && rm -rf /var/lib/apt/lists/*
COPY 0001-Add-FluidSynth-decoder-plugin.patch .

# Apply the patch over the Debian package and build it
RUN cd ./*/ \
 && sed -i "s/^Build-Depends: /&libsmf-dev, libfluidsynth-dev, /g" debian/control \
 && echo "usr/lib/*/moc/decoder_plugins/libfluidsynth_decoder.so" >>debian/moc.install \
 && cp ../0001-Add-FluidSynth-decoder-plugin.patch debian/patches \
 && echo "0001-Add-FluidSynth-decoder-plugin.patch" >>debian/patches/series \
 && quilt push -a \
 && quilt refresh \
 && dch --nmu "Add FluidSynth decoder plugin" \
 && dpkg-buildpackage --unsigned-source --unsigned-changes

FROM debian:bullseye-slim

# For better caching, install first the official Debian MOC package to pull all dependencies
RUN apt-get update \
 && apt-get install -y --no-install-recommends moc moc-ffmpeg-plugin libfluidsynth2 libsmf0 pulseaudio \
 && rm -rf /var/lib/apt/lists/*

# Overwrite the official Debian MOC package with our version
COPY --from=builder /app/moc_*.deb /app/moc-ffmpeg-plugin_*.deb /pkgs/
RUN dpkg -i /pkgs/*

# Fixes showing files with non-ASCII names
ENV LANG="C.UTF-8"

# Set up a basic configuration for testing
RUN useradd -m user
USER user
WORKDIR /app/music
# This MIDI is made by user 'Dogman15' at English Wikipedia, licensed "CC BY 3.0" (https://creativecommons.org/licenses/by/3.0/).
ADD --chmod=666 https://upload.wikimedia.org/wikipedia/commons/5/55/MIDI_sample.mid .
CMD ["mocp"]
