#include "Constraint.h"


// ============================================================
// CONSTRAINT
// ============================================================

Constraint::Constraint(
    const string& name,
    ConstraintType type
)
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


// ============================================================
// PRIMARY KEY
// ============================================================

PrimaryKeyConstraint::PrimaryKeyConstraint(
    const string& name,
    const string& columnName
)
    : Constraint(name, ConstraintType::PRIMARY_KEY)
{
    this->columnName = columnName;
}

string PrimaryKeyConstraint::getColumnName() const
{
    return columnName;
}

bool PrimaryKeyConstraint::validate(
    const Record& record,
    const vector<Record>& existingRecords,
    const vector<Record>& referencedRecords
) const
{
    // PRIMARY KEY bắt buộc phải tồn tại.
    if (!record.has(columnName))
    {
        return false;
    }

    Value value = record.get(columnName);

    // Kiểm tra trùng với các record hiện tại.
    for (const Record& existingRecord : existingRecords)
    {
        if (!existingRecord.has(columnName))
        {
            continue;
        }

        Value existingValue = existingRecord.get(columnName);

        if (value == existingValue)
        {
            return false;
        }
    }

    return true;
}


// ============================================================
// UNIQUE
// ============================================================

UniqueConstraint::UniqueConstraint(
    const string& name,
    const string& columnName
)
    : Constraint(name, ConstraintType::UNIQUE)
{
    this->columnName = columnName;
}

string UniqueConstraint::getColumnName() const
{
    return columnName;
}

bool UniqueConstraint::validate(
    const Record& record,
    const vector<Record>& existingRecords,
    const vector<Record>& referencedRecords
) const
{
    // UNIQUE không bắt buộc phải có giá trị.
    // NOT NULL sẽ xử lý trường hợp thiếu dữ liệu.
    if (!record.has(columnName))
    {
        return true;
    }

    Value value = record.get(columnName);

    for (const Record& existingRecord : existingRecords)
    {
        if (!existingRecord.has(columnName))
        {
            continue;
        }

        Value existingValue = existingRecord.get(columnName);

        if (value == existingValue)
        {
            return false;
        }
    }

    return true;
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
{
    this->columnName = columnName;
    this->referenceTable = referenceTable;
    this->referenceColumn = referenceColumn;
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

    /*
        Tìm giá trị FK trong bảng được tham chiếu.

        Ví dụ:

        PRODUCT.category_id = 2

        CATEGORY:
        category_id
        1
        2  <- tìm thấy
        3
    */

    for (const Record& referencedRecord : referencedRecords)
    {
        if (!referencedRecord.has(referenceColumn))
        {
            continue;
        }

        Value referencedValue =
            referencedRecord.get(referenceColumn);

        if (foreignKeyValue == referencedValue)
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
