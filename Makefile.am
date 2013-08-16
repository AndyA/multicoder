DIST_SUBDIRS = m4 t

SUBDIRS = . t

ACLOCAL_AMFLAGS = -I m4

LIBAV_CFLAGS = \
	$(LIBAVCODEC_CFLAGS) $(LIBAVFORMAT_CFLAGS) $(LIBAVUTIL_CFLAGS) $(LIBSWSCALE_CFLAGS)

LIBAV_LDFLAGS = \
	$(LIBAVCODEC_LIBS) $(LIBAVFORMAT_LIBS) $(LIBAVUTIL_LIBS) $(LIBSWSCALE_LIBS)

lib_LTLIBRARIES = libmulticoder.la

include_HEADERS = multicoder.h

bin_PROGRAMS = multicoder

libmulticoder_la_SOURCES = \
	mc_buffer.c \
	mc_buffer.h \
	mc_demux.c \
	mc_h264_decoder.c \
	mc_log.c \
	mc_mux_hls.c \
	mc_queue.c \
	mc_queue.h \
	mc_util.c \
	mc_util.h \
	multicoder.h

libmulticoder_la_CFLAGS = $(LIBAV_CFLAGS)
libmulticoder_la_LDFLAGS = -avoid-version $(LIBAV_LDFLAGS)

multicoder_SOURCES = multicoder.c multicoder.h
multicoder_CFLAGS = $(LIBAV_CFLAGS)
multicoder_LDFLAGS = $(LIBAV_LDFLAGS)
multicoder_LDADD = libmulticoder.la

test: all
	cd t && $(MAKE) test

