#ifndef HASHMAP_H
#define HASHMAP_H

#include <cstddef>
#include <string>
#include <stdexcept>
#include <utility>
#include <variant>
#include "datastructure/vector/Vector.h"
#include "Pair.h"

using Value = std::variant<std::monostate, int, double, std::string, bool>;

inline std::string valueToKey(const Value& v)
{   //v.index() (variant method) return the index of datatype variant has
    switch (v.index())
    {
        case 0: return "null:";
        case 1: return "i:" + std::to_string(std::get<int>(v));
        case 2: return "d:" + std::to_string(std::get<double>(v));
        case 3: return "s:" + std::get<std::string>(v);
        case 4: return std::get<bool>(v) ? "b:1" : "b:0";
        default: return "";
    }
}

inline std::string valueToHashKey(const Value& v)
{
    return valueToKey(v);
}

inline std::string valueToExactHashKey(const Value& v)
{
    return valueToKey(v);
}

/**
 * @brief Default hash functors for custom HashMap.
 */
template <typename K>
struct Hash;

template <>
struct Hash<std::string>
{
    size_t operator()(const std::string& key) const
    {
        // FNV-1a 64-bit hash
        size_t hash = 14695981039346656037ULL;
        for (char c : key)
        {
            hash ^= static_cast<unsigned char>(c);
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

template <>
struct Hash<int>
{
    size_t operator()(int key) const
    {
        return static_cast<size_t>(key) * 2654435761ULL;
    }
};

template <>
struct Hash<long long>
{
    size_t operator()(long long key) const
    {
        return static_cast<size_t>(key) * 2654435761ULL;
    }
};

template <>
struct Hash<size_t>
{
    size_t operator()(size_t key) const
    {
        return key * 2654435761ULL;
    }
};

template <>
struct Hash<double>
{
    size_t operator()(double key) const
    {
        union {
            double d;
            size_t u;
        } conv;
        conv.d = key;
        return conv.u * 2654435761ULL;
    }
};

template <>
struct Hash<Value>
{
    size_t operator()(const Value& key) const
    {
        return Hash<std::string>{}(valueToKey(key));
    }
};

template <typename T1, typename T2>
struct Hash<Pair<T1, T2>>
{
    size_t operator()(const Pair<T1, T2>& p) const
    {
        size_t h1 = Hash<T1>{}(p.first);
        size_t h2 = Hash<T2>{}(p.second);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

/**
 * @brief Hash Map implemented from scratch using separate chaining (replacing std::unordered_map).
 * @tparam K Key type.
 * @tparam V Value type.
 * @tparam H Hash functor.
 */
template <typename K, typename V, typename H = Hash<K>>
class HashMap
{
public:
    struct Entry
    {
        K first;
        V second;
        Entry* next;

        Entry(const K& k, const V& v, Entry* n = nullptr)
            : first(k), second(v), next(n)
        {
        }

        Entry(const K& k, V&& v, Entry* n = nullptr)
            : first(k), second(std::move(v)), next(n)
        {
        }
    };

private:
    Entry** buckets;
    size_t bucketCount;
    size_t count;
    H hasher;

    size_t getBucketIndex(const K& key, size_t numBuckets) const
    {
        return hasher(key) % numBuckets;
    }

    void rehash(size_t newBucketCount)
    {
        Entry** newBuckets = new Entry*[newBucketCount](); //Dấu ngoặc ở cuối dùng để đặt mọi giá trị trong ô nhớ = 0
        for (size_t i = 0; i < bucketCount; i++)
        {
            Entry* curr = buckets[i];
            while (curr != nullptr)
            {
                Entry* next = curr->next;
                size_t newIdx = hasher(curr->first) % newBucketCount;
                curr->next = newBuckets[newIdx];
                newBuckets[newIdx] = curr;
                curr = next;
            }
        }
        delete[] buckets;
        buckets = newBuckets;
        bucketCount = newBucketCount;
    }

public:
    /**
     * @brief Iterator class to support range-based for loops and STL-like find().
     */
    class iterator
    {
    private:
        const HashMap* map;
        size_t bucketIndex;
        Entry* current;

        void advanceToNextValid()
        {
            if (current != nullptr && current->next != nullptr)
            {
                current = current->next;
                return;
            }
            current = nullptr;
            bucketIndex++;
            while (map != nullptr && bucketIndex < map->bucketCount)
            {
                if (map->buckets[bucketIndex] != nullptr)
                {
                    current = map->buckets[bucketIndex];
                    return;
                }
                bucketIndex++;
            }
        }

    public:
        iterator()
            : map(nullptr), bucketIndex(0), current(nullptr)
        {
        }

        iterator(const HashMap* m, size_t idx, Entry* curr)
            : map(m), bucketIndex(idx), current(curr)
        {
            if (current == nullptr && map != nullptr && bucketIndex < map->bucketCount)
            {
                advanceToNextValid();
            }
        }

        Entry& operator*() const { return *current; }
        Entry* operator->() const { return current; }

        iterator& operator++()
        {
            advanceToNextValid();
            return *this;
        }

        iterator operator++(int)
        {
            iterator temp = *this;
            ++(*this);
            return temp;
        }

        bool operator==(const iterator& other) const
        {
            return current == other.current && bucketIndex == other.bucketIndex;
        }

        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }

        friend class HashMap;
    };

    class const_iterator
    {
    private:
        const HashMap* map;
        size_t bucketIndex;
        const Entry* current;

        void advanceToNextValid()
        {
            if (current != nullptr && current->next != nullptr)
            {
                current = current->next;
                return;
            }
            current = nullptr;
            bucketIndex++;
            while (map != nullptr && bucketIndex < map->bucketCount)
            {
                if (map->buckets[bucketIndex] != nullptr)
                {
                    current = map->buckets[bucketIndex];
                    return;
                }
                bucketIndex++;
            }
        }

    public:
        const_iterator()
            : map(nullptr), bucketIndex(0), current(nullptr)
        {
        }

        const_iterator(const HashMap* m, size_t idx, const Entry* curr)
            : map(m), bucketIndex(idx), current(curr)
        {
            if (current == nullptr && map != nullptr && bucketIndex < map->bucketCount)
            {
                advanceToNextValid();
            }
        }

        const Entry& operator*() const { return *current; }
        const Entry* operator->() const { return current; }

        const_iterator& operator++()
        {
            advanceToNextValid();
            return *this;
        }

        const_iterator operator++(int)
        {
            const_iterator temp = *this;
            ++(*this);
            return temp;
        }

        bool operator==(const const_iterator& other) const
        {
            return current == other.current && bucketIndex == other.bucketIndex;
        }

        bool operator!=(const const_iterator& other) const
        {
            return !(*this == other);
        }

        friend class HashMap;
    };

    /**
     * @brief Default constructor.
     * @param initialBuckets Initial number of buckets.
     */
    explicit HashMap(size_t initialBuckets = 16)
        : bucketCount(initialBuckets < 4 ? 4 : initialBuckets), count(0)
    {
        buckets = new Entry*[bucketCount]();
    }

    /**
     * @brief Copy constructor.
     */
    HashMap(const HashMap& other)
        : bucketCount(other.bucketCount), count(0)
    {
        buckets = new Entry*[bucketCount]();
        for (size_t i = 0; i < other.bucketCount; i++)
        {
            Entry* curr = other.buckets[i];
            while (curr != nullptr)
            {
                insert(curr->first, curr->second);
                curr = curr->next;
            }
        }
    }

    /**
     * @brief Move constructor.
     */
    HashMap(HashMap&& other) noexcept
        : buckets(other.buckets), bucketCount(other.bucketCount), count(other.count), hasher(std::move(other.hasher))
    {
        other.buckets = nullptr;
        other.bucketCount = 0;
        other.count = 0;
    }

    /**
     * @brief Copy assignment operator.
     */
    HashMap& operator=(const HashMap& other)
    {
        if (this != &other)
        {
            clear();
            delete[] buckets;
            bucketCount = other.bucketCount;
            count = 0;
            buckets = new Entry*[bucketCount]();
            for (size_t i = 0; i < other.bucketCount; i++)
            {
                Entry* curr = other.buckets[i];
                while (curr != nullptr)
                {
                    insert(curr->first, curr->second);
                    curr = curr->next;
                }
            }
        }
        return *this;
    }

    /**
     * @brief Move assignment operator.
     */
    HashMap& operator=(HashMap&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            delete[] buckets;
            buckets = other.buckets;
            bucketCount = other.bucketCount;
            count = other.count;
            hasher = std::move(other.hasher);

            other.buckets = nullptr;
            other.bucketCount = 0;
            other.count = 0;
        }
        return *this;
    }

    /**
     * @brief Destructor.
     */
    ~HashMap()
    {
        clear();
        delete[] buckets;
        buckets = nullptr;
        bucketCount = 0;
    }

    /**
     * @brief Get number of elements.
     */
    size_t size() const
    {
        return count;
    }

    /**
     * @brief Check if map is empty.
     */
    bool empty() const
    {
        return count == 0;
    }

    /**
     * @brief Insert or update a key-value pair.
     */
    bool insert(const K& key, const V& value)
    {
        if (count + 1 > bucketCount * 0.75)
        {
            rehash(bucketCount * 2);
        }

        size_t idx = getBucketIndex(key, bucketCount);
        Entry* curr = buckets[idx];
        while (curr != nullptr)
        {
            if (curr->first == key)
            {
                curr->second = value;
                return false; // updated existing
            }
            curr = curr->next;
        }

        buckets[idx] = new Entry(key, value, buckets[idx]);
        count++;
        return true; // inserted new
    }

    /**
     * @brief Insert or update a key-value pair (move value).
     */
    bool insert(const K& key, V&& value)
    {
        if (count + 1 > bucketCount * 0.75)
        {
            rehash(bucketCount * 2);
        }

        size_t idx = getBucketIndex(key, bucketCount);
        Entry* curr = buckets[idx];
        while (curr != nullptr)
        {
            if (curr->first == key)
            {
                curr->second = std::move(value);
                return false;
            }
            curr = curr->next;
        }

        buckets[idx] = new Entry(key, std::move(value), buckets[idx]);
        count++;
        return true;
    }

    /**
     * @brief Subscript operator. Inserts default value if key not found.
     */
    V& operator[](const K& key)
    {
        if (count + 1 > bucketCount * 0.75)
        {
            rehash(bucketCount * 2);
        }

        size_t idx = getBucketIndex(key, bucketCount);
        Entry* curr = buckets[idx];
        while (curr != nullptr)
        {
            if (curr->first == key)
            {
                return curr->second;
            }
            curr = curr->next;
        }

        buckets[idx] = new Entry(key, V(), buckets[idx]);
        count++;
        return buckets[idx]->second;
    }

    /**
     * @brief Get value reference by key. Throws if not found.
     */
    V& get(const K& key)
    {
        size_t idx = getBucketIndex(key, bucketCount);
        Entry* curr = buckets[idx];
        while (curr != nullptr)
        {
            if (curr->first == key)
            {
                return curr->second;
            }
            curr = curr->next;
        }
        throw std::out_of_range("HashMap::get key not found");
    }

    /**
     * @brief Get const value reference by key. Throws if not found.
     */
    const V& get(const K& key) const
    {
        size_t idx = getBucketIndex(key, bucketCount);
        Entry* curr = buckets[idx];
        while (curr != nullptr)
        {
            if (curr->first == key)
            {
                return curr->second;
            }
            curr = curr->next;
        }
        throw std::out_of_range("HashMap::get key not found");
    }

    /**
     * @brief Find entry by key.
     * @return iterator pointing to entry or end() if not found.
     */
    iterator find(const K& key)
    {
        if (bucketCount == 0) return end();
        size_t idx = getBucketIndex(key, bucketCount);
        Entry* curr = buckets[idx];
        while (curr != nullptr)
        {
            if (curr->first == key)
            {
                return iterator(this, idx, curr);
            }
            curr = curr->next;
        }
        return end();
    }

    /**
     * @brief Find entry by key (const).
     * @return const_iterator pointing to entry or end() if not found.
     */
    const_iterator find(const K& key) const
    {
        if (bucketCount == 0) return end();
        size_t idx = getBucketIndex(key, bucketCount);
        Entry* curr = buckets[idx];
        while (curr != nullptr)
        {
            if (curr->first == key)
            {
                return const_iterator(this, idx, curr);
            }
            curr = curr->next;
        }
        return end();
    }

    /**
     * @brief Check whether key exists in map.
     */
    bool contains(const K& key) const
    {
        return find(key) != end();
    }

    /**
     * @brief Check whether key exists in map (alias for contains).
     */
    bool has(const K& key) const
    {
        return contains(key);
    }

    /**
     * @brief Remove key from map.
     * @return True if key was found and removed, false otherwise.
     */
    bool erase(const K& key)
    {
        if (count == 0) return false;
        size_t idx = getBucketIndex(key, bucketCount);
        Entry* curr = buckets[idx];
        Entry* prev = nullptr;

        while (curr != nullptr)
        {
            if (curr->first == key)
            {
                if (prev == nullptr)
                {
                    buckets[idx] = curr->next;
                }
                else
                {
                    prev->next = curr->next;
                }
                delete curr;
                count--;
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        return false;
    }

    /**
     * @brief Alias for erase.
     */
    bool remove(const K& key)
    {
        return erase(key);
    }

    /**
     * @brief Clear all entries from map.
     */
    void clear()
    {
        if (!buckets) return;
        for (size_t i = 0; i < bucketCount; i++)
        {
            Entry* curr = buckets[i];
            while (curr != nullptr)
            {
                Entry* next = curr->next;
                delete curr;
                curr = next;
            }
            buckets[i] = nullptr;
        }
        count = 0;
    }

    /**
     * @brief Get all keys as a Vector.
     */
    Vector<K> getKeys() const
    {
        Vector<K> keys;
        keys.reserve(count);
        for (auto it = begin(); it != end(); ++it)
        {
            keys.push_back(it->first);
        }
        return keys;
    }

    /**
     * @brief Get all values as a Vector.
     */
    Vector<V> getValues() const
    {
        Vector<V> values;
        values.reserve(count);
        for (auto it = begin(); it != end(); ++it)
        {
            values.push_back(it->second);
        }
        return values;
    }

    /**
     * @brief Get all entries as a Vector of Pairs.
     */
    Vector<Pair<K, V>> getEntries() const
    {
        Vector<Pair<K, V>> entries;
        entries.reserve(count);
        for (auto it = begin(); it != end(); ++it)
        {
            entries.push_back(makePair(it->first, it->second));
        }
        return entries;
    }

    // ========================================================
    // ITERATORS FOR RANGE-BASED FOR LOOPS
    // ========================================================

    iterator begin()
    {
        for (size_t i = 0; i < bucketCount; i++)
        {
            if (buckets[i] != nullptr)
            {
                return iterator(this, i, buckets[i]);
            }
        }
        return end();
    }

    iterator end()
    {
        return iterator(this, bucketCount, nullptr);
    }

    const_iterator begin() const
    {
        for (size_t i = 0; i < bucketCount; i++)
        {
            if (buckets[i] != nullptr)
            {
                return const_iterator(this, i, buckets[i]);
            }
        }
        return end();
    }

    const_iterator end() const
    {
        return const_iterator(this, bucketCount, nullptr);
    }

    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }
};

#endif // HASHMAP_H
