# syntax=docker/dockerfile:1

FROM ubuntu:22.04 as build
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y git build-essential cmake python3 curl ninja-build

WORKDIR /build
RUN mkdir webgpudd

WORKDIR /build/webgpudd
COPY CMakeLists.txt .
COPY src src
COPY include include
COPY third_party third_party

WORKDIR /build/webgpudd/third_party/dawn
COPY 0001-DO-NOT-SUBMIT.patch .
COPY 0001-Fix-build-on-debian-11.patch .
RUN patch -p1 < 0001-DO-NOT-SUBMIT.patch
RUN patch -p1 < 0001-Fix-build-on-debian-11.patch

WORKDIR /build/webgpudd
RUN mkdir -p out/Release

WORKDIR out/Release
RUN cmake -G Ninja -D CMAKE_BUILD_TYPE=Release -D DAWN_USE_X11=OFF ../..
RUN ninja -j 8 com.docker.webgpu-runtime

WORKDIR /build

FROM scratch AS export
COPY --from=build /build/webgpudd/out/Release/src/server/com.docker.webgpu-runtime /.
