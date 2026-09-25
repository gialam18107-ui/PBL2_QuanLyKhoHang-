#ifndef CONSTRAINT_H
#define CONSTRAINT_H

#include <string>
#include <vector>
#include "Vector.h"
#include "Record.h"
#include "Condition.h"

using namespace std;

enum class ConstraintType
{
    PRIMARY_KEY,
    UNIQUE,
    FOREIGN_KEY
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
        const vector<Record>& existingRecords,
        const vector<Record>& referencedRecords
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
// PRIMARY KEY
// PRIMARY KEY CONSTRAINT
// ============================================================

class PrimaryKeyConstraint : public Constraint
{
private:
    string columnName;

public:
    PrimaryKeyConstraint(
        const string& name,
        const string& columnName
    );

    Constraint* clone() const override;

    string getColumnName() const;

    bool validate(
        const Record& record,
        const vector<Record>& existingRecords,
        const vector<Record>& referencedRecords
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
// UNIQUE
// UNIQUE CONSTRAINT
// ============================================================

class UniqueConstraint : public Constraint
{
private:
    string columnName;

public:
    UniqueConstraint(
        const string& name,
        const string& columnName
    );

    Constraint* clone() const override;

    string getColumnName() const;

    bool validate(
        const Record& record,
        const vector<Record>& existingRecords,
        const vector<Record>& referencedRecords
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
// FOREIGN KEY
// FOREIGN KEY CONSTRAINT
// ============================================================

class ForeignKeyConstraint : public Constraint
{
private:
    string columnName;

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

    string getColumnName() const;

    string getReferenceTable() const;

    string getReferenceColumn() const;

    bool validate(
        const Record& record,
        const vector<Record>& existingRecords,
        const vector<Record>& referencedRecords
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

#endif
// ============================================================
// NOT NULL CONSTRAINT
// ============================================================
class NotNullConstraint : public Constraint
{
private:
    string columnName;

public:
    NotNullConstraint(
        const string& name,
        const string& columnName
    );

    Constraint* clone() const override;

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
class DefaultConstraint : public Constraint
{
private:
    string columnName;
    Value defaultValue;

public:
    DefaultConstraint(
        const string& name,
        const string& columnName,
        const Value& defaultValue
    );

    Constraint* clone() const override;

    string getColumnName() const;
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
class CheckConstraint : public Constraint
{
private:
    Condition condition;

public:
    CheckConstraint(
        const string& name,
        const Condition& condition
    );

    Constraint* clone() const override;

    const Condition& getCondition() const;

    bool validate(
        const Record& record,
        const Vector<Record>& existingRecords,
        const Vector<Record>& referencedRecords
    ) const override;
};

#endif // CONSTRAINT_H
