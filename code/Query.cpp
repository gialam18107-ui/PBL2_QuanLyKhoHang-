#include "Query.h"

#include <iostream>
#include <algorithm>

using namespace std;


// ============================================================
// CONSTRUCTOR
// ============================================================

Query::Query(
    Database& database
)
    : database(database)
{
}


// ============================================================
// FROM
// ============================================================

Query& Query::from(
    const string& tableName
)
{
    baseTable = tableName;

    return *this;
}


// ============================================================
// SELECT
// ============================================================

Query& Query::select(
    const vector<string>& columns
)
{
    selectedColumns = columns;

    return *this;
}


Query& Query::selectAll()
{
    selectedColumns.clear();

    return *this;
}


// ============================================================
// JOIN
// ============================================================

Query& Query::join(
    const Join& joinObject
)
{
    joins.push_back(
        joinObject
    );

    return *this;
}


// ============================================================
// WHERE
// ============================================================

Query& Query::where(
    const Condition& condition
)
{
    singleCondition = condition;

    conditionGroup.reset();

    return *this;
}


Query& Query::where(
    const ConditionGroup& group
)
{
    conditionGroup = group;

    singleCondition.reset();

    return *this;
}


// ============================================================
// ORDER BY
// ============================================================

Query& Query::orderBy(
    const OrderBy& orderByObject
)
{
    order = orderByObject;

    return *this;
}


// ============================================================
// COMPARE VALUES
// ============================================================

bool Query::compareValues(
    const Value& leftValue,
    const Value& rightValue,
    CompareOperator operation
) const
{
    /*
        Hai giá trị phải cùng kiểu.
    */

    if (
        leftValue.index()
        !=
        rightValue.index()
    )
    {
        return false;
    }


    // ========================================================
    // INTEGER
    // ========================================================

    if (
        holds_alternative<int>(leftValue)
    )
    {
        int left =
            get<int>(leftValue);

        int right =
            get<int>(rightValue);

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
    }


    // ========================================================
    // DOUBLE
    // ========================================================

    if (
        holds_alternative<double>(leftValue)
    )
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
    }


    // ========================================================
    // STRING
    // ========================================================

    if (
        holds_alternative<string>(leftValue)
    )
    {
        const string& left =
            get<string>(leftValue);

        const string& right =
            get<string>(rightValue);

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
    }

    return false;
}


// ============================================================
// EVALUATE CONDITION
// ============================================================

bool Query::evaluateCondition(
    const JoinedRecord& record,
    const Condition& condition
) const
{
    const string& columnName =
        condition.getColumnName();


    if (
        !record.has(columnName)
    )
    {
        return false;
    }


    Value recordValue =
        record.get(columnName);

    Value compareValue =
        condition.getCompareValue();


    return compareValues(
        recordValue,
        compareValue,
        condition.getOperation()
    );
}


// ============================================================
// EVALUATE CONDITION GROUP
// ============================================================

bool Query::evaluateConditionGroup(
    const JoinedRecord& record,
    const ConditionGroup& group
) const
{
    const vector<Condition>& conditions =
        group.getConditions();


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
            {
                return false;
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
        {
            if (
                evaluateCondition(
                    record,
                    condition
                )
            )
            {
                return true;
            }
        }

        return false;
    }


    return false;
}


// ============================================================
// BUILD JOINED RECORDS
// ============================================================

vector<JoinedRecord>
Query::buildJoinedRecords() const
{
    vector<JoinedRecord> result;


    // ========================================================
    // FROM
    // ========================================================

    if (baseTable.empty())
    {
        cout
            << "Query failed: "
            << "FROM table is empty."
            << endl;

        return {};
    }


    auto base =
        database.getTable(
            baseTable
        );

    if (base == nullptr)
    {
        cout
            << "Query failed: "
            << "table not found: "
            << baseTable
            << endl;

        return {};
    }


    // ========================================================
    // TẠO RECORD BAN ĐẦU
    // ========================================================

    for (
        const Record& sourceRecord
        :
        base->getRows()
    )
    {
        JoinedRecord joinedRecord;


        for (
            const auto& pair
            :
            sourceRecord.getValues()
        )
        {
            joinedRecord.set(
                baseTable
                + "."
                + pair.first,
                pair.second
            );
        }


        result.push_back(
            joinedRecord
        );
    }


    // ========================================================
    // XỬ LÝ JOIN
    // ========================================================

    for (
        const Join& joinObject
        :
        joins
    )
    {
        if (
            joinObject.getType()
            !=
            JoinType::INNER
        )
        {
            continue;
        }


        auto rightTable =
            database.getTable(
                joinObject.getRightTable()
            );

        if (rightTable == nullptr)
        {
            cout
                << "Query failed: "
                << "table not found: "
                << joinObject.getRightTable()
                << endl;

            return {};
        }


        if (
            !rightTable->hasColumn(
                joinObject.getRightColumn()
            )
        )
        {
            cout
                << "Query failed: "
                << "column not found: "
                << joinObject.getRightTable()
                << "."
                << joinObject.getRightColumn()
                << endl;

            return {};
        }


        vector<JoinedRecord> joinedResult;


        for (
            const JoinedRecord& currentRecord
            :
            result
        )
        {
            string leftKey =
                joinObject.getLeftTable()
                + "."
                + joinObject.getLeftColumn();


            if (
                !currentRecord.has(leftKey)
            )
            {
                continue;
            }


            Value leftValue =
                currentRecord.get(leftKey);


            for (
                const Record& rightRecord
                :
                rightTable->getRows()
            )
            {
                if (
                    !rightRecord.has(
                        joinObject.getRightColumn()
                    )
                )
                {
                    continue;
                }


                Value rightValue =
                    rightRecord.get(
                        joinObject.getRightColumn()
                    );


                if (
                    leftValue.index()
                    !=
                    rightValue.index()
                )
                {
                    continue;
                }


                if (
                    leftValue != rightValue
                )
                {
                    continue;
                }


                // --------------------------------------------
                // GHÉP RECORD
                // --------------------------------------------

                JoinedRecord newRecord =
                    currentRecord;


                for (
                    const auto& pair
                    :
                    rightRecord.getValues()
                )
                {
                    newRecord.set(
                        joinObject.getRightTable()
                        + "."
                        + pair.first,
                        pair.second
                    );
                }


                joinedResult.push_back(
                    newRecord
                );
            }
        }


        result =
            joinedResult;
    }


    return result;
}


// ============================================================
// APPLY WHERE
// ============================================================

vector<JoinedRecord>
Query::applyWhere(
    const vector<JoinedRecord>& records
) const
{
    if (
        !singleCondition.has_value()
        &&
        !conditionGroup.has_value()
    )
    {
        return records;
    }


    vector<JoinedRecord> result;


    for (
        const JoinedRecord& record
        :
        records
    )
    {
        bool matched = false;


        if (
            singleCondition.has_value()
        )
        {
            matched =
                evaluateCondition(
                    record,
                    singleCondition.value()
                );
        }
        else
        {
            matched =
                evaluateConditionGroup(
                    record,
                    conditionGroup.value()
                );
        }


        if (matched)
        {
            result.push_back(
                record
            );
        }
    }


    return result;
}


// ============================================================
// APPLY ORDER BY
// ============================================================

vector<JoinedRecord>
Query::applyOrderBy(
    vector<JoinedRecord> records
) const
{
    if (!order.has_value())
    {
        return records;
    }


    const string& columnName =
        order->getColumnName();


    sort(
        records.begin(),
        records.end(),
        [this, &columnName](
            const JoinedRecord& a,
            const JoinedRecord& b
        )
        {
            if (
                !a.has(columnName)
                ||
                !b.has(columnName)
            )
            {
                return false;
            }


            Value valueA =
                a.get(columnName);

            Value valueB =
                b.get(columnName);


            if (
                valueA.index()
                !=
                valueB.index()
            )
            {
                return false;
            }


            bool ascending =
                order->getOrder()
                ==
                SortOrder::ASC;


            if (
                holds_alternative<int>(
                    valueA
                )
            )
            {
                int aValue =
                    get<int>(valueA);

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
            {
                double aValue =
                    get<double>(valueA);

                double bValue =
                    get<double>(valueB);

                return ascending
                    ? aValue < bValue
                    : aValue > bValue;
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
            }


            return false;
        }
    );


    return records;
}


// ============================================================
// APPLY SELECT
// ============================================================

vector<JoinedRecord>
Query::applySelect(
    const vector<JoinedRecord>& records
) const
{
    /*
        SELECT *.
    */

    if (selectedColumns.empty())
    {
        return records;
    }


    vector<JoinedRecord> result;


    for (
        const JoinedRecord& sourceRecord
        :
        records
    )
    {
        JoinedRecord selectedRecord;


        for (
            const string& columnName
            :
            selectedColumns
        )
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
        }


        result.push_back(
            selectedRecord
        );
    }


    return result;
}


// ============================================================
// EXECUTE
// ============================================================

vector<JoinedRecord>
Query::execute() const
{
    /*
        1. FROM + JOIN
    */

    vector<JoinedRecord> result =
        buildJoinedRecords();


    /*
        2. WHERE
    */

    result =
        applyWhere(result);


    /*
        3. ORDER BY
    */

    result =
        applyOrderBy(result);


    /*
        4. SELECT
    */

    result =
        applySelect(result);


    return result;
}
