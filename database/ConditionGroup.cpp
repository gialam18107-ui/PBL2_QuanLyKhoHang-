#include "ConditionGroup.h"


ConditionGroup::ConditionGroup(
    LogicalOperator operation
)
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
}


bool ConditionGroup::evaluate(
    const Record& record
) const
{
    if (conditions.empty())
    {
        return false;
    }


    // ========================================================
    // AND
    // ========================================================

    if (
        operation
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
                !condition.evaluate(
                    record
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
        operation
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
                condition.evaluate(
                    record
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


int ConditionGroup::size() const
{
    return static_cast<int>(
        conditions.size()
    );
}


const vector<Condition>&
ConditionGroup::getConditions() const
{
    return conditions;
}


LogicalOperator
ConditionGroup::getOperation() const
{
    return operation;
}
