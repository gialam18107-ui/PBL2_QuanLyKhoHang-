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
      inTransaction(other.inTransaction),
      transactionDepth(other.transactionDepth),
      undoLog(std::move(other.undoLog))
{
    other.inTransaction = false;
    other.transactionDepth = 0;
}

Database& Database::operator=(Database&& other) noexcept
{
    if (this != &other)
    {
        clear();
        undoLog.clear();

        databaseName = std::move(other.databaseName);
        tables = std::move(other.tables);
        inTransaction = other.inTransaction;
        transactionDepth = other.transactionDepth;
        undoLog = std::move(other.undoLog);

        other.inTransaction = false;
        other.transactionDepth = 0;
    }
    return *this;
}

Database::~Database()
{
    clear();
    undoLog.clear();
}

string Database::getName() const { return databaseName; }
void Database::setName(const string& name) { databaseName = name; }

Table* Database::createTable(const string& tableName)
{
    if (hasTable(tableName)) return getMutableTable(tableName);
    Table* table = new Table(tableName);
    tables[tableName] = table;
    return table;
}

bool Database::dropTable(const string& tableName)
{
    auto it = tables.find(tableName);
    if (it == tables.end()) return false;
    Table* dropped = it->second;
    for (size_t i = 0; i < undoLog.size(); ++i)
    {
        if (undoLog[i].table == dropped) undoLog[i].table = nullptr;
    }
    delete dropped;
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

Table* Database::getMutableTable(const string& tableName)
{
    auto it = tables.find(tableName);
    return it == tables.end() ? nullptr : it->second;
}

const Table* Database::getTable(const string& tableName) const
{
    auto it = tables.find(tableName);
    return it == tables.end() ? nullptr : it->second;
}

const Table* Database::getTable(const string& tableName)
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
    return table.contains(columnName, value);
}

bool Database::insert(const string& tableName, const Record& record)
{
    Table* table = getMutableTable(tableName);
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

    size_t newRowIndex = table->getRows().size();
    HashMap<string, int> oldAutoInc;
    if (inTransaction)
    {
        oldAutoInc = table->getAutoIncrementCounters();
    }

    if (!table->insert(record))
    {
        return false;
    }

    if (inTransaction)
    {
        UndoEntry entry;
        entry.type = UndoType::INSERT;
        entry.table = table;
        entry.rowIndex = newRowIndex;
        entry.oldAutoIncrement = std::move(oldAutoInc);
        undoLog.push_back(std::move(entry));
    }
    return true;
}

bool Database::validateForeignKeyUpdates(
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

bool Database::isReferenced(Database& database, const string& tableName, const Vector<Record>& removedRows)
{
    Vector<string> names = database.getTableNames();
    for (size_t tableIndex = 0; tableIndex < names.size(); ++tableIndex)
    {
        const Table* childTable = database.getTable(names[tableIndex]);
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

OperationResult Database::insertRecord(const string& tableName, const Record& record)
{
    Table* table = getMutableTable(tableName);
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
    Table* table = getMutableTable(tableName);
    if (table == nullptr)
    {
        return OperationResult(false, "Bảng '" + tableName + "' không tồn tại trong CSDL.", 0);
    }
    int rowIndex = table->findRowIndexById(recordId);
    if (rowIndex < 0)
    {
        return OperationResult(false, "Không tìm thấy bản ghi với ID " + to_string(recordId) + " trong bảng '" + tableName + "'.", 0);
    }
    const Record& current = table->getRows()[static_cast<size_t>(rowIndex)];
    if (!validateForeignKeyUpdates(*this, *table, current, newValues))
    {
        return OperationResult(false, "Vi phạm ràng buộc khóa ngoại khi cập nhật bảng '" + tableName + "'.", 0);
    }
    Record oldRecord;
    if (inTransaction)
    {
        oldRecord = current;
    }
    if (!table->updateById(recordId, newValues))
    {
        return OperationResult(false, "Cập nhật dữ liệu thất bại do vi phạm ràng buộc bảng.", 0);
    }
    if (inTransaction)
    {
        UndoEntry entry;
        entry.type = UndoType::UPDATE;
        entry.table = table;
        entry.rowIndex = static_cast<size_t>(rowIndex);
        entry.oldRecord = std::move(oldRecord);
        undoLog.push_back(std::move(entry));
    }
    return OperationResult(true, "Cập nhật bản ghi thành công.", 1);
}

OperationResult Database::remove(const string& tableName, int recordId)
{
    Table* table = getMutableTable(tableName);
    if (table == nullptr)
    {
        return OperationResult(false, "Bảng '" + tableName + "' không tồn tại trong cơ sở dữ liệu.", 0);
    }
    string pkCol = table->getPrimaryKeyColumn();
    if (pkCol.empty())
    {
        return OperationResult(false, "Không tìm thấy khóa chính của bảng '" + tableName + "'.", 0);
    }
    int rowIndex = table->findRowIndexById(recordId);
    if (rowIndex < 0)
    {
        return OperationResult(false, "Không tìm thấy bản ghi với ID " + to_string(recordId) + " trong bảng '" + tableName + "'.", 0);
    }
    const Record& record = table->getRows()[static_cast<size_t>(rowIndex)];
    Vector<Record> removedRows;
    removedRows.push_back(record);
    if (isReferenced(*this, tableName, removedRows))
    {
        return OperationResult(false, "Không thể xóa bản ghi: Đang có dữ liệu ở các bảng khác tham chiếu đến bản ghi này (vi phạm toàn vẹn tham chiếu).", 0);
    }
    if (!table->removeById(recordId, !inTransaction))
    {
        return OperationResult(false, "Lỗi khi xóa bản ghi khỏi bảng '" + tableName + "'.", 0);
    }
    if (inTransaction)
    {
        UndoEntry entry;
        entry.type = UndoType::REMOVE;
        entry.table = table;
        entry.rowIndex = static_cast<size_t>(rowIndex);
        undoLog.push_back(std::move(entry));
    }
    return OperationResult(true, "Xóa bản ghi thành công.", 1);
}

void Database::clear()
{
    for (const auto& item : tables) delete item.second;
    tables.clear();
    undoLog.clear();
}

bool Database::beginTransaction()
{
    if (inTransaction)
    {
        ++transactionDepth;
        return true;
    }
    undoLog.clear();
    undoLog.reserve(100);
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
    for (const auto& item : tables)
    {
        if (item.second != nullptr)
        {
            item.second->compactIfNeeded();
        }
    }
    undoLog.clear();
    inTransaction = false;
    transactionDepth = 0;
    return true;
}

bool Database::rollback()
{
    if (!inTransaction) return false;
    for (int i = static_cast<int>(undoLog.size()) - 1; i >= 0; --i)
    {
        const UndoEntry& entry = undoLog[static_cast<size_t>(i)];
        if (entry.table == nullptr) continue;
        switch (entry.type)
        {
        case UndoType::INSERT:
            entry.table->undoInsert(entry.rowIndex, entry.oldAutoIncrement);
            break;
        case UndoType::UPDATE:
            entry.table->undoUpdate(entry.rowIndex, entry.oldRecord);
            break;
        case UndoType::REMOVE:
            entry.table->undoRemove(entry.rowIndex);
            break;
        }
    }
    undoLog.clear();
    inTransaction = false;
    transactionDepth = 0;
    return true;
}

bool Database::isInTransaction() const { return inTransaction; }

int Database::getTransactionDepth() const { return transactionDepth; }
