#include "Database.h"

#include <iostream>

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
    for (const auto& pair : users)
    {
        delete pair.second;
    }
    users.clear();
}

string Database::getName() const
{
    return databaseName;
}

void Database::setName(const string& name)
{
    databaseName = name;
}

void Database::clearTransactionSnapshot()
{
    for (const auto& pair : transactionSnapshot)
    {
        delete pair.second;
    }
    transactionSnapshot.clear();
}

// ============================================================
// TABLE MANAGEMENT
// ============================================================

bool Database::addTable(
    const shared_ptr<Table>& table
)
Table* Database::createTable(const string& tableName)
{
    if (hasTable(tableName))
    {
        cout << "Table already exists: " << tableName << endl;
        return getTable(tableName);
    }

    Table* newTable = new Table(tableName);
    tables[tableName] = newTable;
    return newTable;
}

bool Database::dropTable(const string& tableName)
{
    auto it = tables.find(tableName);
    if (it == tables.end())
    {
        cout << "Drop table failed: table not found: " << tableName << endl;
        return false;
    }

    delete it->second;
    tables.erase(tableName);
    return true;
}

bool Database::addTable(Table* table)
{
    if (table == nullptr)
    {
        return false;
    }

    string tableName =
        table->getName();

    string tableName = table->getName();
    if (hasTable(tableName))
    {
        cout
            << "Table already exists: "
            << tableName
            << endl;

        cout << "Table already exists: " << tableName << endl;
        return false;
    }

    tables[tableName] = table;

    return true;
}


bool Database::hasTable(
    const string& tableName
) const
bool Database::hasTable(const string& tableName) const
{
    return tables.find(tableName)
        != tables.end();
    return tables.find(tableName) != tables.end();
}


shared_ptr<Table>
Database::getTable(
    const string& tableName
)
Table* Database::getTable(const string& tableName)
{
    auto it =
        tables.find(tableName);

    auto it = tables.find(tableName);
    if (it == tables.end())
    {
        return nullptr;
    }

    return it->second;
}


shared_ptr<const Table>
Database::getTable(
    const string& tableName
) const
const Table* Database::getTable(const string& tableName) const
{
    auto it =
        tables.find(tableName);

    auto it = tables.find(tableName);
    if (it == tables.end())
    {
        return nullptr;
    }

    return it->second;
}


int Database::getTableCount() const
{
    return static_cast<int>(
        tables.size()
    );
    return static_cast<int>(tables.size());
}

Vector<string> Database::getTableNames() const
{
    return tables.getKeys();
}

void Database::printTables() const
{
    cout
        << "DATABASE TABLES"
        << endl;
    cout << "DATABASE TABLES" << endl;
    cout << "==============================" << endl;

    cout
        << "=============================="
        << endl;

    if (tables.empty())
    {
        cout
            << "No tables."
            << endl;

        cout << "No tables." << endl;
        return;
    }

    for (
        const auto& pair
        :
        tables
    )
    for (const auto& pair : tables)
    {
        cout
            << "- "
            << pair.first
            << endl;
        cout << "- " << pair.first << endl;
    }
}


// ============================================================
// INSERT
// ============================================================

bool Database::insert(
    const string& tableName,
    const Record& record
)
bool Database::insert(const string& tableName, const Record& record)
{
    auto table =
        getTable(tableName);

    Table* table = getTable(tableName);
    if (table == nullptr)
    {
        cout
            << "Insert failed: table not found: "
            << tableName
            << endl;

        cout << "Insert failed: table not found: " << tableName << endl;
        return false;
    }


    // ========================================================
    // KIỂM TRA FOREIGN KEY
    // ========================================================

    for (
        const auto& constraint
        :
        table->getConstraints()
    )
    // Validate foreign keys
    for (size_t i = 0; i < table->getConstraints().size(); i++)
    {
        if (
            constraint->getType()
            !=
            ConstraintType::FOREIGN_KEY
        )
        Constraint* c = table->getConstraints()[i];
        if (c->getType() != ConstraintType::FOREIGN_KEY)
        {
            continue;
        }

        auto fk =
            dynamic_pointer_cast<
                ForeignKeyConstraint
            >(constraint);
        ForeignKeyConstraint* fk = dynamic_cast<ForeignKeyConstraint*>(c);
        if (!fk) continue;

        if (fk == nullptr)
        Table* refTable = getTable(fk->getReferenceTable());
        if (refTable == nullptr)
        {
            continue;
        }


        // Không có FK column
        // -> NOT NULL sẽ được Table xử lý
        if (
            !record.has(
                fk->getColumnName()
            )
        )
        {
            continue;
        }


        // ----------------------------------------------------
        // Lấy bảng được tham chiếu
        // ----------------------------------------------------

        auto referencedTable =
            getTable(
                fk->getReferenceTable()
            );

        if (referencedTable == nullptr)
        {
            cout
                << "Insert failed: "
                << "referenced table not found: "
                << fk->getReferenceTable()
                << endl;

            cout << "Foreign key error: referenced table not found: " << fk->getReferenceTable() << endl;
            return false;
        }


        // ----------------------------------------------------
        // Kiểm tra FOREIGN KEY
        // ----------------------------------------------------

        if (
            !fk->validate(
                record,
                table->getRows(),
                referencedTable->getRows()
            )
        )
        if (!fk->validate(record, table->getRows(), refTable->getRows()))
        {
            cout
                << "Foreign key constraint failed: "
                << fk->getName()
                << endl;

            cout << "Foreign key constraint failed: " << fk->getName() << endl;
            return false;
        }
    }


    // ========================================================
    // INSERT VÀO TABLE
    // ========================================================

    return table->insert(record);
}


// ============================================================
// SELECT ALL
// ============================================================

vector<Record>
Database::selectAll(
    const string& tableName
) const
Vector<Record> Database::selectAll(const string& tableName) const
{
    auto table =
        getTable(tableName);

    const Table* table = getTable(tableName);
    if (table == nullptr)
    {
        cout
            << "Select failed: table not found: "
            << tableName
            << endl;

        cout << "Select failed: table not found: " << tableName << endl;
        return {};
    }

    return table->getRows();
}


// ============================================================
// UPDATE
// ============================================================

bool Database::update(
    const string& tableName,
    const Condition& condition,
    const Record& newValues
)
bool Database::update(const string& tableName, const Condition& condition, const Record& newValues)
{
    auto table =
        getTable(tableName);

    Table* table = getTable(tableName);
    if (table == nullptr)
    {
        cout
            << "Update failed: table not found: "
            << tableName
            << endl;

        cout << "Update failed: table not found: " << tableName << endl;
        return false;
    }


    const vector<Record>& rows =
        table->getRows();


    // ========================================================
    // KIỂM TRA FOREIGN KEY
    // ========================================================

    for (
        const Record& currentRecord
        :
        rows
    )
    for (size_t i = 0; i < table->getConstraints().size(); i++)
    {
        if (
            !condition.evaluate(
                currentRecord
            )
        )
        {
            continue;
        }
        Constraint* c = table->getConstraints()[i];
        if (c->getType() != ConstraintType::FOREIGN_KEY) continue;
        ForeignKeyConstraint* fk = dynamic_cast<ForeignKeyConstraint*>(c);
        if (!fk) continue;


        // ----------------------------------------------------
        // Tạo Record sau UPDATE
        // ----------------------------------------------------

        Record updatedRecord =
            currentRecord;


        for (
            const auto& pair
            :
            newValues.getValues()
        )
        if (newValues.has(fk->getColumnName()))
        {
            updatedRecord.set(
                pair.first,
                pair.second
            );
        }
            Table* refTable = getTable(fk->getReferenceTable());
            if (refTable == nullptr) return false;


        // ----------------------------------------------------
        // Kiểm tra FK
        // ----------------------------------------------------

        for (
            const auto& constraint
            :
            table->getConstraints()
        )
        {
            if (
                constraint->getType()
                !=
                ConstraintType::FOREIGN_KEY
            )
            if (!fk->validate(newValues, table->getRows(), refTable->getRows()))
            {
                continue;
                cout << "Foreign key constraint failed: " << fk->getName() << endl;
                return false;
            }
        }
    }

            auto fk =
                dynamic_pointer_cast<
                    ForeignKeyConstraint
                >(constraint);
    return table->update(condition, newValues);
}

            if (fk == nullptr)
            {
                continue;
            }
bool Database::update(const string& tableName, const ConditionGroup& group, const Record& newValues)
{
    Table* table = getTable(tableName);
    if (table == nullptr)
    {
        cout << "Update failed: table not found: " << tableName << endl;
        return false;
    }

    for (size_t i = 0; i < table->getConstraints().size(); i++)
    {
        Constraint* c = table->getConstraints()[i];
        if (c->getType() != ConstraintType::FOREIGN_KEY) continue;
        ForeignKeyConstraint* fk = dynamic_cast<ForeignKeyConstraint*>(c);
        if (!fk) continue;

            if (
                !updatedRecord.has(
                    fk->getColumnName()
                )
            )
            {
                continue;
            }
        if (newValues.has(fk->getColumnName()))
        {
            Table* refTable = getTable(fk->getReferenceTable());
            if (refTable == nullptr) return false;


            auto referencedTable =
                getTable(
                    fk->getReferenceTable()
                );

            if (referencedTable == nullptr)
            if (!fk->validate(newValues, table->getRows(), refTable->getRows()))
            {
                cout
                    << "Update failed: "
                    << "referenced table not found: "
                    << fk->getReferenceTable()
                    << endl;

                cout << "Foreign key constraint failed: " << fk->getName() << endl;
                return false;
            }


            if (
                !fk->validate(
                    updatedRecord,
                    table->getRows(),
                    referencedTable->getRows()
                )
            )
            {
                cout
                    << "Foreign key constraint failed: "
                    << fk->getName()
                    << endl;

                return false;
            }
        }
    }


    // ========================================================
    // UPDATE TABLE
    // ========================================================

    return table->update(
        condition,
        newValues
    );
    return table->update(group, newValues);
}


// ============================================================
// DELETE
// DELETE (REMOVE)
// ============================================================

bool Database::remove(
    const string& tableName,
    const Condition& condition
)
bool Database::remove(const string& tableName, const Condition& condition)
{
    auto table =
        getTable(tableName);

    Table* table = getTable(tableName);
    if (table == nullptr)
    {
        cout
            << "Delete failed: table not found: "
            << tableName
            << endl;

        cout << "Remove failed: table not found: " << tableName << endl;
        return false;
    }

    for (const auto& pair : tables)
    {
        Table* otherTable = pair.second;
        if (otherTable == table) continue;

    // ========================================================
    // TÌM RECORD SẼ BỊ XÓA
    // ========================================================
        for (size_t i = 0; i < otherTable->getConstraints().size(); i++)
        {
            Constraint* c = otherTable->getConstraints()[i];
            if (c->getType() != ConstraintType::FOREIGN_KEY) continue;
            ForeignKeyConstraint* fk = dynamic_cast<ForeignKeyConstraint*>(c);
            if (!fk) continue;

    vector<Record> recordsToDelete;

    for (
        const Record& record
        :
        table->getRows()
    )
    {
        if (
            condition.evaluate(record)
        )
        {
            recordsToDelete.push_back(
                record
            );
            if (fk->getReferenceTable() == tableName)
            {
                for (size_t r = 0; r < table->getRows().size(); r++)
                {
                    const Record& parentRow = table->getRows()[r];
                    if (condition.evaluate(parentRow))
                    {
                        if (parentRow.has(fk->getReferenceColumn()))
                        {
                            Value parentVal = parentRow.get(fk->getReferenceColumn());
                            for (size_t cr = 0; cr < otherTable->getRows().size(); cr++)
                            {
                                const Record& childRow = otherTable->getRows()[cr];
                                if (childRow.has(fk->getColumnName()) && childRow.get(fk->getColumnName()) == parentVal)
                                {
                                    cout << "Delete failed: record is referenced by table " << otherTable->getName() << endl;
                                    return false;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return table->remove(condition);
}

    if (recordsToDelete.empty())
bool Database::remove(const string& tableName, const ConditionGroup& group)
{
    Table* table = getTable(tableName);
    if (table == nullptr)
    {
        cout
            << "Delete failed: "
            << "no matching record."
            << endl;

        cout << "Remove failed: table not found: " << tableName << endl;
        return false;
    }


    // ========================================================
    // KIỂM TRA FOREIGN KEY CỦA CÁC BẢNG KHÁC
    // ========================================================

    for (
        const auto& tablePair
        :
        tables
    )
    for (const auto& pair : tables)
    {
        auto otherTable =
            tablePair.second;
        Table* otherTable = pair.second;
        if (otherTable == table) continue;


        // Bỏ qua chính bảng đang DELETE
        if (
            otherTable->getName()
            ==
            tableName
        )
        for (size_t i = 0; i < otherTable->getConstraints().size(); i++)
        {
            continue;
        }
            Constraint* c = otherTable->getConstraints()[i];
            if (c->getType() != ConstraintType::FOREIGN_KEY) continue;
            ForeignKeyConstraint* fk = dynamic_cast<ForeignKeyConstraint*>(c);
            if (!fk) continue;


        for (
            const auto& constraint
            :
            otherTable->getConstraints()
        )
        {
            if (
                constraint->getType()
                !=
                ConstraintType::FOREIGN_KEY
            )
            if (fk->getReferenceTable() == tableName)
            {
                continue;
                for (size_t r = 0; r < table->getRows().size(); r++)
                {
                    const Record& parentRow = table->getRows()[r];
                    if (group.evaluate(parentRow))
                    {
                        if (parentRow.has(fk->getReferenceColumn()))
                        {
                            Value parentVal = parentRow.get(fk->getReferenceColumn());
                            for (size_t cr = 0; cr < otherTable->getRows().size(); cr++)
                            {
                                const Record& childRow = otherTable->getRows()[cr];
                                if (childRow.has(fk->getColumnName()) && childRow.get(fk->getColumnName()) == parentVal)
                                {
                                    cout << "Delete failed: record is referenced by table " << otherTable->getName() << endl;
                                    return false;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return table->remove(group);
}

            auto fk =
                dynamic_pointer_cast<
                    ForeignKeyConstraint
                >(constraint);
void Database::clear()
{
    for (const auto& pair : tables)
    {
        delete pair.second;
    }
    tables.clear();
}

            if (fk == nullptr)
            {
                continue;
            }
// ============================================================
// TRANSACTIONS
// ============================================================

bool Database::beginTransaction()
{
    if (inTransaction)
    {
        cout << "Transaction already in progress." << endl;
        return false;
    }

            // FK của bảng con không trỏ vào
            // bảng đang DELETE
            if (
                fk->getReferenceTable()
                !=
                tableName
            )
            {
                continue;
            }
    clearTransactionSnapshot();
    for (const auto& pair : tables)
    {
        transactionSnapshot[pair.first] = new Table(*pair.second);
    }
    inTransaction = true;
    return true;
}

bool Database::commit()
{
    if (!inTransaction)
    {
        cout << "No active transaction to commit." << endl;
        return false;
    }

            // ------------------------------------------------
            // Kiểm tra từng Record chuẩn bị xóa
            // ------------------------------------------------
    clearTransactionSnapshot();
    inTransaction = false;
    return true;
}

            for (
                const Record& deletedRecord
                :
                recordsToDelete
            )
            {
                if (
                    !deletedRecord.has(
                        fk->getReferenceColumn()
                    )
                )
                {
                    continue;
                }
bool Database::rollback()
{
    if (!inTransaction)
    {
        cout << "No active transaction to rollback." << endl;
        return false;
    }

    for (const auto& pair : tables)
    {
        delete pair.second;
    }
    tables.clear();

                Value deletedValue =
                    deletedRecord.get(
                        fk->getReferenceColumn()
                    );
    for (const auto& pair : transactionSnapshot)
    {
        tables[pair.first] = pair.second;
    }
    transactionSnapshot.clear();
    inTransaction = false;
    return true;
}

bool Database::isInTransaction() const
{
    return inTransaction;
}

                // --------------------------------------------
                // Tìm record con đang tham chiếu
                // --------------------------------------------
// ============================================================
// USER & PERMISSION MANAGEMENT
// ============================================================

                for (
                    const Record& childRecord
                    :
                    otherTable->getRows()
                )
                {
                    if (
                        !childRecord.has(
                            fk->getColumnName()
                        )
                    )
                    {
                        continue;
                    }
bool Database::createUser(const string& username, const string& password)
{
    if (users.find(username) != users.end())
    {
        cout << "User already exists: " << username << endl;
        return false;
    }

    users[username] = new User(username, password);
    return true;
}

                    Value childValue =
                        childRecord.get(
                            fk->getColumnName()
                        );
bool Database::dropUser(const string& username)
{
    auto it = users.find(username);
    if (it == users.end())
    {
        return false;
    }

    delete it->second;
    users.erase(username);
    return true;
}

                    if (
                        deletedValue
                        ==
                        childValue
                    )
                    {
                        cout
                            << "Delete failed: "
                            << "record is referenced by table "
                            << otherTable->getName()
                            << endl;
bool Database::authenticate(const string& username, const string& password) const
{
    auto it = users.find(username);
    if (it == users.end())
    {
        return false;
    }
    return it->second->verifyPassword(password);
}

                        return false;
                    }
                }
            }
        }
bool Database::grantPermission(const string& username, const string& tableName, Permission permission)
{
    auto it = users.find(username);
    if (it == users.end())
    {
        return false;
    }

    if (tableName.empty() || tableName == "*")
    {
        it->second->grantGlobal(permission);
    }
    else
    {
        it->second->grant(tableName, permission);
    }
    return true;
}

    // ========================================================
    // DELETE
    // ========================================================
bool Database::revokePermission(const string& username, const string& tableName, Permission permission)
{
    auto it = users.find(username);
    if (it == users.end())
    {
        return false;
    }

    return table->remove(
        condition
    );
    if (tableName.empty() || tableName == "*")
    {
        it->second->revokeGlobal(permission);
    }
    else
    {
        it->second->revoke(tableName, permission);
    }
    return true;
}

bool Database::hasPermission(const string& username, const string& tableName, Permission permission) const
{
    auto it = users.find(username);
    if (it == users.end())
    {
        return false;
    }
    return it->second->hasPermission(tableName, permission);
}

// ============================================================
// CLEAR
// ============================================================

void Database::clear()
User* Database::getUser(const string& username)
{
    tables.clear();
    auto it = users.find(username);
    if (it == users.end())
    {
        return nullptr;
    }
    return it->second;
}
