#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([fftest1], [0.1.1], [joe@levender.local])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([main.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

AC_CHECK_HEADERS([libavutil/avutil.h \
  libavcodec/avcodec.h \
  libavformat/avformat.h \
  libswresample/swresample.h])

AC_SEARCH_LIBS(deflate, [z])

# libswresample dep: libavutil
# libavcodec dep: libswresample libavutil libz
# libavformat dep: libavcodec libavutil
AC_SEARCH_LIBS(av_frame_alloc, [avutil])
AC_SEARCH_LIBS(swr_init, [swresample])
AC_SEARCH_LIBS(avcodec_free_context, [avcodec])
AC_SEARCH_LIBS(avformat_query_codec, [avformat])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([memmove])

AC_CONFIG_FILES([
	Makefile
])

AC_OUTPUT
