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

class Database;

class Table
{
    friend class TableView;
    friend class Database;
    friend class DataIO::TableIO;

private:
    string name;
    string primaryKeyColumn;
    const Database* database = nullptr;
    Vector<Column> columns;
    HashMap<string, size_t> columnIndexMap;
    Vector<Record> rows;
    Vector<unsigned char> deletedRows;
    size_t deletedRowCount = 0;
    mutable size_t activeViewCount = 0;
    HashMap<string, Vector<Constraint*>> constraints;
    HashMap<string, Index*> indexes;
    HashMap<string, int> autoIncrementCounters;

    void pinView() const { ++activeViewCount; }
    void unpinView() const { if (activeViewCount > 0) --activeViewCount; }
    size_t getActiveViewCount() const { return activeViewCount; }

    void setDatabase(const Database* db) { database = db; }

    bool checkDataType(const Column& column, const Value& value) const;
    bool checkColumnConstraints(const string& columnName, const Value& value, int ignoreRowIndex = -1) const;
    bool checkConstraints(const Record& record, int ignoreRowIndex = -1) const;
    void rebuildIndexes();

    // Internal physical access for TableView and internal friends
    size_t rawRowCount() const;
    const Record& getRawRow(size_t rowIndex) const;
    Record& getRawRow(size_t rowIndex);
    bool isRowDeleted(size_t rowIndex) const;

    // Internal row lookup and referential integrity check for Database
    int findRowIndexById(int id) const;
    bool isReferencingRecord(const string& parentTableName, const Record& parentRecord) const;

    // Transaction & compaction management (Database only)
    void compactIfNeeded();
    const HashMap<string, int>& getAutoIncrementCounters() const;
    void undoInsert(size_t rowIndex, const HashMap<string, int>& oldAutoIncrement);
    void undoUpdate(size_t rowIndex, const Record& oldRecord);
    void undoRemove(size_t rowIndex);

    // Index & Constraint internal management (keyed by columnName)
    bool hasIndex(const string& columnName) const;
    bool dropIndex(const string& columnName);
    Index* getIndex(const string& columnName);
    const Index* getIndex(const string& columnName) const;
    const HashMap<string, Index*>& getIndexes() const;
    const HashMap<string, Vector<Constraint*>>& getConstraints() const;

    // Mutable column pointer (internal use)
    Column* getColumn(const string& columnName);

public:
    explicit Table(const string& name = "");
    Table(const Table& other);
    Table(Table&& other) noexcept;
    Table& operator=(const Table& other);
    Table& operator=(Table&& other) noexcept;
    ~Table();

    /// Get owning database (nullptr if standalone).
    const Database* getDatabase() const { return database; }

    /// Get the table name.
    string getName() const;

    /// Set a new name for the table.
    void setName(const string& name);

    /**
     * @brief Add a column definition to the table schema and auto-create column constraints.
     * @param column Column descriptor (name, type, size, autoIncrement, defaultValue).
     * @param primaryKey Auto-create PrimaryKeyConstraint and NotNullConstraint.
     * @param unique Auto-create UniqueConstraint (when not primaryKey).
     * @param notNull Auto-create NotNullConstraint.
     */
    void addColumn(const Column& column, bool primaryKey = false, bool unique = false, bool notNull = false);

    /// Check if a column exists in the table by name.
    bool hasColumn(const string& columnName) const;

    /// Const version of getColumn.
    const Column* getColumn(const string& columnName) const;

    /// Get the number of columns defined in this table.
    int getColumnCount() const;

    /// Get reference to vector of all column definitions.
    const Vector<Column>& getColumns() const;

    /// Check whether a column has a constraint of the given type.
    bool hasConstraint(const string& columnName, ConstraintType type) const;

    /// Check whether a column enforces NOT NULL (via NotNullConstraint or PrimaryKeyConstraint).
    bool isColumnNotNull(const string& columnName) const;

    /// Check whether a column enforces UNIQUE (via UniqueConstraint or PrimaryKeyConstraint).
    bool isColumnUnique(const string& columnName) const;

    /**
     * @brief Add a constraint (PK, Unique, FK, NotNull, Check). Table takes ownership.
     * If rejected (null, unknown column, duplicate name, or conflicting PK), Table deletes it and returns false.
     * @param constraint Pointer to Constraint subclass allocated with new.
     * @return true if successfully added.
     */
    bool addConstraint(Constraint* constraint);

    /**
     * @brief Create an index on a specific column (columnName is the index key).
     * @param columnName Target column to index.
     * @param type Index type (IndexType::HASH, IndexType::AVL for range, IndexType::HASH_TREE).
     * @return true if index was created and built.
     */
    bool createIndex(const string& columnName, IndexType type = IndexType::HASH);

    /**
     * @brief Insert a record into the table, checking data types and all constraints.
     * @param record Record containing column values to insert.
     * @return true if inserted, false if validation failed.
     */
    bool insert(const Record& record);

    /// Get the primary key column name of this table (O(1)).
    const string& getPrimaryKeyColumn() const;

    /// Get pointer to record by record ID in the primary key column (nullptr if not found or deleted).
    const Record* getRecordById(int id) const;

    /**
     * @brief Update a record directly by its primary key ID (validates only changed columns).
     * @param id Value of the primary key.
     * @param newValues Column-value updates.
     * @return true if updated, false if not found or constraint failed.
     */
    bool updateById(int id, const Record& newValues);

    /**
     * @brief Remove a record directly by its primary key ID.
     * @param id Value of the primary key.
     * @param allowCompact Compact deleted rows immediately if threshold met and no active views.
     * @return true if found and removed.
     */
    bool removeById(int id, bool allowCompact = true);

    /// Get total number of active rows in the table.
    int getRowCount() const;

    /// Check if table contains a specific value in a column (optionally ignoring a physical row index).
    bool contains(const string& columnName, const Value& value, int ignoreRowIndex = -1) const;

    // ========================================================
    // TABLE VIEW: UNIFIED ITERATION (ZERO-COPY & DELETED-SAFE)
    // ========================================================

    /// Return a view to iterate over all active records.
    TableView view() const;

    /// Return a view filtering by indexed column exact match (col == val).
    TableView view(const string& columnName, const Value& value) const;

    /// Return a view filtering by indexed column range match (minVal <= col <= maxVal).
    TableView view(const string& columnName, const Value& minVal, const Value& maxVal) const;

    /// Print table schema structure (columns, constraints, indexes).
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
    : table(nullptr), useIndex(false), snapshotRowCount(0)
{
}

inline TableView::TableView(const Table* tbl)
    : table(tbl), useIndex(false), snapshotRowCount(tbl ? tbl->rawRowCount() : 0)
{
    if (table) table->pinView();
}

inline TableView::TableView(const Table* tbl, Vector<size_t>&& matches)
    : table(tbl), indexMatches(std::move(matches)), useIndex(true), snapshotRowCount(tbl ? tbl->rawRowCount() : 0)
{
    if (table) table->pinView();
}

inline TableView::~TableView()
{
    if (table)
    {
        table->unpinView();
        if (table->getActiveViewCount() == 0)
        {
            const_cast<Table*>(table)->compactIfNeeded();
        }
    }
}

inline TableView::TableView(const TableView& other)
    : table(other.table), indexMatches(other.indexMatches), useIndex(other.useIndex), snapshotRowCount(other.snapshotRowCount)
{
    if (table) table->pinView();
}

inline TableView::TableView(TableView&& other) noexcept
    : table(other.table), indexMatches(std::move(other.indexMatches)), useIndex(other.useIndex), snapshotRowCount(other.snapshotRowCount)
{
    other.table = nullptr;
}

inline TableView& TableView::operator=(const TableView& other)
{
    if (this != &other)
    {
        if (table)
        {
            table->unpinView();
            if (table->getActiveViewCount() == 0)
            {
                const_cast<Table*>(table)->compactIfNeeded();
            }
        }
        table = other.table;
        indexMatches = other.indexMatches;
        useIndex = other.useIndex;
        snapshotRowCount = other.snapshotRowCount;
        if (table) table->pinView();
    }
    return *this;
}

inline TableView& TableView::operator=(TableView&& other) noexcept
{
    if (this != &other)
    {
        if (table)
        {
            table->unpinView();
            if (table->getActiveViewCount() == 0)
            {
                const_cast<Table*>(table)->compactIfNeeded();
            }
        }
        table = other.table;
        indexMatches = std::move(other.indexMatches);
        useIndex = other.useIndex;
        snapshotRowCount = other.snapshotRowCount;
        other.table = nullptr;
    }
    return *this;
}

inline TableView::Iterator::Iterator(const Table* tbl, const Vector<size_t>* m, size_t p, size_t bound)
    : table(tbl), matches(m), pos(p), endBound(bound)
{
    skipDeleted();
}

inline void TableView::Iterator::skipDeleted()
{
    if (!table) return;
    if (!matches)
    {
        while (pos < endBound && table->isRowDeleted(pos))
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
    if (!table) return Iterator(nullptr, nullptr, 0, 0);
    return useIndex
        ? Iterator(table, &indexMatches, 0, indexMatches.size())
        : Iterator(table, nullptr, 0, snapshotRowCount);
}

inline TableView::Iterator TableView::end() const
{
    if (!table) return Iterator(nullptr, nullptr, 0, 0);
    return useIndex
        ? Iterator(table, &indexMatches, indexMatches.size(), indexMatches.size())
        : Iterator(table, nullptr, snapshotRowCount, snapshotRowCount);
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
