#include "UserDelete.h"
#include "database/Constraint.h"
#include "database/Condition.h"
#include "query/Query.h"
#include <iostream>
#include <cctype>

using namespace std;

namespace {

string toLowerStr(string s)
{
    for (char &c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}

string getPrimaryKeyCol(const Table& table)
{
    for (size_t i = 0; i < table.getConstraints().size(); ++i)
    {
        if (table.getConstraints()[i]->getType() == ConstraintType::PRIMARY_KEY)
        {
            auto* pk = dynamic_cast<PrimaryKeyConstraint*>(table.getConstraints()[i]);
            if (pk != nullptr) return pk->getColumnName();
        }
    }
    for (size_t i = 0; i < table.getColumns().size(); ++i)
    {
        if (table.getColumns()[i].isPrimaryKey())
        {
            return table.getColumns()[i].getName();
        }
    }
    return "";
}

} // anonymous namespace

namespace user_delete {

bool deleteRecord(Database* db, const string& tableName, const Value& recordId)
{
    if (db == nullptr) return false;
    Table* table = db->getTable(tableName);
    if (table == nullptr) return false;

    string lowerTbl = toLowerStr(tableName);

    // Rule 4.4: Master tables must never be deleted
    if (lowerTbl == "status" || lowerTbl == "type" || lowerTbl == "unit_status" ||
        lowerTbl == "inbound_status" || lowerTbl == "outbound_status" ||
        lowerTbl == "delivery_status" || lowerTbl == "shipment_status")
    {
        cout << "Delete rejected: Master data table '" << tableName << "' is read-only (Rule 4.4)." << endl;
        return false;
    }

    // Rule 4.2: Standalone deletion of Unit records via normal CRUD is strictly prohibited
    if (lowerTbl == "unit_inventory")
    {
        cout << "Delete rejected: Standalone deletion of Unit records is strictly prohibited (Rule 4.2)." << endl;
        return false;
    }

    string pkCol = getPrimaryKeyCol(*table);
    if (pkCol.empty())
    {
        cout << "Delete failed: Primary key not found for table '" << tableName << "'" << endl;
        return false;
    }

    Value targetPk = recordId;
    const Column* pkColObj = table->getColumn(pkCol);

    // If recordId type differs from primary key (e.g., brand name string passed for table brand with integer brand_id),
    // resolve via unique column lookup
    if (pkColObj != nullptr && pkColObj->getType() == DataType::INTEGER && holds_alternative<string>(recordId))
    {
        bool found = false;
        const Vector<Record>& rows = table->getRows();
        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (table->isRowDeleted(i)) continue;
            for (const auto& item : rows[i].getValues())
            {
                if (item.second == recordId)
                {
                    targetPk = rows[i].get(pkCol);
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        if (!found)
        {
            cout << "Delete failed: Record not found with identifier in '" << tableName << "'" << endl;
            return false;
        }
    }

    // Execute safe deletion via Query module (with index lookup, referential integrity check, confirmation check, and dirty flag)
    Query::Query q(*table, db);
    q.where(pkCol, CompareOperator::EQUAL, targetPk);
    return q.deleteRecords();
}

bool deleteProduct(Database* db, int productId)
{
    return deleteRecord(db, "product", Value(productId));
}

bool deleteBrand(Database* db, const string& brandName)
{
    return deleteRecord(db, "brand", Value(brandName));
}

bool deleteCategory(Database* db, int categoryId)
{
    return deleteRecord(db, "category", Value(categoryId));
}

bool deleteSpec(Database* db, const string& specTableName, int specId)
{
    return deleteRecord(db, specTableName, Value(specId));
}

bool deleteUnconfirmedInbound(Database* db, int txnId)
{
    Table* tbl = db ? db->getTable("inbound_transaction") : nullptr;
    if (tbl == nullptr) return false;

    return deleteRecord(db, "inbound_transaction", Value(txnId));
}

bool deleteUnconfirmedOutbound(Database* db, int txnId)
{
    Table* tbl = db ? db->getTable("outbound_transaction") : nullptr;
    if (tbl == nullptr) return false;

    return deleteRecord(db, "outbound_transaction", Value(txnId));
}

} // namespace user_delete
