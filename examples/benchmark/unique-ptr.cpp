#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <memory>

struct IUnaryOp {
    int rhs;
    IUnaryOp(int rhs_) noexcept : rhs(rhs_) {}

    virtual ~IUnaryOp() noexcept {}
    virtual void apply(int& lhs) const noexcept = 0;
};

struct Addition : IUnaryOp {
    using IUnaryOp::IUnaryOp; // inheriting constructor

    void apply(int& lhs) const noexcept override { lhs += rhs; }
};

struct Substraction : IUnaryOp {
    using IUnaryOp::IUnaryOp; // inheriting constructor

    void apply(int& lhs) const noexcept override { lhs -= rhs; }
};

struct ExclusiveOr : IUnaryOp {
    using IUnaryOp::IUnaryOp; // inheriting constructor

    void apply(int& lhs) const noexcept override { lhs ^= rhs; }
};

using UnaryOp = std::unique_ptr<IUnaryOp>;

// std::chrono helpers
using Seconds = std::chrono::duration<double>;

using TimePoint =
    std::chrono::time_point<std::chrono::high_resolution_clock, Seconds>;

auto now() -> TimePoint { return std::chrono::high_resolution_clock::now(); }

int main() {
    constexpr int NbOp = 100'000'000;

    std::srand(std::time(nullptr));

    std::vector<UnaryOp> pipeline;
    pipeline.reserve(NbOp);

    // building pipeline
    {
        auto start = now();
        for (int i = 0; i < NbOp; ++i) {
            switch (i % 3) {
            case 0: pipeline.emplace_back(new Addition{rand()}); break;
            case 1: pipeline.emplace_back(new Substraction{rand()}); break;
            case 2: pipeline.emplace_back(new ExclusiveOr{rand()}); break;
            }
        }
        auto elapsed = now() - start;
        std::cout << "Building pipeline took " << elapsed.count()
                  << " seconds.\n";
    }
    // evaluation pipeline
    {
        int accum = 0;
        auto start = now();
        for (auto const& op : pipeline)
            op->apply(accum);
        auto elapsed = now() - start;
        std::cout << "Result accum = " << accum << '\n';
        std::cout << "Evaluation pipeline took " << elapsed.count()
                  << " seconds.\n";
    }
}
