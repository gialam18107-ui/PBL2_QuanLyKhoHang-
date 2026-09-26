#ifndef JOINED_RECORD_H
#define JOINED_RECORD_H

#include <string>
#include <iostream>
#include <stdexcept>

#include "HashMap.h"
#include "Record.h"

using namespace std;

using JoinedValue = Value;

class JoinedRecord
{
private:
    HashMap<string, JoinedValue> values;

public:
    JoinedRecord() = default;
    JoinedRecord(const JoinedRecord& other) = default;
    JoinedRecord(JoinedRecord&& other) noexcept = default;
    JoinedRecord& operator=(const JoinedRecord& other) = default;
    JoinedRecord& operator=(JoinedRecord&& other) noexcept = default;
    ~JoinedRecord() = default;

    /**
     * @brief Set or update a column's value (usually with prefix e.g. "product.price").
     */
    void set(const string& columnName, const JoinedValue& value);

    /**
     * @brief Check whether a qualified column exists in this joined record.
     */
    bool has(const string& columnName) const;

    /**
     * @brief Retrieve column value. Throws std::runtime_error if not found.
     * @param columnName Column key (e.g. "product.product_name" or alias).
     * @return JoinedValue SQL variant value.
     */
    JoinedValue get(const string& columnName) const;

    /// Get const reference to stored column map.
    const HashMap<string, JoinedValue>& getValues() const;

    /// Get mutable reference to stored column map.
    HashMap<string, JoinedValue>& getValues();

    /// Get number of columns in this record.
    int size() const;

    /// Clear all stored columns.
    void clear();

    /// Print all columns and values to stdout.
    void print() const;

    /// Equality comparison.
    bool operator==(const JoinedRecord& other) const;

    /// Inequality comparison.
    bool operator!=(const JoinedRecord& other) const;
};

#endif
