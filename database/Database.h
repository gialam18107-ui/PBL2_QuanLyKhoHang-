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
    /**
     * @brief Construct a new Database instance.
     * @param name Name of the database.
     */
    explicit Database(const string& name = "default_db");
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    ~Database();

    /// Get the database name.
    string getName() const;

    /// Set a new name for the database.
    void setName(const string& name);

    // ========================================================
    // TABLE MANAGEMENT (CREATE, DROP, GET)
    // ========================================================

    /**
     * @brief Create a new empty table with given name and register it in the database.
     * @param tableName Name of the table.
     * @return Table* Pointer to the created table, or existing table if already present.
     */
    Table* createTable(const string& tableName);

    /**
     * @brief Drop a table by name and release its memory.
     * @param tableName Name of the table to drop.
     * @return true if dropped, false if table was not found.
     */
    bool dropTable(const string& tableName);

    /**
     * @brief Add an existing table pointer to the database. Database takes ownership.
     * @param table Pointer to Table dynamically allocated with new.
     * @return true if added, false if table is null or name already exists.
     */
    bool addTable(Table* table);

    /**
     * @brief Check whether a table with given name exists in the database.
     * @param tableName Name of the table to check.
     */
    bool hasTable(const string& tableName) const;

    /**
     * @brief Find and return a pointer to the table by name.
     * @param tableName Name of table to retrieve.
     * @return Table* Pointer to table, or nullptr if not found.
     */
    Table* getTable(const string& tableName);

    /// Const version of getTable.
    const Table* getTable(const string& tableName) const;

    /// Get total number of tables registered in the database.
    int getTableCount() const;

    /// Get a vector containing names of all tables in the database.
    Vector<string> getTableNames() const;

    /// Print all table names to stdout.
    void printTables() const;

    // ========================================================
    // DATA MANIPULATION (INSERT, SELECT ALL, UPDATE, DELETE)
    // ========================================================

    /**
     * @brief Insert a record into a table after validating PK, FK, and CHECK constraints.
     * @param tableName Target table name.
     * @param record Record object containing column-value mappings.
     * @return true if successfully inserted, false if constraint violation.
     */
    bool insert(const string& tableName, const Record& record);

    /**
     * @brief Retrieve all records stored in a table.
     * @param tableName Name of the table.
     * @return Vector<Record> List of all rows in the table.
     */
    Vector<Record> selectAll(const string& tableName) const;

    /**
     * @brief Update rows in a table matching a single condition.
     * @param tableName Target table name.
     * @param condition Filter condition to match rows to update.
     * @param newValues Record containing new column values to set.
     * @return true if update succeeded, false if foreign key or table error.
     */
    bool update(const string& tableName, const Condition& condition, const Record& newValues);

    /**
     * @brief Update rows in a table matching a compound ConditionGroup.
     * @param tableName Target table name.
     * @param group Logical group of conditions (AND/OR).
     * @param newValues Record containing new column values to set.
     */
    bool update(const string& tableName, const ConditionGroup& group, const Record& newValues);

    /**
     * @brief Delete rows matching condition. Enforces referential integrity (Rule 2 & 4).
     * @param tableName Target table name.
     * @param condition Filter condition for rows to remove.
     * @return true if deleted, false if rows are referenced by other tables (FK protection).
     */
    bool remove(const string& tableName, const Condition& condition);

    /**
     * @brief Delete rows matching ConditionGroup with referential integrity protection.
     * @param tableName Target table name.
     * @param group Compound filter condition.
     */
    bool remove(const string& tableName, const ConditionGroup& group);

    /// Remove all tables and free all allocated table memory.
    void clear();

    // ========================================================
    // TRANSACTIONS (BEGIN, COMMIT, ROLLBACK)
    // ========================================================

    /**
     * @brief Begin a new transaction by taking deep copy snapshots of all tables.
     * @return true if transaction started, false if already in a transaction.
     */
    bool beginTransaction();

    /**
     * @brief Commit the active transaction, making all modifications permanent.
     * @return true if committed, false if no active transaction.
     */
    bool commit();

    /**
     * @brief Rollback the active transaction, reverting all tables to pre-transaction snapshot.
     * @return true if rolled back successfully.
     */
    bool rollback();

    /// Check if currently in an active transaction block.
    bool isInTransaction() const;

    // ========================================================
    // USER & PERMISSION MANAGEMENT
    // ========================================================

    /**
     * @brief Create a new user with username and password.
     * @param username Login username.
     * @param password Raw password (will be hashed internally).
     * @return true if created, false if username already exists.
     */
    bool createUser(const string& username, const string& password);

    /**
     * @brief Drop an existing user by username.
     * @param username Username to remove.
     */
    bool dropUser(const string& username);

    /**
     * @brief Authenticate user credentials.
     * @param username User's login name.
     * @param password Password to verify.
     * @return true if credentials match, false otherwise.
     */
    bool authenticate(const string& username, const string& password) const;

    /**
     * @brief Grant a permission to a user for a table (or "*" for global).
     * @param username Target username.
     * @param tableName Table name or "*" for all tables.
     * @param permission Permission enum (SELECT, INSERT, UPDATE, DELETE, etc.).
     */
    bool grantPermission(const string& username, const string& tableName, Permission permission);

    /**
     * @brief Revoke a previously granted permission from a user.
     */
    bool revokePermission(const string& username, const string& tableName, Permission permission);

    /**
     * @brief Check if user has specific permission on table.
     */
    bool hasPermission(const string& username, const string& tableName, Permission permission) const;

    /// Get user pointer by username.
    User* getUser(const string& username);
};

#endif // DATABASE_H
