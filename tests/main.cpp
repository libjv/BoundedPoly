
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <jv/bounded-poly.hpp>

#include <array>
#include <memory>
#include <type_traits>

TEST_CASE("is_storable type_trait", "[utils][bounded-poly][is_storable]") {
    struct Trivial {
        std::aligned_storage_t<250> storage;
    };

    struct NotTrivial : Trivial {
        ~NotTrivial() noexcept {}
    };

    CHECK(jv::is_storable_v<int, long long int>);
    CHECK(jv::is_storable_v<int[2], int[3]>);
    CHECK(jv::is_storable_v<void*, Trivial>);
    CHECK(jv::is_storable_v<NotTrivial, Trivial>);

    CHECK(!jv::is_storable_v<int[2], int>);            // bad size
    CHECK(!jv::is_storable_v<int, char[sizeof(int)]>); // bad alignment
    CHECK(!jv::is_storable_v<int,
                             NotTrivial>); // size ok, alignment ok, but not
                                           // trivial
}

TEST_CASE("is_movable type_trait", "[utils][bounded-poly][is_movable]") {
    CHECK(!jv::is_movable_v<int, void, void>);

    struct Base {};
    struct NotBase {};
    struct Derived : Base {};
    struct NotDerived {};

    SECTION("stateless movers") {
        auto mover = [](Base&&, void*) noexcept { /* should do something */ };
        using Mover = decltype(mover);
        auto except_mover = [](Base&&, void*) { /* not marked "noexcept" */ };
        using ExceptMover = decltype(except_mover);
        auto notmover = [](NotBase&&, void*) {};
        using NotMover = decltype(notmover);
        struct NonMovableMover : Mover {
            NonMovableMover(NonMovableMover&&) = delete;
            auto operator=(NonMovableMover &&) -> NonMovableMover& = delete;
        };

        CHECK(jv::is_movable_v<Base, Mover, Derived>);
        CHECK(!jv::is_movable_v<Base, ExceptMover, Derived>); // must not throw
        CHECK(!jv::is_movable_v<Base, Mover, NotDerived>);    // must be derived
        CHECK(!jv::is_movable_v<Base, NotMover,
                                Derived>); // must have appropriate signature
        CHECK(!jv::is_movable_v<NotBase, Mover,
                                Derived>); // must have correct base
        CHECK(jv::is_movable_v<Base, NonMovableMover,
                               Derived>); // a stateless mover does not require
                                          // move semantics
    }

    SECTION("stateful movers") {
        using Mover = jv::UniversalMover<Base>;
        using NotMover = jv::UniversalMover<NotBase>;
        struct NonMovableMover : Mover {
            using Mover::Mover; // inheriting constructors
            NonMovableMover(NonMovableMover&&) = delete;
            auto operator=(NonMovableMover &&) -> NonMovableMover& = delete;
        };
        struct NonConstructibleMover : Mover {
            // we should inherit constructor from Mover
        };

        CHECK(jv::is_movable_v<Base, Mover, Derived>);
        CHECK(!jv::is_movable_v<Base, NotMover,
                                Derived>); // must have appropriate signature
        CHECK(!jv::is_movable_v<NotBase, Mover,
                                Derived>); // must have correct base
        CHECK(!jv::is_movable_v<Base, NonMovableMover,
                                Derived>); // stateful mover requires move
                                           // semantics
        CHECK(!jv::is_movable_v<Base, NonConstructibleMover, Mover>);
    }
}

struct Base {
    Base() = default;
    Base(int i_) : i{new int(i_)} {}
    Base(Base&&) = default;
    auto operator=(Base &&) -> Base& = default;
    // making it virtual to expect actual type with dynamic_cast
    virtual ~Base() noexcept {}
    std::unique_ptr<int> i;
};
struct Derived : Base {
    Derived() = default;
    Derived(int i_, float f_) : Base{i_}, f{new float(f_)} {}
    std::unique_ptr<float> f;
};

using Storage = std::aligned_union_t<0, Base, Derived>;

TEST_CASE("UniversalMover", "[utils][bounded-poly][UniversalMover]") {
    // using unique_ptr to keep trace of move operations

    using Mover = jv::UniversalMover<Base>;
    REQUIRE(jv::is_movable_v<Base, Mover, Derived>);

    Base base{42};
    Derived derived{50, 0};

    Storage dst;

    Mover mover{static_cast<Base const*>(nullptr)};
    // mover can move Base but not Derived
    REQUIRE(*base.i == 42);
    mover(std::move(base), &dst);
    REQUIRE(typeid(reinterpret_cast<Base&>(dst)) == typeid(Base));
    CHECK(base.i == nullptr);
    CHECK(*reinterpret_cast<Base&>(dst).i == 42);
    reinterpret_cast<Base&>(dst).~Base();

    mover = Mover{static_cast<Derived const*>(nullptr)};

    REQUIRE(*derived.i == 50);
    REQUIRE(*derived.f == 0);
    mover(std::move(derived), &dst);
    REQUIRE(typeid(reinterpret_cast<Base&>(dst)) == typeid(Derived));
    CHECK(derived.i == nullptr);
    CHECK(derived.f == nullptr);
    CHECK(*reinterpret_cast<Derived&>(dst).i == 50);
    CHECK(*reinterpret_cast<Derived&>(dst).f == 0);

    // giving back ownership to `derived`
    derived = std::move(reinterpret_cast<Derived&>(dst));
    REQUIRE(*derived.i == 50);
    REQUIRE(*derived.f == 0);
    reinterpret_cast<Derived&>(dst).~Derived();

    // SLICING: We will move Derived using Mover configured for Base
    mover = Mover{static_cast<Base const*>(nullptr)};
    mover(std::move(derived), &dst);
    CHECK(derived.i == nullptr);
    CHECK(derived.f != nullptr); // SLICING! `Derived::f` was not moved
    // `dst` is a `Base`, and not a `Derived`...
    REQUIRE(typeid(reinterpret_cast<Base&>(dst)) == typeid(Base));
    CHECK(*reinterpret_cast<Base&>(dst).i == 50);

    reinterpret_cast<Base&>(dst).~Base();
    // Because of slicing, we need to carefully update `Mover` each time that we
    // change the type of the stored value.
}

TEST_CASE("BoundedPoly + UniversalMover",
          "[utils][bounded-poly][BoundedPoly]") {

    using PolyBase = jv::BoundedPoly<Storage, Base>;
    // using a stateful mover adds overhead to the storage
    REQUIRE(sizeof(PolyBase) > sizeof(Storage));

    struct BigDerived : Base {
        Storage big_space;
    }; // sizeof(BigDerived) < sizeof(Base) because Base is not empty.
    REQUIRE(PolyBase::can_handle_v<BigDerived> == false);

    PolyBase a{std::in_place_type_t<Base>{}, 42};
    PolyBase b{Derived{}};

    REQUIRE(*a->i == 42);
    REQUIRE(b->i == nullptr);
    Base& a_ref = a.get();
    Base& b_ref = b.get();
    REQUIRE(typeid(a_ref) == typeid(Base));
    REQUIRE(typeid(b_ref) == typeid(Derived));
    b = std::move(a);
    REQUIRE(typeid(a_ref) == typeid(Base));
    REQUIRE(typeid(b_ref) == typeid(Base));
    REQUIRE(a->i == nullptr);
    REQUIRE(*b->i == 42);

    struct NoticeableDestructor : Base {
        NoticeableDestructor(bool& b) noexcept : wasDestructed(b) {}
        NoticeableDestructor(NoticeableDestructor&& other) noexcept
            : wasDestructed{other.wasDestructed} {}
        ~NoticeableDestructor() noexcept override { wasDestructed = true; }
        bool& wasDestructed;
    };

    bool was_destructed = false;
    a.emplace<NoticeableDestructor>(was_destructed);
    REQUIRE(typeid(a_ref) == typeid(NoticeableDestructor));
    a = Base{};
    REQUIRE(typeid(a_ref) == typeid(Base));
    REQUIRE(was_destructed);

    b = Derived{};
    REQUIRE(typeid(b_ref) == typeid(Derived));
    a.swap(b);
    REQUIRE(typeid(a_ref) == typeid(Derived));
    REQUIRE(typeid(b_ref) == typeid(Base));
}

struct Base2 {
    Base2() = default;
    Base2(int i_) : i{new int(i_)} {}
    Base2(Base2&&) = default;
    auto operator=(Base2 &&) -> Base2& = default;
    // making it virtual to expect actual type with dynamic_cast
    virtual ~Base2() noexcept = default;

    // moving is part of the vtable
    virtual void move_to(void* dst) && noexcept {
        new (dst) Base2(std::move(*this));
    }

    std::unique_ptr<int> i;
};
struct Derived2 : Base2 {
    Derived2() = default;
    Derived2(int i_, float f_) : Base2{i_}, f{new float(f_)} {}

    void move_to(void* dst) && noexcept override {
        new (dst) Derived2(std::move(*this));
    }

    std::unique_ptr<float> f;
};

using Storage2 = std::aligned_union_t<0, Base2, Derived2>;

TEST_CASE("BoundedPoly + Stateless Mover",
          "[utils][bounded-poly][BoundedPoly]") {
    // This test_case is copy-pasted from the previous test_case to demonstrate
    // that BoundedPoly semantics are the same regardless of the move strategy.

    using PolyBase = jv::BoundedPolyVM<Storage2, Base2, &Base2::move_to>;
    // using a stateless mover adds no overhead to the storage
    REQUIRE(sizeof(PolyBase) == sizeof(Storage2));

    PolyBase a{std::in_place_type_t<Base2>{}, 42};
    PolyBase b{Derived2{}};

    REQUIRE(*a->i == 42);
    REQUIRE(b->i == nullptr);
    Base2& a_ref = a.get();
    Base2& b_ref = b.get();
    REQUIRE(typeid(a_ref) == typeid(Base2));
    REQUIRE(typeid(b_ref) == typeid(Derived2));
    b = std::move(a);
    REQUIRE(typeid(a_ref) == typeid(Base2));
    REQUIRE(typeid(b_ref) == typeid(Base2));
    REQUIRE(a->i == nullptr);
    REQUIRE(*b->i == 42);

    struct NoticeableDestructor : Base2 {
        NoticeableDestructor(bool& b) noexcept : wasDestructed(b) {}
        NoticeableDestructor(NoticeableDestructor&& other) noexcept
            : wasDestructed{other.wasDestructed} {}
        ~NoticeableDestructor() noexcept override { wasDestructed = true; }

        void move_to(void* dst) && noexcept override {
            new (dst) NoticeableDestructor(std::move(*this));
        }

        bool& wasDestructed;
    };

    bool was_destructed = false;
    a.emplace<NoticeableDestructor>(was_destructed);
    REQUIRE(typeid(a_ref) == typeid(NoticeableDestructor));
    a = Base2{};
    REQUIRE(typeid(a_ref) == typeid(Base2));
    REQUIRE(was_destructed);

    b = Derived2{};
    REQUIRE(typeid(b_ref) == typeid(Derived2));
    a.swap(b);
    REQUIRE(typeid(a_ref) == typeid(Derived2));
    REQUIRE(typeid(b_ref) == typeid(Base2));
}