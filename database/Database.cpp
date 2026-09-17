#include "Database.h"

#include <iostream>

using namespace std;


// ============================================================
// TABLE MANAGEMENT
// ============================================================

bool Database::addTable(
    const shared_ptr<Table>& table
)
{
    if (table == nullptr)
    {
        return false;
    }

    string tableName =
        table->getName();

    if (hasTable(tableName))
    {
        cout
            << "Table already exists: "
            << tableName
            << endl;

        return false;
    }

    tables[tableName] = table;

    return true;
}


bool Database::hasTable(
    const string& tableName
) const
{
    return tables.find(tableName)
        != tables.end();
}


shared_ptr<Table>
Database::getTable(
    const string& tableName
)
{
    auto it =
        tables.find(tableName);

    if (it == tables.end())
    {
        return nullptr;
    }

    return it->second;
}


shared_ptr<const Table>
Database::getTable(
    const string& tableName
) const
{
    auto it =
        tables.find(tableName);

    if (it == tables.end())
    {
        return nullptr;
    }

    return it->second;
}


int Database::getTableCount() const
{
    return static_cast<int>(
        tables.size()
    );
}


void Database::printTables() const
{
    cout
        << "DATABASE TABLES"
        << endl;

    cout
        << "=============================="
        << endl;

    if (tables.empty())
    {
        cout
            << "No tables."
            << endl;

        return;
    }

    for (
        const auto& pair
        :
        tables
    )
    {
        cout
            << "- "
            << pair.first
            << endl;
    }
}


// ============================================================
// INSERT
// ============================================================

bool Database::insert(
    const string& tableName,
    const Record& record
)
{
    auto table =
        getTable(tableName);

    if (table == nullptr)
    {
        cout
            << "Insert failed: table not found: "
            << tableName
            << endl;

        return false;
    }


    // ========================================================
    // KIỂM TRA FOREIGN KEY
    // ========================================================

    for (
        const auto& constraint
        :
        table->getConstraints()
    )
    {
        if (
            constraint->getType()
            !=
            ConstraintType::FOREIGN_KEY
        )
        {
            continue;
        }

        auto fk =
            dynamic_pointer_cast<
                ForeignKeyConstraint
            >(constraint);

        if (fk == nullptr)
        {
            continue;
        }


        // Không có FK column
        // -> NOT NULL sẽ được Table xử lý
        if (
            !record.has(
                fk->getColumnName()
            )
        )
        {
            continue;
        }


        // ----------------------------------------------------
        // Lấy bảng được tham chiếu
        // ----------------------------------------------------

        auto referencedTable =
            getTable(
                fk->getReferenceTable()
            );

        if (referencedTable == nullptr)
        {
            cout
                << "Insert failed: "
                << "referenced table not found: "
                << fk->getReferenceTable()
                << endl;

            return false;
        }


        // ----------------------------------------------------
        // Kiểm tra FOREIGN KEY
        // ----------------------------------------------------

        if (
            !fk->validateReference(
                record,
                referencedTable->getRows()
            )
        )
        {
            cout
                << "Foreign key constraint failed: "
                << fk->getName()
                << endl;

            return false;
        }
    }


    // ========================================================
    // INSERT VÀO TABLE
    // ========================================================

    return table->insert(record);
}


// ============================================================
// SELECT ALL
// ============================================================

vector<Record>
Database::selectAll(
    const string& tableName
) const
{
    auto table =
        getTable(tableName);

    if (table == nullptr)
    {
        cout
            << "Select failed: table not found: "
            << tableName
            << endl;

        return {};
    }

    return table->getRows();
}


// ============================================================
// UPDATE
// ============================================================

bool Database::update(
    const string& tableName,
    const Condition& condition,
    const Record& newValues
)
{
    auto table =
        getTable(tableName);

    if (table == nullptr)
    {
        cout
            << "Update failed: table not found: "
            << tableName
            << endl;

        return false;
    }


    const vector<Record>& rows =
        table->getRows();


    // ========================================================
    // KIỂM TRA FOREIGN KEY
    // ========================================================

    for (
        const Record& currentRecord
        :
        rows
    )
    {
        if (
            !condition.evaluate(
                currentRecord
            )
        )
        {
            continue;
        }


        // ----------------------------------------------------
        // Tạo Record sau UPDATE
        // ----------------------------------------------------

        Record updatedRecord =
            currentRecord;


        for (
            const auto& pair
            :
            newValues.getValues()
        )
        {
            updatedRecord.set(
                pair.first,
                pair.second
            );
        }


        // ----------------------------------------------------
        // Kiểm tra FK
        // ----------------------------------------------------

        for (
            const auto& constraint
            :
            table->getConstraints()
        )
        {
            if (
                constraint->getType()
                !=
                ConstraintType::FOREIGN_KEY
            )
            {
                continue;
            }

            auto fk =
                dynamic_pointer_cast<
                    ForeignKeyConstraint
                >(constraint);

            if (fk == nullptr)
            {
                continue;
            }


            if (
                !updatedRecord.has(
                    fk->getColumnName()
                )
            )
            {
                continue;
            }


            auto referencedTable =
                getTable(
                    fk->getReferenceTable()
                );

            if (referencedTable == nullptr)
            {
                cout
                    << "Update failed: "
                    << "referenced table not found: "
                    << fk->getReferenceTable()
                    << endl;

                return false;
            }


            if (
                !fk->validateReference(
                    updatedRecord,
                    referencedTable->getRows()
                )
            )
            {
                cout
                    << "Foreign key constraint failed: "
                    << fk->getName()
                    << endl;

                return false;
            }
        }
    }


    // ========================================================
    // UPDATE TABLE
    // ========================================================

    return table->update(
        condition,
        newValues
    );
}


// ============================================================
// DELETE
// ============================================================

bool Database::remove(
    const string& tableName,
    const Condition& condition
)
{
    auto table =
        getTable(tableName);

    if (table == nullptr)
    {
        cout
            << "Delete failed: table not found: "
            << tableName
            << endl;

        return false;
    }


    // ========================================================
    // TÌM RECORD SẼ BỊ XÓA
    // ========================================================

    vector<Record> recordsToDelete;

    for (
        const Record& record
        :
        table->getRows()
    )
    {
        if (
            condition.evaluate(record)
        )
        {
            recordsToDelete.push_back(
                record
            );
        }
    }


    if (recordsToDelete.empty())
    {
        cout
            << "Delete failed: "
            << "no matching record."
            << endl;

        return false;
    }


    // ========================================================
    // KIỂM TRA FOREIGN KEY CỦA CÁC BẢNG KHÁC
    // ========================================================

    for (
        const auto& tablePair
        :
        tables
    )
    {
        auto otherTable =
            tablePair.second;


        // Bỏ qua chính bảng đang DELETE
        if (
            otherTable->getName()
            ==
            tableName
        )
        {
            continue;
        }


        for (
            const auto& constraint
            :
            otherTable->getConstraints()
        )
        {
            if (
                constraint->getType()
                !=
                ConstraintType::FOREIGN_KEY
            )
            {
                continue;
            }


            auto fk =
                dynamic_pointer_cast<
                    ForeignKeyConstraint
                >(constraint);

            if (fk == nullptr)
            {
                continue;
            }


            // FK của bảng con không trỏ vào
            // bảng đang DELETE
            if (
                fk->getReferenceTable()
                !=
                tableName
            )
            {
                continue;
            }


            // ------------------------------------------------
            // Kiểm tra từng Record chuẩn bị xóa
            // ------------------------------------------------

            for (
                const Record& deletedRecord
                :
                recordsToDelete
            )
            {
                if (
                    !deletedRecord.has(
                        fk->getReferenceColumn()
                    )
                )
                {
                    continue;
                }


                Value deletedValue =
                    deletedRecord.get(
                        fk->getReferenceColumn()
                    );


                // --------------------------------------------
                // Tìm record con đang tham chiếu
                // --------------------------------------------

                for (
                    const Record& childRecord
                    :
                    otherTable->getRows()
                )
                {
                    if (
                        !childRecord.has(
                            fk->getColumnName()
                        )
                    )
                    {
                        continue;
                    }


                    Value childValue =
                        childRecord.get(
                            fk->getColumnName()
                        );


                    if (
                        deletedValue
                        ==
                        childValue
                    )
                    {
                        cout
                            << "Delete failed: "
                            << "record is referenced by table "
                            << otherTable->getName()
                            << endl;

                        return false;
                    }
                }
            }
        }
    }


    // ========================================================
    // DELETE
    // ========================================================

    return table->remove(
        condition
    );
}


// ============================================================
// CLEAR
// ============================================================

void Database::clear()
{
    tables.clear();
}
