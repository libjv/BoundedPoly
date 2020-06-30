
//          Copyright Julien Vernay 2020.
// Distributed under the Boost Software License, Version 1.0.
//        (See accompanying file LICENSE or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#ifndef JVERNAY_UTILS_BOUNDED_POLY_HPP
#define JVERNAY_UTILS_BOUNDED_POLY_HPP

#include <new>
#include <type_traits>
#include <utility>

namespace jv {

//======== CONCEPTS =========/

/// Type trait to check if we can safely store `T` in `Storage`.
template <typename T, typename Storage> struct is_storable {
  static constexpr bool value = //
      sizeof(T) <= sizeof(Storage) && (alignof(Storage) % alignof(T) == 0) &&
      std::is_trivial_v<Storage>;
};

/// Helper alias for `is_storable`.
template <typename T, typename Storage>
constexpr bool is_storable_v = is_storable<T, Storage>::value;

/// Given a `Mover` for type `T`, checks if `A` can be moved with it.
template <typename T, typename Mover, typename A> struct is_movable {
  static constexpr bool value =
      std::is_nothrow_destructible_v<Mover> &&
      std::is_nothrow_invocable_r_v<void, Mover, T &&, void *> &&
      std::is_base_of_v<T, A> &&
      (std::is_empty_v<Mover> ||
       (std::is_nothrow_copy_assignable_v<Mover> &&
        std::is_nothrow_constructible_v<Mover, A const *>));
};

/// Helper alias for `is_movable`.
template <typename T, typename Mover, typename A>
constexpr bool is_movable_v = is_movable<T, Mover, A>::value;

namespace details {

template <typename Mover, bool Static> struct impl_MoverStorage {
  Mover mover_;

  template <typename T> constexpr impl_MoverStorage(T const *t) : mover_{t} {}
  constexpr impl_MoverStorage(Mover const &mover) : mover_{mover} {}

  constexpr void copy_mover(Mover const &mover) noexcept { mover_ = mover; }

  constexpr void swap_mover(impl_MoverStorage &other) noexcept {
    std::swap(mover_, other.mover_);
  }
};

template <typename Mover> struct impl_MoverStorage<Mover, true> {
  static constexpr Mover mover_{};

  template <typename T> constexpr impl_MoverStorage(T const *) noexcept {}
  constexpr impl_MoverStorage(Mover const &) noexcept {}

  template <typename T> constexpr void update_mover() noexcept {}

  constexpr void copy_mover(Mover const &) noexcept {}

  constexpr void swap_mover(impl_MoverStorage &) noexcept {}
};

// MoverStorage is an empty base class if Mover si empty.
// In this case, we store it as a static value.
template <typename Mover>
using MoverStorage = impl_MoverStorage<Mover, std::is_empty_v<Mover>>;

} // namespace details

// A stateful mover which supports every derived type of T.
template <typename T> class UniversalMover {
public:
  template <typename A>
  constexpr UniversalMover(A const *) noexcept
      : mover_{[](T &&src, void *dst) noexcept {
          A &&moveref = static_cast<A &&>(src);
          new (dst) A(std::move(moveref));
        }} {}

  constexpr void operator()(T &&src, void *dst) const noexcept {
    mover_(std::move(src), dst);
  }

private:
  using AbstractMover = void (*)(T &&src, void *dst) noexcept;
  AbstractMover mover_;
};

// A stateless mover which calls a polymorphic method of T.
// T must provide a virtual method of signature `void(void*) && noexcept`.
template <typename T, void (T::*Method)(void *) &&noexcept> class VirtualMover {
public:
  void operator()(T &&src, void *dst) const noexcept {
    (std::move(src).*Method)(dst);
  }
};

/// Type abstractor for polymorphic types.
template <typename Storage, typename Base,
          typename Mover = UniversalMover<Base>>
class BoundedPoly : details::MoverStorage<Mover> {
  using MoverStorage = details::MoverStorage<Mover>;

  static_assert(std::is_nothrow_destructible_v<Base>);
  static_assert(std::has_virtual_destructor_v<Base>);
  static_assert(is_storable_v<Base, Storage>);
  static_assert(std::is_nothrow_destructible_v<Mover>);
  static_assert(std::is_nothrow_invocable_r_v<void, Mover, Base &&, void *>);

public:
  /// can_handle

  template <typename T> struct can_handle {
    static constexpr bool value =
        std::is_nothrow_move_constructible_v<T> && std::is_base_of_v<Base, T> &&
        is_movable_v<Base, Mover, T> && is_storable_v<T, Storage>;
  };
  template <typename T>
  static constexpr bool can_handle_v = can_handle<T>::value;

  /// CONSTRUCTORS

  template <typename Derived>
  BoundedPoly(Derived &&derived)
      : MoverStorage{static_cast<Derived const *>(nullptr)} {
    static_assert(can_handle_v<Derived>);
    new (&storage_) Derived(std::forward<Derived>(derived));
  }

  template <typename Derived, typename... Args>
  BoundedPoly(std::in_place_type_t<Derived>, Args &&... args)
      : MoverStorage{static_cast<Derived const *>(nullptr)} {
    static_assert(std::is_constructible_v<Derived, Args...>);
    static_assert(can_handle_v<Derived>);
    new (&storage_) Derived(std::forward<Args>(args)...);
  }

  BoundedPoly(BoundedPoly const &) = delete;

  BoundedPoly(BoundedPoly &&other) noexcept
      : MoverStorage{(MoverStorage const &)other} {
    this->mover_(std::move(other.get()), &storage_);
  }

  /// ASSIGNMENT OPERATORS

  template <typename Derived>
  auto operator=(Derived derived) noexcept -> BoundedPoly & {
    static_assert(can_handle_v<Derived>);
    MoverStorage mover{static_cast<Derived const *>(nullptr)}; // may throw
    // we know that Derived is nothrow move constructible, so no
    // exception will be thrown
    get().~Base(); // erase the current stored value
    this->copy_mover(mover.mover_);
    new (&storage_) Derived(std::move(derived));
    return *this;
  }

  auto operator=(BoundedPoly const &) -> BoundedPoly & = delete;

  auto operator=(BoundedPoly &&other) noexcept -> BoundedPoly & {
    get().~Base();
    this->copy_mover(other.mover_);
    this->mover_(std::move(other.get()), &storage_);
    return *this;
  }

  /// emplace

  template <typename Derived, typename... Args>
  void emplace(Args &&... args) noexcept {
    static_assert(can_handle_v<Derived>);
    static_assert(std::is_nothrow_constructible_v<Derived, Args...>);
    MoverStorage mover{static_cast<Derived const *>(nullptr)}; // may throw
    get().~Base();
    this->copy_mover(mover.mover_);
    new (&storage_) Derived(std::forward<Args>(args)...);
  }

  /// DESTRUCTOR

  ~BoundedPoly() noexcept { get().~Base(); }

  /// get

  auto get() noexcept -> Base & { return reinterpret_cast<Base &>(storage_); }

  auto get() const noexcept -> Base const & {
    return reinterpret_cast<Base const &>(storage_);
  }

  /// DEREFERENCE OPERATORS

  auto operator*() noexcept -> Base & {
    return reinterpret_cast<Base &>(storage_);
  }

  auto operator*() const noexcept -> Base const & {
    return reinterpret_cast<Base const &>(storage_);
  }

  auto operator->() noexcept -> Base * {
    return reinterpret_cast<Base *>(&storage_);
  }

  auto operator->() const noexcept -> Base const * {
    return reinterpret_cast<Base const *>(&storage_);
  }

  /// swap

  void swap(BoundedPoly &other) noexcept {
    Storage tmp;
    this->mover_(std::move(get()), &tmp);
    other.mover_(std::move(other.get()), &storage_);
    this->mover_(std::move(reinterpret_cast<Base &>(tmp)), &other.storage_);
    reinterpret_cast<Base &>(tmp).~Base();
    this->swap_mover(other);
  }

private:
  Storage storage_;
};

template <typename Storage, typename Base,
          void (Base::*Method)(void *dst) &&noexcept>
using BoundedPolyVM = BoundedPoly<Storage, Base, VirtualMover<Base, Method>>;

} // namespace jv

#endif