#ifndef TABLE_H
#define TABLE_H

#include <string>

#include "datastructure/vector/Vector.h"
#include "datastructure/hashmap/HashMap.h"
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
    Vector<unsigned char> deletedRows;
    size_t deletedRowCount = 0;
    Vector<Constraint*> constraints;
    Vector<Index*> indexes;
    HashMap<string, int> autoIncrementCounters;

    bool checkDataType(const Column& column, const Value& value) const;
    bool checkConstraints(const Record& record, int ignoreRowIndex = -1) const;
    void compactRows();
    void rebuildIndexes();

public:
    explicit Table(const string& name = "");
    Table(const Table& other);
    Table(Table&& other) noexcept;
    Table& operator=(const Table& other);
    Table& operator=(Table&& other) noexcept;
    ~Table();

    /// Get the table name.
    string getName() const;

    /// Set a new name for the table.
    void setName(const string& name);

    /**
     * @brief Add a column definition to the table schema.
     * @param column Column descriptor with name, type, and attributes.
     */
    void addColumn(const Column& column);

    /// Check if a column exists in the table by name.
    bool hasColumn(const string& columnName) const;

    /// Find and return a pointer to a Column, or nullptr if not found.
    Column* getColumn(const string& columnName);

    /// Const version of getColumn.
    const Column* getColumn(const string& columnName) const;

    /// Get the number of columns defined in this table.
    int getColumnCount() const;

    /// Get reference to vector of all column definitions.
    const Vector<Column>& getColumns() const;

    /**
     * @brief Add a table constraint (PK, Unique, FK, Check, etc.). Table takes ownership.
     * @param constraint Pointer to Constraint subclass allocated with new.
     * @return true if successfully added.
     */
    bool addConstraint(Constraint* constraint);

    /// Get reference to list of table constraints.
    const Vector<Constraint*>& getConstraints() const;

    /**
     * @brief Create an index on a specific column.
     * @param indexName Unique identifier for the index.
     * @param columnName Target column to index.
     * @param type Index type (IndexType::HASH for equality, IndexType::BTREE for range).
     * @return true if index was created and built.
     */
    bool createIndex(const string& indexName, const string& columnName, IndexType type = IndexType::HASH);

    /// Drop an index by name.
    bool dropIndex(const string& indexName);

    /// Check if an index exists on the table.
    bool hasIndex(const string& indexName) const;

    /// Get pointer to an index by name.
    Index* getIndex(const string& indexName);
    const Index* getIndex(const string& indexName) const;

    /// Get all indexes defined on this table.
    const Vector<Index*>& getIndexes() const;

    /**
     * @brief Insert a record into the table, checking data types and table constraints.
     * @param record Record containing column values to insert.
     * @return true if inserted, false if validation failed.
     */
    bool insert(const Record& record);

    /**
     * @brief Update records matching a Condition with newValues.
     * @param condition Matching condition for rows to modify.
     * @param newValues Record containing columns and updated values.
     * @return true if at least one row matched and updated without constraint violation.
     */
    bool update(const Condition& condition, const Record& newValues);

    /**
     * @brief Update records matching a ConditionGroup.
     */
    bool update(const ConditionGroup& group, const Record& newValues);

    /**
     * @brief Remove records matching a Condition from the table.
     * @param condition Matching filter for deletion.
     * @return true if matching rows were deleted.
     */
    bool remove(const Condition& condition);

    /**
     * @brief Remove records matching a compound ConditionGroup.
     */
    bool remove(const ConditionGroup& group);

    /// Get total number of rows in the table.
    int getRowCount() const;
    bool isRowDeleted(size_t rowIndex) const;
    const Vector<Record>& getRows() const;

    /// Get mutable reference to stored rows.
    Vector<Record>& getRows();

    /// Print table schema structure (columns, types, constraints, indexes).
    void printStructure() const;

    /// Print all row contents in formatted table style.
    void printRows() const;

    /// Clear all records in the table.
    void clear();
};

#endif
