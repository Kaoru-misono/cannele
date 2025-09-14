#pragma once

#include <memory>

template <typename T>
struct BreakableReference
{
    std::weak_ptr<T> pointer{};
    std::atomic<std::shared_ptr<T>> reference{};

    BreakableReference() = default;
    BreakableReference(std::weak_ptr<T> in_reference)
        : pointer(in_reference)
    {
        reference.store(this->pointer.lock(), std::memory_order_release);
    }

    virtual ~BreakableReference()
    {
        invalidate_reference();
    }

    auto get() -> T*
    {
        if (auto cached = reference.load(std::memory_order_acquire)) {
            return (T*) cached.get();
        }

        if (auto locked = pointer.lock()) {
            return (T*) locked.get();
        }

        CNE_ERROR("Reference lost.");

        return nullptr;
    }

    auto establish_reference() -> void
    {
        auto locked = pointer.lock();
        if (locked) {
            reference.store(locked, std::memory_order_release);
        } else {
            CNE_ERROR("Reference lost.");
        }
    }

    auto invalidate_reference() -> void
    {
        reference.store(nullptr, std::memory_order_release);
    }
};
