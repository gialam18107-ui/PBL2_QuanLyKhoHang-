#ifndef RECORD_H
#define RECORD_H

#include <string>
#include <unordered_map>
#include <variant>
#include <iostream>
#include <stdexcept>

using namespace std;

using Value = variant<int, double, string>;

class Record
{
private:
    unordered_map<string, Value> values;

public:
    Record();

    void set(
        const string& columnName,
        const Value& value
    );

    bool has(
        const string& columnName
    ) const;

    Value get(
        const string& columnName
    ) const;

    const unordered_map<string, Value>&
    getValues() const;

    void remove(
        const string& columnName
    );

    void clear();

    int size() const;

    void print() const;
};

#endif
