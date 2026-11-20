#ifndef CONSTRAINT_H
#define CONSTRAINT_H

#include <string>
#include "Record.h"

using namespace std;

class Table;

enum class ConstraintType
{
    PRIMARY_KEY,
    UNIQUE,
    FOREIGN_KEY,
    NOT_NULL,
    CHECK
};

/**
 * @brief Abstract base class for all column/table constraints.
 * Implements clone() for deep copying and virtual destructor for safe manual memory management.
 */
class Constraint
{
protected:
    string name;
    string columnName;
    ConstraintType type;

public:
    Constraint(
        const string& name,
        const string& columnName,
        ConstraintType type
    );

    virtual ~Constraint() = default;

    virtual Constraint* clone() const = 0;

    const string& getName() const;
    const string& getColumnName() const;
    ConstraintType getType() const;

    /**
     * @brief Validate a single column value against this constraint on the given table.
     * @param value Value of target column being inserted or updated.
     * @param table Owning Table (for index lookup and Database access).
     * @param ignoreRowIndex Physical row index to ignore on update (-1 for insert).
     * @return true if constraint is satisfied.
     */
    virtual bool validate(
        const Value& value,
        const Table& table,
        int ignoreRowIndex = -1
    ) const = 0;
};


// ============================================================
// PRIMARY KEY CONSTRAINT
// ============================================================

/**
 * @brief Enforces uniqueness and non-null values for primary key column.
 */
class PrimaryKeyConstraint : public Constraint
{
public:
    PrimaryKeyConstraint(
        const string& name,
        const string& columnName
    );

    Constraint* clone() const override;

    bool validate(
        const Value& value,
        const Table& table,
        int ignoreRowIndex = -1
    ) const override;
};


// ============================================================
// UNIQUE CONSTRAINT
// ============================================================

/**
 * @brief Enforces that all non-null values in the target column are distinct.
 */
class UniqueConstraint : public Constraint
{
public:
    UniqueConstraint(
        const string& name,
        const string& columnName
    );

    Constraint* clone() const override;

    bool validate(
        const Value& value,
        const Table& table,
        int ignoreRowIndex = -1
    ) const override;
};


// ============================================================
// FOREIGN KEY CONSTRAINT
// ============================================================

/**
 * @brief Enforces referential integrity by requiring non-null column values to exist in referenced parent table.
 */
class ForeignKeyConstraint : public Constraint
{
private:
    string referenceTable;
    string referenceColumn;

public:
    ForeignKeyConstraint(
        const string& name,
        const string& columnName,
        const string& referenceTable,
        const string& referenceColumn
    );

    Constraint* clone() const override;

    /// Get referenced parent table name.
    const string& getReferenceTable() const;

    /// Get referenced parent column name.
    const string& getReferenceColumn() const;

    bool validate(
        const Value& value,
        const Table& table,
        int ignoreRowIndex = -1
    ) const override;
};

// ============================================================
// NOT NULL CONSTRAINT
// ============================================================

/**
 * @brief Enforces that a column value cannot be NULL (monostate).
 */
class NotNullConstraint : public Constraint
{
public:
    NotNullConstraint(
        const string& name,
        const string& columnName
    );

    Constraint* clone() const override;

    bool validate(
        const Value& value,
        const Table& table,
        int ignoreRowIndex = -1
    ) const override;
};

#endif // CONSTRAINT_H
