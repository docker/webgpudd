# syntax=docker/dockerfile:1

FROM rockylinux:8.9 AS build
RUN dnf install -y 'dnf-command(config-manager)'
RUN dnf config-manager --set-enabled devel
RUN dnf install -y git cmake python3 curl ninja-build patch gcc gcc-c++

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
COPY 0001-Fix-build-on-RockyLinux-8.9.patch .
RUN patch -p1 < 0001-DO-NOT-SUBMIT.patch
RUN patch -p1 < 0001-Fix-build-on-debian-11.patch
RUN patch -p1 < 0001-Fix-build-on-RockyLinux-8.9.patch

WORKDIR /build/webgpudd
RUN mkdir -p out/Release

WORKDIR out/Release
RUN cmake -G Ninja -D CMAKE_BUILD_TYPE=Release -D DAWN_USE_X11=OFF ../..
RUN cmake --build . --target com.docker.webgpu-runtime --config Release

WORKDIR /build

FROM scratch AS export
COPY --from=build /build/webgpudd/out/Release/src/server/com.docker.webgpu-runtime /.
