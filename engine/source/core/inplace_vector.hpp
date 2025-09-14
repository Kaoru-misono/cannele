#pragma once

#include <array>
#include <stdexcept>
#include <assert.h>

namespace cannele
{
    // See https://en.cppreference.com/w/cpp/header/inplace_vector.html
    // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p0843r9.html
    // Implementation by array.
    template <class T, size_t N = 16>
    struct inplace_vector
    {
    private:

        size_t current_size{0};
        std::array<T, N> storage{};

    public:

        // types:
        using value_type             = T;
        using pointer                = T*;
        using const_pointer          = const T*;
        using reference              = value_type&;
        using const_reference        = const value_type&;
        using size_type              = size_t;
        using difference_type        = ptrdiff_t;
        using iterator               = std::array<T, N>::iterator;
        using const_iterator         = std::array<T, N>::const_iterator;
        using reverse_iterator       = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;


        constexpr inplace_vector() noexcept;
        constexpr explicit inplace_vector(size_type n);
        constexpr inplace_vector(size_type n, T const& value);
        template <typename InputIterator>
        constexpr inplace_vector(InputIterator first, InputIterator last);
        constexpr inplace_vector(inplace_vector const&);
        constexpr inplace_vector(inplace_vector&&) noexcept(N == 0 || std::is_nothrow_move_constructible_v<T>);
        constexpr inplace_vector(std::initializer_list<T> il);
        constexpr ~inplace_vector();
        constexpr inplace_vector& operator=(inplace_vector const& other);
        constexpr inplace_vector& operator=(inplace_vector&& other) noexcept(N == 0 || std::is_nothrow_move_assignable_v<T>);
        template <typename InputIterator>
        constexpr void assign(InputIterator first, InputIterator last);
        // template<std::_Container_compatible_range<T> R>
        // constexpr void assign_range(R&& rg);
        constexpr void assign(size_type n, T const& u);
        constexpr void assign(std::initializer_list<T> il);

        // iterators
        constexpr iterator               begin()         noexcept { return storage.begin(); }
        constexpr const_iterator         begin()   const noexcept { return storage.begin(); }
        constexpr iterator               end()           noexcept { return storage.begin() + current_size; }
        constexpr const_iterator         end()     const noexcept { return storage.begin() + current_size; }
        // constexpr reverse_iterator       rbegin()        noexcept; Not Provided yet.
        // constexpr const_reverse_iterator rbegin()  const noexcept; Not Provided yet.
        // constexpr reverse_iterator       rend()          noexcept; Not Provided yet.
        // constexpr const_reverse_iterator rend()    const noexcept; Not Provided yet.

        constexpr const_iterator         cbegin()  const noexcept { return storage.cbegin(); }
        constexpr const_iterator         cend()    const noexcept { return storage.cbegin() + current_size; }
        // constexpr const_reverse_iterator crbegin() const noexcept; Not Provided yet.
        // constexpr const_reverse_iterator crend()   const noexcept; Not Provided yet.

        // [containers.sequences.inplace_vector.members] size/capacity
        [[nodiscard]] constexpr bool empty() const noexcept { return current_size == 0; }
        constexpr size_type size() const noexcept           { return current_size; }
        static constexpr size_type max_size() noexcept      { return N; }
        static constexpr size_type capacity() noexcept      { return N; }
        constexpr void resize(size_type sz)                 { resize(sz, T{}); }
        constexpr void resize(size_type sz, T const& c);
        static constexpr void reserve(size_type n) noexcept {}
        static constexpr void shrink_to_fit() noexcept      {}

        // element access
        constexpr reference       operator[](size_type n)       { return storage[n]; }
        constexpr const_reference operator[](size_type n) const { return storage[n]; }
        constexpr reference       at(size_type n)               { return storage.at(n); }
        constexpr const_reference at(size_type n) const         { return storage.at(n); }
        constexpr reference       front()       { return storage[0]; }
        constexpr const_reference front() const { return storage[0]; }
        constexpr reference       back()        { return storage[current_size - 1]; }
        constexpr const_reference back() const  { return storage[current_size - 1]; }

        // [containers.sequences.inplace_vector.data], data access
        constexpr       T* data()       noexcept { return storage.data(); }
        constexpr const T* data() const noexcept { return storage.data(); }

        // [containers.sequences.inplace_vector.modifiers], modifiers
        template <typename... Args>
        constexpr T& emplace_back(Args&&... args);
        constexpr T& push_back(T const& x);
        constexpr T& push_back(T&& x);
        // template<std::_Container_compatible_range<T> R>
        // constexpr void append_range(R&& rg); Not Provided yet.
        constexpr void pop_back();

        // template<typename... Args>
        // constexpr T* try_emplace_back(Args&&... args); Not Provided yet.
        // constexpr T* try_push_back(T const& x); Not Provided yet.
        // constexpr T* try_push_back(T&& x); Not Provided yet.

        // template<class... Args>
        // constexpr T& unchecked_emplace_back(Args&&... args); Not Provided yet.
        // constexpr T& unchecked_push_back(T const& x); Not Provided yet.
        // constexpr T& unchecked_push_back(T&& x); Not Provided yet.

        // template <typename... Args>
        // constexpr iterator emplace(const_iterator position, Args&&... args);                         Not Provided yet.
        // constexpr iterator insert(const_iterator position, T const& x);                              Not Provided yet.
        // constexpr iterator insert(const_iterator position, T&& x);                                   Not Provided yet.
        // constexpr iterator insert(const_iterator position, size_type n, T const& x);                 Not Provided yet.
        // template <class InputIterator>
        // constexpr iterator insert(const_iterator position, InputIterator first, InputIterator last); Not Provided yet.
        // template<std::_Container_compatible_range<T> R>
        // constexpr iterator insert_range(const_iterator position, R&& rg);                            Not Provided yet.
        // constexpr iterator insert(const_iterator position, std::initializer_list<T> il);             Not Provided yet.
        // constexpr iterator erase(const_iterator position);                                           Not Provided yet.
        // constexpr iterator erase(const_iterator first, const_iterator last);                         Not Provided yet.
        constexpr void swap(inplace_vector& x)
        noexcept(N == 0 || (std::is_nothrow_swappable_v<T> &&
                std::is_nothrow_move_constructible_v<T>));
        constexpr void clear() noexcept;

        constexpr friend bool operator==(inplace_vector const& x, inplace_vector const& y)
        {
            return std::equal(x.begin(), x.end(), y.begin(), y.end());
        }

        constexpr auto operator<=>(inplace_vector const& y) const noexcept(noexcept(std::declval<T>() == std::declval<T>())) -> std::weak_ordering
        {
            if constexpr (requires { std::declval<T>() <=> std::declval<T>(); }) {
                return std::lexicographical_compare_three_way(
                    begin(), end(), y.begin(), y.end(), std::compare_three_way{});
            } else if constexpr (requires { std::declval<T>() == std::declval<T>(); }) {
                for (size_t i = 0; i < std::min(size(), y.size()); ++i) {
                    if (storage[i] != y.storage[i]) {
                        return storage[i] < y.storage[i]
                            ? std::weak_ordering::less
                            : std::weak_ordering::greater;
                    }
                }
                return size() <=> y.size();
            } else {
                static_assert(sizeof(T) == 0, "Type T must be comparable");
            }
        }

        constexpr friend void swap(inplace_vector& x, inplace_vector& y)
            noexcept(N == 0 || (std::is_nothrow_swappable_v<T> && std::is_nothrow_move_constructible_v<T>))
        { x.swap(y); }
    };
}

namespace cannele
{
    template <typename T, size_t N>
    constexpr inplace_vector<T, N>::inplace_vector() noexcept
    {}

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>::inplace_vector(size_type n)
        : current_size(n)
    {}

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>::inplace_vector(size_type n, T const& u)
        : current_size(n)
    {
        assign(n, u);
    }

    template <typename T, size_t N>
    template <typename InputIterator>
    constexpr inplace_vector<T, N>::inplace_vector(InputIterator first, InputIterator last)
    {
        assign(first, last);
    }

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>::inplace_vector(inplace_vector const& other)
    {
        assign(other.begin(), other.end());
    }

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>::inplace_vector(inplace_vector&& other) noexcept(N == 0 || std::is_nothrow_move_constructible_v<T>)
    {
        for (; current_size < other.current_size; current_size++) {
            std::construct_at(&storage[current_size], std::move(other.storage[current_size]));
        }
    }

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>::inplace_vector(std::initializer_list<T> il)
    {
        assign(il);
    }

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>::~inplace_vector()
    {}

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>& inplace_vector<T, N>::operator=(inplace_vector const& other)
    {
        assign(other.begin(), other.end());
        return *this;
    }

    template <typename T, size_t N>
    constexpr inplace_vector<T, N>& inplace_vector<T, N>::operator=(inplace_vector&& other) noexcept(N == 0 || std::is_nothrow_move_assignable_v<T>)
    {
        clear();
        for (; current_size < other.current_size; ++current_size) {
            storage[current_size] = std::move(other.storage[current_size]);
        }

        return *this;
    }

    template <typename T, size_t N>
    template <typename InputIterator>
    constexpr void inplace_vector<T, N>::assign(InputIterator first, InputIterator last)
    {
        auto count = 0zu;
        count = std::distance(first, last);
        if (count > N) {
            throw std::length_error("implace_vector assignment exceeds capacity");
        }

        if constexpr (std::forward_iterator<InputIterator>) {

            // Not guaranteed exception safety.
            auto i = 0zu;
            for (; first != last && i < N; first++, i++) {
                storage[i] = *first;
            }

            current_size = count;
        } else {
            static_assert(std::input_iterator<InputIterator>);

            current_size = 0;
            for (; first != last; first++) {
                push_back(*first);
                current_size++;
            }
        }
    }

    template <typename T, size_t N>
    constexpr void inplace_vector<T, N>::assign(size_type n, T const& u)
    {
        current_size = n;
        for (size_type i = 0; i < n; i++) {
            storage[i] = u;
        }
    }

    template <typename T, size_t N>
    constexpr void inplace_vector<T, N>::assign(std::initializer_list<T> il)
    {
        assign(il.begin(), il.end());
    }

    template <typename T, size_t N>
    constexpr auto inplace_vector<T, N>::resize(size_type sz, T const& c) -> void
    {
        if (sz > current_size) {
            while (current_size < sz) emplace_back(c);
        } else {
            current_size = sz;
        }
    }

    template <typename T, size_t N>
    template <typename... Args>
    constexpr auto inplace_vector<T, N>::emplace_back(Args&&... args) -> T&
    {
        assert(current_size < N);

        std::construct_at(&storage[current_size], std::forward<Args>(args)...);
        current_size++;

        return storage[current_size - 1];
    }

    template <typename T, size_t N>
    constexpr auto inplace_vector<T, N>::push_back(T const& x) -> T&
    {
        emplace_back(x);
    }

    template <typename T, size_t N>
    constexpr auto inplace_vector<T, N>::push_back(T&& x) -> T&
    {
        emplace_back(std::move(x));
    }

    template <typename T, size_t N>
    constexpr auto inplace_vector<T, N>::pop_back() -> void
    {
        current_size--;
    }

    template <typename T, size_t N>
    constexpr auto inplace_vector<T, N>::swap(inplace_vector& x) noexcept(N == 0 || (std::is_nothrow_swappable_v<T> && std::is_nothrow_move_constructible_v<T>)) -> void
    {
        std::swap(current_size, x.current_size);
        storage.swap(x.storage);
    }

    template <typename T, size_t N>
    constexpr auto inplace_vector<T, N>::clear() noexcept -> void
    {
        current_size = 0;
    }

    template <typename T, size_t N>
    auto operator == (inplace_vector<T, N> const& x, inplace_vector<T, N> const& y) -> bool
    {
        if (x.size() != y.size()) {
            return false;
        }
        for (size_t i = 0; i < x.size(); i++) {
            if (x[i] != y[i]) {
                return false;
            }
        }

        return true;
    }
}
