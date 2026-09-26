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

    /**
     * @brief Construct an Aggregate specification.
     * @param type Aggregate function (COUNT, SUM, AVG, MIN, MAX).
     * @param column Target column to calculate on (or "*" for COUNT).
     * @param alias Output column alias name (e.g. "total_quantity").
     * @example Aggregate(AggregateType::SUM, "quantity", "total_quantity")
     */
    Aggregate(
        AggregateType type,
        const string& column,
        const string& alias = ""
    );

    /// Get aggregate function type enum.
    AggregateType getType() const;

    /// Get target column name.
    string getColumn() const;

    /// Get user-defined alias name.
    string getAlias() const;

    /// Get effective output column name (alias if specified, else e.g. "SUM(quantity)").
    string getOutputName() const;

    /**
     * @brief Compute the aggregate value across a set of records.
     * @param type Function type (COUNT, SUM, AVG, MIN, MAX).
     * @param column Target column.
     * @param records List of JoinedRecord to aggregate over.
     * @return Value Calculated result (int, double, string, or monostate).
     */
    static Value compute(
        AggregateType type,
        const string& column,
        const Vector<JoinedRecord>& records
    );
};

#endif // AGGREGATE_H
