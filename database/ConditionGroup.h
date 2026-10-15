#ifndef CONDITION_GROUP_H
#define CONDITION_GROUP_H

#include "datastructure/vector/Vector.h"
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
    /**
     * @brief Construct a new ConditionGroup with specified logical operation (AND, OR, NOT).
     */
    ConditionGroup(
        LogicalOperator operation = LogicalOperator::AND
    );

    /**
     * @brief Append a Condition to the group.
     * @param condition Filter condition to add.
     */
    void add(
        const Condition& condition
    );

    /**
     * @brief Evaluate the combined condition group against a record.
     * @return true if record satisfies the group logic.
     */
    bool evaluate(
        const Record& record
    ) const;

    /// Get number of conditions in this group.
    int size() const;

    /// Get list of conditions.
    const Vector<Condition>& getConditions() const;

    /// Get logical operator enum (AND, OR, NOT).
    LogicalOperator getOperation() const;
};

#endif
