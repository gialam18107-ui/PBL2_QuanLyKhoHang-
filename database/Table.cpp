#include "Table.h"

#include <iostream>
#include <stdexcept>

using namespace std;


// ============================================================
// CONSTRUCTOR
// ============================================================

Table::Table(const string& name)
{
    this->name = name;
}


// ============================================================
// TABLE INFORMATION
// ============================================================

string Table::getName() const
{
    return name;
}


// ============================================================
// COLUMN
// ============================================================

void Table::addColumn(const Column& column)
{
    if (hasColumn(column.getName()))
    {
        throw runtime_error(
            "Column already exists: "
            + column.getName()
        );
    }

    columns.push_back(column);
}

bool Table::hasColumn(
    const string& columnName
) const
{
    return getColumn(columnName) != nullptr;
}

Column* Table::getColumn(
    const string& columnName
)
{
    for (Column& column : columns)
    {
        if (column.getName() == columnName)
        {
            return &column;
        }
    }

    return nullptr;
}

const Column* Table::getColumn(
    const string& columnName
) const
{
    for (const Column& column : columns)
    {
        if (column.getName() == columnName)
        {
            return &column;
        }
    }

    return nullptr;
}

int Table::getColumnCount() const
{
    return static_cast<int>(
        columns.size()
    );
}

const vector<Column>& Table::getColumns() const
{
    return columns;
}


// ============================================================
// CONSTRAINT
// ============================================================

bool Table::addConstraint(
    const shared_ptr<Constraint>& constraint
)
{
    if (constraint == nullptr)
    {
        return false;
    }

    for (const auto& existingConstraint : constraints)
    {
        if (
            existingConstraint->getName()
            ==
            constraint->getName()
        )
        {
            return false;
        }
    }

    if (
        constraint->getType()
        ==
        ConstraintType::PRIMARY_KEY
    )
    {
        auto pk =
            dynamic_pointer_cast<
                PrimaryKeyConstraint
            >(constraint);

        if (
            pk != nullptr &&
            !hasColumn(pk->getColumnName())
        )
        {
            return false;
        }
    }

    if (
        constraint->getType()
        ==
        ConstraintType::UNIQUE
    )
    {
        auto unique =
            dynamic_pointer_cast<
                UniqueConstraint
            >(constraint);

        if (
            unique != nullptr &&
            !hasColumn(unique->getColumnName())
        )
        {
            return false;
        }
    }

    if (
        constraint->getType()
        ==
        ConstraintType::FOREIGN_KEY
    )
    {
        auto fk =
            dynamic_pointer_cast<
                ForeignKeyConstraint
            >(constraint);

        if (
            fk != nullptr &&
            !hasColumn(fk->getColumnName())
        )
        {
            return false;
        }
    }

    constraints.push_back(constraint);

    return true;
}

const vector<shared_ptr<Constraint>>&
Table::getConstraints() const
{
    return constraints;
}


// ============================================================
// DATA TYPE
// ============================================================

bool Table::checkDataType(
    const Column& column,
    const Value& value
) const
{
    switch (column.getType())
    {
    case DataType::INTEGER:
        return holds_alternative<int>(value);

    case DataType::VARCHAR:
        return holds_alternative<string>(value);

    case DataType::CHAR:
        return holds_alternative<string>(value);

    case DataType::TEXT:
        return holds_alternative<string>(value);

    case DataType::DATE:
        return holds_alternative<string>(value);
    }

    return false;
}


// ============================================================
// RECORD
// ============================================================

bool Table::checkRecord(
    const Record& record
) const
{
    for (const Column& column : columns)
    {
        if (!record.has(column.getName()))
        {
            if (
                column.isNotNull() &&
                column.getDefaultValue().empty()
            )
            {
                cout
                    << "Record check failed: "
                    << column.getName()
                    << " cannot be NULL."
                    << endl;

                return false;
            }

            continue;
        }

        Value value =
            record.get(column.getName());

        if (
            !checkDataType(
                column,
                value
            )
        )
        {
            cout
                << "Record check failed: wrong "
                << "data type for column "
                << column.getName()
                << endl;

            return false;
        }
    }

    return true;
}


// ============================================================
// CONSTRAINT
// ============================================================

bool Table::checkConstraints(
    const Record& record,
    int ignoreRowIndex
) const
{
    /*
        Tạo danh sách record dùng để kiểm tra.

        Khi UPDATE:
        bỏ qua chính record đang được cập nhật.
    */

    vector<Record> existingRecords;

    for (
        int i = 0;
        i < static_cast<int>(rows.size());
        i++
    )
    {
        if (i == ignoreRowIndex)
        {
            continue;
        }

        existingRecords.push_back(
            rows[i]
        );
    }

    for (
        const auto& constraint
        :
        constraints
    )
    {
        /*
            FOREIGN KEY được Database xử lý
            vì cần truy cập bảng khác.
        */

        if (
            constraint->getType()
            ==
            ConstraintType::FOREIGN_KEY
        )
        {
            continue;
        }

        if (
            !constraint->validate(
                record,
                existingRecords,
                {}
            )
        )
        {
            cout
                << "Constraint failed: "
                << constraint->getName()
                << endl;

            return false;
        }
    }

    return true;
}


// ============================================================
// INSERT
// ============================================================

bool Table::insert(
    const Record& record
)
{
    if (!checkRecord(record))
    {
        return false;
    }

    if (!checkConstraints(record))
    {
        return false;
    }

    rows.push_back(record);

    return true;
}


// ============================================================
// UPDATE
// ============================================================

bool Table::update(
    const Condition& condition,
    const Record& newValues
)
{
    bool updated = false;

    for (
        int i = 0;
        i < static_cast<int>(rows.size());
        i++
    )
    {
        if (!condition.evaluate(rows[i]))
        {
            continue;
        }

        /*
            Tạo bản sao của record cũ.
        */

        Record updatedRecord = rows[i];


        /*
            Ghi đè các giá trị mới.
        */

        for (const auto& pair : newValues.getValues())
        {
            updatedRecord.set(
                pair.first,
                pair.second
            );
        }


        /*
            Kiểm tra cấu trúc dữ liệu.
        */

        if (!checkRecord(updatedRecord))
        {
            continue;
        }


        /*
            Kiểm tra PRIMARY KEY / UNIQUE.

            Bỏ qua chính record đang UPDATE.
        */

        if (
            !checkConstraints(
                updatedRecord,
                i
            )
        )
        {
            continue;
        }


        /*
            Cập nhật thành công.
        */

        rows[i] = updatedRecord;

        updated = true;
    }

    return updated;
}


// ============================================================
// RECORD INFORMATION
// ============================================================

int Table::getRowCount() const
{
    return static_cast<int>(
        rows.size()
    );
}

const vector<Record>&
Table::getRows() const
{
    return rows;
}


// ============================================================
// DISPLAY
// ============================================================

void Table::printStructure() const
{
    cout
        << "TABLE: "
        << name
        << endl;

    cout
        << "-----------------------------"
        << endl;

    for (const Column& column : columns)
    {
        column.print();
    }

    cout << endl;

    if (!constraints.empty())
    {
        cout
            << "CONSTRAINTS:"
            << endl;

        for (
            const auto& constraint
            :
            constraints
        )
        {
            cout
                << "- "
                << constraint->getName()
                << endl;
        }

        cout << endl;
    }
}

void Table::printRows() const
{
    cout
        << "TABLE DATA: "
        << name
        << endl;

    cout
        << "-----------------------------"
        << endl;

    if (rows.empty())
    {
        cout
            << "No data."
            << endl;

        return;
    }

    for (
        int i = 0;
        i < static_cast<int>(rows.size());
        i++
    )
    {
        cout
            << "Record "
            << i + 1
            << ":"
            << endl;

        rows[i].print();

        cout << endl;
    }
}


// ============================================================
// CLEAR
// ============================================================

void Table::clear()
{
    rows.clear();
}

// ============================================================
// REMOVE
// ============================================================

bool Table::remove(
    const Condition& condition
)
{
    int oldSize =
        static_cast<int>(
            rows.size()
        );

    vector<Record> remainingRows;

    for (const Record& record : rows)
    {
        if (!condition.evaluate(record))
        {
            remainingRows.push_back(record);
        }
    }

    rows = remainingRows;

    return (
        static_cast<int>(rows.size())
        != oldSize
    );
}
