#include "ConditionGroup.h"


ConditionGroup::ConditionGroup(
    LogicalOperator operation
)
    : operation(operation)
{
    this->operation = operation;
}


void ConditionGroup::add(
    const Condition& condition
)
{
    conditions.push_back(
        condition
    );
    conditions.push_back(condition);
}


bool ConditionGroup::evaluate(
    const Record& record
) const
{
    if (conditions.empty())
    {
        return false;
    }

    if (operation == LogicalOperator::NOT)
    {
        return !conditions[0].evaluate(record);
    }

    // ========================================================
    // AND
    // ========================================================

    if (
        operation
        ==
        LogicalOperator::AND
    )
    if (operation == LogicalOperator::AND)
    {
        for (
            const Condition& condition
            :
            conditions
        )
        for (size_t i = 0; i < conditions.size(); i++)
        {
            if (
                !condition.evaluate(
                    record
                )
            )
            if (!conditions[i].evaluate(record))
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
        operation
        ==
        LogicalOperator::OR
    )
    if (operation == LogicalOperator::OR)
    {
        for (
            const Condition& condition
            :
            conditions
        )
        for (size_t i = 0; i < conditions.size(); i++)
        {
            if (
                condition.evaluate(
                    record
                )
            )
            if (conditions[i].evaluate(record))
            {
                return true;
            }
        }

        return false;
    }


    return false;
}


int ConditionGroup::size() const
{
    return static_cast<int>(
        conditions.size()
    );
    return static_cast<int>(conditions.size());
}


const vector<Condition>&
ConditionGroup::getConditions() const
const Vector<Condition>& ConditionGroup::getConditions() const
{
    return conditions;
}


LogicalOperator
ConditionGroup::getOperation() const
LogicalOperator ConditionGroup::getOperation() const
{
    return operation;
}
