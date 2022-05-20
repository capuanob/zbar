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

## Bug Fix: Afl Clang Fast is missing a symbolic link for the -lomp library
WORKDIR /usr/local/lib
RUN ln -s /usr/local/Cellar/llvm/4.0.0_1/lib/libomp.dylib libomp.dylib
RUN ldconfig

## Add source code to the build stage.
WORKDIR /
RUN git clone https://github.com/capuanob/zbar.git
WORKDIR /zbar
RUN git checkout mayhem


## Build
RUN autoreconf -vfi
RUN CC=afl-clang-fast CXX=afl-clang-fast++ ./configure
RUN CC=afl-clang-fast CXX=afl-clang-fast make -j$(nproc)
RUN CC=afl-clang-fast CXX=afl-clang-fast make install

##Make corpus directory
RUN mv /zbar/fuzz/corpus /corpus
#WORKDIR /
#RUN git clone https://github.com/dvyukov/go-fuzz-corpus.git
#RUN mkdir corpus && cp zbar/examples/*.png corpus
#RUN cp go-fuzz-corpus/png/corpus/* /corpus
#RUN cp go-fuzz-corpus/jpeg/corpus/* /corpus


##Reduce corpus
ENV AFL_MAP_SIZE=116288
RUN afl-cmin -i /corpus -o /corpus-rdx -- /usr/local/bin/zbarimg @@

# AFL
ENTRYPOINT ["afl-fuzz", "-i", "/corpus-rdx", "-o", "/out", "--"]
CMD ["/usr/local/bin/zbarimg", "-q", "@@"]
