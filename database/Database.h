#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "Table.h"
#include "Condition.h"

using namespace std;

class Database
{
private:
    unordered_map<
        string,
        shared_ptr<Table>
    > tables;

public:

    // ========================================================
    // TABLE MANAGEMENT
    // ========================================================

    bool addTable(
        const shared_ptr<Table>& table
    );

    bool hasTable(
        const string& tableName
    ) const;

    shared_ptr<Table> getTable(
        const string& tableName
    );

    shared_ptr<const Table> getTable(
        const string& tableName
    ) const;

    int getTableCount() const;

    void printTables() const;


    // ========================================================
    // CREATE
    // ========================================================

    bool insert(
        const string& tableName,
        const Record& record
    );


    // ========================================================
    // READ
    // ========================================================

    vector<Record> selectAll(
        const string& tableName
    ) const;


    // ========================================================
    // UPDATE
    // ========================================================

    bool update(
        const string& tableName,
        const Condition& condition,
        const Record& newValues
    );


    // ========================================================
    // DELETE
    // ========================================================

    bool remove(
        const string& tableName,
        const Condition& condition
    );


    // ========================================================
    // CLEAR
    // ========================================================

    void clear();
};

#endif
