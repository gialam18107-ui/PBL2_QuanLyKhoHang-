#ifndef PAIR_H
#define PAIR_H

#include <utility>

/**
 * @brief Custom Pair data structure implemented from scratch.
 * @tparam T1 Type of first element.
 * @tparam T2 Type of second element.
 */
template <typename T1, typename T2>
struct Pair
{
    T1 first;
    T2 second;

    Pair()
        : first(), second()
    {
    }

    Pair(const T1& a, const T2& b)
        : first(a), second(b)
    {
    }

    Pair(T1&& a, T2&& b)
        : first(std::move(a)), second(std::move(b))
    {
    }

    Pair(const T1& a, T2&& b)
        : first(a), second(std::move(b))
    {
    }

    Pair(T1&& a, const T2& b)
        : first(std::move(a)), second(b)
    {
    }

    Pair(const Pair& other)
        : first(other.first), second(other.second)
    {
    }

    Pair(Pair&& other) noexcept
        : first(std::move(other.first)), second(std::move(other.second))
    {
    }

    Pair& operator=(const Pair& other)
    {
        if (this != &other)
        {
            first = other.first;
            second = other.second;
        }
        return *this;
    }

    Pair& operator=(Pair&& other) noexcept
    {
        if (this != &other)
        {
            first = std::move(other.first);
            second = std::move(other.second);
        }
        return *this;
    }

    ~Pair() = default;

    bool operator==(const Pair& other) const
    {
        return first == other.first && second == other.second;
    }

    bool operator!=(const Pair& other) const
    {
        return !(*this == other);
    }

    bool operator<(const Pair& other) const
    {
        if (first < other.first) return true;
        if (other.first < first) return false;
        return second < other.second;
    }
};

template <typename T1, typename T2>
Pair<std::decay_t<T1>, std::decay_t<T2>> makePair(T1&& a, T2&& b)
{
    return Pair<std::decay_t<T1>, std::decay_t<T2>>(std::forward<T1>(a), std::forward<T2>(b));
}

#endif // PAIR_H
