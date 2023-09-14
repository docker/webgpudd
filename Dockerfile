# syntax=docker/dockerfile:1

FROM ubuntu:22.04 as builder

RUN apt-get update && apt-get install -y git build-essential cmake python3 curl

WORKDIR /build
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git depot_tools
ENV PATH="/build/depot_tools:$PATH"
RUN git clone https://dawn.googlesource.com/dawn dawn
WORKDIR dawn
RUN python3 tools/fetch_dawn_dependencies.py --use-test-deps
RUN mkdir -p out/Release
WORKDIR out/Release
RUN cmake -D CMAKE_BUILD_TYPE=Release -D DAWN_USE_X11=OFF -D DAWN_ENABLE_DESKTOP_GL=OFF -D DAWN_ENABLE_VULKAN=OFF -D DAWN_ENABLE_OPENGLES=OFF ../..
RUN make -j8 dawn_proc dawn_wire dawn_common
WORKDIR /build
RUN mkdir webgpudd
WORKDIR /build/webgpudd
COPY Makefile .
COPY src src
COPY include include
RUN make -j libwebgpudd
RUN cp out/libwebgpudd.so /usr/lib/.
RUN cp include/*.h /usr/include/.
WORKDIR /
RUN rm -fr /build
WORKDIR /webgpudd-example
COPY src/examples/matmul.cpp matmul.cpp
COPY src/examples/README.md README.md
