#include "OrderBy.h"

OrderBy::OrderBy(
    const string& columnName,
    SortOrder order
)
{
    this->columnName = columnName;
    this->order = order;
}

string OrderBy::getColumnName() const
{
    return columnName;
}

SortOrder OrderBy::getOrder() const
{
    return order;
}
