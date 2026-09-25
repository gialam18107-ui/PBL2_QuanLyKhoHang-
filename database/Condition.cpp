
#include "Condition.h"

using namespace std;

static bool matchLike(const string& str, const string& pat, size_t sIdx = 0, size_t pIdx = 0)
{
    if (pIdx == pat.size()) return sIdx == str.size();
    if (pat[pIdx] == '%')
    {
        while (pIdx < pat.size() && pat[pIdx] == '%') pIdx++;
        if (pIdx == pat.size()) return true;
        for (size_t i = sIdx; i <= str.size(); i++)
        {
            if (matchLike(str, pat, i, pIdx)) return true;
        }
        return false;
    }
    if (sIdx < str.size() && (pat[pIdx] == '_' || pat[pIdx] == str[sIdx]))
    {
        return matchLike(str, pat, sIdx + 1, pIdx + 1);
    }
    return false;
}

Condition::Condition(
    const string& columnName,
    CompareOperator operation,
    const Value& compareValue
)
    : columnName(columnName), operation(operation), compareValue(compareValue)
{
    this->columnName = columnName;
    this->operation = operation;
    this->compareValue = compareValue;
}

Condition::Condition(
    const string& columnName,
    CompareOperator operation
)
    : columnName(columnName), operation(operation)
{
}

Condition::Condition(
    const string& columnName,
    CompareOperator operation,
    const Vector<Value>& inValues
)
    : columnName(columnName), operation(operation), inValues(inValues)
{
}

Condition::Condition(
    const string& columnName,
    const Value& minVal,
    const Value& maxVal
)
    : columnName(columnName), operation(CompareOperator::BETWEEN), compareValue(minVal), secondCompareValue(maxVal)
{
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
const Value& Condition::getSecondCompareValue() const
{
    if (!record.has(columnName))
    return secondCompareValue;
}

const Vector<Value>& Condition::getInValues() const
{
    return inValues;
}

bool Condition::evaluateValue(const Value& recordValue) const
{
    if (operation == CompareOperator::IS_NULL)
    {
        return isNull(recordValue);
    }
    if (operation == CompareOperator::IS_NOT_NULL)
    {
        return !isNull(recordValue);
    }

    if (isNull(recordValue))
    {
        return false;
    }

    Value recordValue =
        record.get(columnName);

    switch (operation)
    if (operation == CompareOperator::IN || operation == CompareOperator::NOT_IN)
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
        bool found = false;
        for (size_t i = 0; i < inValues.size(); i++)
        {
            return false;
            if (recordValue == inValues[i])
            {
                found = true;
                break;
            }
        }
        return (operation == CompareOperator::IN) ? found : !found;
    }

        if (holds_alternative<int>(recordValue))
    if (operation == CompareOperator::LIKE)
    {
        if (holds_alternative<string>(recordValue) && holds_alternative<string>(compareValue))
        {
            return get<int>(recordValue)
                < get<int>(compareValue);
            return matchLike(get<string>(recordValue), get<string>(compareValue));
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
    }

    case CompareOperator::LESS_EQUAL:
        if (
            recordValue.index()
            != compareValue.index()
        )
        {
            return false;
        }
    auto isNumeric = [](const Value& v) {
        return holds_alternative<int>(v) || holds_alternative<double>(v);
    };

        if (holds_alternative<int>(recordValue))
        {
            return get<int>(recordValue)
                <= get<int>(compareValue);
        }
    auto toDouble = [](const Value& v) -> double {
        if (holds_alternative<int>(v)) return static_cast<double>(get<int>(v));
        if (holds_alternative<double>(v)) return get<double>(v);
        return 0.0;
    };

        if (holds_alternative<double>(recordValue))
    if (operation == CompareOperator::BETWEEN)
    {
        if (isNumeric(recordValue) && isNumeric(compareValue) && isNumeric(secondCompareValue))
        {
            return get<double>(recordValue)
                <= get<double>(compareValue);
            double rv = toDouble(recordValue);
            return rv >= toDouble(compareValue) && rv <= toDouble(secondCompareValue);
        }

        if (holds_alternative<string>(recordValue))
        if (holds_alternative<string>(recordValue) && holds_alternative<string>(compareValue) && holds_alternative<string>(secondCompareValue))
        {
            return get<string>(recordValue)
                <= get<string>(compareValue);
            const string& s = get<string>(recordValue);
            return s >= get<string>(compareValue) && s <= get<string>(secondCompareValue);
        }

        return false;
    }

    case CompareOperator::GREATER:
        if (
            recordValue.index()
            != compareValue.index()
        )
    if (isNumeric(recordValue) && isNumeric(compareValue))
    {
        double left = toDouble(recordValue);
        double right = toDouble(compareValue);
        switch (operation)
        {
            return false;
        case CompareOperator::EQUAL: return left == right;
        case CompareOperator::NOT_EQUAL: return left != right;
        case CompareOperator::LESS: return left < right;
        case CompareOperator::LESS_EQUAL: return left <= right;
        case CompareOperator::GREATER: return left > right;
        case CompareOperator::GREATER_EQUAL: return left >= right;
        default: return false;
        }
    }

        if (holds_alternative<int>(recordValue))
        {
            return get<int>(recordValue)
                > get<int>(compareValue);
        }
    if (recordValue.index() != compareValue.index())
    {
        return false;
    }

        if (holds_alternative<double>(recordValue))
    if (holds_alternative<string>(recordValue))
    {
        const string& left = get<string>(recordValue);
        const string& right = get<string>(compareValue);
        switch (operation)
        {
            return get<double>(recordValue)
                > get<double>(compareValue);
        case CompareOperator::EQUAL: return left == right;
        case CompareOperator::NOT_EQUAL: return left != right;
        case CompareOperator::LESS: return left < right;
        case CompareOperator::LESS_EQUAL: return left <= right;
        case CompareOperator::GREATER: return left > right;
        case CompareOperator::GREATER_EQUAL: return left >= right;
        default: return false;
        }
    }

        if (holds_alternative<string>(recordValue))
        {
            return get<string>(recordValue)
                > get<string>(compareValue);
        }

    if (holds_alternative<bool>(recordValue))
    {
        bool left = get<bool>(recordValue);
        bool right = get<bool>(compareValue);
        if (operation == CompareOperator::EQUAL) return left == right;
        if (operation == CompareOperator::NOT_EQUAL) return left != right;
        return false;
    }

    case CompareOperator::GREATER_EQUAL:
        if (
            recordValue.index()
            != compareValue.index()
        )
        {
            return false;
        }
    return false;
}

        if (holds_alternative<int>(recordValue))
        {
            return get<int>(recordValue)
                >= get<int>(compareValue);
        }
bool Condition::evaluate(const Record& record) const
{
    if (operation == CompareOperator::IS_NULL)
    {
        if (!record.has(columnName)) return true;
        return isNull(record.get(columnName));
    }
    if (operation == CompareOperator::IS_NOT_NULL)
    {
        if (!record.has(columnName)) return false;
        return !isNull(record.get(columnName));
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

    if (!record.has(columnName))
    {
        return false;
    }

    return false;
    return evaluateValue(record.get(columnName));
}