#ifndef CONDITION_H
#define CONDITION_H

#include <string>
#include "Record.h"

using namespace std;

enum class CompareOperator
{
    EQUAL,
    NOT_EQUAL,
    LESS,
    LESS_EQUAL,
    GREATER,
    GREATER_EQUAL
};

class Condition
{
private:
    string columnName;
    CompareOperator operation;
    Value compareValue;

public:
    Condition(
        const string& columnName,
        CompareOperator operation,
        const Value& compareValue
    );

    string getColumnName() const;

    CompareOperator getOperation() const;

    const Value& getCompareValue() const;

    bool evaluate(const Record& record) const;
};

#endif
