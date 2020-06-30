#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <variant>

struct Addition {
    int rhs;
    Addition(int rhs_) noexcept : rhs(rhs_) {}

    void apply(int& lhs) const noexcept { lhs += rhs; }
};

struct Substraction {
    int rhs;
    Substraction(int rhs_) noexcept : rhs(rhs_) {}

    void apply(int& lhs) const noexcept { lhs -= rhs; }
};

struct ExclusiveOr {
    int rhs;
    ExclusiveOr(int rhs_) noexcept : rhs(rhs_) {}

    void apply(int& lhs) const noexcept { lhs ^= rhs; }
};

using UnaryOp = std::variant<Addition, Substraction, ExclusiveOr>;

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
            case 0: pipeline.push_back(Addition{rand()}); break;
            case 1: pipeline.push_back(Substraction{rand()}); break;
            case 2: pipeline.push_back(ExclusiveOr{rand()}); break;
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
            std::visit([&](auto op) { op.apply(accum); }, op);
        auto elapsed = now() - start;
        std::cout << "Result accum = " << accum << '\n';
        std::cout << "Evaluation pipeline took " << elapsed.count()
                  << " seconds.\n";
    }
}
