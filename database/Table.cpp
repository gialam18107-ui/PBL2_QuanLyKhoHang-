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
      constraints(std::move(other.constraints)),
      indexes(std::move(other.indexes)),
      autoIncrementCounters(std::move(other.autoIncrementCounters))
{
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
        constraints = std::move(other.constraints);
        indexes = std::move(other.indexes);
        autoIncrementCounters = std::move(other.autoIncrementCounters);
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

    newIdx->build(rows);
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
    case DataType::BOOLEAN:
        return holds_alternative<bool>(value);
    }
    return true;
}

bool Table::checkConstraints(const Record& record, int ignoreRowIndex) const
{
    const Vector<Record>* targetRows = &rows;
    Vector<Record> validRows;
    if (ignoreRowIndex >= 0 && ignoreRowIndex < static_cast<int>(rows.size()))
    {
        for (size_t i = 0; i < rows.size(); i++)
        {
            if (static_cast<int>(i) != ignoreRowIndex)
            {
                validRows.push_back(rows[i]);
            }
        }
        targetRows = &validRows;
    }

    for (size_t i = 0; i < constraints.size(); i++)
    {
        const Constraint* c = constraints[i];
        // ForeignKey is checked by Database
        if (c->getType() == ConstraintType::FOREIGN_KEY)
        {
            continue;
        }

        if (!c->validate(record, *targetRows, {}))
        {
            cout << "Constraint failed: " << c->getName() << endl;
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
                else if (col.getType() == DataType::BOOLEAN)
                {
                    bool b = (col.getDefaultValue() == "1" || col.getDefaultValue() == "true" || col.getDefaultValue() == "TRUE");
                    recordToInsert.set(colName, b);
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
    Vector<Record> remainingRows;

    for (size_t i = 0; i < rows.size(); i++)
    {
        if (condition.evaluate(rows[i]))
        {
            anyRemoved = true;
        }
        else
        {
            remainingRows.push_back(rows[i]);
        }
    }

    if (anyRemoved)
    {
        rows = std::move(remainingRows);
        // Rebuild all indexes after removal
        for (size_t i = 0; i < indexes.size(); i++)
        {
            indexes[i]->build(rows);
        }
    }

    return anyRemoved;
}

bool Table::remove(const ConditionGroup& group)
{
    bool anyRemoved = false;
    Vector<Record> remainingRows;

    for (size_t i = 0; i < rows.size(); i++)
    {
        if (group.evaluate(rows[i]))
        {
            anyRemoved = true;
        }
        else
        {
            remainingRows.push_back(rows[i]);
        }
    }

    if (anyRemoved)
    {
        rows = std::move(remainingRows);
        for (size_t i = 0; i < indexes.size(); i++)
        {
            indexes[i]->build(rows);
        }
    }

    return anyRemoved;
}

int Table::getRowCount() const
{
    return static_cast<int>(rows.size());
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
    for (size_t i = 0; i < indexes.size(); i++)
    {
        indexes[i]->clear();
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

    if (rows.empty())
    {
        cout << "No records." << endl;
        return;
    }

    for (size_t i = 0; i < rows.size(); i++)
    {
        cout << "Record " << (i + 1) << ":" << endl;
        rows[i].print();
        cout << endl;
    }
}
