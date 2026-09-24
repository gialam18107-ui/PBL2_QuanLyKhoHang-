#include "Query.h"

#include <iostream>
#include <algorithm>

using namespace std;


// ============================================================
// CONSTRUCTOR
// CONSTRUCTORS
// ============================================================

Query::Query(
    Database& database
)
    : database(database)
Query::Query()
    : database(nullptr),
      hasSubqueryFrom(false),
      isSubqueryIn(false),
      isSubqueryNotIn(false),
      isDistinct(false),
      limitCount(-1),
      offsetCount(0)
{
}

Query::Query(Database& database)
    : database(&database),
      hasSubqueryFrom(false),
      isSubqueryIn(false),
      isSubqueryNotIn(false),
      isDistinct(false),
      limitCount(-1),
      offsetCount(0)
{
}

// ============================================================
// FROM
// FROM & SUBQUERIES
// ============================================================

Query& Query::from(
    const string& tableName
)
Query& Query::from(const string& tableName)
{
    baseTable = tableName;
    hasSubqueryFrom = false;
    return *this;
}

Query& Query::from(const Query& subquery, const string& alias)
{
    tableAlias = alias;
    hasSubqueryFrom = true;
    subqueryFromRecords = subquery.execute();

    // Re-prefix records with the subquery alias
    for (size_t i = 0; i < subqueryFromRecords.size(); i++)
    {
        JoinedRecord newRecord;
        for (const auto& pair : subqueryFromRecords[i].getValues())
        {
            string col = pair.first;
            size_t dotPos = col.find('.');
            string shortName = (dotPos != string::npos) ? col.substr(dotPos + 1) : col;
            newRecord.set(alias + "." + shortName, pair.second);
        }
        subqueryFromRecords[i] = newRecord;
    }
    return *this;
}


// ============================================================
// SELECT
// SELECT & DISTINCT
// ============================================================

Query& Query::select(
    const vector<string>& columns
)
Query& Query::select(const Vector<string>& columns)
{
    selectedColumns = columns;
    return *this;
}

Query& Query::select(std::initializer_list<string> columns)
{
    selectedColumns.clear();
    for (const auto& col : columns)
    {
        selectedColumns.push_back(col);
    }
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


// ============================================================
// JOIN
// ============================================================

Query& Query::join(
    const Join& joinObject
)
Query& Query::join(const Join& joinObject)
{
    joins.push_back(
        joinObject
    );

    joins.push_back(joinObject);
    return *this;
}


// ============================================================
// WHERE
// WHERE & SUBQUERY IN / NOT IN
// ============================================================

Query& Query::where(
    const Condition& condition
)
Query& Query::where(const Condition& condition)
{
    singleCondition = condition;

    conditionGroup.reset();

    return *this;
}


Query& Query::where(
    const ConditionGroup& group
)
Query& Query::where(const ConditionGroup& group)
{
    conditionGroup = group;

    singleCondition.reset();

    return *this;
}

Query& Query::whereIn(const string& column, const Query& subquery)
{
    subqueryInColumn = column;
    isSubqueryIn = true;
    isSubqueryNotIn = false;
    subqueryInValues.clear();

// ============================================================
// ORDER BY
// ============================================================
    Vector<JoinedRecord> res = subquery.execute();
    for (size_t i = 0; i < res.size(); i++)
    {
        for (const auto& pair : res[i].getValues())
        {
            subqueryInValues.push_back(pair.second);
            break; // take first column
        }
    }
    return *this;
}

Query& Query::orderBy(
    const OrderBy& orderByObject
)
Query& Query::whereNotIn(const string& column, const Query& subquery)
{
    order = orderByObject;

    whereIn(column, subquery);
    isSubqueryIn = false;
    isSubqueryNotIn = true;
    return *this;
}


// ============================================================
// COMPARE VALUES
// GROUP BY, AGGREGATES, HAVING
// ============================================================

bool Query::compareValues(
    const Value& leftValue,
    const Value& rightValue,
    CompareOperator operation
) const
Query& Query::groupBy(const string& column)
{
    /*
        Hai giá trị phải cùng kiểu.
    */
    groupByColumns.push_back(column);
    return *this;
}

    if (
        leftValue.index()
        !=
        rightValue.index()
    )
Query& Query::groupBy(const Vector<string>& columns)
{
    for (size_t i = 0; i < columns.size(); i++)
    {
        return false;
        groupByColumns.push_back(columns[i]);
    }
    return *this;
}

Query& Query::aggregate(AggregateType type, const string& column, const string& alias)
{
    aggregates.push_back(Aggregate(type, column, alias));
    return *this;
}

    // ========================================================
    // INTEGER
    // ========================================================
Query& Query::aggregate(const Aggregate& agg)
{
    aggregates.push_back(agg);
    return *this;
}

    if (
        holds_alternative<int>(leftValue)
    )
    {
        int left =
            get<int>(leftValue);
Query& Query::having(const Condition& condition)
{
    havingCondition = condition;
    havingConditionGroup.reset();
    return *this;
}

        int right =
            get<int>(rightValue);
Query& Query::having(const ConditionGroup& group)
{
    havingConditionGroup = group;
    havingCondition.reset();
    return *this;
}

        switch (operation)
        {
        case CompareOperator::EQUAL:
            return left == right;
// ============================================================
// ORDER BY, LIMIT, OFFSET
// ============================================================

        case CompareOperator::NOT_EQUAL:
            return left != right;
Query& Query::orderBy(const OrderBy& orderByObject)
{
    orderBys.push_back(orderByObject);
    return *this;
}

        case CompareOperator::LESS:
            return left < right;
Query& Query::orderBy(const string& columnName, SortOrder order)
{
    orderBys.push_back(OrderBy(columnName, order));
    return *this;
}

        case CompareOperator::LESS_EQUAL:
            return left <= right;
Query& Query::limit(int count)
{
    limitCount = count;
    return *this;
}

        case CompareOperator::GREATER:
            return left > right;
Query& Query::offset(int count)
{
    offsetCount = count;
    return *this;
}

        case CompareOperator::GREATER_EQUAL:
            return left >= right;
        }
// ============================================================
// INTERNAL PIPELINE: BUILD BASE RECORDS
// ============================================================

Vector<JoinedRecord> Query::buildBaseRecords() const
{
    if (hasSubqueryFrom)
    {
        return subqueryFromRecords;
    }

    if (baseTable.empty())
    {
        return {};
    }

    // ========================================================
    // DOUBLE
    // ========================================================
    if (database == nullptr)
    {
        return {};
    }

    if (
        holds_alternative<double>(leftValue)
    )
    const Table* base = database->getTable(baseTable);
    if (base == nullptr)
    {
        double left =
            get<double>(leftValue);

        double right =
            get<double>(rightValue);

        switch (operation)
        {
        case CompareOperator::EQUAL:
            return left == right;

        case CompareOperator::NOT_EQUAL:
            return left != right;

        case CompareOperator::LESS:
            return left < right;

        case CompareOperator::LESS_EQUAL:
            return left <= right;

        case CompareOperator::GREATER:
            return left > right;

        case CompareOperator::GREATER_EQUAL:
            return left >= right;
        }
        cout << "Query failed: table not found: " << baseTable << endl;
        return {};
    }


    // ========================================================
    // STRING
    // ========================================================

    if (
        holds_alternative<string>(leftValue)
    )
    Vector<JoinedRecord> result;
    for (size_t i = 0; i < base->getRows().size(); i++)
    {
        const string& left =
            get<string>(leftValue);

        const string& right =
            get<string>(rightValue);

        switch (operation)
        const Record& source = base->getRows()[i];
        JoinedRecord jr;
        for (const auto& pair : source.getValues())
        {
        case CompareOperator::EQUAL:
            return left == right;

        case CompareOperator::NOT_EQUAL:
            return left != right;

        case CompareOperator::LESS:
            return left < right;

        case CompareOperator::LESS_EQUAL:
            return left <= right;

        case CompareOperator::GREATER:
            return left > right;

        case CompareOperator::GREATER_EQUAL:
            return left >= right;
            jr.set(baseTable + "." + pair.first, pair.second);
        }
        result.push_back(jr);
    }

    return false;
    return result;
}


// ============================================================
// EVALUATE CONDITION
// INTERNAL PIPELINE: APPLY JOINS (INNER, LEFT, RIGHT, FULL)
// ============================================================

bool Query::evaluateCondition(
    const JoinedRecord& record,
    const Condition& condition
) const
Vector<JoinedRecord> Query::applyJoins(Vector<JoinedRecord> records) const
{
    const string& columnName =
        condition.getColumnName();


    if (
        !record.has(columnName)
    )
    if (database == nullptr)
    {
        return false;
        return records;
    }

    for (size_t j = 0; j < joins.size(); j++)
    {
        const Join& join = joins[j];
        const Table* rightTable = database->getTable(join.getRightTable());
        if (rightTable == nullptr)
        {
            cout << "Query failed: join table not found: " << join.getRightTable() << endl;
            return {};
        }

    Value recordValue =
        record.get(columnName);
        Vector<JoinedRecord> joinedResult;
        Vector<bool> rightMatched(rightTable->getRows().size(), false);

    Value compareValue =
        condition.getCompareValue();
        string leftKey = join.getLeftTable() + "." + join.getLeftColumn();
        string rightKey = join.getRightColumn();

        for (size_t l = 0; l < records.size(); l++)
        {
            const JoinedRecord& leftRec = records[l];
            bool foundMatch = false;

    return compareValues(
        recordValue,
        compareValue,
        condition.getOperation()
    );
}
            if (leftRec.has(leftKey) && !isNull(leftRec.get(leftKey)))
            {
                Value leftVal = leftRec.get(leftKey);

                for (size_t r = 0; r < rightTable->getRows().size(); r++)
                {
                    const Record& rightRec = rightTable->getRows()[r];
                    if (!rightRec.has(rightKey) || isNull(rightRec.get(rightKey)))
                    {
                        continue;
                    }

// ============================================================
// EVALUATE CONDITION GROUP
// ============================================================
                    if (leftVal == rightRec.get(rightKey))
                    {
                        foundMatch = true;
                        rightMatched[r] = true;

bool Query::evaluateConditionGroup(
    const JoinedRecord& record,
    const ConditionGroup& group
) const
{
    const vector<Condition>& conditions =
        group.getConditions();
                        JoinedRecord comb = leftRec;
                        for (const auto& pair : rightRec.getValues())
                        {
                            comb.set(join.getRightTable() + "." + pair.first, pair.second);
                        }
                        joinedResult.push_back(comb);
                    }
                }
            }


    if (conditions.empty())
    {
        return false;
    }


    // ========================================================
    // AND
    // ========================================================

    if (
        group.getOperation()
        ==
        LogicalOperator::AND
    )
    {
        for (
            const Condition& condition
            :
            conditions
        )
        {
            if (
                !evaluateCondition(
                    record,
                    condition
                )
            )
            // For LEFT JOIN or FULL JOIN: if no match, keep left row with NULL right columns
            if (!foundMatch && (join.getType() == JoinType::LEFT || join.getType() == JoinType::FULL))
            {
                return false;
                JoinedRecord comb = leftRec;
                for (size_t c = 0; c < rightTable->getColumns().size(); c++)
                {
                    comb.set(join.getRightTable() + "." + rightTable->getColumns()[c].getName(), monostate{});
                }
                joinedResult.push_back(comb);
            }
        }

        return true;
    }


    // ========================================================
    // OR
    // ========================================================

    if (
        group.getOperation()
        ==
        LogicalOperator::OR
    )
    {
        for (
            const Condition& condition
            :
            conditions
        )
        // For RIGHT JOIN or FULL JOIN: add unmatched right rows with NULL left columns
        if (join.getType() == JoinType::RIGHT || join.getType() == JoinType::FULL)
        {
            if (
                evaluateCondition(
                    record,
                    condition
                )
            )
            for (size_t r = 0; r < rightTable->getRows().size(); r++)
            {
                return true;
                if (!rightMatched[r])
                {
                    const Record& rightRec = rightTable->getRows()[r];
                    JoinedRecord comb;
                    // Nullify existing left keys
                    if (!records.empty())
                    {
                        for (const auto& pair : records[0].getValues())
                        {
                            comb.set(pair.first, monostate{});
                        }
                    }
                    // Add right columns
                    for (const auto& pair : rightRec.getValues())
                    {
                        comb.set(join.getRightTable() + "." + pair.first, pair.second);
                    }
                    joinedResult.push_back(comb);
                }
            }
        }

        return false;
        records = joinedResult;
    }


    return false;
    return records;
}


// ============================================================
// BUILD JOINED RECORDS
// INTERNAL PIPELINE: EVALUATE CONDITIONS
// ============================================================

vector<JoinedRecord>
Query::buildJoinedRecords() const
bool Query::evaluateCondition(const JoinedRecord& record, const Condition& condition) const
{
    vector<JoinedRecord> result;
    string col = condition.getColumnName();
    Value val = monostate{};


    // ========================================================
    // FROM
    // ========================================================

    if (baseTable.empty())
    if (record.has(col))
    {
        cout
            << "Query failed: "
            << "FROM table is empty."
            << endl;

        return {};
        val = record.get(col);
    }
    else
    {
        // Check if unqualified column matches (e.g. "price" matches "products.price")
        bool found = false;
        for (const auto& pair : record.getValues())
        {
            size_t dotPos = pair.first.find('.');
            if (dotPos != string::npos && pair.first.substr(dotPos + 1) == col)
            {
                val = pair.second;
                found = true;
                break;
            }
        }
        if (!found && condition.getOperation() != CompareOperator::IS_NULL)
        {
            return false;
        }
    }

    return condition.evaluateValue(val);
}

    auto base =
        database.getTable(
            baseTable
        );
bool Query::evaluateConditionGroup(const JoinedRecord& record, const ConditionGroup& group) const
{
    const Vector<Condition>& conds = group.getConditions();
    if (conds.empty()) return false;

    if (base == nullptr)
    if (group.getOperation() == LogicalOperator::NOT)
    {
        cout
            << "Query failed: "
            << "table not found: "
            << baseTable
            << endl;

        return {};
        return !evaluateCondition(record, conds[0]);
    }


    // ========================================================
    // TẠO RECORD BAN ĐẦU
    // ========================================================

    for (
        const Record& sourceRecord
        :
        base->getRows()
    )
    if (group.getOperation() == LogicalOperator::AND)
    {
        JoinedRecord joinedRecord;


        for (
            const auto& pair
            :
            sourceRecord.getValues()
        )
        for (size_t i = 0; i < conds.size(); i++)
        {
            joinedRecord.set(
                baseTable
                + "."
                + pair.first,
                pair.second
            );
            if (!evaluateCondition(record, conds[i])) return false;
        }


        result.push_back(
            joinedRecord
        );
        return true;
    }


    // ========================================================
    // XỬ LÝ JOIN
    // ========================================================

    for (
        const Join& joinObject
        :
        joins
    )
    if (group.getOperation() == LogicalOperator::OR)
    {
        if (
            joinObject.getType()
            !=
            JoinType::INNER
        )
        for (size_t i = 0; i < conds.size(); i++)
        {
            continue;
            if (evaluateCondition(record, conds[i])) return true;
        }
        return false;
    }

    return false;
}

        auto rightTable =
            database.getTable(
                joinObject.getRightTable()
            );
// ============================================================
// INTERNAL PIPELINE: APPLY WHERE & SUBQUERY IN
// ============================================================

        if (rightTable == nullptr)
Vector<JoinedRecord> Query::applyWhere(const Vector<JoinedRecord>& records) const
{
    Vector<JoinedRecord> result;

    for (size_t i = 0; i < records.size(); i++)
    {
        const JoinedRecord& r = records[i];

        // 1. Single condition
        if (singleCondition.has_value() && !evaluateCondition(r, singleCondition.value()))
        {
            cout
                << "Query failed: "
                << "table not found: "
                << joinObject.getRightTable()
                << endl;

            return {};
            continue;
        }


        if (
            !rightTable->hasColumn(
                joinObject.getRightColumn()
            )
        )
        // 2. Condition group
        if (conditionGroup.has_value() && !evaluateConditionGroup(r, conditionGroup.value()))
        {
            cout
                << "Query failed: "
                << "column not found: "
                << joinObject.getRightTable()
                << "."
                << joinObject.getRightColumn()
                << endl;

            return {};
            continue;
        }


        vector<JoinedRecord> joinedResult;


        for (
            const JoinedRecord& currentRecord
            :
            result
        )
        // 3. Subquery IN / NOT IN
        if (isSubqueryIn || isSubqueryNotIn)
        {
            string leftKey =
                joinObject.getLeftTable()
                + "."
                + joinObject.getLeftColumn();
            Value val = monostate{};
            bool colFound = false;


            if (
                !currentRecord.has(leftKey)
            )
            if (r.has(subqueryInColumn))
            {
                continue;
                val = r.get(subqueryInColumn);
                colFound = true;
            }


            Value leftValue =
                currentRecord.get(leftKey);


            for (
                const Record& rightRecord
                :
                rightTable->getRows()
            )
            else
            {
                if (
                    !rightRecord.has(
                        joinObject.getRightColumn()
                    )
                )
                for (const auto& pair : r.getValues())
                {
                    continue;
                    size_t dotPos = pair.first.find('.');
                    if (dotPos != string::npos && pair.first.substr(dotPos + 1) == subqueryInColumn)
                    {
                        val = pair.second;
                        colFound = true;
                        break;
                    }
                }
            }

            if (!colFound) continue;

                Value rightValue =
                    rightRecord.get(
                        joinObject.getRightColumn()
                    );


                if (
                    leftValue.index()
                    !=
                    rightValue.index()
                )
            bool inList = false;
            for (size_t s = 0; s < subqueryInValues.size(); s++)
            {
                if (val == subqueryInValues[s])
                {
                    continue;
                    inList = true;
                    break;
                }
            }

            if (isSubqueryIn && !inList) continue;
            if (isSubqueryNotIn && inList) continue;
        }

                if (
                    leftValue != rightValue
                )
                {
                    continue;
                }
        result.push_back(r);
    }

    return result;
}

                // --------------------------------------------
                // GHÉP RECORD
                // --------------------------------------------
// ============================================================
// INTERNAL PIPELINE: GROUP BY & AGGREGATES
// ============================================================

                JoinedRecord newRecord =
                    currentRecord;
Vector<JoinedRecord> Query::applyGroupByAndAggregates(const Vector<JoinedRecord>& records) const
{
    if (groupByColumns.empty() && aggregates.empty())
    {
        return records;
    }

    // Case 1: No GROUP BY, but aggregate functions specified -> single record output
    if (groupByColumns.empty() && !aggregates.empty())
    {
        JoinedRecord aggRow;
        for (size_t a = 0; a < aggregates.size(); a++)
        {
            Value v = Aggregate::compute(aggregates[a].getType(), aggregates[a].getColumn(), records);
            aggRow.set(aggregates[a].getOutputName(), v);
        }
        Vector<JoinedRecord> res;
        res.push_back(aggRow);
        return res;
    }

                for (
                    const auto& pair
                    :
                    rightRecord.getValues()
                )
    // Case 2: GROUP BY specified
    HashMap<string, Vector<JoinedRecord>> groups;
    Vector<string> groupKeysOrder;

    for (size_t i = 0; i < records.size(); i++)
    {
        const JoinedRecord& r = records[i];
        string key;
        for (size_t g = 0; g < groupByColumns.size(); g++)
        {
            const string& col = groupByColumns[g];
            Value val = monostate{};
            if (r.has(col))
            {
                val = r.get(col);
            }
            else
            {
                for (const auto& pair : r.getValues())
                {
                    newRecord.set(
                        joinObject.getRightTable()
                        + "."
                        + pair.first,
                        pair.second
                    );
                    size_t dotPos = pair.first.find('.');
                    if (dotPos != string::npos && pair.first.substr(dotPos + 1) == col)
                    {
                        val = pair.second;
                        break;
                    }
                }
            }
            key += valueToString(val) + "||";
        }

        if (!groups.contains(key))
        {
            groupKeysOrder.push_back(key);
        }
        groups[key].push_back(r);
    }

                joinedResult.push_back(
                    newRecord
                );
    Vector<JoinedRecord> result;
    for (size_t k = 0; k < groupKeysOrder.size(); k++)
    {
        const string& key = groupKeysOrder[k];
        const Vector<JoinedRecord>& groupRows = groups[key];
        JoinedRecord groupOut;

        // Populate grouped column values
        const JoinedRecord& firstRow = groupRows[0];
        for (size_t g = 0; g < groupByColumns.size(); g++)
        {
            const string& col = groupByColumns[g];
            if (firstRow.has(col))
            {
                groupOut.set(col, firstRow.get(col));
            }
            else
            {
                for (const auto& pair : firstRow.getValues())
                {
                    size_t dotPos = pair.first.find('.');
                    if (dotPos != string::npos && pair.first.substr(dotPos + 1) == col)
                    {
                        groupOut.set(col, pair.second);
                        break;
                    }
                }
            }
        }

        // Compute aggregates for group
        for (size_t a = 0; a < aggregates.size(); a++)
        {
            Value v = Aggregate::compute(aggregates[a].getType(), aggregates[a].getColumn(), groupRows);
            groupOut.set(aggregates[a].getOutputName(), v);
        }

        result =
            joinedResult;
        result.push_back(groupOut);
    }


    return result;
}


// ============================================================
// APPLY WHERE
// INTERNAL PIPELINE: APPLY HAVING
// ============================================================

vector<JoinedRecord>
Query::applyWhere(
    const vector<JoinedRecord>& records
) const
Vector<JoinedRecord> Query::applyHaving(const Vector<JoinedRecord>& records) const
{
    if (
        !singleCondition.has_value()
        &&
        !conditionGroup.has_value()
    )
    if (!havingCondition.has_value() && !havingConditionGroup.has_value())
    {
        return records;
    }


    vector<JoinedRecord> result;


    for (
        const JoinedRecord& record
        :
        records
    )
    Vector<JoinedRecord> result;
    for (size_t i = 0; i < records.size(); i++)
    {
        bool matched = false;
        const JoinedRecord& r = records[i];


        if (
            singleCondition.has_value()
        )
        if (havingCondition.has_value() && !evaluateCondition(r, havingCondition.value()))
        {
            matched =
                evaluateCondition(
                    record,
                    singleCondition.value()
                );
            continue;
        }
        else

        if (havingConditionGroup.has_value() && !evaluateConditionGroup(r, havingConditionGroup.value()))
        {
            matched =
                evaluateConditionGroup(
                    record,
                    conditionGroup.value()
                );
            continue;
        }


        if (matched)
        {
            result.push_back(
                record
            );
        }
        result.push_back(r);
    }


    return result;
}


// ============================================================
// APPLY ORDER BY
// INTERNAL PIPELINE: APPLY SELECT
// ============================================================

vector<JoinedRecord>
Query::applyOrderBy(
    vector<JoinedRecord> records
) const
Vector<JoinedRecord> Query::applySelect(const Vector<JoinedRecord>& records) const
{
    if (!order.has_value())
    if (selectedColumns.empty())
    {
        return records;
    }

    Vector<JoinedRecord> result;

    const string& columnName =
        order->getColumnName();
    for (size_t i = 0; i < records.size(); i++)
    {
        const JoinedRecord& src = records[i];
        JoinedRecord sel;


    sort(
        records.begin(),
        records.end(),
        [this, &columnName](
            const JoinedRecord& a,
            const JoinedRecord& b
        )
        for (size_t c = 0; c < selectedColumns.size(); c++)
        {
            if (
                !a.has(columnName)
                ||
                !b.has(columnName)
            )
            const string& col = selectedColumns[c];
            if (src.has(col))
            {
                return false;
                sel.set(col, src.get(col));
            }
            else
            {
                // Fallback: search for unqualified name or alias
                for (const auto& pair : src.getValues())
                {
                    size_t dotPos = pair.first.find('.');
                    if (dotPos != string::npos && pair.first.substr(dotPos + 1) == col)
                    {
                        sel.set(col, pair.second);
                        break;
                    }
                }
            }
        }

        result.push_back(sel);
    }

            Value valueA =
                a.get(columnName);
    return result;
}

            Value valueB =
                b.get(columnName);
// ============================================================
// INTERNAL PIPELINE: APPLY DISTINCT
// ============================================================

Vector<JoinedRecord> Query::applyDistinct(const Vector<JoinedRecord>& records) const
{
    if (!isDistinct)
    {
        return records;
    }

            if (
                valueA.index()
                !=
                valueB.index()
            )
    Vector<JoinedRecord> result;
    for (size_t i = 0; i < records.size(); i++)
    {
        bool dup = false;
        for (size_t j = 0; j < result.size(); j++)
        {
            if (records[i] == result[j])
            {
                return false;
                dup = true;
                break;
            }
        }
        if (!dup)
        {
            result.push_back(records[i]);
        }
    }
    return result;
}

// ============================================================
// INTERNAL PIPELINE: APPLY ORDER BY
// ============================================================

            bool ascending =
                order->getOrder()
                ==
                SortOrder::ASC;
Vector<JoinedRecord> Query::applyOrderBy(Vector<JoinedRecord> records) const
{
    if (orderBys.empty() || records.size() <= 1)
    {
        return records;
    }

    auto isNumeric = [](const Value& v) {
        return holds_alternative<int>(v) || holds_alternative<double>(v);
    };

            if (
                holds_alternative<int>(
                    valueA
                )
            )
            {
                int aValue =
                    get<int>(valueA);
    auto toDouble = [](const Value& v) -> double {
        if (holds_alternative<int>(v)) return static_cast<double>(get<int>(v));
        if (holds_alternative<double>(v)) return get<double>(v);
        return 0.0;
    };

                int bValue =
                    get<int>(valueB);

                return ascending
                    ? aValue < bValue
                    : aValue > bValue;
            }


            if (
                holds_alternative<double>(
                    valueA
                )
            )
    // Bubble sort or insertion sort on custom Vector for stable sorting
    for (size_t i = 0; i < records.size(); i++)
    {
        for (size_t j = i + 1; j < records.size(); j++)
        {
            for (size_t o = 0; o < orderBys.size(); o++)
            {
                double aValue =
                    get<double>(valueA);
                const string& col = orderBys[o].getColumnName();
                bool asc = (orderBys[o].getOrder() == SortOrder::ASC);

                double bValue =
                    get<double>(valueB);
                Value valA = records[i].has(col) ? records[i].get(col) : monostate{};
                Value valB = records[j].has(col) ? records[j].get(col) : monostate{};

                return ascending
                    ? aValue < bValue
                    : aValue > bValue;
            }
                // Handle NULLs: NULL is considered smaller than any value
                if (isNull(valA) && !isNull(valB))
                {
                    if (!asc) { std::swap(records[i], records[j]); }
                    break;
                }
                if (!isNull(valA) && isNull(valB))
                {
                    if (asc) { std::swap(records[i], records[j]); }
                    break;
                }
                if (isNull(valA) && isNull(valB))
                {
                    continue;
                }

                if (isNumeric(valA) && isNumeric(valB))
                {
                    double da = toDouble(valA);
                    double db = toDouble(valB);
                    if (da != db)
                    {
                        if (asc ? (da > db) : (da < db))
                        {
                            std::swap(records[i], records[j]);
                        }
                        break;
                    }
                    continue;
                }

            if (
                holds_alternative<string>(
                    valueA
                )
            )
            {
                const string& aValue =
                    get<string>(valueA);

                const string& bValue =
                    get<string>(valueB);

                return ascending
                    ? aValue < bValue
                    : aValue > bValue;
                if (holds_alternative<string>(valA) && holds_alternative<string>(valB))
                {
                    const string& sa = get<string>(valA);
                    const string& sb = get<string>(valB);
                    if (sa != sb)
                    {
                        if (asc ? (sa > sb) : (sa < sb))
                        {
                            std::swap(records[i], records[j]);
                        }
                        break;
                    }
                    continue;
                }
            }


            return false;
        }
    );
    }


    return records;
}


// ============================================================
// APPLY SELECT
// INTERNAL PIPELINE: APPLY LIMIT & OFFSET
// ============================================================

vector<JoinedRecord>
Query::applySelect(
    const vector<JoinedRecord>& records
) const
Vector<JoinedRecord> Query::applyLimitAndOffset(const Vector<JoinedRecord>& records) const
{
    /*
        SELECT *.
    */

    if (selectedColumns.empty())
    if (offsetCount <= 0 && limitCount < 0)
    {
        return records;
    }

    Vector<JoinedRecord> result;
    size_t start = offsetCount > 0 ? static_cast<size_t>(offsetCount) : 0;
    size_t count = 0;

    vector<JoinedRecord> result;


    for (
        const JoinedRecord& sourceRecord
        :
        records
    )
    for (size_t i = start; i < records.size(); i++)
    {
        JoinedRecord selectedRecord;


        for (
            const string& columnName
            :
            selectedColumns
        )
        if (limitCount >= 0 && count >= static_cast<size_t>(limitCount))
        {
            if (
                !sourceRecord.has(
                    columnName
                )
            )
            {
                continue;
            }


            selectedRecord.set(
                columnName,
                sourceRecord.get(
                    columnName
                )
            );
            break;
        }


        result.push_back(
            selectedRecord
        );
        result.push_back(records[i]);
        count++;
    }


    return result;
}


// ============================================================
// EXECUTE
// ============================================================

vector<JoinedRecord>
Query::execute() const
Vector<JoinedRecord> Query::execute() const
{
    /*
        1. FROM + JOIN
    */
    Vector<JoinedRecord> res = buildBaseRecords();
    res = applyJoins(res);
    res = applyWhere(res);
    res = applyGroupByAndAggregates(res);
    res = applyHaving(res);
    res = applySelect(res);
    res = applyDistinct(res);
    res = applyOrderBy(res);
    res = applyLimitAndOffset(res);
    return res;
}

    vector<JoinedRecord> result =
        buildJoinedRecords();
// ============================================================
// SET OPERATIONS (UNION, INTERSECT, EXCEPT)
// ============================================================

Vector<JoinedRecord> Query::unionWith(const Query& other, bool distinctOnly) const
{
    Vector<JoinedRecord> resA = execute();
    Vector<JoinedRecord> resB = other.execute();

    /*
        2. WHERE
    */
    Vector<JoinedRecord> combined = resA;
    for (size_t i = 0; i < resB.size(); i++)
    {
        combined.push_back(resB[i]);
    }

    result =
        applyWhere(result);
    if (distinctOnly)
    {
        Vector<JoinedRecord> distinctResult;
        for (size_t i = 0; i < combined.size(); i++)
        {
            bool exists = false;
            for (size_t j = 0; j < distinctResult.size(); j++)
            {
                if (combined[i] == distinctResult[j])
                {
                    exists = true;
                    break;
                }
            }
            if (!exists)
            {
                distinctResult.push_back(combined[i]);
            }
        }
        return distinctResult;
    }

    return combined;
}

    /*
        3. ORDER BY
    */
Vector<JoinedRecord> Query::intersectWith(const Query& other) const
{
    Vector<JoinedRecord> resA = execute();
    Vector<JoinedRecord> resB = other.execute();
    Vector<JoinedRecord> result;

    result =
        applyOrderBy(result);
    for (size_t i = 0; i < resA.size(); i++)
    {
        for (size_t j = 0; j < resB.size(); j++)
        {
            if (resA[i] == resB[j])
            {
                bool alreadyIn = false;
                for (size_t k = 0; k < result.size(); k++)
                {
                    if (result[k] == resA[i])
                    {
                        alreadyIn = true;
                        break;
                    }
                }
                if (!alreadyIn)
                {
                    result.push_back(resA[i]);
                }
                break;
            }
        }
    }

    return result;
}

    /*
        4. SELECT
    */
Vector<JoinedRecord> Query::exceptWith(const Query& other) const
{
    Vector<JoinedRecord> resA = execute();
    Vector<JoinedRecord> resB = other.execute();
    Vector<JoinedRecord> result;

    result =
        applySelect(result);
    for (size_t i = 0; i < resA.size(); i++)
    {
        bool foundInB = false;
        for (size_t j = 0; j < resB.size(); j++)
        {
            if (resA[i] == resB[j])
            {
                foundInB = true;
                break;
            }
        }
        if (!foundInB)
        {
            result.push_back(resA[i]);
        }
    }


    return result;
}


// ============================================================
// GETTERS
// ============================================================

Database& Query::getDatabase() const
Database* Query::getDatabase() const
{
    return database;
}

const string& Query::getBaseTable() const
{
    return baseTable;
}

const vector<string>& Query::getSelectedColumns() const
const Vector<string>& Query::getSelectedColumns() const
{
    return selectedColumns;
}

const vector<Join>& Query::getJoins() const
const Vector<Join>& Query::getJoins() const
{
    return joins;
}

