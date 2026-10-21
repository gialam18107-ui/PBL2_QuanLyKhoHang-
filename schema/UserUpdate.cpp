#include "UserUpdate.h"
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

string toUpperStr(string s)
{
    for (char &c : s) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
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

bool checkTypeMatch(DataType colType, const Value& val)
{
    if (holds_alternative<monostate>(val)) return true;
    switch (colType)
    {
        case DataType::INTEGER:
            return holds_alternative<int>(val);
        case DataType::VARCHAR:
        case DataType::CHAR:
        case DataType::TEXT:
        case DataType::DATE:
            return holds_alternative<string>(val);
        case DataType::BOOLEAN:
            return holds_alternative<bool>(val) || holds_alternative<int>(val);
        default:
            return true;
    }
}

} // anonymous namespace

namespace user_update {

bool updateRecord(
    Database* db,
    const string& tableName,
    const Value& recordId,
    const Vector<string>& modifiedColumns,
    const Vector<Value>& modifyingValues)
{
    if (db == nullptr) return false;
    Table* table = db->getTable(tableName);
    if (table == nullptr) return false;

    if (modifiedColumns.size() != modifyingValues.size() || modifiedColumns.empty())
    {
        cout << "Update failed: Mismatched modified columns and values count." << endl;
        return false;
    }

    string lowerTbl = toLowerStr(tableName);

    // Rule 4.4: Never directly edit master Status/Type tables
    if (lowerTbl == "status" || lowerTbl == "type" || lowerTbl == "unit_status" ||
        lowerTbl == "inbound_status" || lowerTbl == "outbound_status" ||
        lowerTbl == "delivery_status" || lowerTbl == "shipment_status")
    {
        cout << "Update rejected: Master table '" << tableName << "' is read-only." << endl;
        return false;
    }

    // Rule 4.4: Users must never directly edit Unit.status through the Unit table
    if (lowerTbl == "unit_inventory")
    {
        for (size_t i = 0; i < modifiedColumns.size(); ++i)
        {
            if (modifiedColumns[i] == "status")
            {
                cout << "Update rejected: Cannot directly edit 'unit_inventory.status' (Rule 4.4)." << endl;
                return false;
            }
        }
    }

    string pkCol = getPrimaryKeyCol(*table);
    if (pkCol.empty())
    {
        cout << "Update failed: Primary key not found for table '" << tableName << "'" << endl;
        return false;
    }

    // Find target row
    const Vector<Record>& rows = table->getRows();
    const Record* targetRow = nullptr;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (!table->isRowDeleted(i) && rows[i].has(pkCol) && rows[i].get(pkCol) == recordId)
        {
            targetRow = &rows[i];
            break;
        }
    }
    if (targetRow == nullptr)
    {
        cout << "Update failed: Record not found with " << pkCol << endl;
        return false;
    }

    // Rule 4.3: Frozen business data after confirmation
    if (lowerTbl == "inbound_transaction" || lowerTbl == "outbound_transaction" ||
        lowerTbl == "delivery" || lowerTbl == "shipment")
    {
        if (targetRow->has("status"))
        {
            string currStatus = holds_alternative<string>(targetRow->get("status"))
                ? toUpperStr(get<string>(targetRow->get("status")))
                : "";

            if (currStatus == "CONFIRMED" || currStatus == "COMPLETED" || currStatus == "DELIVERED" || currStatus == "SHIPPED")
            {
                // Only status updates are permitted during lifecycle processing
                for (size_t i = 0; i < modifiedColumns.size(); ++i)
                {
                    if (modifiedColumns[i] != "status")
                    {
                        cout << "Update rejected: Confirmed transaction/delivery business data is permanently locked (Rule 4.3)." << endl;
                        return false;
                    }
                }
            }
        }
    }

    // Check column names & datatypes
    Record newValues;
    for (size_t i = 0; i < modifiedColumns.size(); ++i)
    {
        const string& colName = modifiedColumns[i];
        const Column* col = table->getColumn(colName);
        if (col == nullptr)
        {
            cout << "Update failed: Column '" << colName << "' not found in table '" << tableName << "'" << endl;
            return false;
        }

        if (!checkTypeMatch(col->getType(), modifyingValues[i]))
        {
            cout << "Update failed: Invalid datatype for column '" << colName << "'" << endl;
            return false;
        }
        newValues.set(colName, modifyingValues[i]);
    }

    Condition cond(pkCol, CompareOperator::EQUAL, recordId);
    bool success = db->update(tableName, cond, newValues);
    if (success)
    {
        Query::setDirty(true);
    }
    return success;
}

bool updateInboundTransactionStatus(Database* db, int txnId, const string& newStatus)
{
    Vector<string> cols = { "status" };
    Vector<Value> vals = { Value(newStatus) };
    return updateRecord(db, "inbound_transaction", Value(txnId), cols, vals);
}

bool updateDeliveryStatus(Database* db, int deliveryId, const string& newStatus)
{
    Vector<string> cols = { "status" };
    Vector<Value> vals = { Value(newStatus) };
    return updateRecord(db, "delivery", Value(deliveryId), cols, vals);
}

bool updateOutboundTransactionStatus(Database* db, int txnId, const string& newStatus)
{
    Vector<string> cols = { "status" };
    Vector<Value> vals = { Value(newStatus) };
    return updateRecord(db, "outbound_transaction", Value(txnId), cols, vals);
}

bool updateShipmentStatus(Database* db, int shipmentId, const string& newStatus)
{
    Vector<string> cols = { "status" };
    Vector<Value> vals = { Value(newStatus) };
    return updateRecord(db, "shipment", Value(shipmentId), cols, vals);
}

bool updateProductIsSold(Database* db, int productId, bool isSold)
{
    Vector<string> cols = { "is_sold" };
    Vector<Value> vals = { Value(isSold) };
    return updateRecord(db, "product", Value(productId), cols, vals);
}

} // namespace user_update
