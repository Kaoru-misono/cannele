#pragma once

#include <print>

namespace cannele::inline core
{
    template <typename  T>
    using RefCountPtr = std::shared_ptr<T>;

    // Not use any more.
    template <typename T>
    concept ref_count_type = requires (T t) {
        t.add_ref();
        t.release();
    };

    template <ref_count_type T>
    struct RefCountPtr_Deprecated
    {
        using ReferencedType = T;

    protected:

        ReferencedType* ptr{};
        template <ref_count_type U> friend struct RefCountPtr_Deprecated;

    public:

        RefCountPtr_Deprecated() noexcept;
        RefCountPtr_Deprecated(std::nullptr_t) noexcept;

        // Construct from a raw pointer will not increment the reference count.
        // Make sure the raw pointer's ref count is at least 1 when you use this constructor.
        template <ref_count_type U> requires (std::is_convertible_v<U*, T*>) explicit
        RefCountPtr_Deprecated(U* other) noexcept;

        RefCountPtr_Deprecated(RefCountPtr_Deprecated const& other) noexcept;
        RefCountPtr_Deprecated(RefCountPtr_Deprecated&& other) noexcept;

        template <ref_count_type U> requires (std::is_convertible_v<U*, T*>)
        RefCountPtr_Deprecated(RefCountPtr_Deprecated<U> const& other) noexcept;
        template <ref_count_type U> requires (std::is_convertible_v<U*, T*>)
        RefCountPtr_Deprecated(RefCountPtr_Deprecated<U>&& other) noexcept;

        ~RefCountPtr_Deprecated();

        auto operator = (RefCountPtr_Deprecated const& other) noexcept -> RefCountPtr_Deprecated&;
        auto operator = (RefCountPtr_Deprecated&& other) noexcept -> RefCountPtr_Deprecated&;

        template <ref_count_type U> requires (std::is_convertible_v<U*, T*>)
        auto operator = (RefCountPtr_Deprecated<U> const& other) noexcept -> RefCountPtr_Deprecated&;
        template <ref_count_type U> requires (std::is_convertible_v<U*, T*>)
        auto operator = (RefCountPtr_Deprecated<U>&& other) noexcept -> RefCountPtr_Deprecated&;

        auto copy(RefCountPtr_Deprecated const& other) noexcept -> void;
        auto swap(RefCountPtr_Deprecated& other) noexcept -> void;

        auto get() const noexcept -> T*;
        template <ref_count_type U>
        auto get() const noexcept -> U*;

        operator T* () const;
        explicit constexpr operator bool ();

        auto operator -> () const noexcept -> T*;
        auto operator &  () -> T**;

        [[nodiscard]] auto detach() -> T*;
        auto attach(T* other) -> void;

        auto reset() -> uint32_t;

    protected:

        auto add_ref_impl() noexcept -> void;
        auto release_impl() noexcept -> uint32_t;
    };

    template <ref_count_type T, typename... Args> requires (std::is_constructible_v<T, Args...>)
    auto make_ref_count(Args... args) -> RefCountPtr_Deprecated<T>;

    // Inherit from this to get a default ref counter.
    struct DefaultRefCounter
    {
    private:

        std::atomic<uint32_t> ref_count{1};

    public:

        virtual ~DefaultRefCounter() = default;

        auto add_ref() -> uint32_t
        {
            return ++ref_count;
        }

        auto release() -> uint32_t
        {
            auto count = --ref_count;
            if (count == 0) {
                delete this;
            }

            return count;
        }
    };
}

namespace cannele::inline core
{
    template <ref_count_type T>
    RefCountPtr_Deprecated<T>::RefCountPtr_Deprecated() noexcept
        : ptr(nullptr)
    {}

    template <ref_count_type T>
    RefCountPtr_Deprecated<T>::RefCountPtr_Deprecated(std::nullptr_t) noexcept
        : ptr(nullptr)
    {}

    template <ref_count_type T>
    template <ref_count_type U> requires (std::is_convertible_v<U*, T*>)
    RefCountPtr_Deprecated<T>::RefCountPtr_Deprecated(U* other) noexcept
        : ptr(other)
    {}

    template <ref_count_type T>
    RefCountPtr_Deprecated<T>::RefCountPtr_Deprecated(RefCountPtr_Deprecated const& other) noexcept
        : ptr(other.ptr)
    {
        add_ref_impl();
    }

    template <ref_count_type T>
    RefCountPtr_Deprecated<T>::RefCountPtr_Deprecated(RefCountPtr_Deprecated&& other) noexcept
    {
        swap(other);
    }

    template <ref_count_type T>
    template <ref_count_type U> requires (std::is_convertible_v<U*, T*>)
    RefCountPtr_Deprecated<T>::RefCountPtr_Deprecated(RefCountPtr_Deprecated<U> const& other) noexcept
        : ptr(other.ptr)
    {
        add_ref_impl();
    }

    template <ref_count_type T>
    template <ref_count_type U> requires (std::is_convertible_v<U*, T*>)
    RefCountPtr_Deprecated<T>::RefCountPtr_Deprecated(RefCountPtr_Deprecated<U>&& other) noexcept
        : ptr(other.ptr)
    {
        other.ptr = {};
    }

    template <ref_count_type T>
    RefCountPtr_Deprecated<T>::~RefCountPtr_Deprecated()
    {
        release_impl();
    }

    template <ref_count_type T>
    auto RefCountPtr_Deprecated<T>::operator = (RefCountPtr_Deprecated const& other) noexcept -> RefCountPtr_Deprecated&
    {
        if (ptr != other.ptr) {
            copy(other);
        }

        return *this;
    }

    template <ref_count_type T>
    auto RefCountPtr_Deprecated<T>::operator = (RefCountPtr_Deprecated&& other) noexcept -> RefCountPtr_Deprecated&
    {
        if (ptr != other.ptr) {
            swap(other);
        }

        return *this;
    }

    template <ref_count_type T>
    template <ref_count_type U> requires (std::is_convertible_v<U*, T*>)
    auto RefCountPtr_Deprecated<T>::operator = (RefCountPtr_Deprecated<U> const& other) noexcept -> RefCountPtr_Deprecated&
    {
        if (ptr != other.ptr) {
            copy(other);
        }

        return *this;
    }

    template <ref_count_type T>
    template <ref_count_type U> requires (std::is_convertible_v<U*, T*>)
    auto RefCountPtr_Deprecated<T>::operator = (RefCountPtr_Deprecated<U>&& other) noexcept -> RefCountPtr_Deprecated&
    {
        if (ptr != other.ptr) {
            swap(other);
        }

        return *this;
    }

    template <ref_count_type T>
    auto RefCountPtr_Deprecated<T>::copy(RefCountPtr_Deprecated const& other) noexcept -> void
    {
        ptr = other.ptr;
        add_ref_impl();
    }

    template <ref_count_type T>
    auto RefCountPtr_Deprecated<T>::swap(RefCountPtr_Deprecated& other) noexcept -> void
    {
        std::swap(ptr, other.ptr);
    }

    template <ref_count_type T>
    auto RefCountPtr_Deprecated<T>::get() const noexcept -> T*
    {
        return ptr;
    }

    template <ref_count_type T>
    template <ref_count_type U>
    auto RefCountPtr_Deprecated<T>::get() const noexcept -> U*
    {
        static_assert(!std::is_same_v<T, U>);
#if CNE_DEBUG
        auto u = dynamic_cast<U*>(ptr);
        if (!u) {
            std::println("[RefCountPtr_Deprecated] Cast failed: {} -> {}", typeid(T).name(), typeid(U).name());
        }
        return u;
#endif
        return (U*) ptr;
    }

    template <ref_count_type T>
    RefCountPtr_Deprecated<T>::operator T* () const
    {
        return ptr;
    }

    template <ref_count_type T>
    constexpr RefCountPtr_Deprecated<T>::operator bool ()
    {
        return ptr != nullptr;
    }

    template <ref_count_type T>
    auto RefCountPtr_Deprecated<T>::operator -> () const noexcept -> T*
    {
        return ptr;
    }

    template <ref_count_type T>
    auto RefCountPtr_Deprecated<T>::operator & () -> T**
    {
        return &ptr;
    }

    template <ref_count_type T>
    auto RefCountPtr_Deprecated<T>::detach() -> T*
    {
        auto temp = ptr;
        ptr = {};

        return temp;
    }

    template <ref_count_type T>
    auto RefCountPtr_Deprecated<T>::attach(T* other) -> void
    {
        if (ptr) {
            ptr->release();
        }

        ptr = other;
    }

    template <ref_count_type T>
    auto RefCountPtr_Deprecated<T>::reset() -> uint32_t
    {
        return release_impl();
    }

    template <ref_count_type T>
    auto RefCountPtr_Deprecated<T>::add_ref_impl() noexcept -> void
    {
        if (ptr) {
            ptr->add_ref();
        }
    }

    template <ref_count_type T>
    auto RefCountPtr_Deprecated<T>::release_impl() noexcept -> uint32_t
    {
        auto ref = 0u;
        auto temp = ptr;

        if (temp) {
            ptr = {};
            ref = temp->release();
        }

        return ref;
    }

    template <ref_count_type T, typename... Args> requires (std::is_constructible_v<T, Args...>)
    auto make_ref_count(Args... args) -> RefCountPtr_Deprecated<T>
    {
        return RefCountPtr_Deprecated<T>(new T(std::forward<Args>(args)...));
    }
}

namespace std
{
    template <typename T>
    struct hash<cannele::core::RefCountPtr_Deprecated<T>>
    {
        auto operator () (cannele::core::RefCountPtr_Deprecated<T> const& ref) const -> size_t
        {
            return std::hash<T*>()(ref.get());
        }
    };

    template <typename T>
    struct equal_to<cannele::core::RefCountPtr_Deprecated<T>>
    {
        auto operator () (cannele::core::RefCountPtr_Deprecated<T> const& lhs, cannele::core::RefCountPtr_Deprecated<T> const& rhs) const -> bool
        {
            return lhs.get() == rhs.get();
        }
    };
}
