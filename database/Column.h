#ifndef COLUMN_H
#define COLUMN_H

#include <string>

using namespace std;

enum class DataType
{
    INTEGER,
    VARCHAR,
    CHAR,
    TEXT,
    DATE
};

class Column
{
private:
    string name;
    DataType type;
    int size;

    bool primaryKey;
    bool unique;
    bool notNull;
    bool autoIncrement;

    string defaultValue;

public:
    /**
     * @brief Construct a new Column descriptor.
     * @param name Column name.
     * @param type Data type (INTEGER, VARCHAR, TEXT, etc.).
     * @param size Max length for VARCHAR/CHAR (0 for unlimited/auto).
     * @param primaryKey Whether column is primary key.
     * @param unique Whether column values must be unique.
     * @param notNull Whether column disallows NULL values.
     * @param autoIncrement Whether column auto-increments on insert.
     * @param defaultValue Default string representation of value.
     */
    Column(
        const string& name,
        DataType type,
        int size = 0,
        bool primaryKey = false,
        bool unique = false,
        bool notNull = false,
        bool autoIncrement = false,
        const string& defaultValue = ""
    );

    /// Get column name.
    string getName() const;

    /// Get column data type.
    DataType getType() const;

    /// Get column size / length.
    int getSize() const;

    /// Check if marked as primary key.
    bool isPrimaryKey() const;

    /// Check if marked as unique.
    bool isUnique() const;

    /// Check if marked as NOT NULL.
    bool isNotNull() const;

    /// Check if marked as AUTO_INCREMENT.
    bool isAutoIncrement() const;

    /// Get default value string.
    string getDefaultValue() const;

    /// Set column name.
    void setName(const string& name);

    /// Set column data type.
    void setType(DataType type);

    /// Set column size / length limit.
    void setSize(int size);

    /// Set primary key flag.
    void setPrimaryKey(bool value);

    /// Set unique constraint flag.
    void setUnique(bool value);

    /// Set not null constraint flag.
    void setNotNull(bool value);

    /// Set auto increment flag.
    void setAutoIncrement(bool value);

    /// Set default value.
    void setDefaultValue(const string& value);

    /// Get human-readable string name of the data type (e.g. "INTEGER", "VARCHAR(50)").
    string getTypeName() const;

    /// Print column summary to stdout.
    void print() const;
};

#endif

