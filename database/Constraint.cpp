#include "Constraint.h"


// ============================================================
// CONSTRAINT
// ============================================================

Constraint::Constraint(
    const string& name,
    ConstraintType type
)
    : name(name), type(type)
{
    this->name = name;
    this->type = type;
}

string Constraint::getName() const
{
    return name;
}

ConstraintType Constraint::getType() const
{
    return type;
}

bool Constraint::validateUpdate(
    const Record& /*oldRecord*/,
    const Record& newRecord,
    const Vector<Record>& existingRecords,
    const Vector<Record>& referencedRecords
) const
{
    return validate(newRecord, existingRecords, referencedRecords);
}

// ============================================================
// PRIMARY KEY
// ============================================================

PrimaryKeyConstraint::PrimaryKeyConstraint(
    const string& name,
    const string& columnName
)
    : Constraint(name, ConstraintType::PRIMARY_KEY)
    : Constraint(name, ConstraintType::PRIMARY_KEY), columnName(columnName)
{
    this->columnName = columnName;
}

Constraint* PrimaryKeyConstraint::clone() const
{
    return new PrimaryKeyConstraint(*this);
}

string PrimaryKeyConstraint::getColumnName() const
{
    return columnName;
}

bool PrimaryKeyConstraint::validate(
    const Record& record,
    const vector<Record>& existingRecords,
    const vector<Record>& referencedRecords
    const Vector<Record>& existingRecords,
    const Vector<Record>& /*referencedRecords*/
) const
{
    // PRIMARY KEY bắt buộc phải tồn tại.
    if (!record.has(columnName))
    {
        return false;
    }

    Value value = record.get(columnName);
    if (isNull(value))
    {
        return false;
    }

    // Kiểm tra trùng với các record hiện tại.
    for (const Record& existingRecord : existingRecords)
    for (size_t i = 0; i < existingRecords.size(); i++)
    {
        const Record& existingRecord = existingRecords[i];
        if (!existingRecord.has(columnName))
        {
            continue;
        }

        Value existingValue = existingRecord.get(columnName);

        if (value == existingValue)
        if (value == existingRecord.get(columnName))
        {
            return false;
        }
    }

    return true;
}

bool PrimaryKeyConstraint::validateUpdate(
    const Record& oldRecord,
    const Record& newRecord,
    const Vector<Record>& existingRecords,
    const Vector<Record>& referencedRecords
) const
{
    if (!newRecord.has(columnName))
    {
        return false;
    }

    Value newVal = newRecord.get(columnName);
    if (isNull(newVal))
    {
        return false;
    }

    if (oldRecord.has(columnName) && oldRecord.get(columnName) == newVal)
    {
        return true;
    }

    return validate(newRecord, existingRecords, referencedRecords);
}

// ============================================================
// UNIQUE
// ============================================================

UniqueConstraint::UniqueConstraint(
    const string& name,
    const string& columnName
)
    : Constraint(name, ConstraintType::UNIQUE)
    : Constraint(name, ConstraintType::UNIQUE), columnName(columnName)
{
    this->columnName = columnName;
}

Constraint* UniqueConstraint::clone() const
{
    return new UniqueConstraint(*this);
}

string UniqueConstraint::getColumnName() const
{
    return columnName;
}

bool UniqueConstraint::validate(
    const Record& record,
    const vector<Record>& existingRecords,
    const vector<Record>& referencedRecords
    const Vector<Record>& existingRecords,
    const Vector<Record>& /*referencedRecords*/
) const
{
    // UNIQUE không bắt buộc phải có giá trị.
    // NOT NULL sẽ xử lý trường hợp thiếu dữ liệu.
    if (!record.has(columnName))
    {
        return true;
    }

    Value value = record.get(columnName);
    if (isNull(value))
    {
        return true;
    }

    for (const Record& existingRecord : existingRecords)
    for (size_t i = 0; i < existingRecords.size(); i++)
    {
        const Record& existingRecord = existingRecords[i];
        if (!existingRecord.has(columnName))
        {
            continue;
        }

        Value existingValue = existingRecord.get(columnName);

        if (value == existingValue)
        if (value == existingRecord.get(columnName))
        {
            return false;
        }
    }

    return true;
}

bool UniqueConstraint::validateUpdate(
    const Record& oldRecord,
    const Record& newRecord,
    const Vector<Record>& existingRecords,
    const Vector<Record>& referencedRecords
) const
{
    if (!newRecord.has(columnName))
    {
        return true;
    }

    Value newVal = newRecord.get(columnName);
    if (oldRecord.has(columnName) && oldRecord.get(columnName) == newVal)
    {
        return true;
    }

    return validate(newRecord, existingRecords, referencedRecords);
}

// ============================================================
// FOREIGN KEY
// ============================================================

ForeignKeyConstraint::ForeignKeyConstraint(
    const string& name,
    const string& columnName,
    const string& referenceTable,
    const string& referenceColumn
)
    : Constraint(name, ConstraintType::FOREIGN_KEY)
    : Constraint(name, ConstraintType::FOREIGN_KEY),
      columnName(columnName),
      referenceTable(referenceTable),
      referenceColumn(referenceColumn)
{
    this->columnName = columnName;
    this->referenceTable = referenceTable;
    this->referenceColumn = referenceColumn;
}

Constraint* ForeignKeyConstraint::clone() const
{
    return new ForeignKeyConstraint(*this);
}

string ForeignKeyConstraint::getColumnName() const
{
    return columnName;
}

string ForeignKeyConstraint::getReferenceTable() const
{
    return referenceTable;
}

string ForeignKeyConstraint::getReferenceColumn() const
{
    return referenceColumn;
}

bool ForeignKeyConstraint::validate(
    const Record& record,
    const vector<Record>& existingRecords,
    const vector<Record>& referencedRecords
    const Vector<Record>& /*existingRecords*/,
    const Vector<Record>& referencedRecords
) const
{
    /*
        Nếu record không có FK column,
        ta để NOT NULL xử lý trường hợp này.

        Ví dụ:
        PRODUCT.category_id là NOT NULL
        → Table/Database sẽ kiểm tra NOT NULL.

        Còn FK chỉ quan tâm:
        category_id có tồn tại trong CATEGORY không?
    */
    if (!record.has(columnName))
    {
        return true;
    }

    Value foreignKeyValue = record.get(columnName);
    Value value = record.get(columnName);
    if (isNull(value))
    {
        return true;
    }

    /*
        Tìm giá trị FK trong bảng được tham chiếu.
    for (size_t i = 0; i < referencedRecords.size(); i++)
    {
        const Record& refRecord = referencedRecords[i];
        if (!refRecord.has(referenceColumn))
        {
            continue;
        }

        Ví dụ:
        if (value == refRecord.get(referenceColumn))
        {
            return true;
        }
    }

        PRODUCT.category_id = 2
    return false;
}

        CATEGORY:
        category_id
        1
        2  <- tìm thấy
        3
    */
bool ForeignKeyConstraint::validateUpdate(
    const Record& /*oldRecord*/,
    const Record& newRecord,
    const Vector<Record>& /*existingRecords*/,
    const Vector<Record>& referencedRecords
) const
{
    if (!newRecord.has(columnName))
    {
        return true;
    }

    for (const Record& referencedRecord : referencedRecords)
    Value value = newRecord.get(columnName);
    if (isNull(value))
    {
        if (!referencedRecord.has(referenceColumn))
        return true;
    }

    for (size_t i = 0; i < referencedRecords.size(); i++)
    {
        const Record& refRecord = referencedRecords[i];
        if (!refRecord.has(referenceColumn))
        {
            continue;
        }

        Value referencedValue =
            referencedRecord.get(referenceColumn);

        if (foreignKeyValue == referencedValue)
        if (value == refRecord.get(referenceColumn))
        {
            // FK hợp lệ
            return true;
        }
    }

    /*
        Duyệt hết bảng cha nhưng không tìm thấy
        → FOREIGN KEY không hợp lệ.
    */
    return false;
}

// ============================================================
// NOT NULL
// ============================================================

NotNullConstraint::NotNullConstraint(
    const string& name,
    const string& columnName
)
    : Constraint(name, ConstraintType::NOT_NULL), columnName(columnName)
{
}

Constraint* NotNullConstraint::clone() const
{
    return new NotNullConstraint(*this);
}

string NotNullConstraint::getColumnName() const
{
    return columnName;
}

bool NotNullConstraint::validate(
    const Record& record,
    const Vector<Record>& /*existingRecords*/,
    const Vector<Record>& /*referencedRecords*/
) const
{
    if (!record.has(columnName))
    {
        return false;
    }

    return !isNull(record.get(columnName));
}

// ============================================================
// DEFAULT CONSTRAINT
// ============================================================

DefaultConstraint::DefaultConstraint(
    const string& name,
    const string& columnName,
    const Value& defaultValue
)
    : Constraint(name, ConstraintType::DEFAULT), columnName(columnName), defaultValue(defaultValue)
{
}

Constraint* DefaultConstraint::clone() const
{
    return new DefaultConstraint(*this);
}

string DefaultConstraint::getColumnName() const
{
    return columnName;
}

const Value& DefaultConstraint::getDefaultValue() const
{
    return defaultValue;
}

bool DefaultConstraint::validate(
    const Record& /*record*/,
    const Vector<Record>& /*existingRecords*/,
    const Vector<Record>& /*referencedRecords*/
) const
{
    return true;
}

// ============================================================
// CHECK CONSTRAINT
// ============================================================

CheckConstraint::CheckConstraint(
    const string& name,
    const Condition& condition
)
    : Constraint(name, ConstraintType::CHECK), condition(condition)
{
}

Constraint* CheckConstraint::clone() const
{
    return new CheckConstraint(*this);
}

const Condition& CheckConstraint::getCondition() const
{
    return condition;
}

bool CheckConstraint::validate(
    const Record& record,
    const Vector<Record>& /*existingRecords*/,
    const Vector<Record>& /*referencedRecords*/
) const
{
    return condition.evaluate(record);
}
