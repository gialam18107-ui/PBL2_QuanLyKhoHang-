#ifndef TABLE_H
#define TABLE_H

#include <string>

#include "Vector.h"
#include "HashMap.h"
#include "Column.h"
#include "Record.h"
#include "Constraint.h"
#include "Condition.h"
#include "ConditionGroup.h"
#include "Index.h"

using namespace std;

class Table
{
private:
    string name;
    Vector<Column> columns;
    Vector<Record> rows;
    Vector<Constraint*> constraints;
    Vector<Index*> indexes;
    HashMap<string, int> autoIncrementCounters;

    bool checkDataType(const Column& column, const Value& value) const;
    bool checkConstraints(const Record& record, int ignoreRowIndex = -1) const;

public:
    explicit Table(const string& name = "");
    Table(const Table& other);
    Table(Table&& other) noexcept;
    Table& operator=(const Table& other);
    Table& operator=(Table&& other) noexcept;
    ~Table();

    string getName() const;
    void setName(const string& name);
    void addColumn(const Column& column);
    bool hasColumn(const string& columnName) const;
    Column* getColumn(const string& columnName);
    const Column* getColumn(const string& columnName) const;
    int getColumnCount() const;
    const Vector<Column>& getColumns() const;

    bool addConstraint(Constraint* constraint);
    const Vector<Constraint*>& getConstraints() const;

    bool createIndex(const string& indexName, const string& columnName, IndexType type = IndexType::HASH);
    bool dropIndex(const string& indexName);
    bool hasIndex(const string& indexName) const;
    Index* getIndex(const string& indexName);
    const Index* getIndex(const string& indexName) const;
    const Vector<Index*>& getIndexes() const;

    bool insert(const Record& record);
    bool update(const Condition& condition, const Record& newValues);
    bool update(const ConditionGroup& group, const Record& newValues);
    bool remove(const Condition& condition);
    bool remove(const ConditionGroup& group);

    int getRowCount() const;
    const Vector<Record>& getRows() const;
    Vector<Record>& getRows();
    void printStructure() const;
    void printRows() const;
    void clear();
};

#endif
