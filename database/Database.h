#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include "Vector.h"
#include "HashMap.h"
#include "Table.h"
#include "Condition.h"
#include "ConditionGroup.h"
#include "User.h"

using namespace std;

/**
 * @brief Represents a relational database containing tables, user access controls, and transaction state.
 * Implements full manual memory management without smart pointers or STL containers.
 */
class Database
{
private:
    string databaseName;
    HashMap<string, Table*> tables;
    HashMap<string, User*> users;

    // Transaction management
    bool inTransaction;
    HashMap<string, Table*> transactionSnapshot;

    void clearTransactionSnapshot();

public:
    explicit Database(const string& name = "default_db");
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    ~Database();

    string getName() const;
    void setName(const string& name);

    // ========================================================
    // TABLE MANAGEMENT (CREATE, DROP, GET)
    // ========================================================
    Table* createTable(const string& tableName);
    bool dropTable(const string& tableName);
    bool addTable(Table* table);
    bool hasTable(const string& tableName) const;

    Table* getTable(const string& tableName);
    const Table* getTable(const string& tableName) const;

    int getTableCount() const;
    Vector<string> getTableNames() const;
    void printTables() const;

    // ========================================================
    // DATA MANIPULATION (INSERT, SELECT ALL, UPDATE, DELETE)
    // ========================================================
    bool insert(const string& tableName, const Record& record);
    Vector<Record> selectAll(const string& tableName) const;

    bool update(const string& tableName, const Condition& condition, const Record& newValues);
    bool update(const string& tableName, const ConditionGroup& group, const Record& newValues);

    bool remove(const string& tableName, const Condition& condition);
    bool remove(const string& tableName, const ConditionGroup& group);

    void clear();

    // ========================================================
    // TRANSACTIONS (BEGIN, COMMIT, ROLLBACK)
    // ========================================================
    bool beginTransaction();
    bool commit();
    bool rollback();
    bool isInTransaction() const;

    // ========================================================
    // USER & PERMISSION MANAGEMENT
    // ========================================================
    bool createUser(const string& username, const string& password);
    bool dropUser(const string& username);
    bool authenticate(const string& username, const string& password) const;
    bool grantPermission(const string& username, const string& tableName, Permission permission);
    bool revokePermission(const string& username, const string& tableName, Permission permission);
    bool hasPermission(const string& username, const string& tableName, Permission permission) const;
    User* getUser(const string& username);
};

#endif // DATABASE_H
