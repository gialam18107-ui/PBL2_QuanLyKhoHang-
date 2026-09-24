#ifndef OPTIONAL_H
#define OPTIONAL_H

#include <stdexcept>
#include <utility>
#include <new>

/**
 * @brief Custom Optional<T> wrapper implemented from scratch.
 * @tparam T Value type.
 */
template <typename T>
class Optional
{
private:
    bool hasVal;
    alignas(T) char storage[sizeof(T)];

    T* ptr()
    {
        return reinterpret_cast<T*>(storage);
    }

    const T* ptr() const
    {
        return reinterpret_cast<const T*>(storage);
    }

public:
    /**
     * @brief Construct an empty Optional.
     */
    Optional()
        : hasVal(false)
    {
    }

    /**
     * @brief Construct with value (copy).
     */
    Optional(const T& val)
        : hasVal(true)
    {
        new (storage) T(val);
    }

    /**
     * @brief Construct with value (move).
     */
    Optional(T&& val)
        : hasVal(true)
    {
        new (storage) T(std::move(val));
    }

    /**
     * @brief Copy constructor.
     */
    Optional(const Optional& other)
        : hasVal(other.hasVal)
    {
        if (hasVal)
        {
            new (storage) T(*other.ptr());
        }
    }

    /**
     * @brief Move constructor.
     */
    Optional(Optional&& other) noexcept
        : hasVal(other.hasVal)
    {
        if (hasVal)
        {
            new (storage) T(std::move(*other.ptr()));
            other.reset();
        }
    }

    /**
     * @brief Copy assignment operator.
     */
    Optional& operator=(const Optional& other)
    {
        if (this != &other)
        {
            reset();
            if (other.hasVal)
            {
                new (storage) T(*other.ptr());
                hasVal = true;
            }
        }
        return *this;
    }

    /**
     * @brief Move assignment operator.
     */
    Optional& operator=(Optional&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            if (other.hasVal)
            {
                new (storage) T(std::move(*other.ptr()));
                hasVal = true;
                other.reset();
            }
        }
        return *this;
    }

    /**
     * @brief Assign from value (copy).
     */
    Optional& operator=(const T& val)
    {
        reset();
        new (storage) T(val);
        hasVal = true;
        return *this;
    }

    /**
     * @brief Assign from value (move).
     */
    Optional& operator=(T&& val)
    {
        reset();
        new (storage) T(std::move(val));
        hasVal = true;
        return *this;
    }

    /**
     * @brief Destructor.
     */
    ~Optional()
    {
        reset();
    }

    /**
     * @brief Check whether value is present.
     */
    bool has_value() const
    {
        return hasVal;
    }

    /**
     * @brief Implicit conversion to bool checking presence.
     */
    explicit operator bool() const
    {
        return hasVal;
    }

    /**
     * @brief Get value reference. Throws std::runtime_error if empty.
     */
    T& value()
    {
        if (!hasVal)
        {
            throw std::runtime_error("Optional has no value");
        }
        return *ptr();
    }

    /**
     * @brief Get const value reference. Throws std::runtime_error if empty.
     */
    const T& value() const
    {
        if (!hasVal)
        {
            throw std::runtime_error("Optional has no value");
        }
        return *ptr();
    }

    /**
     * @brief Value or fallback.
     */
    T value_or(const T& fallback) const
    {
        return hasVal ? *ptr() : fallback;
    }

    T& operator*()
    {
        return value();
    }

    const T& operator*() const
    {
        return value();
    }

    T* operator->()
    {
        return ptr();
    }

    const T* operator->() const
    {
        return ptr();
    }

    /**
     * @brief Reset/destroy contained value.
     */
    void reset()
    {
        if (hasVal)
        {
            ptr()->~T();
            hasVal = false;
        }
    }
};

#endif // OPTIONAL_H

