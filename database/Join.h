#ifndef JOIN_H
#define JOIN_H

#include <string>

using namespace std;

enum class JoinType
{
    INNER
    INNER,
    LEFT,
    RIGHT,
    FULL
};

class Join
{
private:
    string leftTable;
    string leftColumn;

    string rightTable;
    string rightColumn;

    JoinType type;

public:
    Join() = default;

    Join(
        const string& leftTable,
        const string& leftColumn,
        const string& rightTable,
        const string& rightColumn,
        JoinType type = JoinType::INNER
    );

    string getLeftTable() const;

    string getLeftColumn() const;

    string getRightTable() const;

    string getRightColumn() const;

    JoinType getType() const;
};

#endif
#endif // JOIN_H
