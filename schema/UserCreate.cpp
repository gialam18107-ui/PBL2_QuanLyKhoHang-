#include "UserCreate.h"
#include "query/Query.h"
#include <iostream>

using namespace std;

namespace {

string toUpperStr(string s)
{
    for (char &c : s) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return s;
}

string padNumber(int num, int width)
{
    string s = to_string(num);
    while (static_cast<int>(s.length()) < width)
    {
        s = "0" + s;
    }
    return s;
}

int getNextId(Table* table, const string& idCol)
{
    if (table == nullptr) return 1;
    int maxId = 0;
    const Vector<Record>& rows = table->getRows();
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (table->isRowDeleted(i)) continue;
        if (rows[i].has(idCol))
        {
            const Value& v = rows[i].get(idCol);
            if (holds_alternative<int>(v))
            {
                int val = get<int>(v);
                if (val > maxId) maxId = val;
            }
        }
    }
    return maxId + 1;
}

Record* findRowById(Table* table, const string& idCol, const Value& idVal)
{
    if (table == nullptr) return nullptr;
    Vector<Record>& rows = table->getRows();
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (!table->isRowDeleted(i) && rows[i].has(idCol))
        {
            if (rows[i].get(idCol) == idVal)
            {
                return &rows[i];
            }
        }
    }
    return nullptr;
}

} // anonymous namespace

namespace user_create {

bool insertRecord(Database* db, const string& tableName, const Record& r)
{
    if (db == nullptr) return false;
    Table* table = db->getTable(tableName);
    if (table == nullptr) return false;

    // Validate columns
    for (const auto& item : r.getValues())
    {
        if (!table->hasColumn(item.first))
        {
            cout << "Insert failed: Unknown column '" << item.first << "' in table '" << tableName << "'" << endl;
            return false;
        }
    }

    bool success = db->insert(tableName, r);
    if (success)
    {
        Query::setDirty(true);
    }
    return success;
}

bool createBrand(Database* db, const string& brandName, const string& code)
{
    if (db == nullptr || brandName.empty()) return false;
    Table* tbl = db->getTable("brand");
    if (tbl == nullptr) return false;

    string brandCode = code;
    if (brandCode.empty())
    {
        brandCode = toUpperStr(brandName.substr(0, min(static_cast<size_t>(3), brandName.length())));
    }

    Record r;
    r.set("brand", brandName);
    r.set("code", brandCode);
    return insertRecord(db, "brand", r);
}

bool createCategory(Database* db, const string& categoryName, const string& code)
{
    if (db == nullptr || categoryName.empty()) return false;
    Table* tbl = db->getTable("category");
    if (tbl == nullptr) return false;

    string catCode = code;
    if (catCode.empty())
    {
        catCode = toUpperStr(categoryName.substr(0, min(static_cast<size_t>(3), categoryName.length())));
    }

    int newId = getNextId(tbl, "category_id");
    Record r;
    r.set("category_id", newId);
    r.set("category_name", categoryName);
    r.set("code", catCode);
    return insertRecord(db, "category", r);
}

bool createSpec(Database* db, const string& specTableName, const string& specValue)
{
    if (db == nullptr || specTableName.empty() || specValue.empty()) return false;
    Table* tbl = db->getTable(specTableName);
    if (tbl == nullptr) return false;

    int newId = getNextId(tbl, "id");
    Record r;
    r.set("id", newId);
    r.set("spec", specValue);
    return insertRecord(db, specTableName, r);
}

bool createProduct(Database* db, const Record& prodRecord, const string& categoryTable, const Record& categorySpecRecord)
{
    if (db == nullptr) return false;
    Table* prodTable = db->getTable("product");
    if (prodTable == nullptr) return false;

    Record prod = prodRecord;
    if (!prod.has("product_id"))
    {
        prod.set("product_id", getNextId(prodTable, "product_id"));
    }
    if (!prod.has("is_sold"))
    {
        prod.set("is_sold", false);
    }
    if (!prod.has("quantity"))
    {
        prod.set("quantity", 0);
    }

    int prodId = get<int>(prod.get("product_id"));

    // Insert into product table
    if (!insertRecord(db, "product", prod))
    {
        return false;
    }

    // Initialize product_quantity table record
    Table* pqTable = db->getTable("product_quantity");
    if (pqTable != nullptr)
    {
        Record pq;
        pq.set("product_id", prodId);
        pq.set("available_quan", 0);
        pq.set("warranty_quan", 0);
        pq.set("scrapped_quan", 0);
        insertRecord(db, "product_quantity", pq);
    }

    // Insert category-specific spec row if provided
    if (!categoryTable.empty())
    {
        Table* catTable = db->getTable(categoryTable);
        if (catTable != nullptr)
        {
            Record catSpec = categorySpecRecord;
            catSpec.set("product_id", prodId);
            insertRecord(db, categoryTable, catSpec);
        }
    }

    Query::setDirty(true);
    return true;
}

bool createCpu(Database* db, const Record& prod, const string& socket, const string& tier, const string& series)
{
    Record spec;
    spec.set("socket", socket);
    spec.set("tier", tier);
    spec.set("series", series);
    return createProduct(db, prod, "cpu", spec);
}

bool createGpu(Database* db, const Record& prod, const string& series, const string& producer, const string& vram)
{
    Record spec;
    spec.set("series", series);
    spec.set("producer", producer);
    spec.set("vram", vram);
    return createProduct(db, prod, "gpu", spec);
}

bool createMainboard(Database* db, const Record& prod, const string& cpuSupport, const string& socket, const string& size, const string& chipset, const string& ramSupport)
{
    Record spec;
    spec.set("cpu_support", cpuSupport);
    spec.set("socket", socket);
    spec.set("size", size);
    spec.set("chipset", chipset);
    spec.set("ram_support", ramSupport);
    return createProduct(db, prod, "mainboard", spec);
}

bool createRam(Database* db, const Record& prod, const string& generation, const string& capacity, const string& bus)
{
    Record spec;
    spec.set("generation", generation);
    spec.set("capacity", capacity);
    spec.set("bus", bus);
    return createProduct(db, prod, "ram", spec);
}

bool createSsd(Database* db, const Record& prod, const string& type, const string& gen, const string& capacity)
{
    Record spec;
    spec.set("type", type);
    spec.set("gen", gen);
    spec.set("capacity", capacity);
    return createProduct(db, prod, "ssd", spec);
}

bool createPsu(Database* db, const Record& prod, const string& quality, const string& capacity, const string& size)
{
    Record spec;
    spec.set("quality", quality);
    spec.set("capacity", capacity);
    spec.set("size", size);
    return createProduct(db, prod, "psu", spec);
}

bool createCooling(Database* db, const Record& prod, const string& type)
{
    Record spec;
    spec.set("type", type);
    return createProduct(db, prod, "cooling", spec);
}

bool createCase(Database* db, const Record& prod, const string& size)
{
    Record spec;
    spec.set("size", size);
    return createProduct(db, prod, "case", spec);
}

// ========================================================
// TỰ ĐỘNG SINH MÃ HÓA ĐƠN & GIAO VẬN (QuanLyKhoHang_schema.json)
// ========================================================

string generateInboundCode(Database* db, const string& inboundType)
{
    string typeCode = "PUR";
    int seq = 1;
    if (db != nullptr)
    {
        Table* tbl = db->getTable("inbound_type");
        if (tbl != nullptr)
        {
            Vector<Record>& rows = tbl->getRows();
            for (size_t i = 0; i < rows.size(); ++i)
            {
                if (tbl->isRowDeleted(i)) continue;
                if (rows[i].has("type") && holds_alternative<string>(rows[i].get("type")) &&
                    rows[i].get("type") == Value(inboundType))
                {
                    if (rows[i].has("code") && holds_alternative<string>(rows[i].get("code")))
                    {
                        typeCode = get<string>(rows[i].get("code"));
                    }
                    if (rows[i].has("next_code_no") && holds_alternative<int>(rows[i].get("next_code_no")))
                    {
                        seq = get<int>(rows[i].get("next_code_no"));
                        rows[i].set("next_code_no", seq + 1);
                    }
                    break;
                }
            }
        }
    }
    return "IN-" + typeCode + "-" + padNumber(seq, 6);
}

string generateDeliveryCode(const string& inboundCode, int deliveryNo)
{
    return inboundCode + "-D" + padNumber(deliveryNo, 2);
}

string generateOutboundCode(Database* db, const string& outboundType)
{
    string typeCode = "ORD";
    int seq = 1;
    if (db != nullptr)
    {
        Table* tbl = db->getTable("outbound_type");
        if (tbl != nullptr)
        {
            Vector<Record>& rows = tbl->getRows();
            for (size_t i = 0; i < rows.size(); ++i)
            {
                if (tbl->isRowDeleted(i)) continue;
                if (rows[i].has("type") && holds_alternative<string>(rows[i].get("type")) &&
                    rows[i].get("type") == Value(outboundType))
                {
                    if (rows[i].has("code") && holds_alternative<string>(rows[i].get("code")))
                    {
                        typeCode = get<string>(rows[i].get("code"));
                    }
                    if (rows[i].has("next_code_no") && holds_alternative<int>(rows[i].get("next_code_no")))
                    {
                        seq = get<int>(rows[i].get("next_code_no"));
                        rows[i].set("next_code_no", seq + 1);
                    }
                    break;
                }
            }
        }
    }
    return "OUT-" + typeCode + "-" + padNumber(seq, 6);
}

string generateShipmentCode(const string& outboundCode, int shipmentNo)
{
    return outboundCode + "-S" + padNumber(shipmentNo, 2);
}

bool createInboundPair(
    Database* db,
    const Record& inboundTxn,
    const Record& delivery,
    const Vector<InboundItemDetail>& items)
{
    if (db == nullptr || items.empty()) return false;

    // Rule 4.2 Validation: delivery quantity cannot be larger than inbound transaction quantity
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (items[i].receivedQuantity > items[i].orderedQuantity)
        {
            cout << "Inbound pair validation failed: received quantity (" << items[i].receivedQuantity
                 << ") cannot exceed ordered quantity (" << items[i].orderedQuantity << ")" << endl;
            return false;
        }
        if (static_cast<int>(items[i].serials.size()) > items[i].receivedQuantity)
        {
            cout << "Inbound pair validation failed: serial count (" << items[i].serials.size()
                 << ") exceeds received quantity (" << items[i].receivedQuantity << ")" << endl;
            return false;
        }
    }

    Table* itTable = db->getTable("inbound_transaction");
    Table* dTable = db->getTable("delivery");
    Table* idTable = db->getTable("inbound_detail");
    Table* ddTable = db->getTable("delivery_detail");
    Table* dudTable = db->getTable("delivery_unit_detail");
    Table* uTable = db->getTable("unit_inventory");
    Table* shTable = db->getTable("status_history");
    Table* pqTable = db->getTable("product_quantity");

    if (!itTable || !dTable || !idTable || !ddTable || !uTable || !shTable) return false;

    Record txn = inboundTxn;
    if (!txn.has("inbound_transaction_id"))
    {
        txn.set("inbound_transaction_id", getNextId(itTable, "inbound_transaction_id"));
    }
    string inType = (txn.has("type") && holds_alternative<string>(txn.get("type")))
        ? get<string>(txn.get("type")) : "PURCHASE";
    if (!txn.has("inbound_code") || (holds_alternative<string>(txn.get("inbound_code")) && get<string>(txn.get("inbound_code")).empty()))
    {
        txn.set("inbound_code", generateInboundCode(db, inType));
    }
    int txnId = get<int>(txn.get("inbound_transaction_id"));
    string inCode = holds_alternative<string>(txn.get("inbound_code")) ? get<string>(txn.get("inbound_code")) : "IN-001";
    if (!txn.has("next_delivery_no"))
    {
        txn.set("next_delivery_no", 2);
    }
    if (!insertRecord(db, "inbound_transaction", txn)) return false;

    Record del = delivery;
    if (!del.has("delivery_id"))
    {
        del.set("delivery_id", getNextId(dTable, "delivery_id"));
    }
    del.set("inbound_transaction_id", txnId);
    if (!del.has("delivery_code") || (holds_alternative<string>(del.get("delivery_code")) && get<string>(del.get("delivery_code")).empty()))
    {
        del.set("delivery_code", generateDeliveryCode(inCode, 1));
    }
    int delId = get<int>(del.get("delivery_id"));
    if (!insertRecord(db, "delivery", del)) return false;

    // Process details and attached units
    for (size_t i = 0; i < items.size(); ++i)
    {
        const InboundItemDetail& item = items[i];

        // 1. Inbound detail
        Record idRecord;
        idRecord.set("inbound_detail_id", getNextId(idTable, "inbound_detail_id"));
        idRecord.set("inbound_transaction_id", txnId);
        idRecord.set("product_id", item.productId);
        idRecord.set("ordered_quantity", item.orderedQuantity);
        insertRecord(db, "inbound_detail", idRecord);

        // 2. Delivery detail
        Record ddRecord;
        string ddKey = to_string(delId) + "_" + to_string(item.productId);
        ddRecord.set("id", ddKey);
        ddRecord.set("delivery_id", delId);
        ddRecord.set("product_id", item.productId);
        ddRecord.set("received_quantity", item.receivedQuantity);
        insertRecord(db, "delivery_detail", ddRecord);

        // 3. Process attached serials / unit transitions
        string targetStatus = item.targetUnitStatus.empty() ? "AVAILABLE" : item.targetUnitStatus;

        for (size_t s = 0; s < item.serials.size(); ++s)
        {
            const string& serial = item.serials[s];

            // Check if unit already exists
            Record* existingUnit = findRowById(uTable, "serial", Value(serial));
            int unitId = 0;
            Value oldStatus = monostate{};

            if (existingUnit == nullptr)
            {
                // Rule 6.3: If unit does not exist before, add to Unit Inventory with new_status, old_status == NULL
                unitId = getNextId(uTable, "unit_id");
                Record newUnit;
                newUnit.set("unit_id", unitId);
                newUnit.set("product_id", item.productId);
                newUnit.set("serial", serial);
                newUnit.set("status", targetStatus);
                insertRecord(db, "unit_inventory", newUnit);
            }
            else
            {
                unitId = get<int>(existingUnit->get("unit_id"));
                oldStatus = existingUnit->get("status");
                existingUnit->set("status", targetStatus);
            }

            // Delivery unit detail
            if (dudTable != nullptr)
            {
                Record dudRecord;
                dudRecord.set("delivery_unit_detail_id", getNextId(dudTable, "delivery_unit_detail_id"));
                dudRecord.set("delivery_id", delId);
                dudRecord.set("product_id", item.productId);
                dudRecord.set("serial", serial);
                insertRecord(db, "delivery_unit_detail", dudRecord);
            }

            // Rule 5.2: Record chronologically consistent entry in status_history
            Record shRecord;
            shRecord.set("history_id", getNextId(shTable, "history_id"));
            shRecord.set("unit_id", unitId);
            shRecord.set("old_status", oldStatus);
            shRecord.set("curr_status", targetStatus);
            shRecord.set("transaction", delId);
            shRecord.set("type", string("INBOUND"));
            insertRecord(db, "status_history", shRecord);
        }

        // Update product_quantity
        if (pqTable != nullptr)
        {
            Record* pq = findRowById(pqTable, "product_id", Value(item.productId));
            if (pq != nullptr)
            {
                int currAvail = pq->has("available_quan") ? get<int>(pq->get("available_quan")) : 0;
                pq->set("available_quan", currAvail + item.receivedQuantity);
            }
        }
    }

    Query::setDirty(true);
    return true;
}

bool createOutboundPair(
    Database* db,
    const Record& outboundTxn,
    const Record& shipment,
    const Vector<OutboundItemDetail>& items)
{
    if (db == nullptr || items.empty()) return false;

    // Rule 4.2 Validation: shipped quantity cannot be larger than outbound transaction quantity
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (items[i].shippedQuantity > items[i].orderedQuantity)
        {
            cout << "Outbound pair validation failed: shipped quantity (" << items[i].shippedQuantity
                 << ") cannot exceed ordered quantity (" << items[i].orderedQuantity << ")" << endl;
            return false;
        }
        if (static_cast<int>(items[i].serials.size()) > items[i].shippedQuantity)
        {
            cout << "Outbound pair validation failed: serial count (" << items[i].serials.size()
                 << ") exceeds shipped quantity (" << items[i].shippedQuantity << ")" << endl;
            return false;
        }
    }

    Table* otTable = db->getTable("outbound_transaction");
    Table* sTable = db->getTable("shipment");
    Table* odTable = db->getTable("outbound_detail");
    Table* sdTable = db->getTable("shipment_detail");
    Table* sudTable = db->getTable("shipment_unit_detail");
    Table* uTable = db->getTable("unit_inventory");
    Table* shTable = db->getTable("status_history");
    Table* pqTable = db->getTable("product_quantity");

    if (!otTable || !sTable || !odTable || !sdTable || !uTable || !shTable) return false;

    Record txn = outboundTxn;
    if (!txn.has("outbound_trainsaction_id"))
    {
        txn.set("outbound_trainsaction_id", getNextId(otTable, "outbound_trainsaction_id"));
    }
    string outType = (txn.has("type") && holds_alternative<string>(txn.get("type")))
        ? get<string>(txn.get("type")) : "ORDER";
    if (!txn.has("outbound_code") || (holds_alternative<string>(txn.get("outbound_code")) && get<string>(txn.get("outbound_code")).empty()))
    {
        txn.set("outbound_code", generateOutboundCode(db, outType));
    }
    int txnId = get<int>(txn.get("outbound_trainsaction_id"));
    string outCode = holds_alternative<string>(txn.get("outbound_code")) ? get<string>(txn.get("outbound_code")) : "OUT-001";
    if (!txn.has("next_ship_no"))
    {
        txn.set("next_ship_no", 2);
    }
    if (!insertRecord(db, "outbound_transaction", txn)) return false;

    Record shp = shipment;
    if (!shp.has("shipment_id"))
    {
        shp.set("shipment_id", getNextId(sTable, "shipment_id"));
    }
    shp.set("sell_id", txnId);
    if (!shp.has("shipment_code") || (holds_alternative<string>(shp.get("shipment_code")) && get<string>(shp.get("shipment_code")).empty()))
    {
        shp.set("shipment_code", generateShipmentCode(outCode, 1));
    }
    int shipId = get<int>(shp.get("shipment_id"));
    if (!insertRecord(db, "shipment", shp)) return false;

    // Process details and attached units
    for (size_t i = 0; i < items.size(); ++i)
    {
        const OutboundItemDetail& item = items[i];

        // 1. Outbound detail
        Record odRecord;
        odRecord.set("outbound_detail_id", getNextId(odTable, "outbound_detail_id"));
        odRecord.set("outbound_transaction_id", txnId);
        odRecord.set("product_id", item.productId);
        odRecord.set("ordered_quantity", item.orderedQuantity);
        insertRecord(db, "outbound_detail", odRecord);

        // 2. Shipment detail
        Record sdRecord;
        string sdKey = to_string(shipId) + "_" + to_string(item.productId);
        sdRecord.set("id", sdKey);
        sdRecord.set("shipment_id", shipId);
        sdRecord.set("product_id", item.productId);
        sdRecord.set("received_quantity", item.shippedQuantity);
        insertRecord(db, "shipment_detail", sdRecord);

        // 3. Process attached serials / unit transitions
        string targetStatus = item.targetUnitStatus.empty() ? "SOLD" : item.targetUnitStatus;

        for (size_t s = 0; s < item.serials.size(); ++s)
        {
            const string& serial = item.serials[s];

            Record* existingUnit = findRowById(uTable, "serial", Value(serial));
            if (existingUnit == nullptr)
            {
                cout << "Outbound pair failed: Unit with serial '" << serial << "' does not exist." << endl;
                continue;
            }

            int unitId = get<int>(existingUnit->get("unit_id"));
            Value oldStatus = existingUnit->get("status");
            existingUnit->set("status", targetStatus);

            // Shipment unit detail
            if (sudTable != nullptr)
            {
                Record sudRecord;
                sudRecord.set("shipment_unit_detail_id", getNextId(sudTable, "shipment_unit_detail_id"));
                sudRecord.set("shipment_id", shipId);
                sudRecord.set("product_id", item.productId);
                sudRecord.set("serial", serial);
                insertRecord(db, "shipment_unit_detail", sudRecord);
            }

            // Status history
            Record shRecord;
            shRecord.set("history_id", getNextId(shTable, "history_id"));
            shRecord.set("unit_id", unitId);
            shRecord.set("old_status", oldStatus);
            shRecord.set("curr_status", targetStatus);
            shRecord.set("transaction", shipId);
            shRecord.set("type", string("OUTBOUND"));
            insertRecord(db, "status_history", shRecord);
        }

        // Deduct product_quantity
        if (pqTable != nullptr)
        {
            Record* pq = findRowById(pqTable, "product_id", Value(item.productId));
            if (pq != nullptr)
            {
                int currAvail = pq->has("available_quan") ? get<int>(pq->get("available_quan")) : 0;
                pq->set("available_quan", max(0, currAvail - item.shippedQuantity));
            }
        }
    }

    Query::setDirty(true);
    return true;
}

} // namespace user_create
