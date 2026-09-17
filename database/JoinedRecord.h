#ifndef JOINED_RECORD_H
#define JOINED_RECORD_H

#include <string>
#include <unordered_map>
#include <variant>
#include <iostream>
#include <stdexcept>

using namespace std;

using JoinedValue = variant<int, double, string>;

class JoinedRecord
{
private:
    unordered_map<
        string,
        JoinedValue
    > values;

public:

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

    int size() const;

    void clear();

    void print() const;
};

#endif
