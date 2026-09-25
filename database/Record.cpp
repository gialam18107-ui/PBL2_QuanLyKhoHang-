#include "Record.h"

void Record::set(const string& columnName, const Value& value)
{
    values[columnName] = value;
}

bool Record::has(const string& columnName) const
{
    return values.find(columnName) != values.end();
}

Value Record::get(const string& columnName) const
{
    auto it = values.find(columnName);
    if (it == values.end())
    {
        throw runtime_error("Column not found: " + columnName);
    }
    return it->second;
}

const HashMap<string, Value>& Record::getValues() const
{
    return values;
}

HashMap<string, Value>& Record::getValues()
{
    return values;
}

void Record::remove(const string& columnName)
{
    values.erase(columnName);
}

void Record::clear()
{
    values.clear();
}

int Record::size() const
{
    return static_cast<int>(values.size());
}

void Record::print() const
{
    for (const auto& item : values)
    {
        cout << item.first << " = " << valueToString(item.second) << endl;
    }
}

bool Record::operator==(const Record& other) const
{
    if (values.size() != other.values.size()) return false;
    for (const auto& item : values)
    {
        auto it = other.values.find(item.first);
        if (it == other.values.end() || item.second != it->second) return false;
    }
    return true;
}

bool Record::operator!=(const Record& other) const
{
    return !(*this == other);
}
