# syntax=docker/dockerfile:1

FROM alpine:3.17 as build
RUN apk add build-base linux-headers musl-dev git python3 curl make cmake
WORKDIR /build
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git depot_tools
ENV PATH="/build/depot_tools:$PATH"
RUN git clone https://dawn.googlesource.com/dawn dawn
WORKDIR dawn
RUN git fetch origin
RUN git checkout fb97d04c0c2e2307dc11a9d9de4eab607af111f9
RUN python3 tools/fetch_dawn_dependencies.py
COPY 0001-DO-NOT-SUBMIT.patch .
COPY 0001-Fix-build-on-alpine.patch .
RUN git apply 0001-DO-NOT-SUBMIT.patch
RUN git apply 0001-Fix-build-on-alpine.patch
RUN mkdir -p out/Release
WORKDIR out/Release
RUN cmake -D CMAKE_BUILD_TYPE=Release -D DAWN_USE_X11=OFF -D DAWN_ENABLE_DESKTOP_GL=OFF -D DAWN_ENABLE_VULKAN=OFF -D DAWN_ENABLE_OPENGLES=OFF -D TINT_BUILD_TESTS=OFF ../..
RUN make -j8 dawn_proc dawn_wire dawn_common tint_lang_wgsl_reader_parser tint_lang_wgsl_features
WORKDIR /build
RUN mkdir webgpudd
WORKDIR /build/webgpudd
COPY Makefile .
COPY src src
COPY include include
RUN make -j libwebgpudd

FROM scratch AS export
COPY --from=build /build/webgpudd/out/libwebgpudd.so /
COPY --from=build /build/webgpudd/include/*.h /include/.
