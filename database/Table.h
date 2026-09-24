#ifndef TABLE_H
#define TABLE_H

#include <string>
#include <vector>
#include <memory>

#include "Vector.h"
#include "HashMap.h"
#include "Column.h"
#include "Record.h"
#include "Constraint.h"
#include "Condition.h"
#include "ConditionGroup.h"
#include "Index.h"

using namespace std;

/**
 * @brief Represents a database table containing schema columns, data rows, constraints, and indexes.
 * Completely self-contained with custom Vector/HashMap and manual memory management (Rule of 5).
 */
class Table
{
private:
    string name;

    vector<Column> columns;
    vector<Record> rows;
    Vector<Column> columns;
    Vector<Record> rows;
    Vector<Constraint*> constraints;
    Vector<Index*> indexes;

    vector<shared_ptr<Constraint>> constraints;
    // Track auto-increment values per column
    HashMap<string, int> autoIncrementCounters;

    bool checkRecord(
        const Record& record
    ) const;
    bool checkDataType(const Column& column, const Value& value) const;
    bool checkConstraints(const Record& record, int ignoreRowIndex = -1) const;

    bool checkDataType(
        const Column& column,
        const Value& value
    ) const;

    bool checkConstraints(
        const Record& record,
        int ignoreRowIndex = -1
    ) const;

public:
    Table(const string& name);
    explicit Table(const string& name = "");
    Table(const Table& other);
    Table(Table&& other) noexcept;
    Table& operator=(const Table& other);
    Table& operator=(Table&& other) noexcept;
    ~Table();

    string getName() const;
    void setName(const string& name);


    // ========================================================
    // COLUMN
    // COLUMN MANAGEMENT
    // ========================================================

    void addColumn(
        const Column& column
    );

    bool hasColumn(
        const string& columnName
    ) const;

    Column* getColumn(
        const string& columnName
    );

    const Column* getColumn(
        const string& columnName
    ) const;

    void addColumn(const Column& column);
    bool hasColumn(const string& columnName) const;
    Column* getColumn(const string& columnName);
    const Column* getColumn(const string& columnName) const;
    int getColumnCount() const;
    const Vector<Column>& getColumns() const;

    const vector<Column>& getColumns() const;
    // ========================================================
    // CONSTRAINT MANAGEMENT
    // ========================================================
    bool addConstraint(Constraint* constraint);
    const Vector<Constraint*>& getConstraints() const;


    // ========================================================
    // CONSTRAINT
    // INDEX MANAGEMENT
    // ========================================================
    bool createIndex(const string& indexName, const string& columnName, IndexType type = IndexType::HASH);
    bool dropIndex(const string& indexName);
    bool hasIndex(const string& indexName) const;
    Index* getIndex(const string& indexName);
    const Index* getIndex(const string& indexName) const;
    const Vector<Index*>& getIndexes() const;

    bool addConstraint(
        const shared_ptr<Constraint>& constraint
    );

    const vector<shared_ptr<Constraint>>&
    getConstraints() const;


    // ========================================================
    // RECORD
    // DATA MANIPULATION (INSERT, UPDATE, DELETE)
    // ========================================================
    bool insert(const Record& record);
    bool update(const Condition& condition, const Record& newValues);
    bool update(const ConditionGroup& group, const Record& newValues);
    bool remove(const Condition& condition);
    bool remove(const ConditionGroup& group);

    bool insert(
        const Record& record
    );

    bool update(
        const Condition& condition,
        const Record& newValues
    );

    int getRowCount() const;
    const Vector<Record>& getRows() const;
    Vector<Record>& getRows();

    const vector<Record>& getRows() const;


    // ========================================================
    // DISPLAY
    // DISPLAY & UTILITY
    // ========================================================

    void printStructure() const;

    void printRows() const;


    // ========================================================
    // OTHER
    // ========================================================

    void clear();

    bool remove(
    const Condition& condition);
};

#endif
#endif // TABLE_H
