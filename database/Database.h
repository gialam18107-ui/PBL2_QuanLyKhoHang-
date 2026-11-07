#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include "datastructure/vector/Vector.h"
#include "datastructure/hashmap/HashMap.h"
#include "Table.h"
#include "OperationResult.h"

using namespace std;

namespace DataIO {
    class DatabaseIO;
}

namespace Generator {
    class DataGenerator;
}

/**
 * @brief Represents a relational database containing tables and transaction state.
 * Implements full manual memory management without smart pointers or STL containers.
 */
class Database
{
    friend class DataIO::DatabaseIO;
    friend class Generator::DataGenerator;

private:
    string databaseName;
    HashMap<string, Table*> tables;

    // Transaction management
    bool inTransaction;
    int transactionDepth = 0;
    enum class UndoType
    {
        INSERT,
        UPDATE,
        REMOVE
    };

    struct UndoEntry
    {
        UndoType type;
        Table* table = nullptr;
        size_t rowIndex = 0;
        Record oldRecord;
        HashMap<string, int> oldAutoIncrement;
    };

    Vector<UndoEntry> undoLog;

    // Internal mutable table accessor for Database operations and friends
    Table* getMutableTable(const string& tableName);

    static bool validateForeignKeyUpdates(
        Database& database,
        Table& table,
        const Record& current,
        const Record& newValues
    );

    static bool isReferenced(Database& database, const string& tableName, const Vector<Record>& removedRows);

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
     * @brief Public const accessor: callers can only read schema and view records, cannot mutate directly.
     * @param tableName Name of table to retrieve.
     * @return const Table* Pointer to table, or nullptr if not found.
     */
    const Table* getTable(const string& tableName) const;
    const Table* getTable(const string& tableName);

    /// Get total number of tables registered in the database.
    int getTableCount() const;

    /// Get a vector containing names of all tables in the database.
    Vector<string> getTableNames() const;

    /// Print all table names to stdout.
    void printTables() const;

    // ========================================================
    // DATA MANIPULATION (INSERT, UPDATE, DELETE)
    // ========================================================

    /**
     * @brief Insert a record into a table after validating PK, FK, and CHECK constraints.
     * @param tableName Target table name.
     * @param record Record object containing column-value mappings.
     * @return true if successfully inserted, false if constraint violation.
     */
    bool insert(const string& tableName, const Record& record);

    /**
     * @brief Insert a record and return OperationResult with detailed status.
     */
    OperationResult insertRecord(const string& tableName, const Record& record);

    /**
     * @brief Update a record directly by record ID in the primary key column (Rule 6.3).
     * @param tableName Target table name.
     * @param recordId ID of the record in primary key column.
     * @param newValues Column-value mappings to update.
     * @return OperationResult status.
     */
    OperationResult update(const string& tableName, int recordId, const Record& newValues);

    /**
     * @brief Delete a record directly by table name and ID (Rule 6.3).
     * Validates existence and enforces referential integrity (Rule 2 & 4) before deletion.
     * @param tableName Target table name.
     * @param recordId ID of the record to remove.
     * @return OperationResult status and affected row count.
     */
    OperationResult remove(const string& tableName, int recordId);

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

    /// Get current transaction nesting depth.
    int getTransactionDepth() const;
};

#endif // DATABASE_H
