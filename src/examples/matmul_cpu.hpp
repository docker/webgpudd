#ifndef MATMUL_CPU_H
#define MATMUL_CPU_H

void multiplyMatrices(uint32_t* a, uint32_t* b, uint32_t* res, uint32_t dim) {
    uint32_t batchsz = 64;
    std::vector<std::thread*> threads;

    for (uint32_t by = 0; by < dim; by += batchsz) {
        for (uint32_t bx = 0; bx < dim; bx += batchsz) {
            auto t = new std::thread([&, by, bx] {
                for (uint32_t i = by; i < by+batchsz; ++i) {
                    for (uint32_t j = bx; j < bx+batchsz; ++j) {
                        res[j + dim * i] = 0;
                        for (uint32_t k = 0; k < dim; ++k)
                            res[j + dim * i] += a[k + dim * i] * b[j + dim * k];
                    }
                }
            });
            threads.push_back(t);
        }
    }

    for (auto t : threads) {
        t->join();
        delete t;
    }
}

#endif /* MATMUL_CPU_H */
