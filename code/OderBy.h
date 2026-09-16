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
    OrderBy(
        const string& columnName,
        SortOrder order = SortOrder::ASC
    );

    string getColumnName() const;

    SortOrder getOrder() const;
};

#endif

