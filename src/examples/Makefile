.PHONY: default with-gpu

default:
	g++ --std=c++17 -O2 matmul.cpp -o matmul

with-gpu:
	g++ --std=c++17 -O2 -D BACKEND_GPU -D BACKEND_WEBGPUDD matmul.cpp -o matmul -lwebgpudd
