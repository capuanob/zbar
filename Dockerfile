# Build Stage
FROM aflplusplus/aflplusplus as builder

## Install build dependencies.
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y git make clang build-essential pkg-config imagemagick libgtk2.0-dev python3-dev autotools-dev autoconf autopoint libtool gcc libzbar-dev

## Install ImageMagick
WORKDIR /
RUN wget https://imagemagick.org/archive/ImageMagick.tar.gz -O image_magick.tar.gz
RUN mkdir image_magick && tar xvzf image_magick.tar.gz -C image_magick --strip-components=1
WORKDIR image_magick
RUN ./configure && make && make install && ldconfig /usr/local/lib

## Add source code to the build stage.
ADD . /zbar
WORKDIR /zbar

## Build
RUN autoreconf -vfi
RUN CC=afl-gcc ./configure
RUN make && make install

## Package Stage
FROM aflplusplus/aflplusplus as packager
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

# AFL
ENTRYPOINT ["afl-fuzz", "-i", "/testsuite", "-o", "/out"]
CMD ["/zbarimg", "-q", "@@"]
