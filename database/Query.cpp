#include "Query.h"

#include <iostream>
#include <stdexcept>
#include <algorithm>

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

static string joinedRecordHashKey(const JoinedRecord& record)
{
    Vector<string> keys = record.getValues().getKeys();
    if (keys.size() > 1) std::sort(keys.begin(), keys.end());

    string key = "record:";
    for (size_t index = 0; index < keys.size(); ++index)
    {
        const string& column = keys[index];
        const string valueKey = valueToExactHashKey(record.get(column));
        key += to_string(column.size()) + ":" + column;
        key += to_string(valueKey.size()) + ":" + valueKey;
    }
    return key;
}

static string groupHashKey(const JoinedRecord& record, const Vector<string>& columns)
{
    string key = "group:";
    for (size_t index = 0; index < columns.size(); ++index)
    {
        Value value = monostate{};
        resolveColumn(record, columns[index], value);
        const string valueKey = valueToExactHashKey(value);
        key += to_string(valueKey.size()) + ":" + valueKey;
    }
    return key;
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

    Vector<size_t> rowCandidates;
    bool usedIndex = false;

    // Use an equality index to avoid materializing rows that WHERE will discard.
    // The normal WHERE pass still evaluates the condition, preserving semantics.
    if (singleCondition.has_value()
        && singleCondition.value().getOperation() == CompareOperator::EQUAL
        && !isNull(singleCondition.value().getCompareValue()))
    {
        const string conditionColumn = singleCondition.value().getColumnName();
        const size_t dot = conditionColumn.find_last_of('.');
        bool belongsToBaseTable = false;
        if (dot != string::npos)
        {
            const string qualifier = conditionColumn.substr(0, dot);
            belongsToBaseTable = qualifier == baseTable
                || (!tableAlias.empty() && qualifier == tableAlias);
        }
        else
        {
            // In a joined query, an unqualified column can be ambiguous. Keep
            // the original scan in that case rather than choosing the wrong index.
            belongsToBaseTable = joins.empty();
        }

        if (belongsToBaseTable)
        {
            const string columnName = unqualified(conditionColumn);
            const Vector<Index*>& indexes = table->getIndexes();
            for (size_t index = 0; index < indexes.size(); ++index)
            {
                if (indexes[index]->getColumnName() == columnName)
                {
                    rowCandidates = indexes[index]->find(singleCondition.value().getCompareValue());
                    usedIndex = true;
                    break;
                }
            }
        }
    }

    if (usedIndex)
    {
        // Updates can append an index entry at a later row position; sort IDs
        // to keep the normal table order when no ORDER BY has been requested.
        std::sort(rowCandidates.begin(), rowCandidates.end());
    }
    else
    {
        rowCandidates.clear();
        for (size_t rowIndex = 0; rowIndex < table->getRows().size(); ++rowIndex)
        {
            rowCandidates.push_back(rowIndex);
        }
    }

    Vector<JoinedRecord> result;
    for (size_t candidate = 0; candidate < rowCandidates.size(); ++candidate)
    {
        const size_t rowIndex = rowCandidates[candidate];
        if (rowIndex >= table->getRows().size()) continue;
        if (table->isRowDeleted(rowIndex)) continue;
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

        const Vector<Record>& rightRows = rightTable->getRows();
        Vector<JoinedRecord> joined;
        Vector<int> matched(rightRows.size(), 0);
        const string leftKey = join.getLeftTable() + "." + join.getLeftColumn();
        const string rightColumn = unqualified(join.getRightColumn());

        // Prefer a maintained table index. If the right join column is not
        // indexed, build a temporary hash lookup once (hash join) rather than
        // scanning the entire right table for every left row.
        const Index* rightIndex = nullptr;
        const Vector<Index*>& indexes = rightTable->getIndexes();
        for (size_t index = 0; index < indexes.size(); ++index)
        {
            if (indexes[index]->getColumnName() == rightColumn)
            {
                rightIndex = indexes[index];
                break;
            }
        }

        HashMap<string, Vector<size_t>> temporaryLookup;
        if (rightIndex == nullptr)
        {
            for (size_t rightIndexValue = 0; rightIndexValue < rightRows.size(); ++rightIndexValue)
            {
                const Record& rightRecord = rightRows[rightIndexValue];
                if (rightTable->isRowDeleted(rightIndexValue)) continue;
                if (!rightRecord.has(rightColumn)) continue;
                const Value rightValue = rightRecord.get(rightColumn);
                if (isNull(rightValue)) continue;
                temporaryLookup[valueToHashKey(rightValue)].push_back(rightIndexValue);
            }
        }

        for (size_t leftIndex = 0; leftIndex < records.size(); ++leftIndex)
        {
            const JoinedRecord& leftRecord = records[leftIndex];
            Value leftValue = monostate{};
            const bool foundLeft = resolveColumn(leftRecord, leftKey, leftValue);
            bool foundMatch = false;

            if (foundLeft && !isNull(leftValue))
            {
                Vector<size_t> rightCandidates;
                if (rightIndex != nullptr)
                {
                    rightCandidates = rightIndex->find(leftValue);
                    std::sort(rightCandidates.begin(), rightCandidates.end());
                }
                else
                {
                    auto lookupIt = temporaryLookup.find(valueToHashKey(leftValue));
                    if (lookupIt != temporaryLookup.end())
                    {
                        rightCandidates = lookupIt->second;
                    }
                }

                for (size_t candidate = 0; candidate < rightCandidates.size(); ++candidate)
                {
                    const size_t rightRowIndex = rightCandidates[candidate];
                    if (rightRowIndex >= rightRows.size()) continue;
                    if (rightTable->isRowDeleted(rightRowIndex)) continue;
                    const Record& rightRecord = rightRows[rightRowIndex];
                    if (!rightRecord.has(rightColumn)) continue;
                    if (isNull(rightRecord.get(rightColumn))) continue;
                    // Retain the existing JOIN equality semantics, even if a
                    // numeric canonical key yielded a cross-type candidate.
                    if (leftValue != rightRecord.get(rightColumn)) continue;

                    JoinedRecord combined = leftRecord;
                    for (const auto& item : rightRecord.getValues())
                    {
                        combined.set(join.getRightTable() + "." + item.first, item.second);
                    }
                    joined.push_back(std::move(combined));
                    matched[rightRowIndex] = 1;
                    foundMatch = true;
                }
            }

            if (!foundMatch && (join.getType() == JoinType::LEFT || join.getType() == JoinType::FULL))
            {
                JoinedRecord combined = leftRecord;
                for (size_t columnIndex = 0; columnIndex < rightTable->getColumns().size(); ++columnIndex)
                {
                    combined.set(join.getRightTable() + "." + rightTable->getColumns()[columnIndex].getName(), monostate{});
                }
                joined.push_back(std::move(combined));
            }
        }

        if (join.getType() == JoinType::RIGHT || join.getType() == JoinType::FULL)
        {
            for (size_t rightRowIndex = 0; rightRowIndex < rightRows.size(); ++rightRowIndex)
            {
                if (rightTable->isRowDeleted(rightRowIndex)) continue;
                if (matched[rightRowIndex]) continue;
                JoinedRecord combined;
                if (!records.empty())
                {
                    for (const auto& item : records[0].getValues()) combined.set(item.first, monostate{});
                }
                const Record& rightRecord = rightRows[rightRowIndex];
                for (const auto& item : rightRecord.getValues())
                {
                    combined.set(join.getRightTable() + "." + item.first, item.second);
                }
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
    HashMap<string, Vector<Value>> subqueryLookup;
    if (isSubqueryIn || isSubqueryNotIn)
    {
        for (size_t valueIndex = 0; valueIndex < subqueryInValues.size(); ++valueIndex)
        {
            subqueryLookup[valueToExactHashKey(subqueryInValues[valueIndex])].push_back(subqueryInValues[valueIndex]);
        }
    }

    for (size_t index = 0; index < records.size(); ++index)
    {
        const JoinedRecord& record = records[index];
        if (singleCondition.has_value() && !evaluateCondition(record, singleCondition.value())) continue;
        if (conditionGroup.has_value() && !evaluateConditionGroup(record, conditionGroup.value())) continue;
        if (isSubqueryIn || isSubqueryNotIn)
        {
            Value value = monostate{};
            const bool foundColumn = resolveColumn(record, subqueryInColumn, value);
            bool found = false;
            if (foundColumn)
            {
                auto lookup = subqueryLookup.find(valueToExactHashKey(value));
                if (lookup != subqueryLookup.end())
                {
                    for (size_t candidate = 0; candidate < lookup->second.size(); ++candidate)
                    {
                        if (value == lookup->second[candidate])
                        {
                            found = true;
                            break;
                        }
                    }
                }
            }
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
        HashMap<string, Vector<size_t>> groupBuckets;
        for (size_t recordIndex = 0; recordIndex < records.size(); ++recordIndex)
        {
            const string key = groupHashKey(records[recordIndex], groupByColumns);
            Vector<size_t>& candidates = groupBuckets[key];
            size_t groupIndex = groups.size();
            for (size_t candidate = 0; candidate < candidates.size(); ++candidate)
            {
                const size_t candidateGroup = candidates[candidate];
                bool equal = true;
                for (size_t keyIndex = 0; keyIndex < groupByColumns.size(); ++keyIndex)
                {
                    Value left = monostate{}, right = monostate{};
                    resolveColumn(groups[candidateGroup][0], groupByColumns[keyIndex], left);
                    resolveColumn(records[recordIndex], groupByColumns[keyIndex], right);
                    if (left != right) { equal = false; break; }
                }
                if (equal)
                {
                    groupIndex = candidateGroup;
                    break;
                }
            }
            if (groupIndex == groups.size())
            {
                groups.push_back(Vector<JoinedRecord>());
                candidates.push_back(groupIndex);
            }
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
    HashMap<string, Vector<size_t>> seen;
    for (size_t index = 0; index < records.size(); ++index)
    {
        const string key = joinedRecordHashKey(records[index]);
        Vector<size_t>& candidates = seen[key];
        bool duplicate = false;
        for (size_t candidate = 0; candidate < candidates.size(); ++candidate)
        {
            if (records[index] == result[candidates[candidate]])
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
        {
            candidates.push_back(result.size());
            result.push_back(records[index]);
        }
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
    Vector<JoinedRecord> result;
    Vector<JoinedRecord> left = execute();
    Vector<JoinedRecord> right = other.execute();
    if (!distinctOnly)
    {
        for (size_t index = 0; index < left.size(); ++index) result.push_back(left[index]);
        for (size_t index = 0; index < right.size(); ++index) result.push_back(right[index]);
        return result;
    }

    HashMap<string, Vector<size_t>> seen;
    for (size_t side = 0; side < 2; ++side)
    {
        const Vector<JoinedRecord>& input = side == 0 ? left : right;
        for (size_t index = 0; index < input.size(); ++index)
        {
            const string key = joinedRecordHashKey(input[index]);
            Vector<size_t>& candidates = seen[key];
            bool exists = false;
            for (size_t candidate = 0; candidate < candidates.size(); ++candidate)
            {
                if (input[index] == result[candidates[candidate]])
                {
                    exists = true;
                    break;
                }
            }
            if (!exists)
            {
                candidates.push_back(result.size());
                result.push_back(input[index]);
            }
        }
    }
    return result;
}

Vector<JoinedRecord> Query::intersectWith(const Query& other) const
{
    Vector<JoinedRecord> left = execute();
    Vector<JoinedRecord> right = other.execute();
    Vector<JoinedRecord> result;
    HashMap<string, Vector<size_t>> rightLookup;
    for (size_t index = 0; index < right.size(); ++index)
    {
        rightLookup[joinedRecordHashKey(right[index])].push_back(index);
    }
    for (size_t index = 0; index < left.size(); ++index)
    {
        auto lookup = rightLookup.find(joinedRecordHashKey(left[index]));
        if (lookup == rightLookup.end()) continue;
        for (size_t candidate = 0; candidate < lookup->second.size(); ++candidate)
        {
            if (left[index] == right[lookup->second[candidate]])
            {
                result.push_back(left[index]);
                break;
            }
        }
    }
    return result;
}

Vector<JoinedRecord> Query::exceptWith(const Query& other) const
{
    Vector<JoinedRecord> left = execute();
    Vector<JoinedRecord> right = other.execute();
    Vector<JoinedRecord> result;
    HashMap<string, Vector<size_t>> rightLookup;
    for (size_t index = 0; index < right.size(); ++index)
    {
        rightLookup[joinedRecordHashKey(right[index])].push_back(index);
    }
    for (size_t index = 0; index < left.size(); ++index)
    {
        bool found = false;
        auto lookup = rightLookup.find(joinedRecordHashKey(left[index]));
        if (lookup != rightLookup.end())
        {
            for (size_t candidate = 0; candidate < lookup->second.size(); ++candidate)
            {
                if (left[index] == right[lookup->second[candidate]])
                {
                    found = true;
                    break;
                }
            }
        }
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
