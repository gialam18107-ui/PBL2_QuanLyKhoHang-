#ifndef CONDITION_H
#define CONDITION_H

#include <string>
#include "Record.h"
#include "Vector.h"

using namespace std;

enum class CompareOperator
{
    EQUAL,
    NOT_EQUAL,
    LESS,
    LESS_EQUAL,
    GREATER,
    GREATER_EQUAL
    GREATER_EQUAL,
    IS_NULL,
    IS_NOT_NULL,
    LIKE,
    IN,
    NOT_IN,
    BETWEEN
};

/**
 * @brief Represents a single filtering condition in WHERE, HAVING, or CHECK constraints.
 */
class Condition
{
private:
    string columnName;
    CompareOperator operation;
    Value compareValue;
    Value secondCompareValue; // For BETWEEN (upper bound)
    Vector<Value> inValues;    // For IN / NOT IN

public:
    Condition() = default;

    Condition(
        const string& columnName,
        CompareOperator operation,
        const Value& compareValue
    );

    Condition(
        const string& columnName,
        CompareOperator operation
    );

    Condition(
        const string& columnName,
        CompareOperator operation,
        const Vector<Value>& inValues
    );

    Condition(
        const string& columnName,
        const Value& minVal,
        const Value& maxVal
    );

    string getColumnName() const;

    CompareOperator getOperation() const;

    const Value& getCompareValue() const;
    const Value& getSecondCompareValue() const;
    const Vector<Value>& getInValues() const;

    bool evaluate(const Record& record) const;
    bool evaluateValue(const Value& recordValue) const;
};

#endif
#endif // CONDITION_H
