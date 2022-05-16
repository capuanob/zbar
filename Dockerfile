# Build Stage
FROM aflplusplus/aflplusplus as builder

## Install build dependencies.
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y git make clang build-essential pkg-config imagemagick libgtk2.0-dev python3-dev autotools-dev autoconf autopoint libtool gcc

## Add source code to the build stage.
WORKDIR /
RUN git clone https://github.com/capuanob/zbar.git
WORKDIR /zbar
RUN git checkout mayhem

## Build
RUN autoreconf -vfi
RUN ./configure
RUN make

# Package Stage
#FROM aflplusplus/aflplusplus

# Make corpus directory
#RUN mkdir /corpus
#COPY --from=builder /zbarimg/examples/*.png /corpus

# Copy executable
#COPY --from=builder /zbar/zbarimg/zbarimg /

#ENTRYPOINT ["afl-fuzz", "-i", "/corpus", "-o", "/out"]
#CMD ["/zbarimg", "-q", "@@",]
