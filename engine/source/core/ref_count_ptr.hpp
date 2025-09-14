#pragma once

#include "assert.hpp"
#include "idiom.hpp"
#include <print>

namespace cannele::inline core
{
    struct RefCountObject
    {
    private:

        // Total number of references to this object.
        std::atomic<uint32_t> reference_count{};
        // Number references that are internal (i.e., not externally visible).
        // This can be used to detect whether the object is currently externally referenced or not.
        std::atomic<uint32_t> internal_reference_count{};

    #if CNE_REF_COUNT_DEBUG
        // Track the number of RefCountObject instances.
        static std::atomic<uint64_t> s_objectCount;
    #endif

    public:

        CNE_INTERFACE(RefCountObject);

        RefCountObject(RefCountObject const&) = delete;
        RefCountObject& operator=(const RefCountObject&) = delete;

        auto add_reference() -> uint32_t
        {
            auto count = reference_count.fetch_add(1);
            auto internal_count = internal_reference_count.load(std::memory_order_acquire);
            [[unlikely]]
            if (internal_count > 0 && count == internal_count) {
                // Object is now externally referenced
                make_external();
            }

            return count + 1;
        }

        auto release_reference() -> uint32_t
        {
            auto count = reference_count.fetch_sub(1);
            CNE_ASSERT(count > 0);
            auto internal_count = internal_reference_count.load(std::memory_order_acquire);
            [[unlikely]]
            if (internal_count > 0 && count == internal_count + 1) {
                // Object is now internally referenced only
                make_internal();
            }
            if (count == 1) {
                delete_this();
            }

            return count - 1;
        }

        // Set the number of references that are internal.
        // When the reference count becomes equal or smaller to this value,
        // the object is considered to be internally referenced and `make_internal()` is called.
        // When the reference count is greater than this value, the object is considered to be externally referenced
        // and `make_external()` is called.
        // Note: Calling this function is not thread-safe and should be used with care (i.e. only be called when the object
        // is initially created).
        auto set_internal_reference_count(uint32_t count) -> void
        {
            auto current_count = reference_count.load(std::memory_order_acquire);
            CNE_ASSERT(count <= current_count);
            internal_reference_count.store(count, std::memory_order_release);
            if (count == 0 && current_count > 0) {
                // Object is now externally referenced
                make_external();
            }
            else if (count > 0 && current_count == count) {
                // Object is now internally referenced
                make_internal();
            }
        }

        auto get_reference_count() const -> uint64_t { return reference_count; }
        auto get_internal_reference_count() const -> uint64_t { return internal_reference_count; }

        virtual auto make_external() -> void {}
        virtual auto make_internal() -> void {}
        virtual auto delete_this() -> void { delete this; }

    #if CNE_REF_COUNT_DEBUG
        // Get the number of RefCountObject instances currently alive.
        static auto getObjectCount() -> uint64_t { return s_objectCount.load(std::memory_order_relaxed); }
    #endif
    };

    template <typename T>
    concept ref_count_type = std::is_base_of_v<RefCountObject, T>;

    static inline auto add_reference(RefCountObject* object) -> void
    {
        if (object) {
            object->add_reference();
        }
    }

    static inline auto release_reference(RefCountObject* object) -> void
    {
        if (object) {
            object->release_reference();
        }
    }

    template <ref_count_type T>
    struct RefCountPtr
    {
    private:

        T* pointer;

        template <ref_count_type T2>
        friend struct RefCountPtr;

    public:

        RefCountPtr();
        RefCountPtr(std::nullptr_t);
        RefCountPtr(T* raw_ptr);
        RefCountPtr(RefCountPtr const& other);
        RefCountPtr(RefCountPtr&& other) noexcept;
        template <ref_count_type U> requires std::is_convertible_v<U*, T*>
        RefCountPtr(RefCountPtr<U> const& other);

        ~RefCountPtr();

        auto operator = (RefCountPtr const& other) -> RefCountPtr&;
        auto operator = (RefCountPtr&& other) noexcept -> RefCountPtr&;
        template <ref_count_type U> requires std::is_convertible_v<U*, T*>
        auto operator = (RefCountPtr<U> const& other) -> RefCountPtr&;

        auto operator <=> (RefCountPtr const& other) const = default;
        auto operator <=> (T* const& other) const;

        template <ref_count_type U>
        auto checked_cast() const -> RefCountPtr<U>;

        auto operator * () const -> T&;

        auto operator -> () const -> T*;

        explicit operator T* () const;
        auto get() const -> T*;

        explicit operator bool () const noexcept;

        auto attach(T* raw_ptr) -> void;

        auto detach() -> T*;

        auto reset() -> void;

        auto swap(RefCountPtr& other) -> void;
    };

    template <ref_count_type T, typename... Args>
    auto make_ref_count(Args&&... args) -> RefCountPtr<T>
    {
        auto result = RefCountPtr<T>(new T(std::forward<Args>(args)...));

        return result;
    }

    template <ref_count_type T>
    struct Reference
    {
    private:
        RefCountPtr<T> strong_reference{}; // Hold strong reference to avoid destroyed.
        T* weak_reference{};

    public:
        Reference();
        explicit Reference(T* raw_ptr);
        Reference(RefCountPtr<T> const& ref);

        auto operator = (T* raw_ptr) -> Reference&;
        auto operator = (RefCountPtr<T> const& ref) -> Reference&;

        auto operator -> () const -> T*;
        auto operator * () const -> T&;

        explicit operator T* () const;
        explicit operator bool () const noexcept;

        auto make_strong() -> void;
        auto make_weak() -> void;
    };
}

namespace cannele::inline core
{
    template <ref_count_type T>
    RefCountPtr<T>::RefCountPtr()
        : pointer(nullptr)
    {}

    template <ref_count_type T>
    RefCountPtr<T>::RefCountPtr(std::nullptr_t)
        : pointer(nullptr)
    {}

    template <ref_count_type T>
    RefCountPtr<T>::RefCountPtr(T* raw_ptr)
        : pointer(raw_ptr)
    {
        add_reference(pointer);
    }

    template <ref_count_type T>
    RefCountPtr<T>::RefCountPtr(RefCountPtr const& other)
        : pointer(other.pointer)
    {
        add_reference(pointer);
    }

    template <ref_count_type T>
    RefCountPtr<T>::RefCountPtr(RefCountPtr&& other) noexcept
        : pointer(other.pointer)
    {
        other.pointer = nullptr;
    }

    template <ref_count_type T>
    template <ref_count_type U> requires std::is_convertible_v<U*, T*>
    RefCountPtr<T>::RefCountPtr(RefCountPtr<U> const& other)
        : pointer(other.get())
    {
        add_reference(pointer);
    }

    template <ref_count_type T>
    RefCountPtr<T>::~RefCountPtr()
    {
        release_reference(pointer);
    }

    template <ref_count_type T>
    auto RefCountPtr<T>::operator=(RefCountPtr const& other) -> RefCountPtr&
    {
        if (this != &other) {
            release_reference(pointer);
            pointer = other.pointer;
            add_reference(pointer);
        }
        return *this;
    }

    template <ref_count_type T>
    auto RefCountPtr<T>::operator=(RefCountPtr&& other) noexcept -> RefCountPtr&
    {
        std::swap(pointer, other.pointer);
        return *this;
    }

    template <ref_count_type T>
    template <ref_count_type U> requires std::is_convertible_v<U*, T*>
    auto RefCountPtr<T>::operator=(RefCountPtr<U> const& other) -> RefCountPtr&
    {
        release_reference(pointer);
        pointer = other.get();
        add_reference(pointer);
        return *this;
    }

    template <ref_count_type T>
    auto RefCountPtr<T>::operator<=>(T* const& other) const
    {
        return pointer <=> other;
    }

    template <ref_count_type T>
    template <ref_count_type U>
    auto RefCountPtr<T>::checked_cast() const -> RefCountPtr<U>
    {
        return RefCountPtr<U>(dynamic_cast<U*>(pointer));
    }

    template <ref_count_type T>
    auto RefCountPtr<T>::operator*() const -> T&
    {
        return *pointer;
    }

    template <ref_count_type T>
    auto RefCountPtr<T>::operator->() const -> T*
    {
        return pointer;
    }

    template <ref_count_type T>
    RefCountPtr<T>::operator T*() const
    {
        return pointer;
    }

    template <ref_count_type T>
    auto RefCountPtr<T>::get() const -> T*
    {
        return pointer;
    }

    template <ref_count_type T>
    RefCountPtr<T>::operator bool() const noexcept
    {
        return pointer != nullptr;
    }

    template <ref_count_type T>
    auto RefCountPtr<T>::attach(T* raw_ptr) -> void
    {
        release_reference(pointer);
        pointer = raw_ptr;
    }

    template <ref_count_type T>
    auto RefCountPtr<T>::detach() -> T*
    {
        auto raw_ptr = pointer;
        pointer = nullptr;
        return raw_ptr;
    }

    template <ref_count_type T>
    auto RefCountPtr<T>::reset() -> void
    {
        release_reference(pointer);
        pointer = nullptr;
    }

    template <ref_count_type T>
    auto RefCountPtr<T>::swap(RefCountPtr& other) -> void
    {
        std::swap(pointer, other.pointer);
    }

    template <ref_count_type T>
    Reference<T>::Reference()
    {}

    template <ref_count_type T>
    Reference<T>::Reference(T* raw_ptr)
        : strong_reference(raw_ptr)
        , weak_reference(raw_ptr)
    {}

    template <ref_count_type T>
    Reference<T>::Reference(RefCountPtr<T> const& ref)
        : strong_reference(ref)
        , weak_reference(ref.get())
    {}

    template <ref_count_type T>
    auto Reference<T>::operator=(T* raw_ptr) -> Reference&
    {
        strong_reference = raw_ptr;
        weak_reference = raw_ptr;

        return *this;
    }

    template <ref_count_type T>
    auto Reference<T>::operator=(RefCountPtr<T> const& ref) -> Reference&
    {
        strong_reference = ref;
        weak_reference = ref.get();

        return *this;
    }

    template <ref_count_type T>
    auto Reference<T>::operator->() const -> T*
    {
        return weak_reference;
    }

    template <ref_count_type T>
    auto Reference<T>::operator*() const -> T&
    {
        return *weak_reference;
    }

    template <ref_count_type T>
    Reference<T>::operator T*() const
    {
        return weak_reference;
    }

    template <ref_count_type T>
    Reference<T>::operator bool() const noexcept
    {
        return weak_reference != nullptr;
    }

    template <ref_count_type T>
    auto Reference<T>::make_strong() -> void
    {
        if (strong_reference) return;

        strong_reference = weak_reference;
    }

    template <ref_count_type T>
    auto Reference<T>::make_weak() -> void
    {
        strong_reference = {};
    }
}

namespace std
{
    template <typename T>
    struct hash<cannele::core::RefCountPtr<T>>
    {
        auto operator () (cannele::core::RefCountPtr<T> const& ref) const -> size_t
        {
            return std::hash<T*>()(ref.get());
        }
    };

    template <typename T>
    struct equal_to<cannele::core::RefCountPtr<T>>
    {
        auto operator () (cannele::core::RefCountPtr<T> const& lhs, cannele::core::RefCountPtr<T> const& rhs) const -> bool
        {
            return lhs.get() == rhs.get();
        }
    };
}
