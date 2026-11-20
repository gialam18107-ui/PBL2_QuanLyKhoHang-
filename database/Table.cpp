#include "Table.h"
#include "datastructure/hashmap/Pair.h"
#include <iostream>
#include <stdexcept>

using namespace std;

// ============================================================
// CONSTRUCTORS & DESTRUCTOR (RULE OF 5)
// ============================================================

Table::Table(const string& name)
    : name(name),
      database(nullptr),
      columns(8),
      constraints(8),
      indexes(8)
{
}

Table::Table(const Table& other)
    : name(other.name),
      primaryKeyColumn(other.primaryKeyColumn),
      database(nullptr),
      columns(other.columns),
      rows(other.rows),
      deletedRows(other.deletedRows),
      deletedRowCount(other.deletedRowCount),
      constraints(8),
      indexes(8),
      autoIncrementCounters(other.autoIncrementCounters)
{
    for (const auto& entry : other.constraints)
    {
        Vector<Constraint*> clonedList;
        clonedList.reserve(entry.second.size());
        for (size_t i = 0; i < entry.second.size(); ++i)
        {
            clonedList.push_back(entry.second[i]->clone());
        }
        constraints.insert(entry.first, std::move(clonedList));
    }
    for (const auto& entry : other.indexes)
    {
        indexes.insert(entry.first, entry.second->clone());
    }
}

Table::Table(Table&& other) noexcept
    : name(std::move(other.name)),
      primaryKeyColumn(std::move(other.primaryKeyColumn)),
      database(nullptr),
      columns(std::move(other.columns)),
      rows(std::move(other.rows)),
      deletedRows(std::move(other.deletedRows)),
      deletedRowCount(other.deletedRowCount),
      constraints(std::move(other.constraints)),
      indexes(std::move(other.indexes)),
      autoIncrementCounters(std::move(other.autoIncrementCounters))
{
    other.deletedRowCount = 0;
    other.activeViewCount = 0;
    other.database = nullptr;
}

Table& Table::operator=(const Table& other)
{
    if (this != &other)
    {
        clear();
        for (const auto& entry : constraints)
        {
            for (size_t i = 0; i < entry.second.size(); ++i)
            {
                delete entry.second[i];
            }
        }
        constraints.clear();
        for (const auto& entry : indexes)
        {
            delete entry.second;
        }
        indexes.clear();

        name = other.name;
        primaryKeyColumn = other.primaryKeyColumn;
        database = nullptr;
        columns = other.columns;
        rows = other.rows;
        deletedRows = other.deletedRows;
        deletedRowCount = other.deletedRowCount;
        autoIncrementCounters = other.autoIncrementCounters;

        for (const auto& entry : other.constraints)
        {
            Vector<Constraint*> clonedList;
            clonedList.reserve(entry.second.size());
            for (size_t i = 0; i < entry.second.size(); ++i)
            {
                clonedList.push_back(entry.second[i]->clone());
            }
            constraints.insert(entry.first, std::move(clonedList));
        }
        for (const auto& entry : other.indexes)
        {
            indexes.insert(entry.first, entry.second->clone());
        }
    }
    return *this;
}

Table& Table::operator=(Table&& other) noexcept
{
    if (this != &other)
    {
        clear();
        for (const auto& entry : constraints)
        {
            for (size_t i = 0; i < entry.second.size(); ++i)
            {
                delete entry.second[i];
            }
        }
        constraints.clear();
        for (const auto& entry : indexes)
        {
            delete entry.second;
        }
        indexes.clear();

        name = std::move(other.name);
        primaryKeyColumn = std::move(other.primaryKeyColumn);
        database = nullptr;
        columns = std::move(other.columns);
        rows = std::move(other.rows);
        deletedRows = std::move(other.deletedRows);
        deletedRowCount = other.deletedRowCount;
        constraints = std::move(other.constraints);
        indexes = std::move(other.indexes);
        autoIncrementCounters = std::move(other.autoIncrementCounters);
        other.deletedRowCount = 0;
        other.activeViewCount = 0;
        other.database = nullptr;
    }
    return *this;
}

Table::~Table()
{
    for (const auto& entry : constraints)
    {
        for (size_t i = 0; i < entry.second.size(); ++i)
        {
            delete entry.second[i];
        }
    }
    constraints.clear();

    for (const auto& entry : indexes)
    {
        delete entry.second;
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

void Table::addColumn(const Column& column, bool primaryKey, bool unique, bool notNull)
{
    const string& colName = column.getName();
    if (hasColumn(colName))
    {
        throw runtime_error("Column already exists: " + colName);
    }

    columns.insert(colName, column);

    if (column.isAutoIncrement())
    {
        autoIncrementCounters[colName] = 0;
    }

    if (primaryKey)
    {
        addConstraint(new PrimaryKeyConstraint("pk_" + name + "_" + colName, colName));
        addConstraint(new NotNullConstraint("nn_" + name + "_" + colName, colName));
    }
    else
    {
        if (notNull)
        {
            addConstraint(new NotNullConstraint("nn_" + name + "_" + colName, colName));
        }
        if (unique)
        {
            addConstraint(new UniqueConstraint("uq_" + name + "_" + colName, colName));
        }
    }
}

bool Table::hasColumn(const string& columnName) const
{
    return columns.contains(columnName);
}

Column* Table::getColumn(const string& columnName)
{
    auto it = columns.find(columnName);
    if (it == columns.end())
    {
        return nullptr;
    }
    return &it->second;
}

const Column* Table::getColumn(const string& columnName) const
{
    auto it = columns.find(columnName);
    if (it == columns.end())
    {
        return nullptr;
    }
    return &it->second;
}

int Table::getColumnCount() const
{
    return static_cast<int>(columns.size());
}

const HashMap<string, Column>& Table::getColumns() const
{
    return columns;
}

// ============================================================
// CONSTRAINT MANAGEMENT
// ============================================================

bool Table::hasConstraint(const string& columnName, ConstraintType type) const
{
    auto it = constraints.find(columnName);
    if (it == constraints.end())
    {
        return false;
    }
    const Vector<Constraint*>& colConstraints = it->second;
    for (size_t i = 0; i < colConstraints.size(); ++i)
    {
        if (colConstraints[i]->getType() == type)
        {
            return true;
        }
    }
    return false;
}

bool Table::isColumnNotNull(const string& columnName) const
{
    return hasConstraint(columnName, ConstraintType::NOT_NULL) ||
           hasConstraint(columnName, ConstraintType::PRIMARY_KEY);
}

bool Table::isColumnUnique(const string& columnName) const
{
    return hasConstraint(columnName, ConstraintType::UNIQUE) ||
           hasConstraint(columnName, ConstraintType::PRIMARY_KEY);
}

bool Table::addConstraint(Constraint* constraint)
{
    if (constraint == nullptr)
    {
        return false;
    }

    const string& colName = constraint->getColumnName();
    if (!hasColumn(colName))
    {
        delete constraint;
        return false;
    }

    ConstraintType cType = constraint->getType();
    if (cType == ConstraintType::PRIMARY_KEY && !primaryKeyColumn.empty() && primaryKeyColumn != colName)
    {
        cout << "Constraint failed: table '" << name << "' already has primary key '" << primaryKeyColumn << "'." << endl;
        delete constraint;
        return false;
    }

    for (const auto& entry : constraints)
    {
        const Vector<Constraint*>& colConstraints = entry.second;
        for (size_t i = 0; i < colConstraints.size(); ++i)
        {
            if (colConstraints[i]->getName() == constraint->getName())
            {
                cout << "Constraint already exists: " << constraint->getName() << endl;
                delete constraint;
                return false;
            }
        }
    }

    if ((cType == ConstraintType::PRIMARY_KEY || cType == ConstraintType::UNIQUE || cType == ConstraintType::NOT_NULL) &&
        hasConstraint(colName, cType))
    {
        delete constraint;
        return false;
    }

    auto it = constraints.find(colName);
    if (it == constraints.end())
    {
        Vector<Constraint*> colConstraints;
        colConstraints.reserve(2);
        colConstraints.push_back(constraint);
        constraints.insert(colName, std::move(colConstraints));
    }
    else
    {
        it->second.push_back(constraint);
    }

    if (cType == ConstraintType::PRIMARY_KEY)
    {
        primaryKeyColumn = colName;
    }

    // Automatically provision index for PK / Unique (HASH) or FK (HASH_TREE)
    if (cType == ConstraintType::PRIMARY_KEY || cType == ConstraintType::UNIQUE)
    {
        auto idxIt = indexes.find(colName);
        if (idxIt == indexes.end() || idxIt->second->getType() == IndexType::HASH_TREE)
        {
            createIndex(colName, IndexType::HASH);
        }
    }
    else if (cType == ConstraintType::FOREIGN_KEY)
    {
        if (!indexes.contains(colName))
        {
            createIndex(colName, IndexType::HASH_TREE);
        }
    }

    return true;
}

const HashMap<string, Vector<Constraint*>>& Table::getConstraints() const
{
    return constraints;
}

// ============================================================
// INDEX MANAGEMENT
// ============================================================

bool Table::createIndex(const string& columnName, IndexType type)
{
    if (!hasColumn(columnName))
    {
        cout << "Index creation failed: column " << columnName << " not found." << endl;
        return false;
    }

    auto it = indexes.find(columnName);
    if (it != indexes.end())
    {
        if (it->second->getType() == type)
        {
            return true;
        }
        delete it->second;
        indexes.erase(columnName);
    }

    Index* newIdx = nullptr;
    if (type == IndexType::HASH)
    {
        newIdx = new HashIndex(columnName);
    }
    else if (type == IndexType::AVL)
    {
        newIdx = new AVLTreeIndex(columnName);
    }
    else if (type == IndexType::HASH_TREE)
    {
        newIdx = new HashTreeIndex(columnName);
    }

    if (newIdx == nullptr)
    {
        return false;
    }

    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        if (!isRowDeleted(rowIndex))
        {
            auto valIt = rows[rowIndex].getValues().find(columnName);
            if (valIt != rows[rowIndex].getValues().end())
            {
                newIdx->insert(valIt->second, rowIndex);
            }
        }
    }

    indexes.insert(columnName, newIdx);
    return true;
}

bool Table::dropIndex(const string& columnName)
{
    auto it = indexes.find(columnName);
    if (it == indexes.end())
    {
        return false;
    }
    delete it->second;
    indexes.erase(columnName);
    return true;
}

bool Table::hasIndex(const string& columnName) const
{
    return indexes.contains(columnName);
}

Index* Table::getIndex(const string& columnName)
{
    auto it = indexes.find(columnName);
    return it == indexes.end() ? nullptr : it->second;
}

const Index* Table::getIndex(const string& columnName) const
{
    auto it = indexes.find(columnName);
    return it == indexes.end() ? nullptr : it->second;
}

const HashMap<string, Index*>& Table::getIndexes() const
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
        return true; // Nullability is enforced by NotNullConstraint / PrimaryKeyConstraint
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

bool Table::checkConstraints(const string& columnName, const Value& value) const
{
    auto it = constraints.find(columnName);
    if (it == constraints.end())
    {
        return true;
    }

    const Vector<Constraint*>& colConstraints = it->second;
    for (size_t i = 0; i < colConstraints.size(); ++i)
    {
        const Constraint* c = colConstraints[i];
        if (!c->validate(value, *this))
        {
            cout << "Constraint failed: " << c->getName() << " on table '" << name << "'" << endl;
            return false;
        }
    }
    return true;
}

bool Table::isReferencingRecord(const string& parentTableName, const Record& parentRecord) const
{
    const auto& parentMap = parentRecord.getValues();
    for (const auto& entry : constraints)
    {
        const Vector<Constraint*>& colConstraints = entry.second;
        for (size_t i = 0; i < colConstraints.size(); ++i)
        {
            if (colConstraints[i]->getType() != ConstraintType::FOREIGN_KEY)
            {
                continue;
            }
            const auto* fk = static_cast<const ForeignKeyConstraint*>(colConstraints[i]);
            if (fk->getReferenceTable() != parentTableName)
            {
                continue;
            }
            auto parentIt = parentMap.find(fk->getReferenceColumn());
            if (parentIt == parentMap.end() || isNull(parentIt->second))
            {
                continue;
            }
            if (contains(fk->getColumnName(), parentIt->second))
            {
                return true;
            }
        }
    }
    return false;
}

// ============================================================
// DATA MANIPULATION
// ============================================================

bool Table::insert(const Record& record)
{
    for (const auto& pair : record.getValues())
    {
        if (!hasColumn(pair.first))
        {
            cout << "Insert failed: column not found: " << pair.first << endl;
            return false;
        }
    }

    Record recordToInsert = record;
    Vector<Pair<string, int>> pendingAutoInc;

    // Single pass over schema columns: fill auto-increment / typed defaults, check data types & constraints
    for (const auto& colEntry : columns)
    {
        const string& colName = colEntry.first;
        const Column& col = colEntry.second;

        if (col.isAutoIncrement())
        {
            auto it = recordToInsert.getValues().find(colName);
            if (it == recordToInsert.getValues().end() || isNull(it->second))
            {
                int nextVal = autoIncrementCounters[colName] + 1;
                recordToInsert.set(colName, nextVal);
                pendingAutoInc.push_back(makePair(colName, nextVal));
            }
            else if (holds_alternative<int>(it->second))
            {
                int val = get<int>(it->second);
                if (val > autoIncrementCounters[colName])
                {
                    pendingAutoInc.push_back(makePair(colName, val));
                }
            }
        }
        else if (!recordToInsert.has(colName))
        {
            const Value& defVal = col.getDefaultValue();
            if (!isNull(defVal))
            {
                recordToInsert.set(colName, defVal);
            }
            else
            {
                recordToInsert.set(colName, monostate{});
            }
        }

        auto valIt = recordToInsert.getValues().find(colName);
        if (valIt != recordToInsert.getValues().end())
        {
            if (!checkDataType(col, valIt->second))
            {
                cout << "Invalid data type for column: " << colName << endl;
                return false;
            }
            if (!checkConstraints(colName, valIt->second))
            {
                return false;
            }
        }
    }

    // Commit auto-increment counters only after full validation succeeds
    for (size_t i = 0; i < pendingAutoInc.size(); ++i)
    {
        autoIncrementCounters[pendingAutoInc[i].first] = pendingAutoInc[i].second;
    }

    const auto& insertMap = recordToInsert.getValues();
    size_t newRowIdx = rows.size();
    for (const auto& idxEntry : indexes)
    {
        auto valIt = insertMap.find(idxEntry.first);
        if (valIt != insertMap.end())
        {
            idxEntry.second->insert(valIt->second, newRowIdx);
        }
    }

    rows.push_back(std::move(recordToInsert));
    deletedRows.push_back(0);
    return true;
}

const string& Table::getPrimaryKeyColumn() const
{
    return primaryKeyColumn;
}

int Table::findRowIndexById(const Value& id) const
{
    if (primaryKeyColumn.empty() || isNull(id))
    {
        return -1;
    }

    auto idxIt = indexes.find(primaryKeyColumn);
    if (idxIt != indexes.end())
    {
        size_t rIdx = 0;
        if (idxIt->second->findFirst(id, rIdx) && !isRowDeleted(rIdx))
        {
            return static_cast<int>(rIdx);
        }
        return -1;
    }

    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (isRowDeleted(i)) continue;
        auto valIt = rows[i].getValues().find(primaryKeyColumn);
        if (valIt != rows[i].getValues().end() && valIt->second == id)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const Record* Table::getRecordById(const Value& id) const
{
    int idx = findRowIndexById(id);
    if (idx < 0 || idx >= static_cast<int>(rows.size())) return nullptr;
    return &rows[static_cast<size_t>(idx)];
}

bool Table::removeById(const Value& id, bool allowCompact)
{
    int idx = findRowIndexById(id);
    if (idx < 0) return false;

    size_t rowIdx = static_cast<size_t>(idx);
    const auto& rowMap = rows[rowIdx].getValues();
    for (const auto& idxEntry : indexes)
    {
        auto valIt = rowMap.find(idxEntry.first);
        if (valIt != rowMap.end())
        {
            idxEntry.second->remove(valIt->second, rowIdx);
        }
    }

    deletedRows[rowIdx] = 1;
    ++deletedRowCount;

    if (allowCompact)
    {
        compactIfNeeded();
    }
    return true;
}

bool Table::updateById(const Value& id, const Record& newValues)
{
    int idx = findRowIndexById(id);
    if (idx < 0) return false;

    size_t rowIdx = static_cast<size_t>(idx);
    Record& targetRow = rows[rowIdx];
    const auto& currentMap = targetRow.getValues();

    Vector<Pair<string, Value>> changedFields;
    changedFields.reserve(static_cast<size_t>(newValues.size()));

    for (const auto& pair : newValues.getValues())
    {
        const string& colName = pair.first;
        const Value& newVal = pair.second;

        const Column* col = getColumn(colName);
        if (col == nullptr)
        {
            cout << "Update failed: column not found: " << colName << endl;
            return false;
        }
        if (!checkDataType(*col, newVal))
        {
            cout << "Update failed: invalid data type for: " << colName << endl;
            return false;
        }

        auto curIt = currentMap.find(colName);
        if (curIt != currentMap.end() && curIt->second == newVal)
        {
            continue; // Unchanged value: skip constraint check and index update
        }

        if (!checkConstraints(colName, newVal))
        {
            return false;
        }

        changedFields.push_back(makePair(colName, newVal));
    }

    // Apply validated updates in-place and update only affected indexes
    for (size_t i = 0; i < changedFields.size(); ++i)
    {
        const string& colName = changedFields[i].first;
        const Value& newVal = changedFields[i].second;

        auto idxIt = indexes.find(colName);
        if (idxIt != indexes.end())
        {
            auto oldIt = targetRow.getValues().find(colName);
            if (oldIt != targetRow.getValues().end())
            {
                idxIt->second->remove(oldIt->second, rowIdx);
            }
            idxIt->second->insert(newVal, rowIdx);
        }

        targetRow.set(colName, newVal);
    }

    return true;
}

int Table::getRowCount() const
{
    return static_cast<int>(rows.size() - deletedRowCount);
}

bool Table::isRowDeleted(size_t rowIndex) const
{
    return rowIndex < deletedRows.size() && deletedRows[rowIndex] != 0;
}

bool Table::contains(const string& columnName, const Value& value) const
{
    auto idxIt = indexes.find(columnName);
    if (idxIt != indexes.end())
    {
        return idxIt->second->contains(value);
    }

    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (isRowDeleted(i)) continue;
        auto valIt = rows[i].getValues().find(columnName);
        if (valIt != rows[i].getValues().end() && valIt->second == value)
        {
            return true;
        }
    }
    return false;
}

size_t Table::rawRowCount() const
{
    return rows.size();
}

const Record& Table::getRawRow(size_t rowIndex) const
{
    return rows[rowIndex];
}

Record& Table::getRawRow(size_t rowIndex)
{
    return rows[rowIndex];
}

TableView Table::view() const
{
    return TableView(this);
}

TableView Table::view(const string& columnName, const Value& value) const
{
    auto idxIt = indexes.find(columnName);
    if (idxIt != indexes.end())
    {
        Vector<size_t> matched = idxIt->second->find(value);
        return TableView(this, std::move(matched));
    }

    // Fallback: column has no index. Scan active rows.
    Vector<size_t> matched;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (!isRowDeleted(i))
        {
            auto valIt = rows[i].getValues().find(columnName);
            if (valIt != rows[i].getValues().end() && valIt->second == value)
            {
                matched.push_back(i);
            }
        }
    }
    return TableView(this, std::move(matched));
}

TableView Table::view(const string& columnName, const Value& minVal, const Value& maxVal) const
{
    auto idxIt = indexes.find(columnName);
    if (idxIt != indexes.end() && idxIt->second->getType() == IndexType::AVL)
    {
        Vector<size_t> matched = idxIt->second->findRange(minVal, maxVal);
        return TableView(this, std::move(matched));
    }

    // Fallback: column has no AVL index. Scan active rows within range.
    Vector<size_t> matched;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (!isRowDeleted(i))
        {
            auto valIt = rows[i].getValues().find(columnName);
            if (valIt != rows[i].getValues().end())
            {
                const Value& val = valIt->second;
                if (!(val < minVal) && !(maxVal < val))
                {
                    matched.push_back(i);
                }
            }
        }
    }
    return TableView(this, std::move(matched));
}

void Table::clear()
{
    rows.clear();
    deletedRows.clear();
    deletedRowCount = 0;
    for (const auto& entry : indexes)
    {
        entry.second->clear();
    }
}

void Table::compactIfNeeded()
{
    if (activeViewCount != 0 || rows.empty() || deletedRowCount < rows.size() / 2)
    {
        return;
    }

    size_t writeIdx = 0;
    for (size_t readIdx = 0; readIdx < rows.size(); ++readIdx)
    {
        if (!isRowDeleted(readIdx))
        {
            if (writeIdx != readIdx)
            {
                rows[writeIdx] = std::move(rows[readIdx]);
            }
            deletedRows[writeIdx] = 0;
            ++writeIdx;
        }
    }

    rows.resize(writeIdx);
    deletedRows.resize(writeIdx);
    deletedRowCount = 0;
    rebuildIndexes();
}

const HashMap<string, int>& Table::getAutoIncrementCounters() const
{
    return autoIncrementCounters;
}

void Table::undoInsert(size_t rowIndex, const HashMap<string, int>& oldAutoIncrement)
{
    if (rowIndex >= rows.size()) return;

    const auto& insertedMap = rows[rowIndex].getValues();
    for (const auto& idxEntry : indexes)
    {
        auto valIt = insertedMap.find(idxEntry.first);
        if (valIt != insertedMap.end())
        {
            idxEntry.second->remove(valIt->second, rowIndex);
        }
    }

    if (rowIndex + 1 == rows.size())
    {
        rows.pop_back();
        deletedRows.pop_back();
    }
    else if (!isRowDeleted(rowIndex))
    {
        deletedRows[rowIndex] = 1;
        ++deletedRowCount;
    }

    autoIncrementCounters = oldAutoIncrement;
}

void Table::undoUpdate(size_t rowIndex, const Record& oldRecord)
{
    if (rowIndex >= rows.size()) return;

    const auto& currentMap = rows[rowIndex].getValues();
    const auto& oldMap = oldRecord.getValues();
    for (const auto& idxEntry : indexes)
    {
        const string& colName = idxEntry.first;
        auto curIt = currentMap.find(colName);
        auto oldIt = oldMap.find(colName);
        bool hasCurr = (curIt != currentMap.end());
        bool hasOld = (oldIt != oldMap.end());
        if (hasCurr && hasOld && curIt->second == oldIt->second)
        {
            continue;
        }
        if (hasCurr)
        {
            idxEntry.second->remove(curIt->second, rowIndex);
        }
        if (hasOld)
        {
            idxEntry.second->insert(oldIt->second, rowIndex);
        }
    }

    rows[rowIndex] = oldRecord;
}

void Table::undoRemove(size_t rowIndex)
{
    if (rowIndex >= rows.size() || !isRowDeleted(rowIndex)) return;

    deletedRows[rowIndex] = 0;
    if (deletedRowCount > 0)
    {
        --deletedRowCount;
    }

    const auto& restoredMap = rows[rowIndex].getValues();
    for (const auto& idxEntry : indexes)
    {
        auto valIt = restoredMap.find(idxEntry.first);
        if (valIt != restoredMap.end())
        {
            idxEntry.second->insert(valIt->second, rowIndex);
        }
    }
}

void Table::rebuildIndexes()
{
    for (const auto& idxEntry : indexes)
    {
        const string& colName = idxEntry.first;
        Index* idx = idxEntry.second;
        idx->clear();
        for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
        {
            if (!isRowDeleted(rowIndex))
            {
                auto valIt = rows[rowIndex].getValues().find(colName);
                if (valIt != rows[rowIndex].getValues().end())
                {
                    idx->insert(valIt->second, rowIndex);
                }
            }
        }
    }
}

void Table::printStructure() const
{
    cout << "TABLE: " << name << endl;
    cout << "-----------------------------" << endl;

    for (const auto& entry : columns)
    {
        entry.second.print();
    }

    cout << "\nCONSTRAINTS:" << endl;
    for (const auto& entry : constraints)
    {
        for (size_t i = 0; i < entry.second.size(); ++i)
        {
            cout << "- " << entry.second[i]->getName() << " (" << entry.first << ")" << endl;
        }
    }

    if (!indexes.empty())
    {
        cout << "\nINDEXES:" << endl;
        for (const auto& entry : indexes)
        {
            cout << "- " << entry.first << endl;
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
