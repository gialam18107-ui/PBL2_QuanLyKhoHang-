#ifndef CONDITION_GROUP_H
#define CONDITION_GROUP_H

#include <vector>

#include "Vector.h"
#include "Condition.h"

using namespace std;

enum class LogicalOperator
{
    AND,
    OR
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
    vector<Condition> conditions;
    Vector<Condition> conditions;
    LogicalOperator operation;

public:

    ConditionGroup(
        LogicalOperator operation
        LogicalOperator operation = LogicalOperator::AND
    );

    void add(
        const Condition& condition
    );

    bool evaluate(
        const Record& record
    ) const;

    int size() const;

    const vector<Condition>& getConditions() const;
    const Vector<Condition>& getConditions() const;

    LogicalOperator getOperation() const;
};

#endif
#endif // CONDITION_GROUP_H
