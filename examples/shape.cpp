
#include <iostream>

#include <jv/bounded-poly.hpp>

constexpr float PI = 3.14159265359f;

// tag::IShape[]
struct IShape {
    virtual ~IShape() noexcept {}
    virtual auto get_area() const noexcept -> float = 0;
    virtual void move_to(void*) && noexcept = 0;
};
// end::IShape[]

// tag::ShapeMaxSize[]
constexpr std::size_t ShapeMaxSize = 16;
// end::ShapeMaxSize[]

// tag::Shape[]
using Shape = jv::BoundedPolyVM<std::aligned_storage_t<ShapeMaxSize>, IShape,
                                &IShape::move_to>;
// end::Shape[]
// to specify the Shape type, we just needed to set a max size for storage, and
// to say which method must be called to move shapes.

// tag::Circle[]
struct Circle : IShape {
    float radius = 1;

    Circle(float r) noexcept : radius(r) {}

    auto get_area() const noexcept -> float override {
        return PI * radius * radius;
    }

    void move_to(void* dst) && noexcept override {
        new (dst) Circle(std::move(*this));
    }
};
// end::Circle[]

// tag::Rectangle[]
struct Rectangle : IShape {
    float width, height;

    Rectangle(float w, float h) noexcept : width(w), height(h) {}

    auto get_area() const noexcept -> float override { return width * height; }

    void move_to(void* dst) && noexcept override {
        new (dst) Rectangle(std::move(*this));
    }
};
// end::Rectangle[]

// tag::RectangleEx[]
struct RectangleEx : Rectangle {
    RectangleEx(float w, float h) noexcept : Rectangle(w, h) {}

    float angle = 0;
    std::uint32_t color = 0xFFFFFFFF;

    void move_to(void* dst) && noexcept override {
        new (dst) RectangleEx(std::move(*this));
    }
};

// RectangleEx is too large to be stored
static_assert(sizeof(RectangleEx) > ShapeMaxSize);
// end::RectangleEx[]

// tag::main[]
int main() {

    Shape shape = Circle{1.414}; // <1>
    std::cout << shape->get_area() << " (expected: about PI*2 = 6.28)\n";
    dynamic_cast<Circle&>(shape.get()).radius = 10; // <2>
    std::cout << shape->get_area() << " (expected: about PI*100 = 314)\n";

    shape.emplace<Rectangle>(3, 4); // <3>
    std::cout << shape->get_area() << " (expected: 3*4 = 12)\n";
    dynamic_cast<Rectangle&>(shape.get()).height *= 2; // <4>
    std::cout << shape->get_area() << " (expected: 3*8 = 24)\n";

    std::cout << "Can handle RectangleEx ? " << std::boolalpha
              << Shape::can_handle_v<RectangleEx> << " (expected: false)\n";

    return 0;
}
// end::main[]