# Build Stage
FROM aflplusplus/aflplusplus as builder

## Install build dependencies.
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y git make clang build-essential pkg-config imagemagick libgtk2.0-dev python3-dev autotools-dev autoconf autopoint libtool gcc libzbar-dev

## Install ImageMagick
WORKDIR /
RUN wget https://www.imagemagick.org/download/ImageMagick-7.1.0-34.tar.gz
RUN tar xvzf ImageMagick-7.1.0-34.tar.gz
WORKDIR ImageMagick-7.1.0-34
RUN ./configure && make && make install && ldconfig /usr/local/lib

## Add source code to the build stage.
WORKDIR /
RUN git clone https://github.com/capuanob/zbar.git
WORKDIR /zbar
RUN git checkout mayhem


## Build
RUN autoreconf -vfi
RUN CC=afl-gcc ./configure
RUN make && make install

##Make corpus directory
WORKDIR /
RUN git clone https://github.com/dvyukov/go-fuzz-corpus.git
RUN mkdir corpus && mv zbar/examples/*.png corpus
RUN cp go-fuzz-corpus/png/corpus/* /corpus
RUN cp go-fuzz-corpus/jpeg/corpus/* /corpus
RUN cp go-fuzz-corpus/smtp/corpus/* /corpus

# AFL
ENTRYPOINT ["afl-fuzz", "-i", "/corpus", "-o", "/out"]
CMD ["zbarimg", "-q", "@@"]
