CC := gcc
SMF_FLAGS = $(shell pkg-config --exists smf && pkg-config --cflags --libs smf && echo "-DHAVE_SMF")
# Try to detect MOC's decoder_plugins directory path... not the nicest code
PLUGINS_DIR := $(shell strings "$$(which mocp)" /usr/bin/mocp | grep -x /.*/decoder_plugins | head -1)

.PHONY: all clean install uninstall

all: libfluidsynth_decoder.so

moc-2.5.2.tar.bz2:
	wget http://ftp.daper.net/pub/soft/moc/stable/moc-2.5.2.tar.bz2
	echo "f3a68115602a4788b7cfa9bbe9397a9d5e24c68cb61a57695d1c2c3ecf49db08  moc-2.5.2.tar.bz2" | sha256sum -c

moc-2.5.2/decoder_plugins/fluidsynth/fluidsynth.c: moc-2.5.2.tar.bz2 0001-Add-FluidSynth-decoder-plugin.patch
	tar xf moc-2.5.2.tar.bz2
	patch -d moc-2.5.2 -Np1 -i ../0001-Add-FluidSynth-decoder-plugin.patch

libfluidsynth_decoder.so: moc-2.5.2/decoder_plugins/fluidsynth/fluidsynth.c
	$(CC) -fPIC -DSTANDALONE -Imoc-2.5.2 -shared $(shell pkg-config --cflags --libs fluidsynth) $(SMF_FLAGS) \
		moc-2.5.2/decoder_plugins/fluidsynth/fluidsynth.c -o libfluidsynth_decoder.so

install: libfluidsynth_decoder.so
	install -m 755 libfluidsynth_decoder.so $(PLUGINS_DIR)/libfluidsynth_decoder.so

uninstall:
	rm -f $(PLUGINS_DIR)/libfluidsynth_decoder.so

clean:
	rm -rf moc-2.5.2
	rm -f moc-2.5.2.tar.bz2
	rm -f libfluidsynth_decoder.so
