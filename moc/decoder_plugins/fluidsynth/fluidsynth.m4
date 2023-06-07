dnl fluidsynth

AC_ARG_WITH(fluidsynth, AS_HELP_STRING([--without-fluidsynth],
                                       [Compile without FluidSynth]))

if test "x$with_fluidsynth" != "xno"
then
	PKG_CHECK_MODULES(fluidsynth, fluidsynth >= 2.0.0,
			   [AC_SUBST(fluidsynth_LIBS)
			   AC_SUBST(fluidsynth_CFLAGS)
			   want_fluidsynth="yes"
			   DECODER_PLUGINS="$DECODER_PLUGINS fluidsynth"],
			   [true])
fi

AM_CONDITIONAL([BUILD_fluidsynth], [test "$want_fluidsynth"])
AC_CONFIG_FILES([decoder_plugins/fluidsynth/Makefile])

dnl libsmf (optional: support for duration and seeking)

AC_ARG_WITH(smf, AS_HELP_STRING([--without-smf],
                                 [Compile without smf]))

if test "x$with_smf" != "xno"
then
	PKG_CHECK_MODULES(smf, smf >= 1.3,
			   [AC_SUBST(smf_LIBS)
			   AC_SUBST(smf_CFLAGS)
			   have_smf="yes"
			   AC_DEFINE([HAVE_SMF], [1], [Define if you have smf.])],
			   [true])
fi

AM_CONDITIONAL([HAVE_smf], [test "$have_smf"])
