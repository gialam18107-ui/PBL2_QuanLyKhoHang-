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
    explicit Query(Database& database);
    Query& from(const string& tableName);
    Query& from(const Query& subquery, const string& alias);
    Query& select(const Vector<string>& columns);
    Query& select(initializer_list<string> columns);
    Query& selectAll();
    Query& distinct(bool enable = true);
    Query& join(const Join& joinObject);
    Query& where(const Condition& condition);
    Query& where(const ConditionGroup& group);
    Query& whereIn(const string& column, const Query& subquery);
    Query& whereNotIn(const string& column, const Query& subquery);
    Query& groupBy(const string& column);
    Query& groupBy(const Vector<string>& columns);
    Query& aggregate(AggregateType type, const string& column, const string& alias = "");
    Query& aggregate(const Aggregate& aggregate);
    Query& having(const Condition& condition);
    Query& having(const ConditionGroup& group);
    Query& orderBy(const OrderBy& orderBy);
    Query& orderBy(const string& columnName, SortOrder order = SortOrder::ASC);
    Query& limit(int count);
    Query& offset(int count);
    Vector<JoinedRecord> execute() const;
    Vector<JoinedRecord> unionWith(const Query& other, bool distinctOnly = true) const;
    Vector<JoinedRecord> intersectWith(const Query& other) const;
    Vector<JoinedRecord> exceptWith(const Query& other) const;
    Database& getDatabase() const;
    const string& getBaseTable() const;
    const Vector<string>& getSelectedColumns() const;
    const Vector<Join>& getJoins() const;
};

#endif
