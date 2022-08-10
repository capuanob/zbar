# Build Stage
FROM --platform=linux/amd64 ubuntu:20.04 as builder

## Install build dependencies.
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y git make clang build-essential pkg-config imagemagick libgtk-3-dev python3-dev autotools-dev autoconf autopoint libtool gcc libzbar-dev wget

## Install ImageMagick
WORKDIR /
RUN wget https://imagemagick.org/archive/ImageMagick.tar.gz -O image_magick.tar.gz
RUN mkdir image_magick && tar xvzf image_magick.tar.gz -C image_magick --strip-components=1
WORKDIR image_magick
ENV CC=clang CXX=clang
RUN ./configure && make && make install && ldconfig /usr/local/lib

## Add source code to the build stage.
ADD . /zbar
WORKDIR /zbar

## Build
RUN autoreconf -vfi
RUN CC=clang ./configure
RUN make -j$(nproc) && make install

## Package Stage
FROM --platform=linux/amd64 ubuntu:20.04
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y libgtk2.0-0 libzbar0 wget


## Install ImageMagick
WORKDIR /
RUN wget https://imagemagick.org/archive/ImageMagick.tar.gz -O image_magick.tar.gz
RUN mkdir image_magick && tar xvzf image_magick.tar.gz -C image_magick --strip-components=1
WORKDIR image_magick
RUN ./configure && make && make install && ldconfig /usr/local/lib

##Make corpus directory
WORKDIR /
RUN mkdir testsuite && echo seed testsuite/seed
COPY --from=builder /zbar/zbarimg/.libs/zbarimg /zbarimg
