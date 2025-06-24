#pragma once

#include <core/assert.hpp>
#include <core/hash.hpp>
#include <platform/shader_compile.hpp>

namespace cannele::inline graphics::rhi
{
    struct ShaderPermutationCompileInfo final
    {
        ShaderCompileEnvironment environment{nullptr};
        int32_t permutation_id{};
        size_t hash{};

        ShaderPermutationCompileInfo(ShaderCompileInfo const* create_info, int32_t permutation_id)
            : environment(create_info), permutation_id{permutation_id}
        {
            update_hash();
        }

        auto update_hash() -> void
        {
            hash = hash_combine(environment.compile_info->base_hash, permutation_id);
        }
    };

    struct ShaderPermutationBool final
    {
        using Type = bool;
        static constexpr auto permutation_count = 2;
        static constexpr auto muti_dimensional = false;

        static auto to_id(Type value) -> int32_t
        {
            return value ? 1 : 0;
        }

        static auto from_id(int32_t id) -> Type
        {
            return id == 1;
        }

        static auto to_define(Type value) -> bool
        {
            return value;
        }
    };
    // To Define a TEST_BOOL_DEFINE permutation: struct SP_Test: SHADER_PERMUTATION_BOOL("TEST_BOOL_DEFINE");
    #define SHADER_PERMUTATION_BOOL(DEFINE_NAME) canele::shader::ShaderPermutationBool { static constexpr auto name = DEFINE_NAME;  }

    // Defines at compile time a permutation dimension made of Continuous values.
    template <typename T, int32_t dimension_size, int32_t first_value = 0>
    struct ShaderPermutationInt
    {
        using Type = T;
        static constexpr auto permutation_count = dimension_size;
        static constexpr auto muti_dimensional = false;

        static constexpr auto min = (Type) (first_value);
        static constexpr auto max = (Type) (first_value + dimension_size - 1);

        static auto to_id(Type value) -> int32_t
        {
            auto id = static_cast<int32_t>(value) - first_value;
            CNE_ASSERT(id >= 0 && id < permutation_count);

            return id;
        }

        static auto from_id(int32_t id) -> Type
        {
            CNE_ASSERT(id >= 0 && id < permutation_count);

            return static_cast<Type>(id + first_value);
        }

        static auto to_define(Type value) -> Type
        {
            return to_id(value) + first_value;
        }
    };
    // To Define a TEST_INT_DEFINE permutation [0, N) : struct SP_Test: SHADER_PERMUTATION_INT("TEST_INT_DEFINE", N);
    #define SHADER_PERMUTATION_INT(DEFINE_NAME, COUNT) \
        canele::shader::ShaderPermutationInt<int32_t, COUNT> { static constexpr auto name = DEFINE_NAME;  }
    // To Define a TEST_FLOAT_DEFINE permutation [X, X + N) : struct SP_Test: SHADER_PERMUTATION_RANGE_INT("TEST_FLOAT_DEFINE", X, N);
    #define SHADER_PERMUTATION_RANGE_INT(DEFINE_NAME, START, COUNT) \
        canele::shader::ShaderPermutationInt<int32_t, COUNT, START> { static constexpr auto name = DEFINE_NAME;  }
    #define SHADER_PERMUTATION_ENUM(DEFINE_NAME, ENUM) \
        canele::shader::ShaderPermutationInt<ENUM, (int32_t) ENUM::last> { static constexpr auto name = DEFINE_NAME;  }

    template <int32_t... ts>
    struct ShaderPermutationSparseInt
    {
        using Type = int32_t;

        static constexpr auto permutation_count = 0;
        static constexpr auto muti_dimensional = false;

        static auto to_id(Type value) -> int32_t
        {
            return (int32_t) 0;
        }

        static auto from_id(int32_t id) -> Type
        {
            return (Type) 0;
        }
    };

    template <int32_t unique_value, int32_t... ts>
    struct ShaderPermutationSparseInt<unique_value, ts...>
    {
        using Type = int32_t;

        static constexpr auto permutation_count = ShaderPermutationSparseInt<ts...>::permutation_count + 1;
        static constexpr auto muti_dimensional = false;

        static auto to_id(Type value) -> int32_t
        {
            return (value == unique_value ? permutation_count - 1 :ShaderPermutationSparseInt<ts...>::to_id(value));
        }

        static auto from_id(int32_t id) -> Type
        {
            return (id == permutation_count - 1 ? unique_value : ShaderPermutationSparseInt<ts...>::from_id(id));
        }

        static auto to_define(Type value) -> int32_t
        {
            return (int32_t) value;
        }
    };
    #define SHADER_PERMUTATION_SPARSE_INT(DEFINE_NAME, ...) \
        canele::shader::ShaderPermutationSparseInt<__VA_ARGS__> { static constexpr auto name = DEFINE_NAME;  }

    template <typename... Ts>
    struct ShaderPermutationList
    {
        using Type = ShaderPermutationList<Ts...>;

        static constexpr auto permutation_count = 1;
        static constexpr auto muti_dimensional = true;

        ShaderPermutationList<Ts...>() {}
        explicit ShaderPermutationList<Ts...>(int32_t permutation_id)
        {
            CNE_ASSERT_WITH(permutation_id == 0, "Invalid permutation id");
        }

        template <typename T>
        auto set(typename T::Type) -> void
        {
            static_assert(sizeof(typename T::Type) == 0, "Invalid permutation type");
        }

        template <typename T>
        auto get() -> typename T::Type
        {
            static_assert(sizeof(typename T::Type) == 0, "Invalid permutation type");
            return T::Type();
        }

        auto modify_compilation_environment(ShaderCompileEnvironment* environment) -> void {}

        static auto to_id(Type const& value) -> int32_t
        {
            return 0;
        }

        auto to_id() -> int32_t
        {
            return to_id(*this);
        }

        static auto from_id(int32_t id) -> Type
        {
            return Type(id);
        }

        auto operator == (Type const& other) const -> bool
        {
            return true;
        }
    };

    template <typename T, typename... Ts>
    struct ShaderPermutationList<T, Ts...>
    {
        using Type = ShaderPermutationList<T, Ts...>;
        using Next = ShaderPermutationList<Ts...>;

        static constexpr auto permutation_count = Next::permutation_count * T::permutation_count;
        static constexpr auto muti_dimensional = true;

    private:

        typename T::Type value;
        Next next;

    public:

        ShaderPermutationList<T, Ts...>()
            : value(T::from_id(0))
        {}

        explicit ShaderPermutationList<T, Ts...>(int32_t permutation_id)
            : value(T::from_id(permutation_id % T::permutation_count))
            , next(permutation_id / T::permutation_count)
        {}

        template <typename ToSet>
        auto set(typename ToSet::Type value) -> void
        {
            if constexpr (std::is_same_v<T, ToSet>) {
                this->value = value;
            } else {
                next.template set<ToSet>(value);
            }
        }

        template <typename ToGet>
        auto get() -> ToGet::Type*
        {
            if constexpr (std::is_same_v<T, ToGet>) {
                return &value;
            } else {
                return next.template get<ToGet>();
            }
        };

        auto modify_compilation_environment(ShaderCompileEnvironment* environment) -> void
        {
            if constexpr (T::multi_dimensional) {
                value.modify_compilation_environment(environment);

                next.modify_compilation_environment(environment);
            } else {
                environment->define(T::name, T::to_define(value));

                next.modify_compilation_environment(environment);
            }
        }

        static auto to_id(Type const& list) -> int32_t
        {
            return list.to_id();
        }

        auto to_id() -> int32_t
        {
            return T::to_id(value) + T::permutation_count * next.to_id();
        }

        static auto from_id(int32_t id) -> Type
        {
            return Type(id);
        }

        auto operator == (Type const& other) const -> bool
        {
            return (value == other.value && next == other.next);
        }

        auto operator != (Type const& other) const -> bool
        {
            return !(*this == other);
        }
    };

    struct ShaderPermutationCompileBatched final
    {
        ShaderCompileInfo const* shader_info{};
        std::vector<ShaderPermutationCompileInfo> batched_compile_info{};

        ShaderPermutationCompileBatched() = default;
        explicit ShaderPermutationCompileBatched(ShaderCompileInfo const* create_info)
            : shader_info(create_info)
        {}

        template <typename ShaderType, typename Permutation>
        auto add(Permutation const& permutation) -> ShaderPermutationCompileBatched*
        {
            auto permutation_id = permutation.to_id();
            if (compile_permutation<ShaderType>(permutation_id)) {
                auto compile_info = &batched_compile_info.emplace_back(shader_info, permutation_id);

                modify_compilation_environment<ShaderType>(&compile_info->environment, permutation_id);

                permutation.modify_compilation_environment(&compile_info->environment);
            }

            return this;
        }

        template <typename ShaderType>
        auto add_default() -> ShaderPermutationCompileBatched*
        {
            auto compile_info = &batched_compile_info.emplace_back(shader_info, 0);

            modify_compilation_environment<ShaderType>(&compile_info->environment, 0);

            return this;
        }

        auto update_hashes() -> void
        {
            for (auto& compile_info : batched_compile_info) {
                compile_info.update_hash();
            }
        }

    private:

        template <typename ShaderType>
        auto modify_compilation_environment(ShaderCompileEnvironment* environment, int32_t permutation_id) -> void
        {
            if constexpr (std::is_invocable_v<decltype(&ShaderType::modify_compilation_environment), ShaderCompileEnvironment*, int32_t>) {
                ShaderType::modify_compilation_environment(environment, permutation_id);
            }
        }

        template <typename ShaderType>
        auto compile_permutation(int32_t permutation_id) -> bool
        {
            if constexpr (std::is_invocable_v<decltype(&ShaderType::compile_permutation), int32_t>) {
                static_assert(std::is_same_v<std::invoke_result_t<decltype(&ShaderType::compile_permutation), int32_t>, bool>);

                return ShaderType::compile_permutation(permutation_id);
            } else {
                return true;
            }
        }
    };

}
