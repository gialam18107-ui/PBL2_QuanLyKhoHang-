#include "Constraint.h"
#include "Table.h"
#include "Database.h"

Constraint::Constraint(
    const string& name,
    const string& columnName,
    ConstraintType type
)
    : name(name), columnName(columnName), type(type)
{
}

const string& Constraint::getName() const { return name; }
const string& Constraint::getColumnName() const { return columnName; }
ConstraintType Constraint::getType() const { return type; }

// ============================================================
// PRIMARY KEY CONSTRAINT
// ============================================================

PrimaryKeyConstraint::PrimaryKeyConstraint(
    const string& name,
    const string& columnName
)
    : Constraint(name, columnName, ConstraintType::PRIMARY_KEY)
{
}

Constraint* PrimaryKeyConstraint::clone() const { return new PrimaryKeyConstraint(*this); }

bool PrimaryKeyConstraint::validate(
    const Value& value,
    const Table& table
) const
{
    if (isNull(value))
    {
        return false;
    }
    return !table.contains(columnName, value);
}

// ============================================================
// UNIQUE CONSTRAINT
// ============================================================

UniqueConstraint::UniqueConstraint(
    const string& name,
    const string& columnName
)
    : Constraint(name, columnName, ConstraintType::UNIQUE)
{
}

Constraint* UniqueConstraint::clone() const { return new UniqueConstraint(*this); }

bool UniqueConstraint::validate(
    const Value& value,
    const Table& table
) const
{
    if (isNull(value))
    {
        return true;
    }
    if (holds_alternative<string>(value) && get<string>(value).empty() && !table.isColumnNotNull(columnName))
    {
        return true;
    }
    return !table.contains(columnName, value);
}

// ============================================================
// FOREIGN KEY CONSTRAINT
// ============================================================

ForeignKeyConstraint::ForeignKeyConstraint(
    const string& name,
    const string& columnName,
    const string& referenceTable,
    const string& referenceColumn
)
    : Constraint(name, columnName, ConstraintType::FOREIGN_KEY),
      referenceTable(referenceTable),
      referenceColumn(referenceColumn)
{
}

Constraint* ForeignKeyConstraint::clone() const { return new ForeignKeyConstraint(*this); }
const string& ForeignKeyConstraint::getReferenceTable() const { return referenceTable; }
const string& ForeignKeyConstraint::getReferenceColumn() const { return referenceColumn; }

bool ForeignKeyConstraint::validate(
    const Value& value,
    const Table& table
) const
{
    if (isNull(value))
    {
        return true;
    }
    const Database* db = table.getDatabase();
    if (db == nullptr)
    {
        return false;
    }
    const Table* refTable = db->getTable(referenceTable);
    if (refTable == nullptr)
    {
        return false;
    }
    return refTable->contains(referenceColumn, value);
}

// ============================================================
// NOT NULL CONSTRAINT
// ============================================================

NotNullConstraint::NotNullConstraint(
    const string& name,
    const string& columnName
)
    : Constraint(name, columnName, ConstraintType::NOT_NULL)
{
}

Constraint* NotNullConstraint::clone() const { return new NotNullConstraint(*this); }

bool NotNullConstraint::validate(
    const Value& value,
    const Table& /*table*/
) const
{
    return !isNull(value);
}
