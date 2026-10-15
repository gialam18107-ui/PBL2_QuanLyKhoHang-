#ifndef RECORD_H
#define RECORD_H

#include <string>
#include <variant>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

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
 * @brief Produces a collision-safe, typed key for hash indexes and hash joins.
 * Numeric int/double values that compare equal in Condition::evaluateValue share
 * the same key; strings and booleans always have distinct type prefixes.
 * This is intentionally separate from valueToString(), which is used for I/O.
 */
inline string valueToHashKey(const Value& v)
{
    if (holds_alternative<monostate>(v))
    {
        return "null:";
    }
    if (holds_alternative<int>(v))
    {
        return "number:int:" + to_string(get<int>(v));
    }
    if (holds_alternative<double>(v))
    {
        const double number = get<double>(v);
        if (isfinite(number)
            && trunc(number) == number
            && number >= static_cast<double>(numeric_limits<int>::min())
            && number <= static_cast<double>(numeric_limits<int>::max()))
        {
            return "number:int:" + to_string(static_cast<int>(number));
        }

        ostringstream stream;
        stream << "number:double:" << setprecision(numeric_limits<double>::max_digits10) << number;
        return stream.str();
    }
    if (holds_alternative<string>(v))
    {
        return "string:" + get<string>(v);
    }
    if (holds_alternative<bool>(v))
    {
        return get<bool>(v) ? "bool:1" : "bool:0";
    }
    return "unknown:";
}

/**
 * @brief Hash key matching std::variant's type-sensitive equality semantics.
 * Used for operations (DISTINCT/IN/set operations/GROUP BY) that historically
 * compare Values directly rather than treating int and double as equivalent.
 */
inline string valueToExactHashKey(const Value& v)
{
    if (holds_alternative<monostate>(v)) return "exact:null";
    if (holds_alternative<int>(v)) return "exact:int:" + to_string(get<int>(v));
    if (holds_alternative<double>(v))
    {
        const double number = get<double>(v);
        if (number == 0.0) return "exact:double:0"; // +0.0 == -0.0
        ostringstream stream;
        stream << "exact:double:" << setprecision(numeric_limits<double>::max_digits10) << number;
        return stream.str();
    }
    if (holds_alternative<string>(v)) return "exact:string:" + get<string>(v);
    if (holds_alternative<bool>(v)) return get<bool>(v) ? "exact:bool:1" : "exact:bool:0";
    return "exact:unknown";
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

    /**
     * @brief Set or update a column's value in this record.
     * @param columnName Name of the column.
     * @param value Value (can be int, double, string, bool, or monostate{} for NULL).
     * @example r.set("product_name", string("Core i5")); r.set("quantity", 20);
     */
    void set(const string& columnName, const Value& value);

    /**
     * @brief Check whether a column exists in this record.
     */
    bool has(const string& columnName) const;

    /**
     * @brief Retrieve the value of a column. Throws std::runtime_error if column not found.
     * @param columnName Target column name.
     * @return Value SQL variant value. Use std::get<T>() to access primitive type.
     */
    Value get(const string& columnName) const;

    /// Get const reference to internal column-value map.
    const HashMap<string, Value>& getValues() const;

    /// Get mutable reference to internal column-value map.
    HashMap<string, Value>& getValues();

    /**
     * @brief Remove a column from the record.
     */
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
