#ifndef QUERY_H
#define QUERY_H

#include <string>
#include <vector>
#include <optional>
#include "Vector.h"
#include "HashMap.h"
#include "Optional.h"

#include "Database.h"
#include "Join.h"
#include "JoinedRecord.h"
#include "Condition.h"
#include "ConditionGroup.h"
#include "OrderBy.h"
#include "Aggregate.h"

using namespace std;

/**
 * @brief Comprehensive Query builder supporting full SQL features:
 * SELECT, FROM, JOIN (INNER, LEFT, RIGHT, FULL), WHERE, ORDER BY,
 * DISTINCT, LIMIT, OFFSET, GROUP BY, HAVING, Aggregates, Subqueries, and Set Operations.
 */
class Query
{
private:
    Database& database;
    Database* database;

    string baseTable;
    string tableAlias;

    vector<string> selectedColumns;
    // Subquery as source of FROM
    bool hasSubqueryFrom;
    Vector<JoinedRecord> subqueryFromRecords;

    vector<Join> joins;
    Vector<string> selectedColumns;
    Vector<Join> joins;

    optional<Condition> singleCondition;
    Optional<Condition> singleCondition;
    Optional<ConditionGroup> conditionGroup;

    optional<ConditionGroup> conditionGroup;
    // Subquery WHERE IN / NOT IN
    string subqueryInColumn;
    bool isSubqueryIn;
    bool isSubqueryNotIn;
    Vector<Value> subqueryInValues;

    optional<OrderBy> order;
    Vector<OrderBy> orderBys;

    bool isDistinct;
    int limitCount;
    int offsetCount;

private:
    Vector<string> groupByColumns;
    Vector<Aggregate> aggregates;
    Optional<Condition> havingCondition;
    Optional<ConditionGroup> havingConditionGroup;

    // ========================================================
    // CONDITION
    // QUERY EXECUTION PIPELINE STEPS
    // ========================================================
    Vector<JoinedRecord> buildBaseRecords() const;
    Vector<JoinedRecord> applyJoins(Vector<JoinedRecord> records) const;
    Vector<JoinedRecord> applyWhere(const Vector<JoinedRecord>& records) const;
    Vector<JoinedRecord> applyGroupByAndAggregates(const Vector<JoinedRecord>& records) const;
    Vector<JoinedRecord> applyHaving(const Vector<JoinedRecord>& records) const;
    Vector<JoinedRecord> applySelect(const Vector<JoinedRecord>& records) const;
    Vector<JoinedRecord> applyDistinct(const Vector<JoinedRecord>& records) const;
    Vector<JoinedRecord> applyOrderBy(Vector<JoinedRecord> records) const;
    Vector<JoinedRecord> applyLimitAndOffset(const Vector<JoinedRecord>& records) const;

    bool evaluateCondition(
        const JoinedRecord& record,
        const Condition& condition
    ) const;
    bool evaluateCondition(const JoinedRecord& record, const Condition& condition) const;
    bool evaluateConditionGroup(const JoinedRecord& record, const ConditionGroup& group) const;

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
    Query();
    explicit Query(Database& database);

    explicit Query(
        Database& database
    );


    // ========================================================
    // FROM
    // FROM & SUBQUERIES
    // ========================================================
    Query& from(const string& tableName);
    Query& from(const Query& subquery, const string& alias);

    Query& from(
        const string& tableName
    );


    // ========================================================
    // SELECT
    // SELECT & DISTINCT
    // ========================================================

    Query& select(
        const vector<string>& columns
    );

    Query& select(const Vector<string>& columns);
    Query& select(std::initializer_list<string> columns);
    Query& selectAll();
    Query& distinct(bool enable = true);


    // ========================================================
    // JOIN
    // JOIN (INNER, LEFT, RIGHT, FULL)
    // ========================================================
    Query& join(const Join& joinObject);

    Query& join(
        const Join& join
    );


    // ========================================================
    // WHERE
    // ========================================================
    Query& where(const Condition& condition);
    Query& where(const ConditionGroup& group);
    Query& whereIn(const string& column, const Query& subquery);
    Query& whereNotIn(const string& column, const Query& subquery);

    Query& where(
        const Condition& condition
    );
    // ========================================================
    // GROUP BY, AGGREGATES, HAVING
    // ========================================================
    Query& groupBy(const string& column);
    Query& groupBy(const Vector<string>& columns);
    Query& aggregate(AggregateType type, const string& column, const string& alias = "");
    Query& aggregate(const Aggregate& agg);
    Query& having(const Condition& condition);
    Query& having(const ConditionGroup& group);

    Query& where(
        const ConditionGroup& group
    );


    // ========================================================
    // ORDER BY
    // ORDER BY, LIMIT, OFFSET
    // ========================================================
    Query& orderBy(const OrderBy& orderByObject);
    Query& orderBy(const string& columnName, SortOrder order = SortOrder::ASC);
    Query& limit(int count);
    Query& offset(int count);

    Query& orderBy(
        const OrderBy& orderBy
    );


    // ========================================================
    // EXECUTE
    // ========================================================
    Vector<JoinedRecord> execute() const;

    vector<JoinedRecord> execute() const;
    // ========================================================
    // SET OPERATIONS (UNION, INTERSECT, EXCEPT)
    // ========================================================
    Vector<JoinedRecord> unionWith(const Query& other, bool distinctOnly = true) const;
    Vector<JoinedRecord> intersectWith(const Query& other) const;
    Vector<JoinedRecord> exceptWith(const Query& other) const;


    // ========================================================
    // GETTERS
    // ========================================================

    Database& getDatabase() const;

    Database* getDatabase() const;
    const string& getBaseTable() const;
    const Vector<string>& getSelectedColumns() const;
    const Vector<Join>& getJoins() const;
};

    const vector<string>& getSelectedColumns() const;
#endif // QUERY_H

    const vector<Join>& getJoins() const;
};

#endif
