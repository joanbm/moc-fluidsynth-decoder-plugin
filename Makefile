CC := gcc
SMF_FLAGS = $(shell pkg-config --exists smf && pkg-config --cflags --libs smf && echo "-DHAVE_SMF")
# Try to detect MOC's decoder_plugins directory path... not the nicest code
PLUGINS_DIR := $(shell strings "$$(which mocp)" /usr/bin/mocp | grep -x /.*/decoder_plugins | head -1)

.PHONY: all clean install uninstall

all: libfluidsynth_decoder.so

libfluidsynth_decoder.so: moc/decoder_plugins/fluidsynth/fluidsynth.c
	$(CC) -Wall -Wextra -fPIC -DSTANDALONE -Imoc -shared moc/decoder_plugins/fluidsynth/fluidsynth.c \
		$(shell pkg-config --cflags --libs fluidsynth) $(SMF_FLAGS) -o libfluidsynth_decoder.so

install: libfluidsynth_decoder.so
	install -m 755 libfluidsynth_decoder.so $(PLUGINS_DIR)/libfluidsynth_decoder.so

uninstall:
	rm -f $(PLUGINS_DIR)/libfluidsynth_decoder.so

0001-Add-FluidSynth-decoder-plugin.patch:
	rm -rf moc-original
	svn checkout -r 3005 svn://svn.daper.net/moc/trunk moc-original
	rm -rf moc-original/.svn
	git diff --no-index moc-original moc | \
		sed -e 's|a/moc-original/|a/|g' -e 's|b/moc/|b/|g' \
		> 0001-Add-FluidSynth-decoder-plugin.patch

clean:
	rm -rf moc-original libfluidsynth_decoder.so 0001-Add-FluidSynth-decoder-plugin.patch
