#include "Table.h"
#include <iostream>
#include <stdexcept>

using namespace std;

// ============================================================
// CONSTRUCTORS & DESTRUCTOR (RULE OF 5)
// ============================================================

Table::Table(const string& name)
    : name(name)
{
}

Table::Table(const Table& other)
    : name(other.name),
      columns(other.columns),
      rows(other.rows),
      deletedRows(other.deletedRows),
      deletedRowCount(other.deletedRowCount),
      autoIncrementCounters(other.autoIncrementCounters)
{
    for (size_t i = 0; i < other.constraints.size(); i++)
    {
        constraints.push_back(other.constraints[i]->clone());
    }
    for (size_t i = 0; i < other.indexes.size(); i++)
    {
        indexes.push_back(other.indexes[i]->clone());
    }
}

Table::Table(Table&& other) noexcept
    : name(std::move(other.name)),
      columns(std::move(other.columns)),
      rows(std::move(other.rows)),
      deletedRows(std::move(other.deletedRows)),
      deletedRowCount(other.deletedRowCount),
      constraints(std::move(other.constraints)),
      indexes(std::move(other.indexes)),
      autoIncrementCounters(std::move(other.autoIncrementCounters))
{
    other.deletedRowCount = 0;
}

Table& Table::operator=(const Table& other)
{
    if (this != &other)
    {
        clear();
        for (size_t i = 0; i < constraints.size(); i++)
        {
            delete constraints[i];
        }
        constraints.clear();
        for (size_t i = 0; i < indexes.size(); i++)
        {
            delete indexes[i];
        }
        indexes.clear();

        name = other.name;
        columns = other.columns;
        rows = other.rows;
        deletedRows = other.deletedRows;
        deletedRowCount = other.deletedRowCount;
        autoIncrementCounters = other.autoIncrementCounters;

        for (size_t i = 0; i < other.constraints.size(); i++)
        {
            constraints.push_back(other.constraints[i]->clone());
        }
        for (size_t i = 0; i < other.indexes.size(); i++)
        {
            indexes.push_back(other.indexes[i]->clone());
        }
    }
    return *this;
}

Table& Table::operator=(Table&& other) noexcept
{
    if (this != &other)
    {
        clear();
        for (size_t i = 0; i < constraints.size(); i++)
        {
            delete constraints[i];
        }
        constraints.clear();
        for (size_t i = 0; i < indexes.size(); i++)
        {
            delete indexes[i];
        }
        indexes.clear();

        name = std::move(other.name);
        columns = std::move(other.columns);
        rows = std::move(other.rows);
        deletedRows = std::move(other.deletedRows);
        deletedRowCount = other.deletedRowCount;
        constraints = std::move(other.constraints);
        indexes = std::move(other.indexes);
        autoIncrementCounters = std::move(other.autoIncrementCounters);
        other.deletedRowCount = 0;
    }
    return *this;
}

Table::~Table()
{
    for (size_t i = 0; i < constraints.size(); i++)
    {
        delete constraints[i];
    }
    constraints.clear();

    for (size_t i = 0; i < indexes.size(); i++)
    {
        delete indexes[i];
    }
    indexes.clear();
}

string Table::getName() const
{
    return name;
}

void Table::setName(const string& newName)
{
    name = newName;
}

// ============================================================
// COLUMN MANAGEMENT
// ============================================================

void Table::addColumn(const Column& column)
{
    if (hasColumn(column.getName()))
    {
        throw runtime_error("Column already exists: " + column.getName());
    }

    columns.push_back(column);

    if (column.isAutoIncrement())
    {
        autoIncrementCounters[column.getName()] = 0;
    }
}

bool Table::hasColumn(const string& columnName) const
{
    return getColumn(columnName) != nullptr;
}

Column* Table::getColumn(const string& columnName)
{
    for (size_t i = 0; i < columns.size(); i++)
    {
        if (columns[i].getName() == columnName)
        {
            return &columns[i];
        }
    }
    return nullptr;
}

const Column* Table::getColumn(const string& columnName) const
{
    for (size_t i = 0; i < columns.size(); i++)
    {
        if (columns[i].getName() == columnName)
        {
            return &columns[i];
        }
    }
    return nullptr;
}

int Table::getColumnCount() const
{
    return static_cast<int>(columns.size());
}

const Vector<Column>& Table::getColumns() const
{
    return columns;
}

// ============================================================
// CONSTRAINT MANAGEMENT
// ============================================================

bool Table::addConstraint(Constraint* constraint)
{
    if (constraint == nullptr)
    {
        return false;
    }

    for (size_t i = 0; i < constraints.size(); i++)
    {
        if (constraints[i]->getName() == constraint->getName())
        {
            cout << "Constraint already exists: " << constraint->getName() << endl;
            return false;
        }
    }

    constraints.push_back(constraint);

    // Primary-key, unique and foreign-key columns are frequent lookup/join keys.
    // Create a HASH index automatically unless an index already covers the column.
    string indexedColumn;
    if (const auto* primaryKey = dynamic_cast<const PrimaryKeyConstraint*>(constraint))
    {
        indexedColumn = primaryKey->getColumnName();
    }
    else if (const auto* uniqueConstraint = dynamic_cast<const UniqueConstraint*>(constraint))
    {
        indexedColumn = uniqueConstraint->getColumnName();
    }
    else if (const auto* foreignKey = dynamic_cast<const ForeignKeyConstraint*>(constraint))
    {
        indexedColumn = foreignKey->getColumnName();
    }

    if (!indexedColumn.empty() && hasColumn(indexedColumn))
    {
        bool columnAlreadyIndexed = false;
        for (size_t i = 0; i < indexes.size(); ++i)
        {
            if (indexes[i]->getColumnName() == indexedColumn)
            {
                columnAlreadyIndexed = true;
                break;
            }
        }

        if (!columnAlreadyIndexed)
        {
            string indexName = "__auto_hash_" + indexedColumn;
            int suffix = 1;
            while (hasIndex(indexName))
            {
                indexName = "__auto_hash_" + indexedColumn + "_" + to_string(suffix++);
            }
            createIndex(indexName, indexedColumn, IndexType::HASH);
        }
    }

    return true;
}

const Vector<Constraint*>& Table::getConstraints() const
{
    return constraints;
}

// ============================================================
// INDEX MANAGEMENT
// ============================================================

bool Table::createIndex(const string& indexName, const string& columnName, IndexType type)
{
    if (!hasColumn(columnName))
    {
        cout << "Index creation failed: column " << columnName << " not found." << endl;
        return false;
    }

    if (hasIndex(indexName))
    {
        cout << "Index " << indexName << " already exists." << endl;
        return false;
    }

    Index* newIdx = nullptr;
    if (type == IndexType::HASH)
    {
        newIdx = new HashIndex(indexName, columnName);
    }
    else
    {
        newIdx = new BTreeIndex(indexName, columnName);
    }

    Vector<Record> indexRows;
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        indexRows.push_back(isRowDeleted(rowIndex) ? Record() : rows[rowIndex]);
    }
    newIdx->build(indexRows);
    indexes.push_back(newIdx);
    return true;
}

bool Table::dropIndex(const string& indexName)
{
    for (size_t i = 0; i < indexes.size(); i++)
    {
        if (indexes[i]->getName() == indexName)
        {
            delete indexes[i];
            indexes.erase(i);
            return true;
        }
    }
    return false;
}

bool Table::hasIndex(const string& indexName) const
{
    return getIndex(indexName) != nullptr;
}

Index* Table::getIndex(const string& indexName)
{
    for (size_t i = 0; i < indexes.size(); i++)
    {
        if (indexes[i]->getName() == indexName)
        {
            return indexes[i];
        }
    }
    return nullptr;
}

const Index* Table::getIndex(const string& indexName) const
{
    for (size_t i = 0; i < indexes.size(); i++)
    {
        if (indexes[i]->getName() == indexName)
        {
            return indexes[i];
        }
    }
    return nullptr;
}

const Vector<Index*>& Table::getIndexes() const
{
    return indexes;
}

// ============================================================
// DATA TYPE & CONSTRAINT VALIDATION
// ============================================================

bool Table::checkDataType(const Column& column, const Value& value) const
{
    if (isNull(value))
    {
        return !column.isNotNull();
    }

    switch (column.getType())
    {
    case DataType::INTEGER:
        return holds_alternative<int>(value);
    case DataType::VARCHAR:
    case DataType::CHAR:
    case DataType::TEXT:
    case DataType::DATE:
        return holds_alternative<string>(value);
    }
    return true;
}

bool Table::checkConstraints(const Record& record, int ignoreRowIndex) const
{
    Vector<Record> validRows;
    bool validRowsBuilt = false;

    for (size_t i = 0; i < constraints.size(); i++)
    {
        const Constraint* constraint = constraints[i];
        if (constraint->getType() == ConstraintType::FOREIGN_KEY)
        {
            // Foreign keys are validated by Database against the referenced table.
            continue;
        }

        string constrainedColumn;
        if (const auto* primaryKey = dynamic_cast<const PrimaryKeyConstraint*>(constraint))
        {
            constrainedColumn = primaryKey->getColumnName();
        }
        else if (const auto* uniqueConstraint = dynamic_cast<const UniqueConstraint*>(constraint))
        {
            constrainedColumn = uniqueConstraint->getColumnName();
        }

        const Index* columnIndex = nullptr;
        if (!constrainedColumn.empty())
        {
            for (size_t index = 0; index < indexes.size(); ++index)
            {
                if (indexes[index]->getColumnName() == constrainedColumn)
                {
                    columnIndex = indexes[index];
                    break;
                }
            }
        }

        if (columnIndex != nullptr)
        {
            const bool primaryKey = constraint->getType() == ConstraintType::PRIMARY_KEY;
            if (!record.has(constrainedColumn) || isNull(record.get(constrainedColumn)))
            {
                if (primaryKey)
                {
                    cout << "Constraint failed: " << constraint->getName() << endl;
                    return false;
                }
                // UNIQUE permits missing/NULL values in this implementation.
                continue;
            }

            const Value candidateValue = record.get(constrainedColumn);
            const Vector<size_t> candidates = columnIndex->find(candidateValue);
            bool duplicate = false;
            for (size_t candidate = 0; candidate < candidates.size(); ++candidate)
            {
                const size_t rowIndex = candidates[candidate];
                if (rowIndex == static_cast<size_t>(ignoreRowIndex) && ignoreRowIndex >= 0)
                {
                    continue;
                }
                if (rowIndex < rows.size()
                    && !isRowDeleted(rowIndex)
                    && rows[rowIndex].has(constrainedColumn)
                    && rows[rowIndex].get(constrainedColumn) == candidateValue)
                {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate)
            {
                cout << "Constraint failed: " << constraint->getName() << endl;
                return false;
            }
            continue;
        }

        // Build the legacy validation input only when a non-indexed constraint
        // requires it, avoiding a full copy of all rows for indexed PK/UNIQUE.
        if (!validRowsBuilt)
        {
            for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
            {
                if (!isRowDeleted(rowIndex) && static_cast<int>(rowIndex) != ignoreRowIndex)
                {
                    validRows.push_back(rows[rowIndex]);
                }
            }
            validRowsBuilt = true;
        }

        if (!constraint->validate(record, validRows, {}))
        {
            cout << "Constraint failed: " << constraint->getName() << endl;
            return false;
        }
    }

    return true;
}

// ============================================================
// DATA MANIPULATION
// ============================================================

bool Table::insert(const Record& record)
{
    Record recordToInsert = record;

    // Handle auto-increment and default values
    for (size_t i = 0; i < columns.size(); i++)
    {
        const Column& col = columns[i];
        const string& colName = col.getName();

        if (col.isAutoIncrement())
        {
            if (!recordToInsert.has(colName) || isNull(recordToInsert.get(colName)))
            {
                int nextVal = autoIncrementCounters[colName] + 1;
                autoIncrementCounters[colName] = nextVal;
                recordToInsert.set(colName, nextVal);
            }
            else if (holds_alternative<int>(recordToInsert.get(colName)))
            {
                int val = get<int>(recordToInsert.get(colName));
                if (val > autoIncrementCounters[colName])
                {
                    autoIncrementCounters[colName] = val;
                }
            }
        }
        else if (!recordToInsert.has(colName))
        {
            if (!col.getDefaultValue().empty())
            {
                if (col.getType() == DataType::INTEGER)
                {
                    try { recordToInsert.set(colName, stoi(col.getDefaultValue())); }
                    catch (...) { recordToInsert.set(colName, 0); }
                }
                else
                {
                    recordToInsert.set(colName, col.getDefaultValue());
                }
            }
            else if (col.isNotNull())
            {
                cout << "Insert failed: column " << colName << " cannot be null." << endl;
                return false;
            }
            else
            {
                recordToInsert.set(colName, monostate{});
            }
        }
    }

    // Verify data types
    for (size_t i = 0; i < columns.size(); i++)
    {
        const Column& col = columns[i];
        if (recordToInsert.has(col.getName()))
        {
            if (!checkDataType(col, recordToInsert.get(col.getName())))
            {
                cout << "Invalid data type for column: " << col.getName() << endl;
                return false;
            }
        }
    }

    // Verify constraints
    if (!checkConstraints(recordToInsert))
    {
        return false;
    }

    size_t newRowIdx = rows.size();
    rows.push_back(recordToInsert);
    deletedRows.push_back(0);

    // Update indexes
    for (size_t i = 0; i < indexes.size(); i++)
    {
        const string& colName = indexes[i]->getColumnName();
        if (recordToInsert.has(colName))
        {
            indexes[i]->insert(recordToInsert.get(colName), newRowIdx);
        }
    }

    return true;
}

bool Table::update(const Condition& condition, const Record& newValues)
{
    bool anyUpdated = false;

    for (size_t i = 0; i < rows.size(); i++)
    {
        if (isRowDeleted(i)) continue;
        if (!condition.evaluate(rows[i]))
        {
            continue;
        }

        Record updatedRecord = rows[i];
        for (const auto& pair : newValues.getValues())
        {
            const Column* col = getColumn(pair.first);
            if (col == nullptr)
            {
                cout << "Update failed: column not found: " << pair.first << endl;
                return false;
            }

            if (!checkDataType(*col, pair.second))
            {
                cout << "Update failed: invalid data type for: " << pair.first << endl;
                return false;
            }

            updatedRecord.set(pair.first, pair.second);
        }

        if (!checkConstraints(updatedRecord, static_cast<int>(i)))
        {
            return false;
        }

        // Update indexes
        for (size_t j = 0; j < indexes.size(); j++)
        {
            const string& colName = indexes[j]->getColumnName();
            if (newValues.has(colName))
            {
                indexes[j]->remove(rows[i].get(colName), i);
                indexes[j]->insert(updatedRecord.get(colName), i);
            }
        }

        rows[i] = updatedRecord;
        anyUpdated = true;
    }

    return anyUpdated;
}

bool Table::update(const ConditionGroup& group, const Record& newValues)
{
    bool anyUpdated = false;

    for (size_t i = 0; i < rows.size(); i++)
    {
        if (isRowDeleted(i)) continue;
        if (!group.evaluate(rows[i]))
        {
            continue;
        }

        Record updatedRecord = rows[i];
        for (const auto& pair : newValues.getValues())
        {
            const Column* col = getColumn(pair.first);
            if (col == nullptr)
            {
                cout << "Update failed: column not found: " << pair.first << endl;
                return false;
            }

            if (!checkDataType(*col, pair.second))
            {
                cout << "Update failed: invalid data type for: " << pair.first << endl;
                return false;
            }

            updatedRecord.set(pair.first, pair.second);
        }

        if (!checkConstraints(updatedRecord, static_cast<int>(i)))
        {
            return false;
        }

        for (size_t j = 0; j < indexes.size(); j++)
        {
            const string& colName = indexes[j]->getColumnName();
            if (newValues.has(colName))
            {
                indexes[j]->remove(rows[i].get(colName), i);
                indexes[j]->insert(updatedRecord.get(colName), i);
            }
        }

        rows[i] = updatedRecord;
        anyUpdated = true;
    }

    return anyUpdated;
}

bool Table::remove(const Condition& condition)
{
    bool anyRemoved = false;

    for (size_t i = 0; i < rows.size(); i++)
    {
        if (!isRowDeleted(i) && condition.evaluate(rows[i]))
        {
            deletedRows[i] = 1;
            ++deletedRowCount;
            anyRemoved = true;
        }
    }

    if (anyRemoved)
    {
        if (deletedRowCount >= rows.size() / 2)
        {
            compactRows();
        }
    }

    return anyRemoved;
}

bool Table::remove(const ConditionGroup& group)
{
    bool anyRemoved = false;

    for (size_t i = 0; i < rows.size(); i++)
    {
        if (!isRowDeleted(i) && group.evaluate(rows[i]))
        {
            deletedRows[i] = 1;
            ++deletedRowCount;
            anyRemoved = true;
        }
    }

    if (anyRemoved)
    {
        if (deletedRowCount >= rows.size() / 2)
        {
            compactRows();
        }
    }

    return anyRemoved;
}

int Table::getRowCount() const
{
    return static_cast<int>(rows.size() - deletedRowCount);
}

bool Table::isRowDeleted(size_t rowIndex) const
{
    return rowIndex < deletedRows.size() && deletedRows[rowIndex] != 0;
}

const Vector<Record>& Table::getRows() const
{
    return rows;
}

Vector<Record>& Table::getRows()
{
    return rows;
}

void Table::clear()
{
    rows.clear();
    deletedRows.clear();
    deletedRowCount = 0;
    for (size_t i = 0; i < indexes.size(); i++)
    {
        indexes[i]->clear();
    }
}

void Table::compactRows()
{
    Vector<Record> activeRows;
    activeRows.reserve(rows.size() - deletedRowCount);
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        if (!isRowDeleted(rowIndex))
        {
            activeRows.push_back(rows[rowIndex]);
        }
    }

    rows = std::move(activeRows);
    deletedRows.clear();
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        deletedRows.push_back(0);
    }
    deletedRowCount = 0;
    rebuildIndexes();
}

void Table::rebuildIndexes()
{
    for (size_t index = 0; index < indexes.size(); ++index)
    {
        indexes[index]->build(rows);
    }
}

void Table::printStructure() const
{
    cout << "TABLE: " << name << endl;
    cout << "-----------------------------" << endl;

    for (size_t i = 0; i < columns.size(); i++)
    {
        columns[i].print();
    }

    cout << "\nCONSTRAINTS:" << endl;
    for (size_t i = 0; i < constraints.size(); i++)
    {
        cout << "- " << constraints[i]->getName() << endl;
    }

    if (!indexes.empty())
    {
        cout << "\nINDEXES:" << endl;
        for (size_t i = 0; i < indexes.size(); i++)
        {
            cout << "- " << indexes[i]->getName() << " ON (" << indexes[i]->getColumnName() << ")" << endl;
        }
    }
    cout << endl;
}

void Table::printRows() const
{
    cout << "TABLE DATA: " << name << endl;
    cout << "-----------------------------" << endl;

    if (getRowCount() == 0)
    {
        cout << "No records." << endl;
        return;
    }

    size_t displayedRow = 0;
    for (size_t i = 0; i < rows.size(); i++)
    {
        if (isRowDeleted(i)) continue;
        cout << "Record " << (++displayedRow) << ":" << endl;
        rows[i].print();
        cout << endl;
    }
}
