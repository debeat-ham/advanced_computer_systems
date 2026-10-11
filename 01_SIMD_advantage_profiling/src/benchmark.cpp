#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(__GNUC__)
#define NOINLINE __attribute__((noinline))
#define RESTRICT __restrict__
#else
#define NOINLINE
#define RESTRICT
#endif

template <typename T>
struct AlignedArray {
    explicit AlignedArray(std::size_t count) {
        void* raw = nullptr;
        if (posix_memalign(&raw, 64, count * sizeof(T)) != 0)
            throw std::bad_alloc();
        data.reset(static_cast<T*>(raw));
    }
    struct Free { void operator()(T* p) const { std::free(p); } };
    std::unique_ptr<T, Free> data;
    T* get() const { return data.get(); }
};

template <typename T>
NOINLINE void saxpy(const T* RESTRICT x, T* RESTRICT y,
                    T a, std::size_t n, std::size_t stride) {
    for (std::size_t i = 0; i < n; ++i)
        y[i * stride] = a * x[i * stride] + y[i * stride];
}

template <typename T>
NOINLINE void multiply(const T* RESTRICT x, const T* RESTRICT y,
                       T* RESTRICT z, std::size_t n, std::size_t stride) {
    for (std::size_t i = 0; i < n; ++i)
        z[i * stride] = x[i * stride] * y[i * stride];
}

template <typename T>
NOINLINE T dot(const T* RESTRICT x, const T* RESTRICT y,
               std::size_t n, std::size_t stride) {
    T sum = T(0);
    for (std::size_t i = 0; i < n; ++i)
        sum += x[i * stride] * y[i * stride];
    return sum;
}

volatile double observable_result = 0.0;

template <typename T>
int run(const std::string& kernel, std::size_t n, std::size_t stride,
        std::size_t offset, int trials) {
    if (n == 0 || stride == 0 || trials < 1 || offset > 64 ||
        n > (100000000ULL - offset) / stride) {
        std::cerr << "Invalid size, stride, offset, or trial count.\n";
        return 1;
    }
    const std::size_t count = offset + (n - 1) * stride + 1;
    AlignedArray<T> x_storage(count), y_storage(count), z_storage(count);
    T* x = x_storage.get() + offset;
    T* y = y_storage.get() + offset;
    T* z = z_storage.get() + offset;
    for (std::size_t i = 0; i < count; ++i) {
        x_storage.get()[i] = T(1);
        y_storage.get()[i] = T(2);
        z_storage.get()[i] = T(0);
    }
    const T a = T(2);
    auto reset_output = [&]() {
        if (kernel == "saxpy") {
            for (std::size_t i = 0; i < n; ++i) y[i * stride] = T(2);
        } else if (kernel == "mul") {
            for (std::size_t i = 0; i < n; ++i) z[i * stride] = T(0);
        }
    };
    auto execute = [&]() -> T {
        if (kernel == "saxpy") { saxpy(x, y, a, n, stride); return y[0]; }
        if (kernel == "mul") { multiply(x, y, z, n, stride); return z[0]; }
        return dot(x, y, n, stride);
    };
    if (kernel != "saxpy" && kernel != "mul" && kernel != "dot") {
        std::cerr << "Kernel must be saxpy, mul, or dot.\n";
        return 1;
    }
    for (int w = 0; w < 3; ++w) {
        reset_output();
        observable_result = static_cast<double>(execute());
    }
    std::vector<double> times;
    times.reserve(trials);
    T last_result = T(0);
    for (int t = 0; t < trials; ++t) {
        reset_output(); // Excluded from timing; same starting values each trial.
        const auto start = std::chrono::steady_clock::now();
        last_result = execute();
        const auto stop = std::chrono::steady_clock::now();
        observable_result = static_cast<double>(last_result);
        times.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
    }
    bool correct = true;
    if (kernel == "dot") {
        const double expected = 2.0 * static_cast<double>(n);
        correct = std::fabs(static_cast<double>(last_result) - expected)
                  <= std::max(0.01, expected * 0.001);
    } else {
        const T expected = kernel == "saxpy" ? T(4) : T(2);
        for (std::size_t i = 0; i < n; ++i) {
            const T actual = kernel == "saxpy" ? y[i * stride] : z[i * stride];
            if (actual != expected) { correct = false; break; }
        }
    }
    std::vector<double> sorted = times;
    std::sort(sorted.begin(), sorted.end());
    const double median = trials % 2 ? sorted[trials / 2]
        : (sorted[trials / 2 - 1] + sorted[trials / 2]) / 2.0;
    std::cerr << "Kernel=" << kernel << " type=" << (sizeof(T) == 4 ? "f32" : "f64")
              << " N=" << n << " stride=" << stride << " offset=" << offset
              << " median=" << std::fixed << std::setprecision(6) << median
              << " ms correctness=" << (correct ? "PASS" : "FAIL") << '\n';
    std::cout << "kernel,dtype,n,stride,offset,trial,time_ms,correct\n";
    for (int t = 0; t < trials; ++t)
        std::cout << kernel << ',' << (sizeof(T) == 4 ? "f32" : "f64") << ','
                  << n << ',' << stride << ',' << offset << ',' << t << ','
                  << std::fixed << std::setprecision(9) << times[t] << ','
                  << (correct ? 1 : 0) << '\n';
    return correct ? 0 : 2;
}

int main(int argc, char** argv) {
    if (argc < 4 || argc > 7) {
        std::cerr << "Usage: " << argv[0]
                  << " <saxpy|mul|dot> <f32|f64> <N> [stride=1] [offset=0] [trials=10]\n";
        return 1;
    }
    try {
        const std::string kernel = argv[1], dtype = argv[2];
        auto parse = [](const char* s) -> std::size_t {
            const std::string value(s);
            if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
                throw std::invalid_argument("Expected a nonnegative integer");
            return static_cast<std::size_t>(std::stoull(value));
        };
        const std::size_t n = parse(argv[3]);
        const std::size_t stride = argc > 4 ? parse(argv[4]) : 1;
        const std::size_t offset = argc > 5 ? parse(argv[5]) : 0;
        const std::size_t trials = argc > 6 ? parse(argv[6]) : 10;
        if (trials > 100000) throw std::invalid_argument("Too many trials");
        if (dtype == "f32") return run<float>(kernel, n, stride, offset, static_cast<int>(trials));
        if (dtype == "f64") return run<double>(kernel, n, stride, offset, static_cast<int>(trials));
        std::cerr << "Type must be f32 or f64.\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
