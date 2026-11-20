#ifndef COLUMN_H
#define COLUMN_H

#include <string>
#include "Record.h"

using namespace std;

enum class DataType
{
    INTEGER,
    VARCHAR,
    CHAR,
    TEXT,
    DATE,
    BOOLEAN
};

class Column
{
private:
    string name;
    DataType type;
    int size;
    bool autoIncrement;
    Value defaultValue;

public:
    /**
     * @brief Construct a new Column descriptor.
     * @param name Column name.
     * @param type Data type (INTEGER, VARCHAR, TEXT, etc.).
     * @param size Max length for VARCHAR/CHAR (0 for unlimited/auto).
     * @param autoIncrement Whether column auto-increments on insert.
     * @param defaultValue Typed default Value (monostate{} for no default).
     */
    Column(
        const string& name,
        DataType type,
        int size = 0,
        bool autoIncrement = false,
        const Value& defaultValue = Value{}
    );

    /// Get column name.
    const string& getName() const;

    /// Get column data type.
    DataType getType() const;

    /// Get column size / length.
    int getSize() const;

    /// Check if marked as AUTO_INCREMENT.
    bool isAutoIncrement() const;

    /// Get typed default value.
    const Value& getDefaultValue() const;

    /// Set column name.
    void setName(const string& name);

    /// Set column data type.
    void setType(DataType type);

    /// Set column size / length limit.
    void setSize(int size);

    /// Set auto increment flag.
    void setAutoIncrement(bool value);

    /// Set typed default value.
    void setDefaultValue(const Value& value);

    /// Get human-readable string name of the data type (e.g. "INTEGER", "VARCHAR").
    string getTypeName() const;

    /// Print column summary to stdout.
    void print() const;
};

#endif
