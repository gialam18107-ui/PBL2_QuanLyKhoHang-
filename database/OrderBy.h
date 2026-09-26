#ifndef ORDER_BY_H
#define ORDER_BY_H

#include <string>
#include "Record.h"

using namespace std;

enum class SortOrder
{
    ASC,
    DESC
};

class OrderBy
{
private:
    string columnName;
    SortOrder order;

public:
    /**
     * @brief Construct an OrderBy sort specification.
     * @param columnName Target column to sort by.
     * @param order Sort direction (SortOrder::ASC or SortOrder::DESC).
     * @example OrderBy("product.quantity", SortOrder::DESC)
     */
    OrderBy(
        const string& columnName,
        SortOrder order = SortOrder::ASC
    );

    /// Get column name to sort on.
    string getColumnName() const;

    /// Get sort direction (ASC or DESC).
    SortOrder getOrder() const;
};

#endif

