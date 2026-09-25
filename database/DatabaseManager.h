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

    Database* createDatabase(const string& dbName);
    bool dropDatabase(const string& dbName);
    bool hasDatabase(const string& dbName) const;

    Database* useDatabase(const string& dbName);
    Database* getCurrentDatabase() const;
    Database* getDatabase(const string& dbName) const;

    Vector<string> getDatabaseNames() const;
};

#endif // DATABASE_MANAGER_H
