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

    void set(const string& columnName, const JoinedValue& value);
    bool has(const string& columnName) const;
    JoinedValue get(const string& columnName) const;
    const HashMap<string, JoinedValue>& getValues() const;
    HashMap<string, JoinedValue>& getValues();
    int size() const;
    void clear();
    void print() const;
    bool operator==(const JoinedRecord& other) const;
    bool operator!=(const JoinedRecord& other) const;
};

#endif
