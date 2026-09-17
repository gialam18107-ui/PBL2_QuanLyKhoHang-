#ifndef CONDITION_GROUP_H
#define CONDITION_GROUP_H

#include <vector>

#include "Condition.h"

using namespace std;

enum class LogicalOperator
{
    AND,
    OR
};

class ConditionGroup
{
private:
    vector<Condition> conditions;
    LogicalOperator operation;

public:

    ConditionGroup(
        LogicalOperator operation
    );

    void add(
        const Condition& condition
    );

    bool evaluate(
        const Record& record
    ) const;

    int size() const;

    const vector<Condition>& getConditions() const;

    LogicalOperator getOperation() const;
};

#endif
