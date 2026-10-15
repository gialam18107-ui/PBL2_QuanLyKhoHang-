#ifndef VECTOR_H
#define VECTOR_H

#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <new>

/**
 * @brief Dynamic array container implemented from scratch (replacing std::vector).
 * @tparam T Type of elements.
 */
template <typename T>
class Vector
{
private:
    T* elements;
    size_t sz;
    size_t cap;

    /**
     * @brief Reallocate internal buffer to a new capacity.
     * @param newCap Target capacity in number of elements.
     */
    void reallocate(size_t newCap)
    {
        T* newBuffer = static_cast<T*>(::operator new(newCap * sizeof(T)));
        for (size_t i = 0; i < sz; i++)
        {
            new (&newBuffer[i]) T(std::move(elements[i])); //Placement New
            elements[i].~T();
        }
        if (elements)
        {
            ::operator delete(elements);
        }
        elements = newBuffer;
        cap = newCap;
    }

public:
    /**
     * @brief Default constructor.
     */
    Vector()
        : elements(nullptr), sz(0), cap(0)
    {
    }

    /**
     * @brief Construct with pre-allocated capacity.
     * @param initialCapacity Number of elements to reserve space for.
     */
    explicit Vector(size_t initialCapacity)
        : elements(nullptr), sz(0), cap(0)
    {
        if (initialCapacity > 0)
        {
            reserve(initialCapacity);
        }
    }

    /**
     * @brief Construct with count copies of value.
     * @param count Number of elements.
     * @param value Value to fill.
     */
    Vector(size_t count, const T& value)
        : elements(nullptr), sz(0), cap(0)
    {
        if (count > 0)
        {
            reserve(count);
            for (size_t i = 0; i < count; i++)
            {
                new (&elements[i]) T(value);
            }
            sz = count;
        }
    }

    /**
     * @brief Construct from initializer list.
     * @param init Initializer list.
     */
    Vector(std::initializer_list<T> init)
        : elements(nullptr), sz(0), cap(0)
    {
        reserve(init.size());
        for (const auto& item : init)
        {
            push_back(item);
        }
    }

    /**
     * @brief Copy constructor.
     * @param other Vector to copy.
     */
    Vector(const Vector& other)
        : elements(nullptr), sz(0), cap(0)
    {
        if (other.sz > 0)
        {
            reserve(other.sz);
            for (size_t i = 0; i < other.sz; i++)
            {
                new (&elements[i]) T(other.elements[i]);
            }
            sz = other.sz;
        }
    }

    /**
     * @brief Move constructor.
     * @param other Vector to move from.
     */
    Vector(Vector&& other) noexcept
        : elements(other.elements), sz(other.sz), cap(other.cap)
    {
        other.elements = nullptr;
        other.sz = 0;
        other.cap = 0;
    }

    /**
     * @brief Copy assignment operator.
     * @param other Vector to copy.
     * @return Reference to this.
     */
    Vector& operator=(const Vector& other)
    {
        if (this != &other)
        {
            clear();
            if (other.sz > cap)
            {
                if (elements)
                {
                    ::operator delete(elements);
                    elements = nullptr;
                    cap = 0;
                }
                reserve(other.sz);
            }
            for (size_t i = 0; i < other.sz; i++)
            {
                new (&elements[i]) T(other.elements[i]);
            }
            sz = other.sz;
        }
        return *this;
    }

    /**
     * @brief Move assignment operator.
     * @param other Vector to move from.
     * @return Reference to this.
     */
    Vector& operator=(Vector&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            if (elements)
            {
                ::operator delete(elements);
            }
            elements = other.elements;
            sz = other.sz;
            cap = other.cap;

            other.elements = nullptr;
            other.sz = 0;
            other.cap = 0;
        }
        return *this;
    }

    /**
     * @brief Destructor. Destroys all elements and frees allocated memory.
     */
    ~Vector()
    {
        clear();
        if (elements)
        {
            ::operator delete(elements);
            elements = nullptr;
        }
        cap = 0;
    }

    /**
     * @brief Get number of elements.
     */
    size_t size() const
    {
        return sz;
    }

    /**
     * @brief Get current capacity.
     */
    size_t capacity() const
    {
        return cap;
    }

    /**
     * @brief Check if vector is empty.
     */
    bool empty() const
    {
        return sz == 0;
    }

    /**
     * @brief Access element at index (without bounds check).
     */
    T& operator[](size_t index)
    {
        return elements[index];
    }

    /**
     * @brief Access element at index (const, without bounds check).
     */
    const T& operator[](size_t index) const
    {
        return elements[index];
    }

    /**
     * @brief Access element at index with bounds check.
     */
    T& at(size_t index)
    {
        if (index >= sz)
        {
            throw std::out_of_range("Vector::at index out of range");
        }
        return elements[index];
    }

    /**
     * @brief Access element at index with bounds check (const).
     */
    const T& at(size_t index) const
    {
        if (index >= sz)
        {
            throw std::out_of_range("Vector::at index out of range");
        }
        return elements[index];
    }

    /**
     * @brief Access first element.
     */
    T& front()
    {
        return elements[0];
    }

    const T& front() const
    {
        return elements[0];
    }

    /**
     * @brief Access last element.
     */
    T& back()
    {
        return elements[sz - 1];
    }

    const T& back() const
    {
        return elements[sz - 1];
    }

    /**
     * @brief Append element to the back.
     */
    void push_back(const T& value)
    {
        if (sz == cap)
        {
            reserve(cap == 0 ? 8 : cap * 2);
        }
        new (&elements[sz]) T(value);
        sz++;
    }

    /**
     * @brief Append element to the back (move semantics).
     */
    void push_back(T&& value)
    {
        if (sz == cap)
        {
            reserve(cap == 0 ? 8 : cap * 2);
        }
        new (&elements[sz]) T(std::move(value));
        sz++;
    }

    /**
     * @brief Construct element in place at the back.
     */
    template <typename... Args>
    void emplace_back(Args&&... args)
    {
        if (sz == cap)
        {
            reserve(cap == 0 ? 8 : cap * 2);
        }
        new (&elements[sz]) T(std::forward<Args>(args)...);
        sz++;
    }

    /**
     * @brief Remove last element.
     */
    void pop_back()
    {
        if (sz > 0)
        {
            sz--;
            elements[sz].~T();
        }
    }

    /**
     * @brief Clear all elements without releasing buffer.
     */
    void clear()
    {
        for (size_t i = 0; i < sz; i++)
        {
            elements[i].~T();
        }
        sz = 0;
    }

    /**
     * @brief Reserve capacity.
     */
    void reserve(size_t newCap)
    {
        if (newCap > cap)
        {
            reallocate(newCap);
        }
    }

    /**
     * @brief Resize vector to new size.
     */
    void resize(size_t newSize, const T& defaultValue = T())
    {
        if (newSize < sz)
        {
            for (size_t i = newSize; i < sz; i++)
            {
                elements[i].~T();
            }
            sz = newSize;
        }
        else if (newSize > sz)
        {
            if (newSize > cap)
            {
                reserve(newSize);
            }
            for (size_t i = sz; i < newSize; i++)
            {
                new (&elements[i]) T(defaultValue);
            }
            sz = newSize;
        }
    }

    /**
     * @brief Erase element at index.
     */
    void erase(size_t index)
    {
        if (index >= sz)
        {
            throw std::out_of_range("Vector::erase index out of range");
        }
        elements[index].~T();
        for (size_t i = index; i + 1 < sz; i++)
        {
            new (&elements[i]) T(std::move(elements[i + 1]));
            elements[i + 1].~T(); //Phải hủy thì mới chơi bài new placement đc
        }
        sz--;
    }

    /**
     * @brief Insert element at index.
     */
    void insert(size_t index, const T& value)
    {
        if (index > sz)
        {
            throw std::out_of_range("Vector::insert index out of range");
        }
        if (sz == cap)
        {
            reserve(cap == 0 ? 8 : cap * 2);
        }
        if (index == sz)
        {
            new (&elements[sz]) T(value);
        }
        else
        {
            new (&elements[sz]) T(std::move(elements[sz - 1]));
            for (size_t i = sz - 1; i > index; i--)
            {
                elements[i] = std::move(elements[i - 1]);
            }
            elements[index] = value;
        }
        sz++;
    }

    // ========================================================
    // ITERATORS FOR RANGE-BASED FOR LOOPS
    // ========================================================

    T* begin() { return elements; }
    const T* begin() const { return elements; }
    const T* cbegin() const { return elements; }

    T* end() { return elements + sz; }
    const T* end() const { return elements + sz; }
    const T* cend() const { return elements + sz; }

    bool operator==(const Vector& other) const
    {
        if (sz != other.sz) return false;
        for (size_t i = 0; i < sz; i++)
        {
            if (!(elements[i] == other.elements[i]))
            {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const Vector& other) const
    {
        return !(*this == other);
    }
};

#endif // VECTOR_H
