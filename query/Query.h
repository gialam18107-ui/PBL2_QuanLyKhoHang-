#ifndef QUERY_H
#define QUERY_H

#include "database/Record.h"
#include "database/Table.h"
#include "database/Condition.h"
#include "database/ConditionGroup.h"
#include "database/Index.h"
#include "datastructure/hashmap/Pair.h"
#include "datastructure/hashmap/HashMap.h"
#include "datastructure/vector/Vector.h"
#include <algorithm>
#include <cstddef>
#include <string>
#include <variant>

using namespace std;

// Forward declaration of Database
class Database;

namespace Query {

    // ========================================================
    // CACHE INVALIDATION & DIRTY FLAG
    // ========================================================
    bool isDirty();
    void setDirty(bool dirty = true);
    void clearDirty();

    // ========================================================
    // AGGREGATE FUNCTIONS (SUM, COUNT)
    // ========================================================
    Vector<Record> sum(Vector<Record> &data, string groupedColumn, string calculatedColumn);
    Vector<Record> sum(Vector<Record> &data, Pair<string, string> groupedColumns, string calculatedColumn);
    Vector<Record> sum(Vector<Record> &data, Pair<string, string> groupedColumns, Pair<string, string> calculatedColumn);

    Vector<Record> count(Vector<Record> &data, string groupedColumn, string calculatedColumn);
    Vector<Record> count(Vector<Record> &data, Pair<string, string> groupedColumns, string calculatedColumn);
    Vector<Record> count(Vector<Record> &data, Pair<string, string> groupedColumns, Pair<string, string> calculatedColumn);

    // ========================================================
    // SORT FUNCTIONS 
    // ========================================================
    void querySort(Vector<Record> &data, string groupedColumn, bool ascending);
    void querySort(Vector<Record> &data, Pair<string, string> groupedColumns, bool ascending);

    // ========================================================
    // INDEX HELPERS & FILTERING 
    // ========================================================
    const Index* findIndexForFilter(const Table &table, const string &selectedColumn, bool isRange = false);

    Vector<Record> filterRecords(const Table &table, string selectedColumn, const Value &value);
    Vector<Record> filterRecords(const Table &table, string selectedColumn, const Value &minVal, const Value &maxVal);
    Vector<Record> filterRecords(const Table &table, const Condition &condition);
    Vector<Record> filterRecords(const Table &table, const ConditionGroup &group);

    // ========================================================
    // DELETION FUNCTIONS (SAFE RECORD REMOVAL) 
    // ========================================================
    bool deleteRecords(Table &table, string selectedColumn, const Value &value, Database* db = nullptr);
    bool deleteRecords(Table &table, string selectedColumn, const Value &minVal, const Value &maxVal, Database* db = nullptr);
    bool deleteRecords(Table &table, const Condition &condition, Database* db = nullptr);
    bool deleteRecords(Table &table, const ConditionGroup &group, Database* db = nullptr);

    // ========================================================
    // QUERY CLASS (OBJECT-ORIENTED QUERY & DELETION INTERFACE)
    // ========================================================
    class Query {
    private:
        Table* table;
        Database* database;
        Vector<Condition> conditions;
        LogicalOperator logicalOp;

        bool isSafeToDelete(const Vector<Record>& targetRows) const;

    public:
        Query();
        explicit Query(Table& targetTable, Database* db = nullptr);
        explicit Query(Table* targetTable, Database* db = nullptr);

        Query& from(Table& targetTable);
        Query& from(Table* targetTable);
        Query& setDatabase(Database* db);

        // Where condition builders
        Query& where(const Condition& condition);
        Query& where(const ConditionGroup& group);
        Query& where(const string& column, CompareOperator op, const Value& val);
        Query& where(const string& column, const Value& val);
        Query& whereRange(const string& column, const Value& minVal, const Value& maxVal);

        // Execution leveraging indexes
        Vector<Record> execute() const;

        // Deletion method safely removing all matching records
        bool deleteRecords();
        bool remove() { return deleteRecords(); }

        // Getters & utility
        Table* getTable() const;
        Database* getDatabase() const;
        const Vector<Condition>& getConditions() const;
        void clearConditions();
    };

} // namespace Query

using QueryClass = Query::Query;

#endif // QUERY_H
