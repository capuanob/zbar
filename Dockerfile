# Build Stage
FROM aflplusplus/aflplusplus as builder

## Install build dependencies.
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y git make clang build-essential pkg-config imagemagick libgtk2.0-dev python3-dev autotools-dev autoconf autopoint libtool gcc libzbar-dev libomp-dev

## Install ImageMagick
WORKDIR /
RUN wget https://www.imagemagick.org/download/ImageMagick.tar.gz -O image_magick.tar.gz
RUN mkdir image_magick && tar xvzf image_magick.tar.gz -C image_magick --strip-components=1
WORKDIR image_magick
RUN CC=afl-clang-fast CXX=afl-clang-fast++ ./configure && make -j$(nproc) && make install && ldconfig /usr/local/lib

## Add source code to the build stage.
WORKDIR /
RUN ls && git clone https://github.com/capuanob/zbar.git
WORKDIR /zbar
RUN git checkout mayhem


## Build
RUN autoreconf -vfi
RUN CC=afl-gcc-fast CXX=afl-g++-fast ./configure
RUN make -j$(nproc)
RUN make install

##Make debugging corpus directory
RUN mkdir /tests && echo seed > /tests/seed

ENV AFL_MAP_SIZE=1000000

RUN /zbar/zbarimg/.libs/zbarimg --version

# AFL
ENTRYPOINT ["afl-fuzz", "-m", "none", "-i", "/tests", "-o", "/out"]
CMD ["/zbar/zbarimg/.libs/zbarimg", "-q", "@@"]
