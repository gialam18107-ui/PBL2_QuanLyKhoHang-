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
      users(std::move(other.users)),
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
        for (const auto& item : users) delete item.second;
        users.clear();

        databaseName = std::move(other.databaseName);
        tables = std::move(other.tables);
        users = std::move(other.users);
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
    for (const auto& item : users) delete item.second;
    users.clear();
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

bool Database::insert(const string& tableName, const Record& record)
{
    Table* table = getTable(tableName);
    if (table == nullptr) return false;

    for (size_t index = 0; index < table->getConstraints().size(); ++index)
    {
        Constraint* constraint = table->getConstraints()[index];
        if (constraint->getType() != ConstraintType::FOREIGN_KEY) continue;
        auto* foreignKey = dynamic_cast<ForeignKeyConstraint*>(constraint);
        if (foreignKey == nullptr || !record.has(foreignKey->getColumnName())
            || isNull(record.get(foreignKey->getColumnName()))) continue;
        const Table* referenced = getTable(foreignKey->getReferenceTable());
        if (referenced == nullptr
            || !foreignKey->validate(record, table->getRows(), referenced->getRows()))
        {
            cout << "Foreign key constraint failed: " << constraint->getName() << endl;
            return false;
        }
    }
    return table->insert(record);
}

Vector<Record> Database::selectAll(const string& tableName) const
{
    const Table* table = getTable(tableName);
    return table == nullptr ? Vector<Record>() : table->getRows();
}

static bool validateForeignKeyUpdates(
    Database& database,
    Table& table,
    const Record& current,
    const Record& newValues
)
{
    Record updated = current;
    for (const auto& item : newValues.getValues()) updated.set(item.first, item.second);

    for (size_t index = 0; index < table.getConstraints().size(); ++index)
    {
        Constraint* constraint = table.getConstraints()[index];
        if (constraint->getType() != ConstraintType::FOREIGN_KEY) continue;
        auto* foreignKey = dynamic_cast<ForeignKeyConstraint*>(constraint);
        if (foreignKey == nullptr || !newValues.has(foreignKey->getColumnName())) continue;
        const Table* referenced = database.getTable(foreignKey->getReferenceTable());
        if (referenced == nullptr
            || !foreignKey->validate(updated, table.getRows(), referenced->getRows())) return false;
    }
    return true;
}

bool Database::update(const string& tableName, const Condition& condition, const Record& newValues)
{
    Table* table = getTable(tableName);
    if (table == nullptr) return false;
    for (size_t index = 0; index < table->getRows().size(); ++index)
    {
        const Record& row = table->getRows()[index];
        if (condition.evaluate(row) && !validateForeignKeyUpdates(*this, *table, row, newValues)) return false;
    }
    return table->update(condition, newValues);
}

bool Database::update(const string& tableName, const ConditionGroup& group, const Record& newValues)
{
    Table* table = getTable(tableName);
    if (table == nullptr) return false;
    for (size_t index = 0; index < table->getRows().size(); ++index)
    {
        const Record& row = table->getRows()[index];
        if (group.evaluate(row) && !validateForeignKeyUpdates(*this, *table, row, newValues)) return false;
    }
    return table->update(group, newValues);
}

static bool isReferenced(Database& database, const string& tableName, const Vector<Record>& removedRows)
{
    Vector<string> names = database.getTableNames();
    for (size_t tableIndex = 0; tableIndex < names.size(); ++tableIndex)
    {
        Table* childTable = database.getTable(names[tableIndex]);
        if (childTable == nullptr || childTable->getName() == tableName) continue;
        for (size_t constraintIndex = 0; constraintIndex < childTable->getConstraints().size(); ++constraintIndex)
        {
            auto* foreignKey = dynamic_cast<ForeignKeyConstraint*>(childTable->getConstraints()[constraintIndex]);
            if (foreignKey == nullptr || foreignKey->getReferenceTable() != tableName) continue;
            for (size_t removedIndex = 0; removedIndex < removedRows.size(); ++removedIndex)
            {
                const Record& parent = removedRows[removedIndex];
                if (!parent.has(foreignKey->getReferenceColumn())) continue;
                Value parentValue = parent.get(foreignKey->getReferenceColumn());
                for (size_t rowIndex = 0; rowIndex < childTable->getRows().size(); ++rowIndex)
                {
                    const Record& child = childTable->getRows()[rowIndex];
                    if (child.has(foreignKey->getColumnName())
                        && child.get(foreignKey->getColumnName()) == parentValue) return true;
                }
            }
        }
    }
    return false;
}

bool Database::remove(const string& tableName, const Condition& condition)
{
    Table* table = getTable(tableName);
    if (table == nullptr) return false;
    Vector<Record> removed;
    for (size_t index = 0; index < table->getRows().size(); ++index)
        if (condition.evaluate(table->getRows()[index])) removed.push_back(table->getRows()[index]);
    return !isReferenced(*this, tableName, removed) && table->remove(condition);
}

bool Database::remove(const string& tableName, const ConditionGroup& group)
{
    Table* table = getTable(tableName);
    if (table == nullptr) return false;
    Vector<Record> removed;
    for (size_t index = 0; index < table->getRows().size(); ++index)
        if (group.evaluate(table->getRows()[index])) removed.push_back(table->getRows()[index]);
    return !isReferenced(*this, tableName, removed) && table->remove(group);
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

bool Database::createUser(const string& username, const string& password)
{
    if (users.find(username) != users.end()) return false;
    users[username] = new User(username, password);
    return true;
}

bool Database::dropUser(const string& username)
{
    auto it = users.find(username);
    if (it == users.end()) return false;
    delete it->second;
    users.erase(username);
    return true;
}

bool Database::authenticate(const string& username, const string& password) const
{
    auto it = users.find(username);
    return it != users.end() && it->second->verifyPassword(password);
}

bool Database::grantPermission(const string& username, const string& tableName, Permission permission)
{
    auto it = users.find(username);
    if (it == users.end()) return false;
    if (tableName.empty() || tableName == "*") it->second->grantGlobal(permission);
    else it->second->grant(tableName, permission);
    return true;
}

bool Database::revokePermission(const string& username, const string& tableName, Permission permission)
{
    auto it = users.find(username);
    if (it == users.end()) return false;
    if (tableName.empty() || tableName == "*") it->second->revokeGlobal(permission);
    else it->second->revoke(tableName, permission);
    return true;
}

bool Database::hasPermission(const string& username, const string& tableName, Permission permission) const
{
    auto it = users.find(username);
    return it != users.end() && it->second->hasPermission(tableName, permission);
}

User* Database::getUser(const string& username)
{
    auto it = users.find(username);
    return it == users.end() ? nullptr : it->second;
}
