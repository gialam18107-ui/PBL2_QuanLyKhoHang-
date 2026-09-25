#include "JoinedRecord.h"


void JoinedRecord::set(
    const string& columnName,
    const JoinedValue& value
)
{
    values[columnName] = value;
}


bool JoinedRecord::has(
    const string& columnName
) const
{
    return values.find(columnName) != values.end();
}


JoinedValue JoinedRecord::get(
    const string& columnName
) const
{
    auto it = values.find(columnName);
    if (it == values.end()) throw runtime_error("Column not found: " + columnName);
    return it->second;
}

const HashMap<string, JoinedValue>& JoinedRecord::getValues() const
{
    return values;
}

HashMap<string, JoinedValue>& JoinedRecord::getValues()
{
    return values;
}


int JoinedRecord::size() const
{
    return static_cast<int>(values.size());
}


void JoinedRecord::clear()
{
    values.clear();
}


void JoinedRecord::print() const
{
    for (const auto& item : values)
    {
        cout << item.first << " = " << valueToString(item.second) << endl;
    }
}

bool JoinedRecord::operator==(const JoinedRecord& other) const
{
    if (values.size() != other.values.size()) return false;
    for (const auto& item : values)
    {
        auto it = other.values.find(item.first);
        if (it == other.values.end() || item.second != it->second) return false;
    }
    return true;
}

bool JoinedRecord::operator!=(const JoinedRecord& other) const
{
    return !(*this == other);
}
