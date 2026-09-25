#include "DatabaseManager.h"
#include <iostream>

DatabaseManager::DatabaseManager()
    : currentDatabase(nullptr)
{
}

DatabaseManager::~DatabaseManager()
{
    for (const auto& pair : databases)
    {
        delete pair.second;
    }
    databases.clear();
    currentDatabase = nullptr;
}

Database* DatabaseManager::createDatabase(const string& dbName)
{
    if (hasDatabase(dbName))
    {
        cout << "Database already exists: " << dbName << endl;
        return getDatabase(dbName);
    }

    Database* newDb = new Database(dbName);
    databases[dbName] = newDb;
    if (currentDatabase == nullptr)
    {
        currentDatabase = newDb;
    }
    return newDb;
}

bool DatabaseManager::dropDatabase(const string& dbName)
{
    auto it = databases.find(dbName);
    if (it == databases.end())
    {
        return false;
    }

    if (currentDatabase == it->second)
    {
        currentDatabase = nullptr;
    }

    delete it->second;
    databases.erase(dbName);
    return true;
}

bool DatabaseManager::hasDatabase(const string& dbName) const
{
    return databases.find(dbName) != databases.end();
}

Database* DatabaseManager::useDatabase(const string& dbName)
{
    Database* db = getDatabase(dbName);
    if (db != nullptr)
    {
        currentDatabase = db;
    }
    return currentDatabase;
}

Database* DatabaseManager::getCurrentDatabase() const
{
    return currentDatabase;
}

Database* DatabaseManager::getDatabase(const string& dbName) const
{
    auto it = databases.find(dbName);
    if (it == databases.end())
    {
        return nullptr;
    }
    return it->second;
}

Vector<string> DatabaseManager::getDatabaseNames() const
{
    return databases.getKeys();
}
