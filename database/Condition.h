#ifndef CONDITION_H
#define CONDITION_H

#include <string>
#include "Record.h"
#include "datastructure/vector/Vector.h"

using namespace std;

enum class CompareOperator
{
    EQUAL,
    NOT_EQUAL,
    LESS,
    LESS_EQUAL,
    GREATER,
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

    /**
     * @brief Construct standard comparison condition (column <op> compareValue).
     * @example Condition("price", CompareOperator::GREATER, 100000)
     */
    Condition(
        const string& columnName,
        CompareOperator operation,
        const Value& compareValue
    );

    /**
     * @brief Construct unary condition (IS_NULL, IS_NOT_NULL).
     * @example Condition("description", CompareOperator::IS_NOT_NULL)
     */
    Condition(
        const string& columnName,
        CompareOperator operation
    );

    /**
     * @brief Construct set membership condition (IN / NOT_IN).
     * @example Condition("category", CompareOperator::IN, { "CPU", "GPU" })
     */
    Condition(
        const string& columnName,
        CompareOperator operation,
        const Vector<Value>& inValues
    );

    /**
     * @brief Construct BETWEEN condition (inclusive range: minVal <= column <= maxVal).
     * @example Condition("quantity", 10, 50)
     */
    Condition(
        const string& columnName,
        const Value& minVal,
        const Value& maxVal
    );

    /// Get target column name.
    string getColumnName() const;

    /// Get comparison operator enum.
    CompareOperator getOperation() const;

    /// Get primary comparison value.
    const Value& getCompareValue() const;

    /// Get second comparison value (upper bound for BETWEEN).
    const Value& getSecondCompareValue() const;

    /// Get value list for IN / NOT IN.
    const Vector<Value>& getInValues() const;

    /**
     * @brief Evaluate condition against a specific Record.
     * @return true if record satisfies the condition.
     */
    bool evaluate(const Record& record) const;

    /**
     * @brief Evaluate condition directly against an extracted Value.
     */
    bool evaluateValue(const Value& recordValue) const;
};

#endif
