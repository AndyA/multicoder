DIST_SUBDIRS = m4 t libhls

SUBDIRS = . t libhls

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -Ilibhls

LIBAV_CFLAGS = \
	$(LIBAVCODEC_CFLAGS) $(LIBAVFORMAT_CFLAGS) $(LIBAVUTIL_CFLAGS) $(LIBSWSCALE_CFLAGS)

LIBAV_LDFLAGS = \
	$(LIBAVCODEC_LIBS) $(LIBAVFORMAT_LIBS) $(LIBAVUTIL_LIBS) $(LIBSWSCALE_LIBS)

lib_LTLIBRARIES = libmulticoder.la

include_HEADERS = multicoder.h

bin_PROGRAMS = multicoder multivod

libmulticoder_la_SOURCES = \
	mc_buffer.c \
	mc_buffer.h \
	mc_demux.c \
	mc_h264.c \
	mc_log.c \
	mc_model.c \
	mc_model.h \
	mc_mux_hls.c \
	mc_queue.c \
	mc_queue.h \
	mc_segname.c \
	mc_segname.h \
	mc_sequence.c \
	mc_sequence.h \
	mc_util.c \
	mc_util.h \
	multicoder.h

libmulticoder_la_CFLAGS = $(LIBAV_CFLAGS)
libmulticoder_la_LDFLAGS = -avoid-version $(LIBAV_LDFLAGS)

multicoder_SOURCES = multicoder.c multicoder.h
multicoder_CFLAGS = $(LIBAV_CFLAGS)
multicoder_LDFLAGS = $(LIBAV_LDFLAGS)
multicoder_LDADD = libmulticoder.la libhls/libhls.la

multivod_SOURCES = multivod.c multicoder.h
multivod_CFLAGS = $(LIBAV_CFLAGS)
multivod_LDFLAGS = $(LIBAV_LDFLAGS)
multivod_LDADD = libmulticoder.la libhls/libhls.la

test: all
	cd libhls && $(MAKE) test
	cd t && $(MAKE) test

hls/libhls.la:
	cd libhls && $(MAKE)

