#include "Constraint.h"

Constraint::Constraint(
    const string& name,
    ConstraintType type
)
    : name(name), type(type)
{
}

string Constraint::getName() const { return name; }
ConstraintType Constraint::getType() const { return type; }

bool Constraint::validateUpdate(
    const Record&,
    const Record& newRecord,
    const Vector<Record>& existingRecords,
    const Vector<Record>& referencedRecords
) const
{
    return validate(newRecord, existingRecords, referencedRecords);
}

PrimaryKeyConstraint::PrimaryKeyConstraint(
    const string& name,
    const string& columnName
)
    : Constraint(name, ConstraintType::PRIMARY_KEY), columnName(columnName)
{
}

Constraint* PrimaryKeyConstraint::clone() const { return new PrimaryKeyConstraint(*this); }
string PrimaryKeyConstraint::getColumnName() const { return columnName; }

bool PrimaryKeyConstraint::validate(
    const Record& record,
    const Vector<Record>& existingRecords,
    const Vector<Record>&
) const
{
    if (!record.has(columnName) || isNull(record.get(columnName))) return false;
    Value value = record.get(columnName);
    for (size_t index = 0; index < existingRecords.size(); ++index)
    {
        const Record& existing = existingRecords[index];
        if (existing.has(columnName) && existing.get(columnName) == value) return false;
    }
    return true;
}

bool PrimaryKeyConstraint::validateUpdate(
    const Record& oldRecord,
    const Record& newRecord,
    const Vector<Record>& existingRecords,
    const Vector<Record>&
) const
{
    if (!newRecord.has(columnName) || isNull(newRecord.get(columnName))) return false;
    Value value = newRecord.get(columnName);
    for (size_t index = 0; index < existingRecords.size(); ++index)
    {
        const Record& existing = existingRecords[index];
        if (existing != oldRecord && existing.has(columnName) && existing.get(columnName) == value) return false;
    }
    return true;
}

UniqueConstraint::UniqueConstraint(
    const string& name,
    const string& columnName
)
    : Constraint(name, ConstraintType::UNIQUE), columnName(columnName)
{
}

Constraint* UniqueConstraint::clone() const { return new UniqueConstraint(*this); }
string UniqueConstraint::getColumnName() const { return columnName; }

bool UniqueConstraint::validate(
    const Record& record,
    const Vector<Record>& existingRecords,
    const Vector<Record>&
) const
{
    if (!record.has(columnName) || isNull(record.get(columnName))) return true;
    Value value = record.get(columnName);
    if (holds_alternative<string>(value) && get<string>(value).empty()) return true;
    for (size_t index = 0; index < existingRecords.size(); ++index)
    {
        const Record& existing = existingRecords[index];
        if (existing.has(columnName) && existing.get(columnName) == value) return false;
    }
    return true;
}

bool UniqueConstraint::validateUpdate(
    const Record& oldRecord,
    const Record& newRecord,
    const Vector<Record>& existingRecords,
    const Vector<Record>&
) const
{
    if (!newRecord.has(columnName) || isNull(newRecord.get(columnName))) return true;
    Value value = newRecord.get(columnName);
    if (holds_alternative<string>(value) && get<string>(value).empty()) return true;
    for (size_t index = 0; index < existingRecords.size(); ++index)
    {
        const Record& existing = existingRecords[index];
        if (existing != oldRecord && existing.has(columnName) && existing.get(columnName) == value) return false;
    }
    return true;
}

ForeignKeyConstraint::ForeignKeyConstraint(
    const string& name,
    const string& columnName,
    const string& referenceTable,
    const string& referenceColumn
)
    : Constraint(name, ConstraintType::FOREIGN_KEY),
      columnName(columnName),
      referenceTable(referenceTable),
      referenceColumn(referenceColumn)
{
}

Constraint* ForeignKeyConstraint::clone() const { return new ForeignKeyConstraint(*this); }
string ForeignKeyConstraint::getColumnName() const { return columnName; }
string ForeignKeyConstraint::getReferenceTable() const { return referenceTable; }
string ForeignKeyConstraint::getReferenceColumn() const { return referenceColumn; }

bool ForeignKeyConstraint::validate(
    const Record& record,
    const Vector<Record>&,
    const Vector<Record>& referencedRecords
) const
{
    if (!record.has(columnName) || isNull(record.get(columnName))) return true;
    Value value = record.get(columnName);
    for (size_t index = 0; index < referencedRecords.size(); ++index)
    {
        const Record& referenced = referencedRecords[index];
        if (referenced.has(referenceColumn) && referenced.get(referenceColumn) == value) return true;
    }
    return false;
}

bool ForeignKeyConstraint::validateUpdate(
    const Record&,
    const Record& newRecord,
    const Vector<Record>&,
    const Vector<Record>& referencedRecords
) const
{
    return validate(newRecord, Vector<Record>(), referencedRecords);
}

NotNullConstraint::NotNullConstraint(
    const string& name,
    const string& columnName
)
    : Constraint(name, ConstraintType::NOT_NULL), columnName(columnName)
{
}

Constraint* NotNullConstraint::clone() const { return new NotNullConstraint(*this); }
string NotNullConstraint::getColumnName() const { return columnName; }

bool NotNullConstraint::validate(
    const Record& record,
    const Vector<Record>&,
    const Vector<Record>&
) const
{
    return record.has(columnName) && !isNull(record.get(columnName));
}

DefaultConstraint::DefaultConstraint(
    const string& name,
    const string& columnName,
    const Value& defaultValue
)
    : Constraint(name, ConstraintType::DEFAULT), columnName(columnName), defaultValue(defaultValue)
{
}

Constraint* DefaultConstraint::clone() const { return new DefaultConstraint(*this); }
string DefaultConstraint::getColumnName() const { return columnName; }
const Value& DefaultConstraint::getDefaultValue() const { return defaultValue; }

bool DefaultConstraint::validate(const Record&, const Vector<Record>&, const Vector<Record>&) const
{
    return true;
}
