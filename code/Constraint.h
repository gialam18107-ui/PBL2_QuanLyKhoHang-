#ifndef CONSTRAINT_H
#define CONSTRAINT_H

#include <string>
#include <vector>
#include "Record.h"

using namespace std;

enum class ConstraintType
{
    PRIMARY_KEY,
    UNIQUE,
    FOREIGN_KEY
};

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
    ) const = 0;
};


// ============================================================
// PRIMARY KEY
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

    string getColumnName() const;

    bool validate(
        const Record& record,
        const vector<Record>& existingRecords,
        const vector<Record>& referencedRecords
    ) const override;
};


// ============================================================
// UNIQUE
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

    string getColumnName() const;

    bool validate(
        const Record& record,
        const vector<Record>& existingRecords,
        const vector<Record>& referencedRecords
    ) const override;
};


// ============================================================
// FOREIGN KEY
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

    string getColumnName() const;

    string getReferenceTable() const;

    string getReferenceColumn() const;

    bool validate(
        const Record& record,
        const vector<Record>& existingRecords,
        const vector<Record>& referencedRecords
    ) const override;
};

#endif
