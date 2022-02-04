# 
bin_PROGRAMS =

AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS =
AM_LDFLAGS =
LDADD = 
LDADD += $(top_srcdir)/mod.ld

if ENABLE_DEBUG
AM_CPPFLAGS += -g
else
AM_CPPFLAGS += -O -ffunction-sections -fdata-sections
AM_LDFLAGS += -Wl,--gc-sections
endif

if WITH_FFMPEG
bin_PROGRAMS += fftest1
fftest1_SOURCES = fftest1.c
endif

bin_PROGRAMS += evmtest1
evmtest1_SOURCES = evmtest1.cpp ev.c time.c misc.c cfg.c \
    mod_cli.c mod_test1_buf1.c

