#include "Column.h"
#include <iostream>

using namespace std;

Column::Column(
    const string& name,
    DataType type,
    int size,
    bool primaryKey,
    bool unique,
    bool notNull,
    bool autoIncrement,
    const string& defaultValue
)
{
    this->name = name;
    this->type = type;
    this->size = size;
    this->primaryKey = primaryKey;
    this->unique = unique;
    this->notNull = notNull;
    this->autoIncrement = autoIncrement;
    this->defaultValue = defaultValue;
}

string Column::getName() const
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

bool Column::isPrimaryKey() const
{
    return primaryKey;
}

bool Column::isUnique() const
{
    return unique;
}

bool Column::isNotNull() const
{
    return notNull;
}

bool Column::isAutoIncrement() const
{
    return autoIncrement;
}

string Column::getDefaultValue() const
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

void Column::setPrimaryKey(bool value)
{
    this->primaryKey = value;
}

void Column::setUnique(bool value)
{
    this->unique = value;
}

void Column::setNotNull(bool value)
{
    this->notNull = value;
}

void Column::setAutoIncrement(bool value)
{
    this->autoIncrement = value;
}

void Column::setDefaultValue(const string& value)
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
    }

    return "UNKNOWN";
}

void Column::print() const
{
    cout << name << " ";

    cout << getTypeName();

    if (size > 0)
    {
        cout << "(" << size << ")";
    }

    if (primaryKey)
    {
        cout << " PRIMARY KEY";
    }

    if (unique)
    {
        cout << " UNIQUE";
    }

    if (notNull)
    {
        cout << " NOT NULL";
    }

    if (autoIncrement)
    {
        cout << " AUTO_INCREMENT";
    }

    if (!defaultValue.empty())
    {
        cout << " DEFAULT " << defaultValue;
    }

    cout << endl;
}
