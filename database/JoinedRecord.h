#ifndef JOINED_RECORD_H
#define JOINED_RECORD_H

#include <string>
#include <unordered_map>
#include <variant>
#include <iostream>
#include <stdexcept>

#include "HashMap.h"
#include "Vector.h"
#include "Record.h"

using namespace std;

using JoinedValue = variant<int, double, string>;
using JoinedValue = Value;

/**
 * @brief Represents a record resulting from query projections and joins.
 * Uses custom HashMap instead of std::unordered_map.
 */
class JoinedRecord
{
private:
    unordered_map<
        string,
        JoinedValue
    > values;
    HashMap<string, JoinedValue> values;

public:
    JoinedRecord() = default;
    JoinedRecord(const JoinedRecord& other) = default;
    JoinedRecord(JoinedRecord&& other) noexcept = default;
    JoinedRecord& operator=(const JoinedRecord& other) = default;
    JoinedRecord& operator=(JoinedRecord&& other) noexcept = default;
    ~JoinedRecord() = default;

    void set(
        const string& columnName,
        const JoinedValue& value
    );

    bool has(
        const string& columnName
    ) const;

    JoinedValue get(
        const string& columnName
    ) const;

    const unordered_map<
        string,
        JoinedValue
    >& getValues() const;
    const HashMap<string, JoinedValue>& getValues() const;
    HashMap<string, JoinedValue>& getValues();

    int size() const;

    void clear();
    void print() const;

    void print() const;
    bool operator==(const JoinedRecord& other) const;
    bool operator!=(const JoinedRecord& other) const;
};

#endif
#endif // JOINED_RECORD_H
