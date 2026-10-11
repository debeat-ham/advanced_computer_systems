#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <vector>

void saxpy(const float* x, float* y, float a, std::size_t n){
    for (std::size_t i = 0; i < n; ++i) {
        y[i] = a * x[i] + y[i];
    }
}

int main(int argc, char* argv[]){
    // Number of elements, optionally supplied on command line.
    const std::size_t n =
        (argc > 1) ? std::strtoull(argv[1], nullptr, 10) : 1000000;

    if (n == 0) {
        std::cerr << "Array size must be positive.\n";
        return 1;
    }

    const float a = 2.0f;
    const int warmups = 3;
    const int trials = 10;

    std::vector<float> x(n);
    std::vector<float> y(n);

    // Initialization is NOT timed.
    for (std::size_t i = 0; i < n; ++i) {
        x[i] = 1.0f;
        y[i] = 3.0f;
    }

    // Warm-up runs: prepare caches and execution environment.
    for (int w = 0; w < warmups; ++w) {
        saxpy(x.data(), y.data(), a, n);
    }

    // Restore y before measurement.
    // This also keeps the expected result predictable.
    for (std::size_t i = 0; i < n; ++i) {
        y[i] = 3.0f;
    }

    std::vector<double> times_ms;
    times_ms.reserve(trials);

    for (int t = 0; t < trials; ++t) {
        auto start = std::chrono::steady_clock::now();

        saxpy(x.data(), y.data(), a, n);

        auto stop = std::chrono::steady_clock::now();

        const double ms =
            std::chrono::duration<double, std::milli>(
                stop - start).count();

        times_ms.push_back(ms);
    }

    // After 10 trials, y should equal 3 + 10*(2*1) = 23.
    const float expected = 3.0f + trials * a;

    bool correct = true;
    for (std::size_t i = 0; i < n; ++i) {
        if (std::fabs(y[i] - expected) > 0.0001f) {
            correct = false;
            break;
        }
    }


    std::sort(times_ms.begin(), times_ms.end());

    const double median_ms = (times_ms[4] + times_ms[5]) / 2.0; //10 trials, so median is average of 5th and 6th

    std::cout << "Elements: " << n << '\n';
    std::cout << "Median time: " << median_ms << " ms\n";

    std::cout << "Individual trials (ms): ";
    for (double t : times_ms) {
        std::cout << t << " ";
    }
    std::cout << '\n';


    
    std::cout << "Correctness: "
              << (correct ? "PASS" : "FAIL") << '\n';

    return correct ? 0 : 1;
}
