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

    string getName() const;
    DataType getType() const;
    int getSize() const;

    bool isPrimaryKey() const;
    bool isUnique() const;
    bool isNotNull() const;
    bool isAutoIncrement() const;

    string getDefaultValue() const;

    void setName(const string& name);
    void setType(DataType type);
    void setSize(int size);

    void setPrimaryKey(bool value);
    void setUnique(bool value);
    void setNotNull(bool value);
    void setAutoIncrement(bool value);

    void setDefaultValue(const string& value);

    string getTypeName() const;
    void print() const;
};

#endif

