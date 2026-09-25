#include "Query.h"

#include <iostream>
#include <stdexcept>

using namespace std;

static bool resolveColumn(const JoinedRecord& record, const string& column, Value& value)
{
    if (record.has(column))
    {
        value = record.get(column);
        return true;
    }
    for (const auto& item : record.getValues())
    {
        size_t dot = item.first.find('.');
        if (dot != string::npos && item.first.substr(dot + 1) == column)
        {
            value = item.second;
            return true;
        }
    }
    return false;
}

static string unqualified(const string& column)
{
    size_t dot = column.find_last_of('.');
    return dot == string::npos ? column : column.substr(dot + 1);
}

Query::Query()
    : database(nullptr), hasSubqueryFrom(false), isSubqueryIn(false),
      isSubqueryNotIn(false), isDistinct(false), limitCount(-1), offsetCount(0)
{
}

Query::Query(Database& database)
    : database(&database), hasSubqueryFrom(false), isSubqueryIn(false),
      isSubqueryNotIn(false), isDistinct(false), limitCount(-1), offsetCount(0)
{
}

Query& Query::from(const string& tableName)
{
    baseTable = tableName;
    tableAlias.clear();
    hasSubqueryFrom = false;
    return *this;
}

Query& Query::from(const Query& subquery, const string& alias)
{
    tableAlias = alias;
    baseTable.clear();
    hasSubqueryFrom = true;
    subqueryFromRecords = subquery.execute();
    for (size_t index = 0; index < subqueryFromRecords.size(); ++index)
    {
        JoinedRecord renamed;
        for (const auto& item : subqueryFromRecords[index].getValues())
            renamed.set(alias + "." + unqualified(item.first), item.second);
        subqueryFromRecords[index] = std::move(renamed);
    }
    return *this;
}

Query& Query::select(const Vector<string>& columns)
{
    selectedColumns = columns;
    return *this;
}

Query& Query::select(initializer_list<string> columns)
{
    selectedColumns.clear();
    for (const string& column : columns) selectedColumns.push_back(column);
    return *this;
}

Query& Query::selectAll()
{
    selectedColumns.clear();
    return *this;
}

Query& Query::distinct(bool enable)
{
    isDistinct = enable;
    return *this;
}

Query& Query::join(const Join& joinObject)
{
    joins.push_back(joinObject);
    return *this;
}

Query& Query::where(const Condition& condition)
{
    singleCondition = condition;
    conditionGroup.reset();
    isSubqueryIn = false;
    isSubqueryNotIn = false;
    return *this;
}

Query& Query::where(const ConditionGroup& group)
{
    conditionGroup = group;
    singleCondition.reset();
    isSubqueryIn = false;
    isSubqueryNotIn = false;
    return *this;
}

Query& Query::whereIn(const string& column, const Query& subquery)
{
    subqueryInColumn = column;
    isSubqueryIn = true;
    isSubqueryNotIn = false;
    subqueryInValues.clear();
    Vector<JoinedRecord> result = subquery.execute();
    for (size_t index = 0; index < result.size(); ++index)
    {
        for (const auto& item : result[index].getValues())
        {
            subqueryInValues.push_back(item.second);
            break;
        }
    }
    singleCondition.reset();
    conditionGroup.reset();
    return *this;
}

Query& Query::whereNotIn(const string& column, const Query& subquery)
{
    whereIn(column, subquery);
    isSubqueryIn = false;
    isSubqueryNotIn = true;
    return *this;
}

Query& Query::groupBy(const string& column)
{
    groupByColumns.push_back(column);
    return *this;
}

Query& Query::groupBy(const Vector<string>& columns)
{
    for (size_t index = 0; index < columns.size(); ++index) groupByColumns.push_back(columns[index]);
    return *this;
}

Query& Query::aggregate(AggregateType type, const string& column, const string& alias)
{
    aggregates.push_back(Aggregate(type, column, alias));
    return *this;
}

Query& Query::aggregate(const Aggregate& aggregate)
{
    aggregates.push_back(aggregate);
    return *this;
}

Query& Query::having(const Condition& condition)
{
    havingCondition = condition;
    havingConditionGroup.reset();
    return *this;
}

Query& Query::having(const ConditionGroup& group)
{
    havingConditionGroup = group;
    havingCondition.reset();
    return *this;
}

Query& Query::orderBy(const OrderBy& orderBy)
{
    orderBys.push_back(orderBy);
    return *this;
}

Query& Query::orderBy(const string& columnName, SortOrder order)
{
    orderBys.push_back(OrderBy(columnName, order));
    return *this;
}

Query& Query::limit(int count)
{
    limitCount = count < 0 ? 0 : count;
    return *this;
}

Query& Query::offset(int count)
{
    offsetCount = count < 0 ? 0 : count;
    return *this;
}

bool Query::compareValues(const Value& left, const Value& right, CompareOperator operation) const
{
    if (isNull(left) || isNull(right))
    {
        if (operation == CompareOperator::EQUAL) return isNull(left) && isNull(right);
        if (operation == CompareOperator::NOT_EQUAL) return isNull(left) != isNull(right);
        return false;
    }
    bool leftNumeric = holds_alternative<int>(left) || holds_alternative<double>(left);
    bool rightNumeric = holds_alternative<int>(right) || holds_alternative<double>(right);
    if (leftNumeric && rightNumeric)
    {
        double l = holds_alternative<int>(left) ? get<int>(left) : get<double>(left);
        double r = holds_alternative<int>(right) ? get<int>(right) : get<double>(right);
        switch (operation)
        {
        case CompareOperator::EQUAL: return l == r;
        case CompareOperator::NOT_EQUAL: return l != r;
        case CompareOperator::LESS: return l < r;
        case CompareOperator::LESS_EQUAL: return l <= r;
        case CompareOperator::GREATER: return l > r;
        case CompareOperator::GREATER_EQUAL: return l >= r;
        default: return false;
        }
    }
    if (left.index() != right.index()) return false;
    if (holds_alternative<string>(left))
    {
        const string& l = get<string>(left);
        const string& r = get<string>(right);
        switch (operation)
        {
        case CompareOperator::EQUAL: return l == r;
        case CompareOperator::NOT_EQUAL: return l != r;
        case CompareOperator::LESS: return l < r;
        case CompareOperator::LESS_EQUAL: return l <= r;
        case CompareOperator::GREATER: return l > r;
        case CompareOperator::GREATER_EQUAL: return l >= r;
        default: return false;
        }
    }
    if (holds_alternative<bool>(left))
    {
        bool l = get<bool>(left), r = get<bool>(right);
        if (operation == CompareOperator::EQUAL) return l == r;
        if (operation == CompareOperator::NOT_EQUAL) return l != r;
    }
    return false;
}

bool Query::evaluateCondition(const JoinedRecord& record, const Condition& condition) const
{
    Value value = monostate{};
    bool found = resolveColumn(record, condition.getColumnName(), value);
    if (!found && condition.getOperation() != CompareOperator::IS_NULL) return false;
    return condition.evaluateValue(value);
}

bool Query::evaluateConditionGroup(const JoinedRecord& record, const ConditionGroup& group) const
{
    const Vector<Condition>& conditions = group.getConditions();
    if (conditions.empty()) return false;
    if (group.getOperation() == LogicalOperator::NOT) return !evaluateCondition(record, conditions[0]);
    if (group.getOperation() == LogicalOperator::AND)
    {
        for (size_t index = 0; index < conditions.size(); ++index)
            if (!evaluateCondition(record, conditions[index])) return false;
        return true;
    }
    for (size_t index = 0; index < conditions.size(); ++index)
        if (evaluateCondition(record, conditions[index])) return true;
    return false;
}

Vector<JoinedRecord> Query::buildBaseRecords() const
{
    if (hasSubqueryFrom) return subqueryFromRecords;
    if (database == nullptr || baseTable.empty()) return {};
    const Table* table = database->getTable(baseTable);
    if (table == nullptr) return {};
    Vector<JoinedRecord> result;
    for (size_t rowIndex = 0; rowIndex < table->getRows().size(); ++rowIndex)
    {
        JoinedRecord record;
        const Record& row = table->getRows()[rowIndex];
        for (const auto& item : row.getValues())
        {
            record.set(baseTable + "." + item.first, item.second);
            if (!tableAlias.empty()) record.set(tableAlias + "." + item.first, item.second);
        }
        result.push_back(std::move(record));
    }
    return result;
}

Vector<JoinedRecord> Query::applyJoins(Vector<JoinedRecord> records) const
{
    if (database == nullptr) return records;
    for (size_t joinIndex = 0; joinIndex < joins.size(); ++joinIndex)
    {
        const Join& join = joins[joinIndex];
        const Table* rightTable = database->getTable(join.getRightTable());
        if (rightTable == nullptr) return {};
        Vector<JoinedRecord> joined;
        Vector<int> matched(rightTable->getRows().size(), 0);
        string leftKey = join.getLeftTable() + "." + join.getLeftColumn();
        string rightColumn = unqualified(join.getRightColumn());
        for (size_t leftIndex = 0; leftIndex < records.size(); ++leftIndex)
        {
            const JoinedRecord& leftRecord = records[leftIndex];
            Value leftValue = monostate{};
            bool foundLeft = resolveColumn(leftRecord, leftKey, leftValue);
            bool foundMatch = false;
            if (foundLeft && !isNull(leftValue))
            {
                for (size_t rightIndex = 0; rightIndex < rightTable->getRows().size(); ++rightIndex)
                {
                    const Record& rightRecord = rightTable->getRows()[rightIndex];
                    if (!rightRecord.has(rightColumn) || isNull(rightRecord.get(rightColumn))) continue;
                    if (leftValue != rightRecord.get(rightColumn)) continue;
                    JoinedRecord combined = leftRecord;
                    for (const auto& item : rightRecord.getValues())
                        combined.set(join.getRightTable() + "." + item.first, item.second);
                    joined.push_back(std::move(combined));
                    matched[rightIndex] = 1;
                    foundMatch = true;
                }
            }
            if (!foundMatch && (join.getType() == JoinType::LEFT || join.getType() == JoinType::FULL))
            {
                JoinedRecord combined = leftRecord;
                for (size_t columnIndex = 0; columnIndex < rightTable->getColumns().size(); ++columnIndex)
                    combined.set(join.getRightTable() + "." + rightTable->getColumns()[columnIndex].getName(), monostate{});
                joined.push_back(std::move(combined));
            }
        }
        if (join.getType() == JoinType::RIGHT || join.getType() == JoinType::FULL)
        {
            for (size_t rightIndex = 0; rightIndex < rightTable->getRows().size(); ++rightIndex)
            {
                if (matched[rightIndex]) continue;
                JoinedRecord combined;
                if (!records.empty())
                    for (const auto& item : records[0].getValues()) combined.set(item.first, monostate{});
                const Record& rightRecord = rightTable->getRows()[rightIndex];
                for (const auto& item : rightRecord.getValues()) combined.set(join.getRightTable() + "." + item.first, item.second);
                joined.push_back(std::move(combined));
            }
        }
        records = std::move(joined);
    }
    return records;
}

Vector<JoinedRecord> Query::applyWhere(const Vector<JoinedRecord>& records) const
{
    Vector<JoinedRecord> result;
    for (size_t index = 0; index < records.size(); ++index)
    {
        const JoinedRecord& record = records[index];
        if (singleCondition.has_value() && !evaluateCondition(record, singleCondition.value())) continue;
        if (conditionGroup.has_value() && !evaluateConditionGroup(record, conditionGroup.value())) continue;
        if (isSubqueryIn || isSubqueryNotIn)
        {
            Value value = monostate{};
            bool foundColumn = resolveColumn(record, subqueryInColumn, value);
            bool found = false;
            if (foundColumn)
                for (size_t valueIndex = 0; valueIndex < subqueryInValues.size(); ++valueIndex)
                    if (value == subqueryInValues[valueIndex]) { found = true; break; }
            if (isSubqueryIn ? !found : found) continue;
        }
        result.push_back(record);
    }
    return result;
}

Vector<JoinedRecord> Query::applyGroupByAndAggregates(const Vector<JoinedRecord>& records) const
{
    if (aggregates.empty() && groupByColumns.empty()) return records;
    Vector<Vector<JoinedRecord>> groups;
    if (groupByColumns.empty())
    {
        groups.push_back(records);
    }
    else
    {
        for (size_t recordIndex = 0; recordIndex < records.size(); ++recordIndex)
        {
            size_t groupIndex = 0;
            for (; groupIndex < groups.size(); ++groupIndex)
            {
                bool equal = true;
                for (size_t keyIndex = 0; keyIndex < groupByColumns.size(); ++keyIndex)
                {
                    Value left = monostate{}, right = monostate{};
                    resolveColumn(groups[groupIndex][0], groupByColumns[keyIndex], left);
                    resolveColumn(records[recordIndex], groupByColumns[keyIndex], right);
                    if (left != right) { equal = false; break; }
                }
                if (equal) break;
            }
            if (groupIndex == groups.size()) groups.push_back(Vector<JoinedRecord>());
            groups[groupIndex].push_back(records[recordIndex]);
        }
    }
    Vector<JoinedRecord> result;
    for (size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        JoinedRecord output;
        if (!groups[groupIndex].empty())
            for (size_t keyIndex = 0; keyIndex < groupByColumns.size(); ++keyIndex)
            {
                Value key = monostate{};
                if (resolveColumn(groups[groupIndex][0], groupByColumns[keyIndex], key))
                    output.set(groupByColumns[keyIndex], key);
            }
        for (size_t aggregateIndex = 0; aggregateIndex < aggregates.size(); ++aggregateIndex)
        {
            const Aggregate& aggregate = aggregates[aggregateIndex];
            output.set(aggregate.getOutputName(), Aggregate::compute(aggregate.getType(), aggregate.getColumn(), groups[groupIndex]));
        }
        result.push_back(std::move(output));
    }
    return result;
}

Vector<JoinedRecord> Query::applyHaving(const Vector<JoinedRecord>& records) const
{
    Vector<JoinedRecord> result;
    for (size_t index = 0; index < records.size(); ++index)
    {
        if (havingCondition.has_value() && !evaluateCondition(records[index], havingCondition.value())) continue;
        if (havingConditionGroup.has_value() && !evaluateConditionGroup(records[index], havingConditionGroup.value())) continue;
        result.push_back(records[index]);
    }
    return result;
}

Vector<JoinedRecord> Query::applySelect(const Vector<JoinedRecord>& records) const
{
    if (selectedColumns.empty()) return records;
    Vector<JoinedRecord> result;
    for (size_t rowIndex = 0; rowIndex < records.size(); ++rowIndex)
    {
        JoinedRecord projected;
        for (size_t columnIndex = 0; columnIndex < selectedColumns.size(); ++columnIndex)
        {
            const string& column = selectedColumns[columnIndex];
            if (column == "*")
            {
                for (const auto& item : records[rowIndex].getValues()) projected.set(item.first, item.second);
                continue;
            }
            string prefix = column.substr(0, column.size() > 1 ? column.size() - 1 : 0);
            if (column.size() > 2 && column.substr(column.size() - 2) == ".*")
            {
                for (const auto& item : records[rowIndex].getValues())
                    if (item.first.compare(0, prefix.size(), prefix) == 0) projected.set(item.first, item.second);
                continue;
            }
            Value value = monostate{};
            if (resolveColumn(records[rowIndex], column, value)) projected.set(column, value);
        }
        result.push_back(std::move(projected));
    }
    return result;
}

Vector<JoinedRecord> Query::applyDistinct(const Vector<JoinedRecord>& records) const
{
    if (!isDistinct) return records;
    Vector<JoinedRecord> result;
    for (size_t index = 0; index < records.size(); ++index)
    {
        bool duplicate = false;
        for (size_t prior = 0; prior < result.size(); ++prior)
            if (records[index] == result[prior]) { duplicate = true; break; }
        if (!duplicate) result.push_back(records[index]);
    }
    return result;
}

Vector<JoinedRecord> Query::applyOrderBy(Vector<JoinedRecord> records) const
{
    for (size_t orderIndex = orderBys.size(); orderIndex > 0; --orderIndex)
    {
        const OrderBy& order = orderBys[orderIndex - 1];
        for (size_t index = 1; index < records.size(); ++index)
        {
            size_t current = index;
            while (current > 0)
            {
                Value left = monostate{}, right = monostate{};
                bool hasLeft = resolveColumn(records[current - 1], order.getColumnName(), left);
                bool hasRight = resolveColumn(records[current], order.getColumnName(), right);
                bool outOfOrder = hasLeft && hasRight
                    && (order.getOrder() == SortOrder::ASC
                        ? compareValues(left, right, CompareOperator::GREATER)
                        : compareValues(left, right, CompareOperator::LESS));
                if (!outOfOrder) break;
                JoinedRecord temp = std::move(records[current - 1]);
                records[current - 1] = std::move(records[current]);
                records[current] = std::move(temp);
                --current;
            }
        }
    }
    return records;
}

Vector<JoinedRecord> Query::applyLimitAndOffset(const Vector<JoinedRecord>& records) const
{
    Vector<JoinedRecord> result;
    size_t start = static_cast<size_t>(offsetCount);
    if (start >= records.size()) return result;
    size_t end = records.size();
    if (limitCount >= 0 && static_cast<size_t>(limitCount) < end - start) end = start + static_cast<size_t>(limitCount);
    for (size_t index = start; index < end; ++index) result.push_back(records[index]);
    return result;
}

Vector<JoinedRecord> Query::execute() const
{
    Vector<JoinedRecord> records = buildBaseRecords();
    records = applyJoins(std::move(records));
    records = applyWhere(records);
    records = applyGroupByAndAggregates(records);
    records = applyHaving(records);
    records = applyOrderBy(std::move(records));
    records = applySelect(records);
    records = applyDistinct(records);
    return applyLimitAndOffset(records);
}

Vector<JoinedRecord> Query::unionWith(const Query& other, bool distinctOnly) const
{
    Vector<JoinedRecord> result = execute();
    Vector<JoinedRecord> right = other.execute();
    for (size_t index = 0; index < right.size(); ++index) result.push_back(right[index]);
    if (!distinctOnly) return result;
    Vector<JoinedRecord> unique;
    for (size_t index = 0; index < result.size(); ++index)
    {
        bool exists = false;
        for (size_t prior = 0; prior < unique.size(); ++prior)
            if (result[index] == unique[prior]) { exists = true; break; }
        if (!exists) unique.push_back(result[index]);
    }
    return unique;
}

Vector<JoinedRecord> Query::intersectWith(const Query& other) const
{
    Vector<JoinedRecord> left = execute(), right = other.execute(), result;
    for (size_t index = 0; index < left.size(); ++index)
        for (size_t rightIndex = 0; rightIndex < right.size(); ++rightIndex)
            if (left[index] == right[rightIndex]) { result.push_back(left[index]); break; }
    return result;
}

Vector<JoinedRecord> Query::exceptWith(const Query& other) const
{
    Vector<JoinedRecord> left = execute(), right = other.execute(), result;
    for (size_t index = 0; index < left.size(); ++index)
    {
        bool found = false;
        for (size_t rightIndex = 0; rightIndex < right.size(); ++rightIndex)
            if (left[index] == right[rightIndex]) { found = true; break; }
        if (!found) result.push_back(left[index]);
    }
    return result;
}

Database& Query::getDatabase() const
{
    if (database == nullptr) throw runtime_error("Query has no database");
    return *database;
}

const string& Query::getBaseTable() const { return baseTable; }
const Vector<string>& Query::getSelectedColumns() const { return selectedColumns; }
const Vector<Join>& Query::getJoins() const { return joins; }

