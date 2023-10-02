#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef BACKEND_GPU
#include <webgpu.h>

#ifdef BACKEND_DAWN_NATIVE
#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#endif /* BACKEND_DAWN_NATIVE */

#ifdef BACKEND_WEBGPUDD
#include <webgpudd.h>
#endif /* BACKEND_WEBGPUDD */

#include "matmul_gpu.hpp"
#endif /* BACKEND_GPU */

#include "matmul_cpu.hpp"

class SquareMatrix {
public:
    SquareMatrix(int dimension) : mData(nullptr), mDimension(dimension) {};
    ~SquareMatrix() {
        if (mData) {
            std::free(mData);
            mData = nullptr;
        }
    };

    int init() {
        auto data = (uint32_t *)std::malloc(mDimension * mDimension * sizeof(uint32_t));
        if (!data) {
            return -ENOMEM;
        }
        mData = data;
        return 0;
    };

    int init(uint32_t n) {
        if (init()) {
            return -ENOMEM;
        }
        for (uint32_t i = 0; i < mDimension * mDimension; ++i)
            mData[i] = n;
        return 0;
    };

    uint32_t *mData;

private:
    uint32_t mDimension;
};

enum BackendType {
    CPU,
#ifdef BACKEND_GPU
    GPU,
#endif /* BACKEND_GPU */
};

struct Config {
    BackendType backend;
    int repetitions;
};

int configure(Config &cfg, int argc, char** argv) {
    if (argc != 3) {
        return -EINVAL;
    }
    cfg.repetitions = atoi(argv[2]);
    if (cfg.repetitions < 1) {
        return -EINVAL;
    }
#ifdef BACKEND_GPU
    if (std::string(argv[1]) == "gpu") {
        cfg.backend = GPU;
        return 0;
    }
#endif /* BACKEND_GPU */
    if (std::string(argv[1]) == "cpu") {
        cfg.backend = CPU;
        return 0;
    }
    return -1;
}

int main(int argc, char** argv) {
    Config cfg;
    int ret;

    ret = configure(cfg, argc, argv);
    if (ret) {
        return ret;
    }

    constexpr uint32_t dimension = 1024;
    SquareMatrix res(dimension);
    SquareMatrix ma(dimension);
    SquareMatrix mb(dimension);

    if (res.init()) {
        return -1;
    }
    if (ma.init(2)) {
        return -1;
    }
    if (mb.init(3)) {
        return -1;
    }

#ifdef BACKEND_GPU
    if (cfg.backend == GPU) {
        WGPUContext ctx;
        ret = setUpWGPU(ctx);
        if (ret) {
            return ret;
        }

        while (cfg.repetitions--) {
            ret = multiplyMatricesGPU(ctx.instance, ctx.device, ctx.queue, ma.mData, mb.mData, res.mData, dimension);
            if (ret) {
                std::cerr << "GPU computation failed" << std::endl;
                return ret;
            }
        }

#ifdef BACKEND_WEBGPUDD
        finaliseWebGPUDD();
#endif /* BACKEND_WEBGPUDD */
    }
#endif /* BACKEND_GPU */

    if (cfg.backend == CPU) {
        while (cfg.repetitions--) {
            std::chrono::steady_clock::time_point beginCPU = std::chrono::steady_clock::now();
            multiplyMatrices(ma.mData, mb.mData, res.mData, dimension);
            std::chrono::steady_clock::time_point endCPU = std::chrono::steady_clock::now();
            std::cout << "[" << dimension << "x" << dimension << "] of " << ma.mData[0] << "s * " << "[" << dimension << "x" << dimension << "] of " << mb.mData[0] << "s" << std::endl;
            std::cout << "RESULT: [" << dimension << "x" << dimension << "] of " << res.mData[0] << "s" << std::endl;
            std::cout << "CPU time = " << std::chrono::duration_cast<std::chrono::microseconds>(endCPU - beginCPU).count() << "[µs]" << std::endl;
        }
    }

    return 0;
}
