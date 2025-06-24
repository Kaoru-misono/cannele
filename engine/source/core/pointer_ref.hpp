#pragma once

#include <typeindex>
#include <print>

namespace cannele::inline core
{
    // A Pointer Ref to provide type safe down casts, avoid using dynamic_cast.
    template <typename T>
    struct PointerRef final
    {
        using Type = T;
        using TypeIndex = std::type_index;

    private:
        Type* ptr{};
        TypeIndex native_type;

        template <typename T2>
        auto copy(PointerRef<T2> const& other) -> void;

        template <typename T2>
        friend struct PointerRef;

    public:
        PointerRef() noexcept;

        PointerRef(Type* other) noexcept;
        PointerRef(Type* other, TypeIndex other_type) noexcept;

        template <typename U>
        explicit PointerRef(U* other) noexcept;

        template <typename T2> requires (std::is_convertible_v<T2*, Type*>)
        explicit PointerRef(PointerRef<T2> const& other) noexcept;

        PointerRef(PointerRef const&) noexcept;
        PointerRef(PointerRef&&) noexcept;

        template <typename T2> requires (std::is_convertible_v<T2*, Type*>)
        auto operator=(PointerRef<T2> const& other) noexcept -> PointerRef&;

        auto operator=(PointerRef const&) noexcept -> PointerRef&;
        auto operator=(PointerRef&&) noexcept -> PointerRef&;

        auto operator->() noexcept -> Type*;
        auto operator->() const noexcept -> Type const*;
        auto operator*() noexcept -> Type&;
        auto operator*() const noexcept -> Type const&;

        explicit operator bool() const noexcept;

        auto get() const noexcept -> Type*;

        template <typename U>
        auto get() noexcept -> U*;

        template <typename T2> requires (std::is_convertible_v<Type*, T2*>)
        auto as() noexcept -> PointerRef<T2>;

        template <typename T2>
        auto unsafe_as() noexcept -> PointerRef<T2>;

        auto type() const noexcept -> TypeIndex;
    };
}

namespace cannele::inline core
{
    template <typename T>
    template <typename T2>
    auto PointerRef<T>::copy(PointerRef<T2> const& other) -> void
    {
        ptr = static_cast<Type*>(other.ptr);
        native_type = other.native_type;
    }

    template <typename T>
    PointerRef<T>::PointerRef() noexcept
        : native_type(typeid(Type))
    {}

    template <typename T>
    PointerRef<T>::PointerRef(Type* other) noexcept
        : ptr(other)
        , native_type(typeid(Type))
    {}

    template <typename T>
    PointerRef<T>::PointerRef(Type* other, TypeIndex other_type) noexcept
        : ptr(other)
        , native_type(other_type)
    {}

    template <typename T>
    template <typename U>
    PointerRef<T>::PointerRef(U* other) noexcept
        : ptr(static_cast<Type*>(other))
        , native_type(typeid(U))
    {}

    template <typename T>
    template <typename T2> requires (std::is_convertible_v<T2*, T*>)
    PointerRef<T>::PointerRef(PointerRef<T2> const& other) noexcept
        : ptr(static_cast<Type*>(other.ptr))
        , native_type(other.native_type)
    {}

    template <typename T>
    PointerRef<T>::PointerRef(PointerRef const&) noexcept = default;

    template <typename T>
    PointerRef<T>::PointerRef(PointerRef&&) noexcept = default;

    template <typename T>
    template <typename T2> requires (std::is_convertible_v<T2*, T*>)
    auto PointerRef<T>::operator=(PointerRef<T2> const& other) noexcept -> PointerRef&
    {
        copy(other);

        return *this;
    }

    template <typename T>
    auto PointerRef<T>::operator=(PointerRef const&) noexcept -> PointerRef& = default;

    template <typename T>
    auto PointerRef<T>::operator=(PointerRef&&) noexcept -> PointerRef& = default;

    template <typename T>
    auto PointerRef<T>::operator->() noexcept -> Type* { return ptr; }

    template <typename T>
    auto PointerRef<T>::operator->() const noexcept -> Type const* { return ptr; }

    template <typename T>
    auto PointerRef<T>::operator*() noexcept -> Type& { return *ptr; }

    template <typename T>
    auto PointerRef<T>::operator*() const noexcept -> Type const& { return *ptr; }

    template <typename T>
    PointerRef<T>::operator bool() const noexcept { return ptr != nullptr; }

    template <typename T>
    auto PointerRef<T>::get() const noexcept -> Type* { return ptr; }

    template <typename T>
    template <typename U>
    auto PointerRef<T>::get() noexcept -> U*
    {
        if (typeid(U) == native_type) {
            return static_cast<U*>(ptr);
        } else {
            std::println("Bad cast, expected {}, got {}", native_type.name(), typeid(U).name());
        }
        return nullptr;
    }

    template <typename T>
    template <typename T2> requires (std::is_convertible_v<T*, T2*>)
    auto PointerRef<T>::as() noexcept -> PointerRef<T2>
    {
        return PointerRef<T2>(*this);
    }

    template <typename T>
    template <typename T2>
    auto PointerRef<T>::unsafe_as() noexcept -> PointerRef<T2>
    {
        auto result = PointerRef<T2>();
        result.copy(*this);
        return result;
    }

    template <typename T>
    auto PointerRef<T>::type() const noexcept -> TypeIndex { return native_type; }
}

namespace std
{
    template <typename T>
    struct hash<cannele::core::PointerRef<T>>
    {
        auto operator () (cannele::core::PointerRef<T> const& ref) const -> size_t
        {
            return std::hash<T*>()(ref.get());
        }
    };

    template <typename T>
    struct equal_to<cannele::core::PointerRef<T>>
    {
        auto operator () (cannele::core::PointerRef<T> const& lhs, cannele::core::PointerRef<T> const& rhs) const -> bool
        {
            return lhs.get() == rhs.get() && lhs.type() == rhs.type();
        }
    };
}
