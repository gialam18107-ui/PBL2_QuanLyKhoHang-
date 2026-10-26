#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include "datastructure/vector/Vector.h"
#include "datastructure/hashmap/HashMap.h"
#include "Table.h"

using namespace std;

/**
 * @brief Represents a relational database containing tables and transaction state.
 * Implements full manual memory management without smart pointers or STL containers.
 */
class Database
{
private:
    string databaseName;
    HashMap<string, Table*> tables;

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
     * @brief Insert a record into a table after validating column names, data types, PK, FK, UNIQUE, NOT NULL, and CHECK constraints.
     * @param tableName Target table name.
     * @param r Record object containing column-value mappings.
     * @return true if successfully inserted, false if table not found or constraint violation.
     */
    bool insert(const string& tableName, const Record& r);

    /**
     * @brief Retrieve all records stored in a table.
     * @param tableName Name of the table.
     * @return Vector<Record> List of all rows in the table.
     */
    Vector<Record> selectAll(const string& tableName) const;

    /**
     * @brief Update a record in a table identified by its primary key ID.
     * Validates column names, data types, primary key immutability, constraints, and referential integrity.
     * @param tableName Target table name.
     * @param record_id Primary key ID of the record to update.
     * @param update_infor Record containing new column values to set.
     * @return true if update succeeded, false if table/record not found or validation failed.
     */
    bool update(const string& tableName, int record_id, const Record& update_infor);

    /**
     * @brief Delete a record identified by its primary key ID. Enforces referential integrity across child tables.
     * @param tableName Target table name.
     * @param record_id Primary key ID of the record to remove.
     * @return true if deleted, false if table/record not found or record is referenced by other tables.
     */
    bool remove(const string& tableName, int record_id);

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
};

#endif // DATABASE_H
