# syntax=docker/dockerfile:1

FROM alpine:3.17 as build
RUN apk add build-base linux-headers musl-dev git python3 curl cmake ninja

WORKDIR /build
RUN mkdir webgpudd

WORKDIR /build/webgpudd
COPY CMakeLists.txt .
COPY src src
COPY include include
COPY third_party third_party

WORKDIR /build/webgpudd/third_party/dawn
COPY 0001-DO-NOT-SUBMIT.patch .
COPY 0001-Fix-build-on-alpine.patch .
RUN patch -p1 < 0001-DO-NOT-SUBMIT.patch
RUN patch -p1 < 0001-Fix-build-on-alpine.patch

WORKDIR /build/webgpudd
RUN mkdir -p out/Release

WORKDIR out/Release
RUN cmake -D CMAKE_BUILD_TYPE=Release -D WEBGPUDD_BUILD_RUNTIME=OFF -G Ninja ../..
RUN ninja -j 8 webgpudd

WORKDIR /build

FROM scratch AS export
COPY --from=build /build/webgpudd/include/webgpudd-internal.h /include/.
COPY --from=build /build/webgpudd/out/Release/src/client/libwebgpudd.so /.
COPY --from=build /build/webgpudd/out/Release/third_party/dawn/gen/include/dawn/webgpu.h /include/.
