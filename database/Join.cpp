#include "Join.h"

Join::Join(
    const string& leftTable,
    const string& leftColumn,
    const string& rightTable,
    const string& rightColumn,
    JoinType type
)
{
    this->leftTable = leftTable;
    this->leftColumn = leftColumn;

    this->rightTable = rightTable;
    this->rightColumn = rightColumn;

    this->type = type;
}

string Join::getLeftTable() const
{
    return leftTable;
}

string Join::getLeftColumn() const
{
    return leftColumn;
}

string Join::getRightTable() const
{
    return rightTable;
}

string Join::getRightColumn() const
{
    return rightColumn;
}

JoinType Join::getType() const
{
    return type;
}
