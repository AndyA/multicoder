#!/bin/bash

CPPFLAGS='-I/opt/ffmpeg/include' \
LDFLAGS='-L/opt/ffmpeg/lib -Wl,-rpath /opt/ffmpeg/lib' \
  ./configure --disable-shared

# vim:ts=2:sw=2:sts=2:et:ft=sh

