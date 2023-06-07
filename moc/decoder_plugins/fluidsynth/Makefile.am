lib_LTLIBRARIES = libfluidsynth_decoder.la
libdir = $(plugindir)/$(DECODER_PLUGIN_DIR)
libfluidsynth_decoder_la_LDFLAGS = @PLUGIN_LDFLAGS@
libfluidsynth_decoder_la_LIBADD = $(fluidsynth_LIBS)
libfluidsynth_decoder_la_CFLAGS = $(fluidsynth_CFLAGS) -I$(top_srcdir)
if HAVE_smf
    libfluidsynth_decoder_la_LIBADD += $(smf_LIBS)
    libfluidsynth_decoder_la_CFLAGS += $(smf_CFLAGS)
endif
libfluidsynth_decoder_la_SOURCES = fluidsynth.c
