#include "Database.h"

#include <iostream>
#include <utility>

using namespace std;

Database::Database(const string& name)
    : databaseName(name), inTransaction(false), transactionDepth(0)
{
}

Database::Database(Database&& other) noexcept
    : databaseName(std::move(other.databaseName)),
      tables(std::move(other.tables)),
      users(std::move(other.users)),
      inTransaction(other.inTransaction),
      transactionDepth(other.transactionDepth),
      transactionSnapshot(std::move(other.transactionSnapshot))
{
    other.inTransaction = false;
    other.transactionDepth = 0;
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
        transactionDepth = other.transactionDepth;
        transactionSnapshot = std::move(other.transactionSnapshot);

        other.inTransaction = false;
        other.transactionDepth = 0;
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

    // Fallback scan for unindexed columns
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
            || !containsColumnValue(*referenced, foreignKey->getReferenceColumn(), record.get(foreignKey->getColumnName())))
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
        if (referenced == nullptr) return false;
        if (!updated.has(foreignKey->getColumnName()) || isNull(updated.get(foreignKey->getColumnName())))
        {
            continue;
        }
        if (!containsColumnValue(*referenced, foreignKey->getReferenceColumn(), updated.get(foreignKey->getColumnName())))
        {
            return false;
        }
    }
    return true;
}

bool Database::update(const string& tableName, const Condition& condition, const Record& newValues)
{
    Table* table = getTable(tableName);
    if (table == nullptr) return false;
    for (size_t index = 0; index < table->getRows().size(); ++index)
    {
        if (table->isRowDeleted(index)) continue;
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
        if (table->isRowDeleted(index)) continue;
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
                if (containsColumnValue(*childTable, foreignKey->getColumnName(), parentValue)) return true;
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
        if (!table->isRowDeleted(index) && condition.evaluate(table->getRows()[index]))
            removed.push_back(table->getRows()[index]);
    return !isReferenced(*this, tableName, removed) && table->remove(condition);
}

bool Database::remove(const string& tableName, const ConditionGroup& group)
{
    Table* table = getTable(tableName);
    if (table == nullptr) return false;
    Vector<Record> removed;
    for (size_t index = 0; index < table->getRows().size(); ++index)
        if (!table->isRowDeleted(index) && group.evaluate(table->getRows()[index]))
            removed.push_back(table->getRows()[index]);
    return !isReferenced(*this, tableName, removed) && table->remove(group);
}

OperationResult Database::insertRecord(const string& tableName, const Record& record)
{
    Table* table = getTable(tableName);
    if (table == nullptr)
    {
        return OperationResult(false, "Bảng '" + tableName + "' không tồn tại trong CSDL.", 0);
    }
    if (!insert(tableName, record))
    {
        return OperationResult(false, "Vi phạm ràng buộc dữ liệu hoặc khóa ngoại khi thêm vào bảng '" + tableName + "'.", 0);
    }
    return OperationResult(true, "Thêm bản ghi thành công.", 1);
}

OperationResult Database::update(const string& tableName, int recordId, const Record& newValues)
{
    Table* table = getTable(tableName);
    if (table == nullptr)
    {
        return OperationResult(false, "Bảng '" + tableName + "' không tồn tại trong CSDL.", 0);
    }
    const Record* current = table->getRecordById(recordId);
    if (current == nullptr)
    {
        return OperationResult(false, "Không tìm thấy bản ghi với ID " + to_string(recordId) + " trong bảng '" + tableName + "'.", 0);
    }
    if (!validateForeignKeyUpdates(*this, *table, *current, newValues))
    {
        return OperationResult(false, "Vi phạm ràng buộc khóa ngoại khi cập nhật bảng '" + tableName + "'.", 0);
    }
    if (!table->updateById(recordId, newValues))
    {
        return OperationResult(false, "Cập nhật dữ liệu thất bại do vi phạm ràng buộc bảng.", 0);
    }
    return OperationResult(true, "Cập nhật bản ghi thành công.", 1);
}

OperationResult Database::remove(const string& tableName, int recordId)
{
    Table* table = getTable(tableName);
    if (table == nullptr)
    {
        return OperationResult(false, "Bảng '" + tableName + "' không tồn tại trong cơ sở dữ liệu.", 0);
    }
    string pkCol = table->getPrimaryKeyColumn();
    if (pkCol.empty())
    {
        return OperationResult(false, "Không tìm thấy khóa chính của bảng '" + tableName + "'.", 0);
    }
    const Record* record = table->getRecordById(recordId);
    if (record == nullptr)
    {
        return OperationResult(false, "Không tìm thấy bản ghi với ID " + to_string(recordId) + " trong bảng '" + tableName + "'.", 0);
    }
    Vector<Record> removedRows;
    removedRows.push_back(*record);
    if (isReferenced(*this, tableName, removedRows))
    {
        return OperationResult(false, "Không thể xóa bản ghi: Đang có dữ liệu ở các bảng khác tham chiếu đến bản ghi này (vi phạm toàn vẹn tham chiếu).", 0);
    }
    if (!table->removeById(recordId))
    {
        return OperationResult(false, "Lỗi khi xóa bản ghi khỏi bảng '" + tableName + "'.", 0);
    }
    return OperationResult(true, "Xóa bản ghi thành công.", 1);
}

void Database::clear()
{
    for (const auto& item : tables) delete item.second;
    tables.clear();
}

bool Database::beginTransaction()
{
    if (inTransaction)
    {
        ++transactionDepth;
        return true;
    }
    clearTransactionSnapshot();
    for (const auto& item : tables) transactionSnapshot[item.first] = new Table(*item.second);
    inTransaction = true;
    transactionDepth = 1;
    return true;
}

bool Database::commit()
{
    if (!inTransaction) return false;
    if (transactionDepth > 1)
    {
        --transactionDepth;
        return true;
    }
    clearTransactionSnapshot();
    inTransaction = false;
    transactionDepth = 0;
    return true;
}

bool Database::rollback()
{
    if (!inTransaction) return false;
    // Restore state while preserving Table pointers
    for (const auto& item : transactionSnapshot)
    {
        Table* current = getTable(item.first);
        if (current != nullptr)
        {
            *current = *item.second;
        }
        else
        {
            tables[item.first] = new Table(*item.second);
        }
    }
    clearTransactionSnapshot();
    inTransaction = false;
    transactionDepth = 0;
    return true;
}

bool Database::isInTransaction() const { return inTransaction; }

int Database::getTransactionDepth() const { return transactionDepth; }

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
