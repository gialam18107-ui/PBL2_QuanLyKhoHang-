#ifndef AGGREGATE_H
#define AGGREGATE_H

#include <string>
#include "Vector.h"
#include "Record.h"
#include "JoinedRecord.h"

using namespace std;

enum class AggregateType
{
    COUNT,
    SUM,
    AVG,
    MIN,
    MAX
};

/**
 * @brief Represents an aggregate function specification in a SELECT or GROUP BY query.
 */
class Aggregate
{
private:
    AggregateType type;
    string column;
    string alias;

public:
    Aggregate() = default;

    Aggregate(
        AggregateType type,
        const string& column,
        const string& alias = ""
    );

    AggregateType getType() const;
    string getColumn() const;
    string getAlias() const;
    string getOutputName() const;

    /**
     * @brief Compute the aggregate value across a set of records.
     */
    static Value compute(
        AggregateType type,
        const string& column,
        const Vector<JoinedRecord>& records
    );
};

#endif // AGGREGATE_H
