#ifndef TABLE_H
#define TABLE_H

#include <string>

#include "datastructure/vector/Vector.h"
#include "datastructure/hashmap/HashMap.h"
#include "Column.h"
#include "Record.h"
#include "Constraint.h"
#include "Index.h"
#include "TableView.h"

using namespace std;

namespace DataIO {
    class TableIO;
}

namespace Generator {
    class DataGenerator;
}

class Database;

// Forward declarations for unit testing internal index structures
void test_product_indexes_with_real_data(Database& db);
void test_cpu_indexes_with_real_data(Database& db);
void test_inbound_transaction_date_index_with_real_data(Database& db);
void test_delivery_date_index_with_real_data(Database& db);
void test_customer_indexes_with_real_data(Database& db);
void test_status_history_indexes_with_real_data(Database& db);

class Table
{
    friend class TableView;
    friend class Database;
    friend class DataIO::TableIO;
    friend class Generator::DataGenerator;
    friend void test_product_indexes_with_real_data(Database& db);
    friend void test_cpu_indexes_with_real_data(Database& db);
    friend void test_inbound_transaction_date_index_with_real_data(Database& db);
    friend void test_delivery_date_index_with_real_data(Database& db);
    friend void test_customer_indexes_with_real_data(Database& db);
    friend void test_status_history_indexes_with_real_data(Database& db);

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

    // Internal physical access for TableView and internal friends
    size_t rawRowCount() const;
    const Record& getRawRow(size_t rowIndex) const;
    Record& getRawRow(size_t rowIndex);

    // Internal row lookup for Database
    int findRowIndexById(int id) const;

    // Transaction & compaction management (Database only)
    void compactIfNeeded();
    const HashMap<string, int>& getAutoIncrementCounters() const;
    void undoInsert(size_t rowIndex, const HashMap<string, int>& oldAutoIncrement);
    void undoUpdate(size_t rowIndex, const Record& oldRecord);
    void undoRemove(size_t rowIndex);

    // Index & Constraint internal management
    bool hasIndex(const string& indexName) const;
    bool dropIndex(const string& indexName);
    Index* getIndex(const string& indexName);
    const Index* getIndex(const string& indexName) const;
    const Vector<Index*>& getIndexes() const;
    const Vector<Constraint*>& getConstraints() const;

    // Mutable column pointer (internal use)
    Column* getColumn(const string& columnName);

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

    /**
     * @brief Create an index on a specific column.
     * @param indexName Unique identifier for the index.
     * @param columnName Target column to index.
     * @param type Index type (IndexType::HASH, IndexType::AVL for range, IndexType::HASH_TREE).
     * @return true if index was created and built.
     */
    bool createIndex(const string& indexName, const string& columnName, IndexType type = IndexType::HASH);

    /**
     * @brief Insert a record into the table, checking data types and table constraints.
     * @param record Record containing column values to insert.
     * @return true if inserted, false if validation failed.
     */
    bool insert(const Record& record);

    /// Get the primary key column name of this table.
    string getPrimaryKeyColumn() const;

    /// Get pointer to record by record ID in the primary key column (nullptr if not found or deleted).
    const Record* getRecordById(int id) const;

    /**
     * @brief Update a record directly by its primary key ID (Rule 6.3).
     * @param id Value of the primary key.
     * @param newValues Column-value updates.
     * @return true if updated, false if not found or constraint failed.
     */
    bool updateById(int id, const Record& newValues);

    /**
     * @brief Remove a record directly by its primary key ID (Rule 6.3).
     * @param id Value of the primary key.
     * @return true if found and removed.
     */
    bool removeById(int id, bool allowCompact = true);

    /// Get total number of active rows in the table.
    int getRowCount() const;

    /// Check if table contains a specific value in a column (using index if available).
    bool contains(const string& columnName, const Value& value) const;

    // ========================================================
    // TABLE VIEW: UNIFIED ITERATION (ZERO-COPY & DELETED-SAFE)
    // ========================================================

    /// Return a view to iterate over all active records.
    TableView view() const;

    /// Return a view filtering by indexed column exact match (col == val).
    TableView view(const string& columnName, const Value& value) const;

    /// Return a view filtering by indexed column range match (minVal <= col <= maxVal).
    TableView view(const string& columnName, const Value& minVal, const Value& maxVal) const;

    // ========================================================
    // COMPATIBILITY ACCESSORS
    // ========================================================

    const Vector<Record>& getRows() const;
    bool isRowDeleted(size_t rowIndex) const;

    /// Print table schema structure (columns, types, constraints, indexes).
    void printStructure() const;

    /// Print all row contents in formatted table style.
    void printRows() const;

    /// Clear all records in the table.
    void clear();
};

// ============================================================
// TABLEVIEW INLINE IMPLEMENTATION
// ============================================================

inline TableView::TableView()
    : table(nullptr), useIndex(false)
{
}

inline TableView::TableView(const Table* tbl)
    : table(tbl), useIndex(false)
{
}

inline TableView::TableView(const Table* tbl, Vector<size_t>&& matches)
    : table(tbl), indexMatches(std::move(matches)), useIndex(true)
{
}

inline TableView::Iterator::Iterator(const Table* tbl, const Vector<size_t>* m, size_t p)
    : table(tbl), matches(m), pos(p)
{
    skipDeleted();
}

inline void TableView::Iterator::skipDeleted()
{
    if (!table) return;
    if (!matches)
    {
        while (pos < table->rawRowCount() && table->isRowDeleted(pos))
        {
            ++pos;
        }
    }
    else
    {
        while (pos < matches->size())
        {
            size_t idx = (*matches)[pos];
            if (idx < table->rawRowCount() && !table->isRowDeleted(idx))
            {
                break;
            }
            ++pos;
        }
    }
}

inline const Record& TableView::Iterator::operator*() const
{
    size_t actualIdx = matches ? (*matches)[pos] : pos;
    return table->getRawRow(actualIdx);
}

inline const Record* TableView::Iterator::operator->() const
{
    size_t actualIdx = matches ? (*matches)[pos] : pos;
    return &table->getRawRow(actualIdx);
}

inline TableView::Iterator& TableView::Iterator::operator++()
{
    ++pos;
    skipDeleted();
    return *this;
}

inline TableView::Iterator TableView::Iterator::operator++(int)
{
    Iterator tmp = *this;
    ++(*this);
    return tmp;
}

inline bool TableView::Iterator::operator==(const Iterator& other) const
{
    return table == other.table && pos == other.pos;
}

inline bool TableView::Iterator::operator!=(const Iterator& other) const
{
    return !(*this == other);
}

inline size_t TableView::Iterator::rowIndex() const
{
    return matches ? (*matches)[pos] : pos;
}

inline TableView::Iterator TableView::begin() const
{
    if (!table) return Iterator(nullptr, nullptr, 0);
    return useIndex
        ? Iterator(table, &indexMatches, 0)
        : Iterator(table, nullptr, 0);
}

inline TableView::Iterator TableView::end() const
{
    if (!table) return Iterator(nullptr, nullptr, 0);
    return useIndex
        ? Iterator(table, &indexMatches, indexMatches.size())
        : Iterator(table, nullptr, table->rawRowCount());
}

inline bool TableView::empty() const
{
    return begin() == end();
}

inline size_t TableView::size() const
{
    size_t count = 0;
    for (auto it = begin(); it != end(); ++it)
    {
        ++count;
    }
    return count;
}

#endif // TABLE_H
