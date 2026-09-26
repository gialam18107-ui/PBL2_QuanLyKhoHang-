#ifndef JOIN_H
#define JOIN_H

#include <string>

using namespace std;

enum class JoinType
{
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

    /**
     * @brief Construct a table Join specification.
     * @param leftTable Left table name (usually base table or previously joined table).
     * @param leftColumn Left join key column.
     * @param rightTable Right table name being joined.
     * @param rightColumn Right join key column.
     * @param type Join type (JoinType::INNER, LEFT, RIGHT, FULL).
     * @example Join("product", "brand", "brand", "brand", JoinType::INNER)
     */
    Join(
        const string& leftTable,
        const string& leftColumn,
        const string& rightTable,
        const string& rightColumn,
        JoinType type = JoinType::INNER
    );

    /// Get left table name.
    string getLeftTable() const;

    /// Get left key column name.
    string getLeftColumn() const;

    /// Get right table name.
    string getRightTable() const;

    /// Get right key column name.
    string getRightColumn() const;

    /// Get join type enum.
    JoinType getType() const;
};

#endif
