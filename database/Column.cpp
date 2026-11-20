#include "Column.h"
#include <iostream>

using namespace std;

Column::Column(
    const string& name,
    DataType type,
    int size,
    bool autoIncrement,
    const Value& defaultValue
)
    : name(name),
      type(type),
      size(size),
      autoIncrement(autoIncrement),
      defaultValue(defaultValue)
{
}

const string& Column::getName() const
{
    return name;
}

DataType Column::getType() const
{
    return type;
}

int Column::getSize() const
{
    return size;
}

bool Column::isAutoIncrement() const
{
    return autoIncrement;
}

const Value& Column::getDefaultValue() const
{
    return defaultValue;
}

void Column::setName(const string& name)
{
    this->name = name;
}

void Column::setType(DataType type)
{
    this->type = type;
}

void Column::setSize(int size)
{
    this->size = size;
}

void Column::setAutoIncrement(bool value)
{
    this->autoIncrement = value;
}

void Column::setDefaultValue(const Value& value)
{
    this->defaultValue = value;
}

string Column::getTypeName() const
{
    switch (type)
    {
    case DataType::INTEGER:
        return "INTEGER";

    case DataType::VARCHAR:
        return "VARCHAR";

    case DataType::CHAR:
        return "CHAR";

    case DataType::TEXT:
        return "TEXT";

    case DataType::DATE:
        return "DATE";

    case DataType::BOOLEAN:
        return "BOOLEAN";
    }

    return "UNKNOWN";
}

void Column::print() const
{
    cout << name << " " << getTypeName();

    if (size > 0)
    {
        cout << "(" << size << ")";
    }

    if (autoIncrement)
    {
        cout << " AUTO_INCREMENT";
    }

    if (!isNull(defaultValue))
    {
        cout << " DEFAULT " << valueToString(defaultValue);
    }

    cout << endl;
}
