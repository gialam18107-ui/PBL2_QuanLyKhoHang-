#ifndef CONDITION_GROUP_H
#define CONDITION_GROUP_H

#include "Vector.h"
#include "Condition.h"

using namespace std;

enum class LogicalOperator
{
    AND,
    OR,
    NOT
};

/**
 * @brief Represents a group of conditions combined with a logical operator (AND, OR, NOT).
 * Uses custom Vector instead of std::vector.
 */
class ConditionGroup
{
private:
    Vector<Condition> conditions;
    LogicalOperator operation;

public:

    ConditionGroup(
        LogicalOperator operation = LogicalOperator::AND
    );

    void add(
        const Condition& condition
    );

    bool evaluate(
        const Record& record
    ) const;

    int size() const;

    const Vector<Condition>& getConditions() const;

    LogicalOperator getOperation() const;
};

#endif
