#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include "Vector.h"
#include "HashMap.h"
#include "Database.h"

using namespace std;

/**
 * @brief Manages multiple databases, supporting CREATE DATABASE, DROP DATABASE, and USE DATABASE.
 */
class DatabaseManager
{
private:
    HashMap<string, Database*> databases;
    Database* currentDatabase;

public:
    DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    ~DatabaseManager();

    /**
     * @brief Create a new database by name and manage its lifecycle.
     * @param dbName Unique name of the database.
     * @return Database* Pointer to created Database, or existing one if already created.
     */
    Database* createDatabase(const string& dbName);

    /**
     * @brief Drop and delete a database by name.
     * @param dbName Name of database to delete.
     * @return true if dropped, false if not found.
     */
    bool dropDatabase(const string& dbName);

    /**
     * @brief Check whether a database exists in the manager.
     */
    bool hasDatabase(const string& dbName) const;

    /**
     * @brief Switch active working database (equivalent to SQL USE <dbName>).
     * @param dbName Name of the database to switch to.
     * @return Database* Pointer to the active database.
     */
    Database* useDatabase(const string& dbName);

    /// Get pointer to currently active working database.
    Database* getCurrentDatabase() const;

    /// Get pointer to a database by name without switching active database.
    Database* getDatabase(const string& dbName) const;

    /// Get list of all managed database names.
    Vector<string> getDatabaseNames() const;
};

#endif // DATABASE_MANAGER_H
