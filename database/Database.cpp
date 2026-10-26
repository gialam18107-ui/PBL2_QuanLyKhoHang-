#include "Database.h"

#include <iostream>
#include <utility>

using namespace std;

Database::Database(const string& name)
    : databaseName(name), inTransaction(false)
{
}

Database::Database(Database&& other) noexcept
    : databaseName(std::move(other.databaseName)),
      tables(std::move(other.tables)),
      inTransaction(other.inTransaction),
      transactionSnapshot(std::move(other.transactionSnapshot))
{
    other.inTransaction = false;
}

Database& Database::operator=(Database&& other) noexcept
{
    if (this != &other)
    {
        clear();
        clearTransactionSnapshot();

        databaseName = std::move(other.databaseName);
        tables = std::move(other.tables);
        inTransaction = other.inTransaction;
        transactionSnapshot = std::move(other.transactionSnapshot);
        other.inTransaction = false;
    }
    return *this;
}

Database::~Database()
{
    clear();
    clearTransactionSnapshot();
}

string Database::getName() const { return databaseName; }
void Database::setName(const string& name) { databaseName = name; }

void Database::clearTransactionSnapshot()
{
    for (const auto& item : transactionSnapshot) delete item.second;
    transactionSnapshot.clear();
}

Table* Database::createTable(const string& tableName)
{
    if (hasTable(tableName)) return getTable(tableName);
    Table* table = new Table(tableName);
    tables[tableName] = table;
    return table;
}

bool Database::dropTable(const string& tableName)
{
    auto it = tables.find(tableName);
    if (it == tables.end()) return false;
    delete it->second;
    tables.erase(tableName);
    return true;
}

bool Database::addTable(Table* table)
{
    if (table == nullptr || hasTable(table->getName())) return false;
    tables[table->getName()] = table;
    return true;
}

bool Database::hasTable(const string& tableName) const
{
    return tables.find(tableName) != tables.end();
}

Table* Database::getTable(const string& tableName)
{
    auto it = tables.find(tableName);
    return it == tables.end() ? nullptr : it->second;
}

const Table* Database::getTable(const string& tableName) const
{
    auto it = tables.find(tableName);
    return it == tables.end() ? nullptr : it->second;
}

int Database::getTableCount() const { return static_cast<int>(tables.size()); }
Vector<string> Database::getTableNames() const { return tables.getKeys(); }

void Database::printTables() const
{
    cout << "DATABASE TABLES" << endl << "==============================" << endl;
    if (tables.empty())
    {
        cout << "No tables." << endl;
        return;
    }
    for (const auto& item : tables) cout << "- " << item.first << endl;
}

static bool containsColumnValue(const Table& table, const string& columnName, const Value& value)
{
    const Vector<Index*>& indexes = table.getIndexes();
    for (size_t index = 0; index < indexes.size(); ++index)
    {
        if (indexes[index]->getColumnName() != columnName) continue;
        const Vector<size_t> candidates = indexes[index]->find(value);
        const Vector<Record>& rows = table.getRows();
        for (size_t candidate = 0; candidate < candidates.size(); ++candidate)
        {
            const size_t rowIndex = candidates[candidate];
            if (rowIndex < rows.size()
                && !table.isRowDeleted(rowIndex)
                && rows[rowIndex].has(columnName)
                && rows[rowIndex].get(columnName) == value)
            {
                return true;
            }
        }
        return false;
    }

    // Keep compatibility for referenced columns that are not indexed.
    const Vector<Record>& rows = table.getRows();
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        if (!table.isRowDeleted(rowIndex)
            && rows[rowIndex].has(columnName)
            && rows[rowIndex].get(columnName) == value)
        {
            return true;
        }
    }
    return false;
}

static Value resolveDefaultColumnValue(const Column& col)
{
    if (col.getDefaultValue().empty()) return monostate{};
    if (col.getType() == DataType::INTEGER)
    {
        try { return stoi(col.getDefaultValue()); }
        catch (...) { return 0; }
    }
    if (col.getType() == DataType::BOOLEAN)
    {
        return (col.getDefaultValue() == "1" || col.getDefaultValue() == "true" || col.getDefaultValue() == "TRUE");
    }
    return col.getDefaultValue();
}

static bool isColumnValueReferenced(
    const Database& database,
    const string& parentTableName,
    const string& parentColumnName,
    const Value& parentValue
)
{
    if (isNull(parentValue)) return false;

    Vector<string> names = database.getTableNames();
    for (size_t tableIndex = 0; tableIndex < names.size(); ++tableIndex)
    {
        const Table* childTable = database.getTable(names[tableIndex]);
        if (childTable == nullptr || childTable->getName() == parentTableName) continue;

        for (size_t constraintIndex = 0; constraintIndex < childTable->getConstraints().size(); ++constraintIndex)
        {
            const auto* foreignKey = dynamic_cast<const ForeignKeyConstraint*>(childTable->getConstraints()[constraintIndex]);
            if (foreignKey == nullptr) continue;
            if (foreignKey->getReferenceTable() != parentTableName) continue;
            if (foreignKey->getReferenceColumn() != parentColumnName) continue;

            if (containsColumnValue(*childTable, foreignKey->getColumnName(), parentValue))
            {
                return true;
            }
        }
    }
    return false;
}

static bool isRecordReferenced(const Database& database, const string& tableName, const Record& parentRecord)
{
    Vector<string> names = database.getTableNames();
    for (size_t tableIndex = 0; tableIndex < names.size(); ++tableIndex)
    {
        const Table* childTable = database.getTable(names[tableIndex]);
        if (childTable == nullptr || childTable->getName() == tableName) continue;

        for (size_t constraintIndex = 0; constraintIndex < childTable->getConstraints().size(); ++constraintIndex)
        {
            const auto* foreignKey = dynamic_cast<const ForeignKeyConstraint*>(childTable->getConstraints()[constraintIndex]);
            if (foreignKey == nullptr || foreignKey->getReferenceTable() != tableName) continue;

            const string& refCol = foreignKey->getReferenceColumn();
            if (!parentRecord.has(refCol) || isNull(parentRecord.get(refCol))) continue;

            if (containsColumnValue(*childTable, foreignKey->getColumnName(), parentRecord.get(refCol)))
            {
                return true;
            }
        }
    }
    return false;
}

bool Database::insert(const string& tableName, const Record& r)
{
    Table* table = getTable(tableName);
    if (table == nullptr) return false;

    for (size_t index = 0; index < table->getConstraints().size(); ++index)
    {
        Constraint* constraint = table->getConstraints()[index];
        if (constraint->getType() != ConstraintType::FOREIGN_KEY) continue;
        auto* foreignKey = dynamic_cast<ForeignKeyConstraint*>(constraint);
        if (foreignKey == nullptr) continue;

        const string& fkCol = foreignKey->getColumnName();
        Value fkVal;
        if (r.has(fkCol))
        {
            fkVal = r.get(fkCol);
        }
        else if (const Column* col = table->getColumn(fkCol))
        {
            fkVal = resolveDefaultColumnValue(*col);
        }

        if (isNull(fkVal)) continue;

        const Table* referenced = getTable(foreignKey->getReferenceTable());
        if (referenced == nullptr
            || !containsColumnValue(*referenced, foreignKey->getReferenceColumn(), fkVal))
        {
            cout << "Foreign key constraint failed: " << constraint->getName() << endl;
            return false;
        }
    }
    return table->insert(r);
}

Vector<Record> Database::selectAll(const string& tableName) const
{
    const Table* table = getTable(tableName);
    Vector<Record> result;
    if (table == nullptr) return result;
    const Vector<Record>& rows = table->getRows();
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
        if (!table->isRowDeleted(rowIndex)) result.push_back(rows[rowIndex]);
    }
    return result;
}

static bool validateForeignKeyUpdates(
    const Database& database,
    const Table& table,
    const Record& update_infor
)
{
    for (size_t index = 0; index < table.getConstraints().size(); ++index)
    {
        const Constraint* constraint = table.getConstraints()[index];
        if (constraint->getType() != ConstraintType::FOREIGN_KEY) continue;
        const auto* foreignKey = dynamic_cast<const ForeignKeyConstraint*>(constraint);
        if (foreignKey == nullptr || !update_infor.has(foreignKey->getColumnName())) continue;

        const Value& newFkVal = update_infor.get(foreignKey->getColumnName());
        if (isNull(newFkVal))
        {
            continue; // NULL foreign-key values are permitted if column allows NULL (checked by Table::update).
        }

        const Table* referenced = database.getTable(foreignKey->getReferenceTable());
        if (referenced == nullptr
            || !containsColumnValue(*referenced, foreignKey->getReferenceColumn(), newFkVal))
        {
            cout << "Foreign key constraint failed: " << constraint->getName() << endl;
            return false;
        }
    }
    return true;
}

bool Database::update(const string& tableName, int record_id, const Record& update_infor)
{
    Table* table = getTable(tableName);
    if (table == nullptr || update_infor.size() == 0) return false;

    int rowIndex = table->findRowIndexById(record_id);
    if (rowIndex < 0) return false;

    const string pkCol = table->getPrimaryKeyColumn();
    const Record& currentRow = table->getRows()[static_cast<size_t>(rowIndex)];

    // Validate column existence and primary key immutability before checking foreign keys
    for (const auto& item : update_infor.getValues())
    {
        if (!table->hasColumn(item.first))
        {
            cout << "Update failed: column not found: " << item.first << endl;
            return false;
        }
        if (item.first == pkCol && !(item.second == Value(record_id)))
        {
            cout << "Update failed: cannot modify primary key column: " << pkCol << endl;
            return false;
        }
    }

    if (!validateForeignKeyUpdates(*this, *table, update_infor))
    {
        return false;
    }

    // Protect referential integrity if updating a parent column currently referenced by child tables
    for (const auto& item : update_infor.getValues())
    {
        if (!currentRow.has(item.first) || isNull(currentRow.get(item.first))) continue;
        const Value oldVal = currentRow.get(item.first);
        if (oldVal == item.second) continue;

        if (isColumnValueReferenced(*this, tableName, item.first, oldVal))
        {
            cout << "Update failed: column '" << item.first << "' is referenced by another table." << endl;
            return false;
        }
    }

    return table->update(record_id, update_infor);
}

bool Database::remove(const string& tableName, int record_id)
{
    Table* table = getTable(tableName);
    if (table == nullptr) return false;

    int rowIndex = table->findRowIndexById(record_id);
    if (rowIndex < 0) return false;

    const Record& targetRow = table->getRows()[static_cast<size_t>(rowIndex)];
    if (isRecordReferenced(*this, tableName, targetRow))
    {
        cout << "Remove failed: record is referenced by another table." << endl;
        return false;
    }

    return table->remove(record_id);
}

void Database::clear()
{
    for (const auto& item : tables) delete item.second;
    tables.clear();
}

bool Database::beginTransaction()
{
    if (inTransaction) return false;
    clearTransactionSnapshot();
    for (const auto& item : tables) transactionSnapshot[item.first] = new Table(*item.second);
    inTransaction = true;
    return true;
}

bool Database::commit()
{
    if (!inTransaction) return false;
    clearTransactionSnapshot();
    inTransaction = false;
    return true;
}

bool Database::rollback()
{
    if (!inTransaction) return false;
    clear();
    for (const auto& item : transactionSnapshot) tables[item.first] = item.second;
    transactionSnapshot.clear();
    inTransaction = false;
    return true;
}

bool Database::isInTransaction() const { return inTransaction; }
