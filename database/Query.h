#ifndef QUERY_H
#define QUERY_H

#include <string>
#include <vector>
#include <optional>

#include "Database.h"
#include "Join.h"
#include "JoinedRecord.h"
#include "Condition.h"
#include "ConditionGroup.h"
#include "OrderBy.h"

using namespace std;

class Query
{
private:
    Database& database;

    string baseTable;

    vector<string> selectedColumns;

    vector<Join> joins;

    optional<Condition> singleCondition;

    optional<ConditionGroup> conditionGroup;

    optional<OrderBy> order;


private:

    // ========================================================
    // CONDITION
    // ========================================================

    bool evaluateCondition(
        const JoinedRecord& record,
        const Condition& condition
    ) const;

    bool evaluateConditionGroup(
        const JoinedRecord& record,
        const ConditionGroup& group
    ) const;

    bool compareValues(
        const Value& leftValue,
        const Value& rightValue,
        CompareOperator operation
    ) const;


    // ========================================================
    // QUERY PROCESSING
    // ========================================================

    vector<JoinedRecord>
    buildJoinedRecords() const;

    vector<JoinedRecord>
    applyWhere(
        const vector<JoinedRecord>& records
    ) const;

    vector<JoinedRecord>
    applyOrderBy(
        vector<JoinedRecord> records
    ) const;

    vector<JoinedRecord>
    applySelect(
        const vector<JoinedRecord>& records
    ) const;


public:

    explicit Query(
        Database& database
    );


    // ========================================================
    // FROM
    // ========================================================

    Query& from(
        const string& tableName
    );


    // ========================================================
    // SELECT
    // ========================================================

    Query& select(
        const vector<string>& columns
    );

    Query& selectAll();


    // ========================================================
    // JOIN
    // ========================================================

    Query& join(
        const Join& join
    );


    // ========================================================
    // WHERE
    // ========================================================

    Query& where(
        const Condition& condition
    );

    Query& where(
        const ConditionGroup& group
    );


    // ========================================================
    // ORDER BY
    // ========================================================

    Query& orderBy(
        const OrderBy& orderBy
    );


    // ========================================================
    // EXECUTE
    // ========================================================

    vector<JoinedRecord> execute() const;
};

#endif
