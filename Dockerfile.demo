# syntax=docker/dockerfile:1

FROM ubuntu:22.04 as builder
RUN apt-get update && apt-get install -y build-essential vim
WORKDIR /dc23-demo
COPY src/examples/matmul.cpp matmul.cpp
COPY src/examples/*.hpp .
COPY src/examples/Makefile Makefile
COPY src/examples/HOW_IT_WORKS.md HOW_IT_WORKS.md
RUN g++ --std=c++17 -O2 matmul.cpp -o matmul
ENV PATH=/dc23-demo:$PATH
