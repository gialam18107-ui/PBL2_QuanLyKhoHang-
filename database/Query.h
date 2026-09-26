#ifndef QUERY_H
#define QUERY_H

#include <string>
#include <initializer_list>

#include "Vector.h"
#include "Optional.h"
#include "Database.h"
#include "Join.h"
#include "JoinedRecord.h"
#include "Condition.h"
#include "ConditionGroup.h"
#include "OrderBy.h"
#include "Aggregate.h"

using namespace std;

class Query
{
private:
    Database* database;
    string baseTable;
    string tableAlias;
    Vector<string> selectedColumns;
    bool hasSubqueryFrom;
    Vector<JoinedRecord> subqueryFromRecords;
    Vector<Join> joins;
    Optional<Condition> singleCondition;
    Optional<ConditionGroup> conditionGroup;
    string subqueryInColumn;
    bool isSubqueryIn;
    bool isSubqueryNotIn;
    Vector<Value> subqueryInValues;
    Vector<OrderBy> orderBys;
    bool isDistinct;
    int limitCount;
    int offsetCount;
    Vector<string> groupByColumns;
    Vector<Aggregate> aggregates;
    Optional<Condition> havingCondition;
    Optional<ConditionGroup> havingConditionGroup;

    Vector<JoinedRecord> buildBaseRecords() const;
    Vector<JoinedRecord> applyJoins(Vector<JoinedRecord> records) const;
    Vector<JoinedRecord> applyWhere(const Vector<JoinedRecord>& records) const;
    Vector<JoinedRecord> applyGroupByAndAggregates(const Vector<JoinedRecord>& records) const;
    Vector<JoinedRecord> applyHaving(const Vector<JoinedRecord>& records) const;
    Vector<JoinedRecord> applySelect(const Vector<JoinedRecord>& records) const;
    Vector<JoinedRecord> applyDistinct(const Vector<JoinedRecord>& records) const;
    Vector<JoinedRecord> applyOrderBy(Vector<JoinedRecord> records) const;
    Vector<JoinedRecord> applyLimitAndOffset(const Vector<JoinedRecord>& records) const;
    bool evaluateCondition(const JoinedRecord& record, const Condition& condition) const;
    bool evaluateConditionGroup(const JoinedRecord& record, const ConditionGroup& group) const;
    bool compareValues(const Value& left, const Value& right, CompareOperator operation) const;

public:
    Query();

    /**
     * @brief Construct a Query builder bound to a specific Database.
     * @param database Database instance to run queries against.
     */
    explicit Query(Database& database);

    /**
     * @brief Set base table name for the query (FROM <table>).
     */
    Query& from(const string& tableName);

    /**
     * @brief Use another subquery as derived table source (FROM (<subquery>) AS <alias>).
     */
    Query& from(const Query& subquery, const string& alias);

    /**
     * @brief Specify columns to retrieve (e.g. {"product.product_name", "product.price"}).
     */
    Query& select(const Vector<string>& columns);

    /**
     * @brief Specify columns using initializer list syntax: .select({"col1", "col2"}).
     */
    Query& select(initializer_list<string> columns);

    /**
     * @brief Select all columns from base and joined tables (SELECT *).
     */
    Query& selectAll();

    /**
     * @brief Filter out duplicate rows from results (SELECT DISTINCT).
     */
    Query& distinct(bool enable = true);

    /**
     * @brief Add table join (INNER, LEFT, RIGHT, FULL JOIN).
     * @param joinObject Join specification object.
     */
    Query& join(const Join& joinObject);

    /**
     * @brief Add single filtering condition (WHERE column <op> value).
     */
    Query& where(const Condition& condition);

    /**
     * @brief Add compound filtering condition group (WHERE ... AND / OR ...).
     */
    Query& where(const ConditionGroup& group);

    /**
     * @brief Subquery membership filter (WHERE column IN (subquery)).
     */
    Query& whereIn(const string& column, const Query& subquery);

    /**
     * @brief Subquery non-membership filter (WHERE column NOT IN (subquery)).
     */
    Query& whereNotIn(const string& column, const Query& subquery);

    /**
     * @brief Group results by a single column (GROUP BY column).
     */
    Query& groupBy(const string& column);

    /**
     * @brief Group results by multiple columns.
     */
    Query& groupBy(const Vector<string>& columns);

    /**
     * @brief Add aggregate calculation (COUNT, SUM, AVG, MIN, MAX).
     * @param type Aggregate function type.
     * @param column Target column or "*" for COUNT(*).
     * @param alias Output column alias name in JoinedRecord.
     */
    Query& aggregate(AggregateType type, const string& column, const string& alias = "");

    /**
     * @brief Add Aggregate object specification.
     */
    Query& aggregate(const Aggregate& aggregate);

    /**
     * @brief Filter aggregated groups by a single condition (HAVING condition).
     */
    Query& having(const Condition& condition);

    /**
     * @brief Filter aggregated groups by a compound condition group.
     */
    Query& having(const ConditionGroup& group);

    /**
     * @brief Sort results using OrderBy object.
     */
    Query& orderBy(const OrderBy& orderBy);

    /**
     * @brief Sort results by column name and direction (ASC / DESC).
     */
    Query& orderBy(const string& columnName, SortOrder order = SortOrder::ASC);

    /**
     * @brief Limit maximum number of records returned.
     */
    Query& limit(int count);

    /**
     * @brief Skip initial number of records (used for pagination).
     */
    Query& offset(int count);

    /**
     * @brief Execute the constructed SQL query and return results.
     * @return Vector<JoinedRecord> Matching rows.
     */
    Vector<JoinedRecord> execute() const;

    /**
     * @brief Combine results with another query using UNION.
     * @param other Query to union with.
     * @param distinctOnly If true, remove duplicate rows (UNION), else keep duplicates (UNION ALL).
     */
    Vector<JoinedRecord> unionWith(const Query& other, bool distinctOnly = true) const;

    /**
     * @brief Return rows common to both query results (INTERSECT).
     */
    Vector<JoinedRecord> intersectWith(const Query& other) const;

    /**
     * @brief Return rows in this query but NOT in other query (EXCEPT).
     */
    Vector<JoinedRecord> exceptWith(const Query& other) const;

    /// Get reference to underlying Database.
    Database& getDatabase() const;

    /// Get base table name.
    const string& getBaseTable() const;

    /// Get list of selected column names.
    const Vector<string>& getSelectedColumns() const;

    /// Get list of table joins.
    const Vector<Join>& getJoins() const;
};

#endif
