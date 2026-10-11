#include <iostream>
#include <vector>
#include <cmath>

//void saxpy( const float* x, float* y, float a){
void saxpy(const std::vector<float>& x, std::vector<float>& y, float a){
    for (std::size_t i = 0; i < x.size(); ++i) {
        y[i] = a * x[i] + y[i];
    }
}

int main(){
    const std::size_t N = 8;
    const float a = 2.0f;

    std::vector<float> x(N);
    std::vector<float> y(N);

    for (std::size_t i = 0; i < N; ++i) {
        x[i] = static_cast<float>(i + 1);
        y[i] = 10.0f * static_cast<float>(i + 1);
    }

    saxpy(x, y, a);

    bool correct = true;

    for (std::size_t i = 0; i < N; ++i) {
        float expected = 12.0f * static_cast<float>(i + 1);

        if (std::fabs(y[i] - expected) > 0.0001f) {
            correct = false;
        }

        std::cout << "y[" << i << "] = " << y[i] << '\n';
    }

    std::cout << (correct ? "PASS" : "FAIL") << '\n';

    return correct ? 0 : 1;
}