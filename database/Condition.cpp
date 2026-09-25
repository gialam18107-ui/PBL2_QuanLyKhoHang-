#include "Condition.h"

using namespace std;

static bool matchLike(const string& text, const string& pattern, size_t textIndex = 0, size_t patternIndex = 0)
{
    if (patternIndex == pattern.size()) return textIndex == text.size();
    if (pattern[patternIndex] == '%')
    {
        while (patternIndex < pattern.size() && pattern[patternIndex] == '%') ++patternIndex;
        if (patternIndex == pattern.size()) return true;
        for (size_t index = textIndex; index <= text.size(); ++index)
        {
            if (matchLike(text, pattern, index, patternIndex)) return true;
        }
        return false;
    }
    return textIndex < text.size()
        && (pattern[patternIndex] == '_' || pattern[patternIndex] == text[textIndex])
        && matchLike(text, pattern, textIndex + 1, patternIndex + 1);
}

static bool isNumericValue(const Value& value)
{
    return holds_alternative<int>(value) || holds_alternative<double>(value);
}

static double numericValue(const Value& value)
{
    return holds_alternative<int>(value)
        ? static_cast<double>(get<int>(value))
        : get<double>(value);
}

Condition::Condition(const string& columnName, CompareOperator operation, const Value& compareValue)
    : columnName(columnName), operation(operation), compareValue(compareValue)
{
}

Condition::Condition(const string& columnName, CompareOperator operation)
    : columnName(columnName), operation(operation)
{
}

Condition::Condition(const string& columnName, CompareOperator operation, const Vector<Value>& inValues)
    : columnName(columnName), operation(operation), inValues(inValues)
{
}

Condition::Condition(const string& columnName, const Value& minVal, const Value& maxVal)
    : columnName(columnName), operation(CompareOperator::BETWEEN), compareValue(minVal), secondCompareValue(maxVal)
{
}

string Condition::getColumnName() const { return columnName; }
CompareOperator Condition::getOperation() const { return operation; }
const Value& Condition::getCompareValue() const { return compareValue; }
const Value& Condition::getSecondCompareValue() const { return secondCompareValue; }
const Vector<Value>& Condition::getInValues() const { return inValues; }

bool Condition::evaluate(const Record& record) const
{
    if (operation == CompareOperator::IS_NULL) return !record.has(columnName) || isNull(record.get(columnName));
    if (operation == CompareOperator::IS_NOT_NULL) return record.has(columnName) && !isNull(record.get(columnName));
    return record.has(columnName) && evaluateValue(record.get(columnName));
}

bool Condition::evaluateValue(const Value& recordValue) const
{
    if (operation == CompareOperator::IS_NULL) return isNull(recordValue);
    if (operation == CompareOperator::IS_NOT_NULL) return !isNull(recordValue);
    if (isNull(recordValue)) return false;

    if (operation == CompareOperator::IN || operation == CompareOperator::NOT_IN)
    {
        bool found = false;
        for (size_t index = 0; index < inValues.size(); ++index)
        {
            if (recordValue == inValues[index])
            {
                found = true;
                break;
            }
        }
        return operation == CompareOperator::IN ? found : !found;
    }

    if (operation == CompareOperator::LIKE)
    {
        return holds_alternative<string>(recordValue)
            && holds_alternative<string>(compareValue)
            && matchLike(get<string>(recordValue), get<string>(compareValue));
    }

    if (operation == CompareOperator::BETWEEN)
    {
        if (isNumericValue(recordValue) && isNumericValue(compareValue) && isNumericValue(secondCompareValue))
        {
            double value = numericValue(recordValue);
            return value >= numericValue(compareValue) && value <= numericValue(secondCompareValue);
        }
        if (holds_alternative<string>(recordValue)
            && holds_alternative<string>(compareValue)
            && holds_alternative<string>(secondCompareValue))
        {
            const string& value = get<string>(recordValue);
            return value >= get<string>(compareValue) && value <= get<string>(secondCompareValue);
        }
        return false;
    }

    if (isNumericValue(recordValue) && isNumericValue(compareValue))
    {
        double left = numericValue(recordValue);
        double right = numericValue(compareValue);
        switch (operation)
        {
        case CompareOperator::EQUAL: return left == right;
        case CompareOperator::NOT_EQUAL: return left != right;
        case CompareOperator::LESS: return left < right;
        case CompareOperator::LESS_EQUAL: return left <= right;
        case CompareOperator::GREATER: return left > right;
        case CompareOperator::GREATER_EQUAL: return left >= right;
        default: return false;
        }
    }

    if (recordValue.index() != compareValue.index()) return false;
    if (holds_alternative<string>(recordValue))
    {
        const string& left = get<string>(recordValue);
        const string& right = get<string>(compareValue);
        switch (operation)
        {
        case CompareOperator::EQUAL: return left == right;
        case CompareOperator::NOT_EQUAL: return left != right;
        case CompareOperator::LESS: return left < right;
        case CompareOperator::LESS_EQUAL: return left <= right;
        case CompareOperator::GREATER: return left > right;
        case CompareOperator::GREATER_EQUAL: return left >= right;
        default: return false;
        }
    }
    if (holds_alternative<bool>(recordValue))
    {
        bool left = get<bool>(recordValue);
        bool right = get<bool>(compareValue);
        if (operation == CompareOperator::EQUAL) return left == right;
        if (operation == CompareOperator::NOT_EQUAL) return left != right;
    }
    return false;
}