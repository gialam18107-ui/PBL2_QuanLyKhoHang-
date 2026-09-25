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
    {
        for (size_t index = 0; index < conditions.size(); ++index)
        {
            if (!conditions[index].evaluate(record))
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
        for (size_t index = 0; index < conditions.size(); ++index)
        {
            if (conditions[index].evaluate(record))
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
    return static_cast<int>(conditions.size());
}


const Vector<Condition>&
ConditionGroup::getConditions() const
{
    return conditions;
}


LogicalOperator
ConditionGroup::getOperation() const
{
    return operation;
}
