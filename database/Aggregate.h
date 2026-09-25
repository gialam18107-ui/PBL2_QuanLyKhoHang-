#include "Aggregate.h"

Aggregate::Aggregate(
    AggregateType type,
    const string& column,
    const string& alias
)
    : type(type), column(column), alias(alias)
{
}

AggregateType Aggregate::getType() const
{
    return type;
}

string Aggregate::getColumn() const
{
    return column;
}

string Aggregate::getAlias() const
{
    return alias;
}

string Aggregate::getOutputName() const
{
    if (!alias.empty())
    {
        return alias;
    }

    string fn;
    switch (type)
    {
    case AggregateType::COUNT: fn = "COUNT"; break;
    case AggregateType::SUM:   fn = "SUM"; break;
    case AggregateType::AVG:   fn = "AVG"; break;
    case AggregateType::MIN:   fn = "MIN"; break;
    case AggregateType::MAX:   fn = "MAX"; break;
    }
    return fn + "(" + column + ")";
}

Value Aggregate::compute(
    AggregateType type,
    const string& column,
    const Vector<JoinedRecord>& records
)
{
    if (type == AggregateType::COUNT)
    {
        if (column == "*" || column.empty())
        {
            return static_cast<int>(records.size());
        }

        int count = 0;
        for (size_t i = 0; i < records.size(); i++)
        {
            if (records[i].has(column) && !isNull(records[i].get(column)))
            {
                count++;
            }
        }
        return count;
    }

    if (records.empty())
    {
        return monostate{};
    }

    auto toDouble = [](const Value& v) -> double {
        if (holds_alternative<int>(v)) return static_cast<double>(get<int>(v));
        if (holds_alternative<double>(v)) return get<double>(v);
        return 0.0;
    };

    auto isNumeric = [](const Value& v) {
        return holds_alternative<int>(v) || holds_alternative<double>(v);
    };

    if (type == AggregateType::SUM || type == AggregateType::AVG)
    {
        double sum = 0.0;
        int count = 0;
        bool hasDouble = false;

        for (size_t i = 0; i < records.size(); i++)
        {
            if (records[i].has(column))
            {
                Value val = records[i].get(column);
                if (!isNull(val) && isNumeric(val))
                {
                    if (holds_alternative<double>(val)) hasDouble = true;
                    sum += toDouble(val);
                    count++;
                }
            }
        }

        if (count == 0)
        {
            return monostate{};
        }

        if (type == AggregateType::AVG)
        {
            return sum / count;
        }

        if (hasDouble)
        {
            return sum;
        }
        return static_cast<int>(sum);
    }

    if (type == AggregateType::MIN || type == AggregateType::MAX)
    {
        bool hasVal = false;
        Value result = monostate{};

        for (size_t i = 0; i < records.size(); i++)
        {
            if (!records[i].has(column)) continue;
            Value val = records[i].get(column);
            if (isNull(val)) continue;

            if (!hasVal)
            {
                result = val;
                hasVal = true;
                continue;
            }

            if (isNumeric(val) && isNumeric(result))
            {
                double v = toDouble(val);
                double r = toDouble(result);
                if (type == AggregateType::MIN && v < r) result = val;
                if (type == AggregateType::MAX && v > r) result = val;
            }
            else if (holds_alternative<string>(val) && holds_alternative<string>(result))
            {
                const string& sVal = get<string>(val);
                const string& sRes = get<string>(result);
                if (type == AggregateType::MIN && sVal < sRes) result = val;
                if (type == AggregateType::MAX && sVal > sRes) result = val;
            }
        }

        return result;
    }

    return monostate{};
}

