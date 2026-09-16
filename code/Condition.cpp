
#include "Condition.h"

using namespace std;

Condition::Condition(
    const string& columnName,
    CompareOperator operation,
    const Value& compareValue
)
{
    this->columnName = columnName;
    this->operation = operation;
    this->compareValue = compareValue;
}

string Condition::getColumnName() const
{
    return columnName;
}

CompareOperator Condition::getOperation() const
{
    return operation;
}

const Value& Condition::getCompareValue() const
{
    return compareValue;
}

bool Condition::evaluate(const Record& record) const
{
    if (!record.has(columnName))
    {
        return false;
    }

    Value recordValue =
        record.get(columnName);

    switch (operation)
    {
    case CompareOperator::EQUAL:
        return recordValue == compareValue;

    case CompareOperator::NOT_EQUAL:
        return recordValue != compareValue;

    case CompareOperator::LESS:
        if (
            recordValue.index()
            != compareValue.index()
        )
        {
            return false;
        }

        if (holds_alternative<int>(recordValue))
        {
            return get<int>(recordValue)
                < get<int>(compareValue);
        }

        if (holds_alternative<double>(recordValue))
        {
            return get<double>(recordValue)
                < get<double>(compareValue);
        }

        if (holds_alternative<string>(recordValue))
        {
            return get<string>(recordValue)
                < get<string>(compareValue);
        }

        return false;

    case CompareOperator::LESS_EQUAL:
        if (
            recordValue.index()
            != compareValue.index()
        )
        {
            return false;
        }

        if (holds_alternative<int>(recordValue))
        {
            return get<int>(recordValue)
                <= get<int>(compareValue);
        }

        if (holds_alternative<double>(recordValue))
        {
            return get<double>(recordValue)
                <= get<double>(compareValue);
        }

        if (holds_alternative<string>(recordValue))
        {
            return get<string>(recordValue)
                <= get<string>(compareValue);
        }

        return false;

    case CompareOperator::GREATER:
        if (
            recordValue.index()
            != compareValue.index()
        )
        {
            return false;
        }

        if (holds_alternative<int>(recordValue))
        {
            return get<int>(recordValue)
                > get<int>(compareValue);
        }

        if (holds_alternative<double>(recordValue))
        {
            return get<double>(recordValue)
                > get<double>(compareValue);
        }

        if (holds_alternative<string>(recordValue))
        {
            return get<string>(recordValue)
                > get<string>(compareValue);
        }

        return false;

    case CompareOperator::GREATER_EQUAL:
        if (
            recordValue.index()
            != compareValue.index()
        )
        {
            return false;
        }

        if (holds_alternative<int>(recordValue))
        {
            return get<int>(recordValue)
                >= get<int>(compareValue);
        }

        if (holds_alternative<double>(recordValue))
        {
            return get<double>(recordValue)
                >= get<double>(compareValue);
        }

        if (holds_alternative<string>(recordValue))
        {
            return get<string>(recordValue)
                >= get<string>(compareValue);
        }

        return false;
    }

    return false;
}