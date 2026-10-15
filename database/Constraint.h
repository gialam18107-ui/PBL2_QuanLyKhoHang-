#ifndef CONSTRAINT_H
#define CONSTRAINT_H

#include <string>
#include "datastructure/vector/Vector.h"
#include "Record.h"
#include "Condition.h"

using namespace std;

enum class ConstraintType
{
    PRIMARY_KEY,
    UNIQUE,
    FOREIGN_KEY,
    NOT_NULL,
    DEFAULT,
    CHECK
};

/**
 * @brief Abstract base class for all table constraints.
 * Implements clone() for deep copying and virtual destructor for safe manual memory management.
 */
class Constraint
{
protected:
    string name;
    ConstraintType type;

public:
    Constraint(
        const string& name,
        ConstraintType type
    );

    virtual ~Constraint() = default;

    virtual Constraint* clone() const = 0;

    string getName() const;
    ConstraintType getType() const;

    /*
        existingRecords:
        Các record hiện có trong chính bảng đang kiểm tra.

        referencedRecords:
        Các record của bảng được tham chiếu.
        PRIMARY KEY và UNIQUE không cần dùng tham số này.
        FOREIGN KEY sẽ dùng nó.
    */
    virtual bool validate(
        const Record& record,
        const Vector<Record>& existingRecords,
        const Vector<Record>& referencedRecords
    ) const = 0;

    virtual bool validateUpdate(
        const Record& oldRecord,
        const Record& newRecord,
        const Vector<Record>& existingRecords,
        const Vector<Record>& referencedRecords
    ) const;
};


// ============================================================
// PRIMARY KEY CONSTRAINT
// ============================================================

/**
 * @brief Enforces uniqueness and non-null values for primary key column.
 */
class PrimaryKeyConstraint : public Constraint
{
private:
    string columnName;

public:
    /**
     * @brief Construct a PrimaryKeyConstraint.
     * @param name Constraint identifier (e.g. "pk_product").
     * @param columnName Primary key column name.
     */
    PrimaryKeyConstraint(
        const string& name,
        const string& columnName
    );

    Constraint* clone() const override;

    /// Get targeted primary key column name.
    string getColumnName() const;

    bool validate(
        const Record& record,
        const Vector<Record>& existingRecords,
        const Vector<Record>& referencedRecords
    ) const override;

    bool validateUpdate(
        const Record& oldRecord,
        const Record& newRecord,
        const Vector<Record>& existingRecords,
        const Vector<Record>& referencedRecords
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
private:
    string columnName;

public:
    /**
     * @brief Construct a UniqueConstraint.
     * @param name Constraint identifier (e.g. "uq_product_code").
     * @param columnName Target column name.
     */
    UniqueConstraint(
        const string& name,
        const string& columnName
    );

    Constraint* clone() const override;

    /// Get targeted unique column name.
    string getColumnName() const;

    bool validate(
        const Record& record,
        const Vector<Record>& existingRecords,
        const Vector<Record>& referencedRecords
    ) const override;

    bool validateUpdate(
        const Record& oldRecord,
        const Record& newRecord,
        const Vector<Record>& existingRecords,
        const Vector<Record>& referencedRecords
    ) const override;
};


// ============================================================
// FOREIGN KEY CONSTRAINT
// ============================================================

/**
 * @brief Enforces referential integrity by requiring column values to exist in referenced parent table.
 */
class ForeignKeyConstraint : public Constraint
{
private:
    string columnName;
    string referenceTable;
    string referenceColumn;

public:
    /**
     * @brief Construct a ForeignKeyConstraint.
     * @param name Constraint identifier (e.g. "fk_product_category").
     * @param columnName Child table column name.
     * @param referenceTable Parent table name.
     * @param referenceColumn Parent referenced column name.
     */
    ForeignKeyConstraint(
        const string& name,
        const string& columnName,
        const string& referenceTable,
        const string& referenceColumn
    );

    Constraint* clone() const override;

    /// Get child column name.
    string getColumnName() const;

    /// Get referenced parent table name.
    string getReferenceTable() const;

    /// Get referenced parent column name.
    string getReferenceColumn() const;

    bool validate(
        const Record& record,
        const Vector<Record>& existingRecords,
        const Vector<Record>& referencedRecords
    ) const override;

    bool validateUpdate(
        const Record& oldRecord,
        const Record& newRecord,
        const Vector<Record>& existingRecords,
        const Vector<Record>& referencedRecords
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
private:
    string columnName;

public:
    /**
     * @brief Construct a NotNullConstraint.
     */
    NotNullConstraint(
        const string& name,
        const string& columnName
    );

    Constraint* clone() const override;

    /// Get targeted column name.
    string getColumnName() const;

    bool validate(
        const Record& record,
        const Vector<Record>& existingRecords,
        const Vector<Record>& referencedRecords
    ) const override;
};

// ============================================================
// DEFAULT CONSTRAINT
// ============================================================

/**
 * @brief Specifies a default value when a column is omitted during insertion.
 */
class DefaultConstraint : public Constraint
{
private:
    string columnName;
    Value defaultValue;

public:
    /**
     * @brief Construct a DefaultConstraint.
     */
    DefaultConstraint(
        const string& name,
        const string& columnName,
        const Value& defaultValue
    );

    Constraint* clone() const override;

    /// Get targeted column name.
    string getColumnName() const;

    /// Get specified default value.
    const Value& getDefaultValue() const;

    bool validate(
        const Record& record,
        const Vector<Record>& existingRecords,
        const Vector<Record>& referencedRecords
    ) const override;
};

// ============================================================
// CHECK CONSTRAINT
// ============================================================

/**
 * @brief Enforces a logical condition on row values (e.g. price > 0, rating >= 1).
 */
class CheckConstraint : public Constraint
{
private:
    Condition condition;

public:
    /**
     * @brief Construct a CheckConstraint.
     * @param name Constraint identifier.
     * @param condition Condition that all rows must satisfy.
     */
    CheckConstraint(
        const string& name,
        const Condition& condition
    );

    Constraint* clone() const override;

    /// Get the validation condition.
    const Condition& getCondition() const;

    bool validate(
        const Record& record,
        const Vector<Record>& existingRecords,
        const Vector<Record>& referencedRecords
    ) const override;
};

#endif // CONSTRAINT_H
