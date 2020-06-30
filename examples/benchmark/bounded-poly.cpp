#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <jv/bounded-poly.hpp>

struct IUnaryOp {
    int rhs;
    IUnaryOp(int rhs_) noexcept : rhs(rhs_) {}

    virtual ~IUnaryOp() noexcept {}
    virtual void move_to(void* dst) && noexcept = 0;
    virtual void apply(int& lhs) const noexcept = 0;
};

struct Addition : IUnaryOp {
    using IUnaryOp::IUnaryOp; // inheriting constructor

    void apply(int& lhs) const noexcept override { lhs += rhs; }

    void move_to(void* dst) && noexcept override {
        new (dst) Addition(std::move(*this));
    }
};

struct Substraction : IUnaryOp {
    using IUnaryOp::IUnaryOp; // inheriting constructor

    void apply(int& lhs) const noexcept override { lhs -= rhs; }

    void move_to(void* dst) && noexcept override {
        new (dst) Substraction(std::move(*this));
    }
};

struct ExclusiveOr : IUnaryOp {
    using IUnaryOp::IUnaryOp; // inheriting constructor

    void apply(int& lhs) const noexcept override { lhs ^= rhs; }

    void move_to(void* dst) && noexcept override {
        new (dst) ExclusiveOr(std::move(*this));
    }
};

using UnaryOp = jv::BoundedPolyVM<
    std::aligned_union_t<0, Addition, Substraction, ExclusiveOr>, IUnaryOp,
    &IUnaryOp::move_to>;

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
            op->apply(accum);
        auto elapsed = now() - start;
        std::cout << "Result accum = " << accum << '\n';
        std::cout << "Evaluation pipeline took " << elapsed.count()
                  << " seconds.\n";
    }
}
