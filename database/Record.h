#ifndef RECORD_H
#define RECORD_H

#include <string>
#include <variant>
#include <iostream>
#include <stdexcept>

#include "HashMap.h"
#include "Vector.h"

using namespace std;

using Value = variant<monostate, int, double, string, bool>;

/**
 * @brief Helper functions for SQL Value types.
 */
inline bool isNull(const Value& v)
{
    return holds_alternative<monostate>(v);
}

inline string valueToString(const Value& v)
{
    if (holds_alternative<monostate>(v))
    {
        return "NULL";
    }
    if (holds_alternative<int>(v))
    {
        return to_string(get<int>(v));
    }
    if (holds_alternative<double>(v))
    {
        return to_string(get<double>(v));
    }
    if (holds_alternative<string>(v))
    {
        return get<string>(v);
    }
    if (holds_alternative<bool>(v))
    {
        return get<bool>(v) ? "TRUE" : "FALSE";
    }
    return "";
}

/**
 * @brief Represents a single row in a database table.
 * Uses custom HashMap instead of std::unordered_map.
 */
class Record
{
private:
    HashMap<string, Value> values;

public:
    Record() = default;
    Record(const Record& other) = default;
    Record(Record&& other) noexcept = default;
    Record& operator=(const Record& other) = default;
    Record& operator=(Record&& other) noexcept = default;
    ~Record() = default;

    void set(const string& columnName, const Value& value);

    bool has(const string& columnName) const;

    Value get(const string& columnName) const;

    const HashMap<string, Value>& getValues() const;
    HashMap<string, Value>& getValues();

    void remove(const string& columnName);

    /**
     * @brief Clear all values in the record.
     */
    void clear();

    /**
     * @brief Get number of columns in the record.
     */
    int size() const;

    /**
     * @brief Print record contents to standard output.
     */
    void print() const;

    /**
     * @brief Equality comparison between two records.
     */
    bool operator==(const Record& other) const;

    /**
     * @brief Inequality comparison.
     */
    bool operator!=(const Record& other) const;
};

#endif
