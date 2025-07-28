#pragma once

#include "idiom.hpp"
#include "assert.hpp"

#include <thread>

namespace cannele::inline core
{
    template <typename Instance>
    struct ThreadExclusive
    {
        CNE_PINNED(ThreadExclusive);

        inline static thread_local Instance* opt_instance{};

        ThreadExclusive();
        ~ThreadExclusive();

        static auto current() -> Instance*;
        static auto try_current() -> Instance*;
        static auto reset_current(Instance* opt_other = {}) -> void;
    };
}

namespace cannele::inline core
{
    template <typename Instance>
    ThreadExclusive<Instance>::ThreadExclusive()
    {
        if (opt_instance) {
            CNE_ERROR("{} is thread exclusive, can not construct because there is already an instance.", typeid(Instance).name());
        } else {
            opt_instance = (Instance*) this;
        }
    }

    template <typename Instance>
    ThreadExclusive<Instance>::~ThreadExclusive()
    {
        if (opt_instance == (Instance*) this) opt_instance = {};
    }

    template <typename Instance>
    auto ThreadExclusive<Instance>::current() -> Instance*
    {
        if (auto instance = opt_instance) {
            return instance;
        } else {
            CNE_ERROR("{} has no instance in called thread {}, create it first.", typeid(Instance).name(), std::this_thread::get_id());
            CNE_UNREACHABLE();
        }
    }

    template <typename Instance>
    auto ThreadExclusive<Instance>::try_current() -> Instance*
    {
        return opt_instance;
    }

    template <typename Instance>
    auto ThreadExclusive<Instance>::reset_current(Instance* opt_other) -> void
    {
        opt_instance = opt_other;
    }
}
