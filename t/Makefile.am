TESTBIN = basic queue segname sequence

TESTPERL = basic.t

EXTRA_DIST = commands.json

LIBAV_CFLAGS = \
	$(LIBAVCODEC_CFLAGS) $(LIBAVFORMAT_CFLAGS) $(LIBAVUTIL_CFLAGS) $(LIBSWSCALE_CFLAGS)

LIBAV_LDFLAGS = \
	$(LIBAVCODEC_LIBS) $(LIBAVFORMAT_LIBS) $(LIBAVUTIL_LIBS) $(LIBSWSCALE_LIBS)

noinst_PROGRAMS = wrap $(TESTBIN)

noinst_LTLIBRARIES = libtest-support.la

libtest_support_la_SOURCES = tap.c tap.h framework.c framework.h jd_test.c jd_test.h
libtest_support_la_LDFLAGS = -static $(LIBAV_LDFLAGS)

LDADD = libtest-support.la ../libmulticoder.la
AM_LDFLAGS = $(LIBAV_LDFLAGS)
AM_CFLAGS = $(LIBAV_CFLAGS)

wrap_LDADD =
wrap_LDFLAGS =
wrap_CFLAGS =

test: $(TESTBIN)
	prove -e ./wrap $(TESTBIN) $(TESTPERL)

valgrind: $(TESTBIN)
	prove -e 'valgrind -q' $(addprefix ./,$(TESTBIN))
