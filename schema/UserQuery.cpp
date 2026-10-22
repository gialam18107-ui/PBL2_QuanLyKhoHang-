#include "UserQuery.h"
#include "datastructure/hashmap/HashMap.h"
#include "query/Query.h"

using namespace std;

namespace user_query {

// ============================================================
// SORT TRÊN KẾT QUẢ VECTOR<RECORD>
// ============================================================
void sort_query_results(Vector<Record>& records, const string& chosenColumn, bool ascending)
{
    Query::querySort(records, chosenColumn, ascending);
}

// 1. PRODUCT QUERY: Đã loại bỏ do PRODUCT_QUANTITY_QUERY đã hiển thị đầy đủ thông tin sản phẩm và số lượng tồn kho.


// ============================================================
// 2. PRODUCT QUANTITY QUERY (Index: product.is_sold)
// ============================================================
void product_quantity_query(Database *db, Vector<Record> &results)
{
    product_quantity_query(db, results, "", Value());
}

void product_quantity_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    results.clear();
    if (!db) return;

    Table* pqTable = db->getTable("product_quantity");
    Table* pTable = db->getTable("product");
    if (!pqTable || !pTable) return;

    Vector<Record> prodSource;
    if (!indexedColumn.empty())
    {
        prodSource = Query::filterRecords(*pTable, indexedColumn, val);
    }
    else
    {
        for (size_t i = 0; i < pTable->getRows().size(); ++i)
        {
            if (!pTable->isRowDeleted(i))
            {
                prodSource.push_back(pTable->getRows()[i]);
            }
        }
    }

    HashMap<Value, Record> prodMap;
    for (const auto& r : prodSource)
    {
        if (r.has("product_id"))
        {
            prodMap[r.get("product_id")] = r;
        }
    }

    const auto& pqRows = pqTable->getRows();
    for (size_t i = 0; i < pqRows.size(); ++i)
    {
        if (pqTable->isRowDeleted(i)) continue;
        const auto& pq = pqRows[i];
        if (!pq.has("product_id")) continue;

        auto it = prodMap.find(pq.get("product_id"));
        if (it != prodMap.end())
        {
            const auto& p = it->second;
            Record out;
            if (p.has("product_id"))
            {
                out.set("product.product_id", p.get("product_id"));
                out.set("product_id", p.get("product_id"));
            }
            if (p.has("product_code")) out.set("product.product_code", p.get("product_code"));
            if (p.has("barcode")) out.set("product.barcode", p.get("barcode"));
            if (p.has("sku")) out.set("product.sku", p.get("sku"));
            if (p.has("product_name")) out.set("product.product_name", p.get("product_name"));
            if (pq.has("available_quan")) out.set("product_quantity.available_quan", pq.get("available_quan"));
            if (pq.has("warranty_quan")) out.set("product_quantity.warranty_quan", pq.get("warranty_quan"));
            if (pq.has("scrapped_quan")) out.set("product_quantity.scrapped_quan", pq.get("scrapped_quan"));
            results.push_back(std::move(out));
        }
    }
}

// ============================================================
// 3. UNIT INVENTORY QUERY (Index: serial, status)
// ============================================================
void unit_inventory_query(Database *db, Vector<Record> &results)
{
    unit_inventory_query(db, results, "", Value());
}

void unit_inventory_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    results.clear();
    if (!db) return;

    Table* uTable = db->getTable("unit_inventory");
    Table* pTable = db->getTable("product");
    if (!uTable || !pTable) return;

    Vector<Record> uSource;
    if (!indexedColumn.empty())
    {
        uSource = Query::filterRecords(*uTable, indexedColumn, val);
    }
    else
    {
        for (size_t i = 0; i < uTable->getRows().size(); ++i)
        {
            if (!uTable->isRowDeleted(i)) uSource.push_back(uTable->getRows()[i]);
        }
    }

    HashMap<Value, Record> prodMap;
    const auto& pRows = pTable->getRows();
    for (size_t i = 0; i < pRows.size(); ++i)
    {
        if (pTable->isRowDeleted(i)) continue;
        if (pRows[i].has("product_id"))
        {
            prodMap[pRows[i].get("product_id")] = pRows[i];
        }
    }

    for (const auto& u : uSource)
    {
        if (!u.has("product_id")) continue;

        auto it = prodMap.find(u.get("product_id"));
        if (it != prodMap.end())
        {
            const auto& p = it->second;
            Record out;
            if (u.has("unit_id"))
            {
                out.set("unit_inventory.unit_id", u.get("unit_id"));
                out.set("unit_id", u.get("unit_id"));
            }
            if (p.has("product_name")) out.set("product.product_name", p.get("product_name"));
            if (u.has("serial")) out.set("unit_inventory.serial", u.get("serial"));
            if (u.has("status")) out.set("unit_inventory.status", u.get("status"));
            results.push_back(std::move(out));
        }
    }
}

// ============================================================
// ============================================================
// 4. STATUS HISTORY QUERY
// - Cột transaction: hiển thị delivery_code (INBOUND) hoặc shipment_code (OUTBOUND)
// - Cột date: delivery_at hoặc ship_at
// - Tìm kiếm transaction: tìm kiếm trên delivery.delivery_code và shipment.shipment_code (Hash) trước
// - Tìm kiếm date: tìm kiếm trên delivery.delivery_at và shipment.ship_at (AVL tree range) trước
// - Tìm kiếm type: tìm kiếm trên status_history.type (HashTree)
// ============================================================
void status_history_query(Database *db, Vector<Record> &results)
{
    status_history_query(db, results, "", Value());
}

void status_history_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    status_history_query(db, results, indexedColumn, val, Value());
}

void status_history_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal)
{
    results.clear();
    if (!db) return;

    const Value& val = minVal;

    Table* shTable = db->getTable("status_history");
    Table* uTable = db->getTable("unit_inventory");
    Table* dTable = db->getTable("delivery");
    Table* sTable = db->getTable("shipment");
    if (!shTable || !uTable) return;

    // 1. Map unit_id -> unit_inventory (để lấy serial)
    HashMap<Value, Record> unitMap;
    const auto& uRows = uTable->getRows();
    for (size_t i = 0; i < uRows.size(); ++i)
    {
        if (uTable->isRowDeleted(i)) continue;
        if (uRows[i].has("unit_id"))
        {
            unitMap[uRows[i].get("unit_id")] = uRows[i];
        }
    }

    // 2. Maps cho delivery: delivery_id -> delivery_code, delivery_id -> delivery_at
    HashMap<Value, Value> delivCodeMap;
    HashMap<Value, Value> delivDateMap;
    if (dTable)
    {
        for (size_t i = 0; i < dTable->getRows().size(); ++i)
        {
            if (dTable->isRowDeleted(i)) continue;
            const auto& d = dTable->getRows()[i];
            if (d.has("delivery_id"))
            {
                if (d.has("delivery_code")) delivCodeMap[d.get("delivery_id")] = d.get("delivery_code");
                if (d.has("delivery_at")) delivDateMap[d.get("delivery_id")] = d.get("delivery_at");
            }
        }
    }

    // 3. Maps cho shipment: shipment_id -> shipment_code, shipment_id -> ship_at
    HashMap<Value, Value> shipCodeMap;
    HashMap<Value, Value> shipDateMap;
    if (sTable)
    {
        for (size_t i = 0; i < sTable->getRows().size(); ++i)
        {
            if (sTable->isRowDeleted(i)) continue;
            const auto& s = sTable->getRows()[i];
            if (s.has("shipment_id"))
            {
                if (s.has("shipment_code")) shipCodeMap[s.get("shipment_id")] = s.get("shipment_code");
                if (s.has("ship_at")) shipDateMap[s.get("shipment_id")] = s.get("ship_at");
            }
        }
    }

    // Chuẩn hóa tên cột lọc
    string col = indexedColumn;
    if (col.rfind("status_history.", 0) == 0) col = col.substr(15);
    else if (col.rfind("unit_inventory.", 0) == 0) col = col.substr(15);

    bool filterByTxCode = (col == "transaction");
    bool filterByDate = (col == "date");
    bool filterBySerial = (col == "serial");
    bool hasRange = (minVal.index() != 0 && maxVal.index() != 0);

    HashMap<Value, bool> matchedDelivIds;
    HashMap<Value, bool> matchedShipIds;

    // Khi tìm kiếm theo transaction: tìm kiếm bên delivery và shipment trước qua Hash index!
    if (filterByTxCode)
    {
        if (dTable)
        {
            Vector<Record> matchedD = Query::filterRecords(*dTable, "delivery_code", val);
            for (size_t i = 0; i < matchedD.size(); ++i)
            {
                if (matchedD[i].has("delivery_id")) matchedDelivIds[matchedD[i].get("delivery_id")] = true;
            }
        }
        if (sTable)
        {
            Vector<Record> matchedS = Query::filterRecords(*sTable, "shipment_code", val);
            for (size_t i = 0; i < matchedS.size(); ++i)
            {
                if (matchedS[i].has("shipment_id")) matchedShipIds[matchedS[i].get("shipment_id")] = true;
            }
        }
    }
    // Khi tìm kiếm theo date: tìm kiếm bên delivery và shipment trước qua AVL Tree index!
    else if (filterByDate)
    {
        if (dTable)
        {
            Vector<Record> matchedD;
            if (hasRange)
                matchedD = Query::filterRecords(*dTable, "delivery_at", minVal, maxVal);
            else
                matchedD = Query::filterRecords(*dTable, "delivery_at", val);

            for (size_t i = 0; i < matchedD.size(); ++i)
            {
                if (matchedD[i].has("delivery_id")) matchedDelivIds[matchedD[i].get("delivery_id")] = true;
            }
        }
        if (sTable)
        {
            Vector<Record> matchedS;
            if (hasRange)
                matchedS = Query::filterRecords(*sTable, "ship_at", minVal, maxVal);
            else
                matchedS = Query::filterRecords(*sTable, "ship_at", val);

            for (size_t i = 0; i < matchedS.size(); ++i)
            {
                if (matchedS[i].has("shipment_id")) matchedShipIds[matchedS[i].get("shipment_id")] = true;
            }
        }
    }

    Vector<Record> shSource;
    if (filterByTxCode || filterByDate)
    {
        const auto& shRows = shTable->getRows();
        for (size_t i = 0; i < shRows.size(); ++i)
        {
            if (shTable->isRowDeleted(i)) continue;
            const auto& sh = shRows[i];
            string t = "";
            if (sh.has("type") && holds_alternative<string>(sh.get("type")))
            {
                t = get<string>(sh.get("type"));
            }
            if (t == "INBOUND")
            {
                if (sh.has("transaction") && matchedDelivIds.find(sh.get("transaction")) != matchedDelivIds.end())
                {
                    shSource.push_back(sh);
                }
            }
            else if (t == "OUTBOUND")
            {
                if (sh.has("transaction") && matchedShipIds.find(sh.get("transaction")) != matchedShipIds.end())
                {
                    shSource.push_back(sh);
                }
            }
        }
    }
    else if (filterBySerial)
    {
        Vector<Record> matchedU = Query::filterRecords(*uTable, "serial", val);
        HashMap<Value, bool> matchedUnitIds;
        for (size_t i = 0; i < matchedU.size(); ++i)
        {
            if (matchedU[i].has("unit_id")) matchedUnitIds[matchedU[i].get("unit_id")] = true;
        }
        const auto& shRows = shTable->getRows();
        for (size_t i = 0; i < shRows.size(); ++i)
        {
            if (shTable->isRowDeleted(i)) continue;
            if (shRows[i].has("unit_id") && matchedUnitIds.find(shRows[i].get("unit_id")) != matchedUnitIds.end())
            {
                shSource.push_back(shRows[i]);
            }
        }
    }
    else if (!col.empty())
    {
        shSource = Query::filterRecords(*shTable, col, val);
    }
    else
    {
        for (size_t i = 0; i < shTable->getRows().size(); ++i)
        {
            if (!shTable->isRowDeleted(i))
            {
                shSource.push_back(shTable->getRows()[i]);
            }
        }
    }

    for (const auto& sh : shSource)
    {
        if (!sh.has("unit_id")) continue;

        auto it = unitMap.find(sh.get("unit_id"));
        if (it != unitMap.end())
        {
            const auto& u = it->second;
            Record out;
            if (sh.has("history_id"))
            {
                out.set("status_history.history_id", sh.get("history_id"));
                out.set("history_id", sh.get("history_id"));
            }
            if (u.has("serial")) out.set("unit_inventory.serial", u.get("serial"));
            if (sh.has("old_status")) out.set("status_history.old_status", sh.get("old_status"));
            if (sh.has("curr_status")) out.set("status_history.curr_status", sh.get("curr_status"));

            // Hiển thị code của delivery và shipment
            Value txCodeVal = "";
            Value dateVal;
            string t = "";
            if (sh.has("type") && holds_alternative<string>(sh.get("type")))
            {
                t = get<string>(sh.get("type"));
            }
            if (t == "INBOUND")
            {
                if (sh.has("transaction"))
                {
                    auto cit = delivCodeMap.find(sh.get("transaction"));
                    if (cit != delivCodeMap.end()) txCodeVal = cit->second;
                    auto dit = delivDateMap.find(sh.get("transaction"));
                    if (dit != delivDateMap.end()) dateVal = dit->second;
                }
            }
            else if (t == "OUTBOUND")
            {
                if (sh.has("transaction"))
                {
                    auto cit = shipCodeMap.find(sh.get("transaction"));
                    if (cit != shipCodeMap.end()) txCodeVal = cit->second;
                    auto sit = shipDateMap.find(sh.get("transaction"));
                    if (sit != shipDateMap.end()) dateVal = sit->second;
                }
            }

            out.set("status_history.transaction", txCodeVal);
            if (sh.has("type")) out.set("status_history.type", sh.get("type"));
            out.set("status_history.date", dateVal);

            results.push_back(std::move(out));
        }
    }
}

// ============================================================
// 5. INBOUND TRANSACTION QUERY (Index: inbound_transaction.order_at)
// ============================================================
void inbound_transaction_query(Database *db, Vector<Record> &results)
{
    inbound_transaction_query(db, results, "", Value());
}

void inbound_transaction_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    results.clear();
    if (!db) return;

    Table* itTable = db->getTable("inbound_transaction");
    Table* cTable = db->getTable("customer");
    if (!itTable || !cTable) return;

    Vector<Record> itSource;
    if (!indexedColumn.empty())
    {
        itSource = Query::filterRecords(*itTable, indexedColumn, val);
    }
    else
    {
        for (size_t i = 0; i < itTable->getRows().size(); ++i)
        {
            if (!itTable->isRowDeleted(i)) itSource.push_back(itTable->getRows()[i]);
        }
    }

    HashMap<Value, Record> custMap;
    const auto& cRows = cTable->getRows();
    for (size_t i = 0; i < cRows.size(); ++i)
    {
        if (cTable->isRowDeleted(i)) continue;
        if (cRows[i].has("customer_id"))
        {
            custMap[cRows[i].get("customer_id")] = cRows[i];
        }
    }

    for (const auto& it : itSource)
    {
        if (!it.has("customer_id")) continue;
        auto cit = custMap.find(it.get("customer_id"));
        if (cit != custMap.end())
        {
            const auto& c = cit->second;
            Record out;
            if (it.has("inbound_transaction_id"))
            {
                out.set("inbound_transaction.inbound_transaction_id", it.get("inbound_transaction_id"));
                out.set("inbound_transaction_id", it.get("inbound_transaction_id"));
                out.set("inbound_id", it.get("inbound_transaction_id"));
            }
            if (it.has("inbound_code")) out.set("inbound_transaction.inbound_code", it.get("inbound_code"));
            if (c.has("customer_code")) out.set("customer.customer_code", c.get("customer_code"));
            if (it.has("order_at")) out.set("inbound_transaction.order_at", it.get("order_at"));
            if (it.has("type")) out.set("inbound_transaction.type", it.get("type"));
            if (it.has("status")) out.set("inbound_transaction.status", it.get("status"));
            if (it.has("note")) out.set("inbound_transaction.note", it.get("note"));
            results.push_back(std::move(out));
        }
    }
}

void inbound_transaction_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal)
{
    results.clear();
    if (!db) return;

    Table* itTable = db->getTable("inbound_transaction");
    Table* cTable = db->getTable("customer");
    if (!itTable || !cTable) return;

    Vector<Record> itSource = Query::filterRecords(*itTable, indexedColumn, minVal, maxVal);

    HashMap<Value, Record> custMap;
    const auto& cRows = cTable->getRows();
    for (size_t i = 0; i < cRows.size(); ++i)
    {
        if (cTable->isRowDeleted(i)) continue;
        if (cRows[i].has("customer_id"))
        {
            custMap[cRows[i].get("customer_id")] = cRows[i];
        }
    }

    for (const auto& it : itSource)
    {
        if (!it.has("customer_id")) continue;
        auto cit = custMap.find(it.get("customer_id"));
        if (cit != custMap.end())
        {
            const auto& c = cit->second;
            Record out;
            if (it.has("inbound_transaction_id"))
            {
                out.set("inbound_transaction.inbound_transaction_id", it.get("inbound_transaction_id"));
                out.set("inbound_transaction_id", it.get("inbound_transaction_id"));
                out.set("inbound_id", it.get("inbound_transaction_id"));
            }
            if (it.has("inbound_code")) out.set("inbound_transaction.inbound_code", it.get("inbound_code"));
            if (c.has("customer_code")) out.set("customer.customer_code", c.get("customer_code"));
            if (it.has("order_at")) out.set("inbound_transaction.order_at", it.get("order_at"));
            if (it.has("type")) out.set("inbound_transaction.type", it.get("type"));
            if (it.has("status")) out.set("inbound_transaction.status", it.get("status"));
            if (it.has("note")) out.set("inbound_transaction.note", it.get("note"));
            results.push_back(std::move(out));
        }
    }
}

// ============================================================
// 6. INBOUND DETAIL QUERY
// ============================================================
void inbound_detail_query(Database *db, Vector<Record> &results)
{
    inbound_detail_query(db, results, "", Value());
}

void inbound_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    results.clear();
    if (!db) return;

    Table* idTable = db->getTable("inbound_detail");
    Table* itTable = db->getTable("inbound_transaction");
    Table* pTable = db->getTable("product");
    if (!idTable || !itTable || !pTable) return;

    Vector<Record> idSource;
    if (!indexedColumn.empty())
    {
        if (indexedColumn == "inbound_code")
        {
            Vector<Record> txns = Query::filterRecords(*itTable, "inbound_code", val);
            if (!txns.empty() && txns[0].has("inbound_transaction_id"))
            {
                idSource = Query::filterRecords(*idTable, "inbound_transaction_id", txns[0].get("inbound_transaction_id"));
            }
        }
        else if (indexedColumn == "product_code")
        {
            Vector<Record> prods = Query::filterRecords(*pTable, "product_code", val);
            if (!prods.empty() && prods[0].has("product_id"))
            {
                idSource = Query::filterRecords(*idTable, "product_id", prods[0].get("product_id"));
            }
        }
        else
        {
            idSource = Query::filterRecords(*idTable, indexedColumn, val);
        }
    }
    else
    {
        for (size_t i = 0; i < idTable->getRows().size(); ++i)
        {
            if (!idTable->isRowDeleted(i)) idSource.push_back(idTable->getRows()[i]);
        }
    }

    HashMap<Value, Record> itMap;
    const auto& itRows = itTable->getRows();
    for (size_t i = 0; i < itRows.size(); ++i)
    {
        if (itTable->isRowDeleted(i)) continue;
        if (itRows[i].has("inbound_transaction_id"))
        {
            itMap[itRows[i].get("inbound_transaction_id")] = itRows[i];
        }
    }

    HashMap<Value, Record> prodMap;
    const auto& pRows = pTable->getRows();
    for (size_t i = 0; i < pRows.size(); ++i)
    {
        if (pTable->isRowDeleted(i)) continue;
        if (pRows[i].has("product_id"))
        {
            prodMap[pRows[i].get("product_id")] = pRows[i];
        }
    }

    for (const auto& id : idSource)
    {
        if (!id.has("inbound_transaction_id") || !id.has("product_id")) continue;

        auto itit = itMap.find(id.get("inbound_transaction_id"));
        auto pit = prodMap.find(id.get("product_id"));
        if (itit != itMap.end() && pit != prodMap.end())
        {
            const auto& it = itit->second;
            const auto& p = pit->second;
            Record out;
            if (id.has("inbound_detail_id"))
            {
                out.set("inbound_detail.inbound_detail_id", id.get("inbound_detail_id"));
                out.set("inbound_detail_id", id.get("inbound_detail_id"));
            }
            if (it.has("inbound_code")) out.set("inbound_transaction.inbound_code", it.get("inbound_code"));
            if (p.has("product_code")) out.set("product.product_code", p.get("product_code"));
            if (p.has("product_name")) out.set("product.product_name", p.get("product_name"));
            if (id.has("ordered_quantity")) out.set("inbound_detail.ordered_quantity", id.get("ordered_quantity"));
            results.push_back(std::move(out));
        }
    }
}

// ============================================================
// 7. DELIVERY QUERY (Index: delivery.delivery_at, expected_at)
// ============================================================
void delivery_query(Database *db, Vector<Record> &results)
{
    delivery_query(db, results, "", Value());
}

void delivery_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    delivery_query(db, results, indexedColumn, val, val);
}

void delivery_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal)
{
    results.clear();
    if (!db) return;

    Table* dTable = db->getTable("delivery");
    Table* itTable = db->getTable("inbound_transaction");
    if (!dTable || !itTable) return;

    Vector<Record> dSource;
    if (!indexedColumn.empty())
    {
        dSource = Query::filterRecords(*dTable, indexedColumn, minVal, maxVal);
    }
    else
    {
        for (size_t i = 0; i < dTable->getRows().size(); ++i)
        {
            if (!dTable->isRowDeleted(i)) dSource.push_back(dTable->getRows()[i]);
        }
    }

    HashMap<Value, Record> inTxnMap;
    const auto& itRows = itTable->getRows();
    for (size_t i = 0; i < itRows.size(); ++i)
    {
        if (itTable->isRowDeleted(i)) continue;
        if (itRows[i].has("inbound_transaction_id"))
        {
            inTxnMap[itRows[i].get("inbound_transaction_id")] = itRows[i];
        }
    }

    for (const auto& d : dSource)
    {
        Record out;
        if (d.has("delivery_id"))
        {
            out.set("delivery.delivery_id", d.get("delivery_id"));
            out.set("delivery_id", d.get("delivery_id"));
        }
        if (d.has("delivery_code")) out.set("delivery.delivery_code", d.get("delivery_code"));

        if (d.has("inbound_transaction_id"))
        {
            auto it = inTxnMap.find(d.get("inbound_transaction_id"));
            if (it != inTxnMap.end() && it->second.has("inbound_code"))
            {
                out.set("inbound_transaction.inbound_code", it->second.get("inbound_code"));
            }
        }

        if (d.has("delivery_at")) out.set("delivery.delivery_at", d.get("delivery_at"));
        if (d.has("expected_at")) out.set("delivery.expected_at", d.get("expected_at"));
        if (d.has("status")) out.set("delivery.status", d.get("status"));

        results.push_back(std::move(out));
    }
}

// ============================================================
// 8. DELIVERY DETAIL QUERY (Index: delivery.delivery_at, expected_at)
// ============================================================
void delivery_detail_query(Database *db, Vector<Record> &results)
{
    delivery_detail_query(db, results, "", Value());
}

void delivery_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    delivery_detail_query(db, results, indexedColumn, val, val);
}

void delivery_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal)
{
    results.clear();
    if (!db) return;

    Table* ddTable = db->getTable("delivery_detail");
    Table* dTable = db->getTable("delivery");
    Table* pTable = db->getTable("product");
    if (!ddTable || !dTable || !pTable) return;

    Vector<Record> dSource;
    Vector<Record> pSource;
    bool filterOnProduct = (indexedColumn == "product_code");

    if (!indexedColumn.empty())
    {
        if (filterOnProduct)
        {
            pSource = Query::filterRecords(*pTable, "product_code", minVal);
            for (size_t i = 0; i < dTable->getRows().size(); ++i)
            {
                if (!dTable->isRowDeleted(i)) dSource.push_back(dTable->getRows()[i]);
            }
        }
        else
        {
            dSource = Query::filterRecords(*dTable, indexedColumn, minVal, maxVal);
        }
    }
    else
    {
        for (size_t i = 0; i < dTable->getRows().size(); ++i)
        {
            if (!dTable->isRowDeleted(i)) dSource.push_back(dTable->getRows()[i]);
        }
    }

    HashMap<Value, Record> dMap;
    for (const auto& d : dSource)
    {
        if (d.has("delivery_id"))
        {
            dMap[d.get("delivery_id")] = d;
        }
    }

    HashMap<Value, Record> prodMap;
    if (filterOnProduct)
    {
        for (const auto& p : pSource)
        {
            if (p.has("product_id"))
            {
                prodMap[p.get("product_id")] = p;
            }
        }
    }
    else
    {
        const auto& pRows = pTable->getRows();
        for (size_t i = 0; i < pRows.size(); ++i)
        {
            if (pTable->isRowDeleted(i)) continue;
            if (pRows[i].has("product_id"))
            {
                prodMap[pRows[i].get("product_id")] = pRows[i];
            }
        }
    }

    const auto& ddRows = ddTable->getRows();
    for (size_t i = 0; i < ddRows.size(); ++i)
    {
        if (ddTable->isRowDeleted(i)) continue;
        const auto& dd = ddRows[i];
        if (!dd.has("delivery_id") || !dd.has("product_id")) continue;

        auto dit = dMap.find(dd.get("delivery_id"));
        auto pit = prodMap.find(dd.get("product_id"));
        if (dit != dMap.end() && pit != prodMap.end())
        {
            const auto& d = dit->second;
            const auto& p = pit->second;
            Record out;
            if (dd.has("id"))
            {
                out.set("delivery_detail.id", dd.get("id"));
                out.set("delivery_detail_id", dd.get("id"));
                out.set("id", dd.get("id"));
            }
            if (d.has("delivery_code")) out.set("delivery.delivery_code", d.get("delivery_code"));
            if (p.has("product_code")) out.set("product.product_code", p.get("product_code"));
            if (p.has("product_name")) out.set("product.product_name", p.get("product_name"));
            if (dd.has("received_quantity")) out.set("delivery_detail.received_quantity", dd.get("received_quantity"));
            results.push_back(std::move(out));
        }
    }
}

// ============================================================
// 9. OUTBOUND TRANSACTION QUERY (Index: outbound_transaction.order_at)
// ============================================================
void outbound_transaction_query(Database *db, Vector<Record> &results)
{
    outbound_transaction_query(db, results, "", Value());
}

void outbound_transaction_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    results.clear();
    if (!db) return;

    Table* otTable = db->getTable("outbound_transaction");
    Table* cTable = db->getTable("customer");
    if (!otTable || !cTable) return;

    Vector<Record> otSource;
    if (!indexedColumn.empty())
    {
        otSource = Query::filterRecords(*otTable, indexedColumn, val);
    }
    else
    {
        for (size_t i = 0; i < otTable->getRows().size(); ++i)
        {
            if (!otTable->isRowDeleted(i)) otSource.push_back(otTable->getRows()[i]);
        }
    }

    HashMap<Value, Record> custMap;
    const auto& cRows = cTable->getRows();
    for (size_t i = 0; i < cRows.size(); ++i)
    {
        if (cTable->isRowDeleted(i)) continue;
        if (cRows[i].has("customer_id"))
        {
            custMap[cRows[i].get("customer_id")] = cRows[i];
        }
    }

    for (const auto& ot : otSource)
    {
        if (!ot.has("customer_id")) continue;
        auto cit = custMap.find(ot.get("customer_id"));
        if (cit != custMap.end())
        {
            const auto& c = cit->second;
            Record out;
            Value otId;
            if (ot.has("outbound_trainsaction_id")) otId = ot.get("outbound_trainsaction_id");
            else if (ot.has("outbound_transaction_id")) otId = ot.get("outbound_transaction_id");
            if (!holds_alternative<monostate>(otId))
            {
                out.set("outbound_transaction.outbound_transaction_id", otId);
                out.set("outbound_transaction_id", otId);
                out.set("outbound_id", otId);
            }
            if (ot.has("outbound_code")) out.set("outbound_transaction.outbound_code", ot.get("outbound_code"));
            if (c.has("customer_code")) out.set("customer.customer_code", c.get("customer_code"));
            if (ot.has("order_at")) out.set("outbound_transaction.order_at", ot.get("order_at"));
            if (ot.has("type")) out.set("outbound_transaction.type", ot.get("type"));
            if (ot.has("status")) out.set("outbound_transaction.status", ot.get("status"));
            if (ot.has("note")) out.set("outbound_transaction.note", ot.get("note"));
            results.push_back(std::move(out));
        }
    }
}

void outbound_transaction_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal)
{
    results.clear();
    if (!db) return;

    Table* otTable = db->getTable("outbound_transaction");
    Table* cTable = db->getTable("customer");
    if (!otTable || !cTable) return;

    Vector<Record> otSource = Query::filterRecords(*otTable, indexedColumn, minVal, maxVal);

    HashMap<Value, Record> custMap;
    const auto& cRows = cTable->getRows();
    for (size_t i = 0; i < cRows.size(); ++i)
    {
        if (cTable->isRowDeleted(i)) continue;
        if (cRows[i].has("customer_id"))
        {
            custMap[cRows[i].get("customer_id")] = cRows[i];
        }
    }

    for (const auto& ot : otSource)
    {
        if (!ot.has("customer_id")) continue;
        auto cit = custMap.find(ot.get("customer_id"));
        if (cit != custMap.end())
        {
            const auto& c = cit->second;
            Record out;
            Value otId;
            if (ot.has("outbound_trainsaction_id")) otId = ot.get("outbound_trainsaction_id");
            else if (ot.has("outbound_transaction_id")) otId = ot.get("outbound_transaction_id");
            if (!holds_alternative<monostate>(otId))
            {
                out.set("outbound_transaction.outbound_transaction_id", otId);
                out.set("outbound_transaction_id", otId);
                out.set("outbound_id", otId);
            }
            if (ot.has("outbound_code")) out.set("outbound_transaction.outbound_code", ot.get("outbound_code"));
            if (c.has("customer_code")) out.set("customer.customer_code", c.get("customer_code"));
            if (ot.has("order_at")) out.set("outbound_transaction.order_at", ot.get("order_at"));
            if (ot.has("type")) out.set("outbound_transaction.type", ot.get("type"));
            if (ot.has("status")) out.set("outbound_transaction.status", ot.get("status"));
            if (ot.has("note")) out.set("outbound_transaction.note", ot.get("note"));
            results.push_back(std::move(out));
        }
    }
}

// ============================================================
// 10. OUTBOUND DETAIL QUERY
// ============================================================
void outbound_detail_query(Database *db, Vector<Record> &results)
{
    outbound_detail_query(db, results, "", Value());
}

void outbound_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    results.clear();
    if (!db) return;

    Table* odTable = db->getTable("outbound_detail");
    Table* otTable = db->getTable("outbound_transaction");
    Table* pTable = db->getTable("product");
    if (!odTable || !otTable || !pTable) return;

    Vector<Record> odSource;
    if (!indexedColumn.empty())
    {
        if (indexedColumn == "outbound_code")
        {
            Vector<Record> txns = Query::filterRecords(*otTable, "outbound_code", val);
            if (!txns.empty())
            {
                Value otId;
                if (txns[0].has("outbound_trainsaction_id")) otId = txns[0].get("outbound_trainsaction_id");
                else if (txns[0].has("outbound_transaction_id")) otId = txns[0].get("outbound_transaction_id");
                if (!holds_alternative<monostate>(otId))
                {
                    odSource = Query::filterRecords(*odTable, "outbound_transaction_id", otId);
                }
            }
        }
        else if (indexedColumn == "product_code")
        {
            Vector<Record> prods = Query::filterRecords(*pTable, "product_code", val);
            if (!prods.empty() && prods[0].has("product_id"))
            {
                odSource = Query::filterRecords(*odTable, "product_id", prods[0].get("product_id"));
            }
        }
        else
        {
            odSource = Query::filterRecords(*odTable, indexedColumn, val);
        }
    }
    else
    {
        for (size_t i = 0; i < odTable->getRows().size(); ++i)
        {
            if (!odTable->isRowDeleted(i)) odSource.push_back(odTable->getRows()[i]);
        }
    }

    HashMap<Value, Record> otMap;
    const auto& otRows = otTable->getRows();
    for (size_t i = 0; i < otRows.size(); ++i)
    {
        if (otTable->isRowDeleted(i)) continue;
        if (otRows[i].has("outbound_trainsaction_id"))
        {
            otMap[otRows[i].get("outbound_trainsaction_id")] = otRows[i];
        }
        else if (otRows[i].has("outbound_transaction_id"))
        {
            otMap[otRows[i].get("outbound_transaction_id")] = otRows[i];
        }
    }

    HashMap<Value, Record> prodMap;
    const auto& pRows = pTable->getRows();
    for (size_t i = 0; i < pRows.size(); ++i)
    {
        if (pTable->isRowDeleted(i)) continue;
        if (pRows[i].has("product_id"))
        {
            prodMap[pRows[i].get("product_id")] = pRows[i];
        }
    }

    for (const auto& od : odSource)
    {
        if (!od.has("outbound_transaction_id") || !od.has("product_id")) continue;

        auto otit = otMap.find(od.get("outbound_transaction_id"));
        auto pit = prodMap.find(od.get("product_id"));
        if (otit != otMap.end() && pit != prodMap.end())
        {
            const auto& ot = otit->second;
            const auto& p = pit->second;
            Record out;
            if (od.has("outbound_detail_id"))
            {
                out.set("outbound_detail.outbound_detail_id", od.get("outbound_detail_id"));
                out.set("outbound_detail_id", od.get("outbound_detail_id"));
            }
            if (ot.has("outbound_code")) out.set("outbound_transaction.outbound_code", ot.get("outbound_code"));
            if (p.has("product_code")) out.set("product.product_code", p.get("product_code"));
            if (p.has("product_name")) out.set("product.product_name", p.get("product_name"));
            if (od.has("ordered_quantity")) out.set("outbound_detail.ordered_quantity", od.get("ordered_quantity"));
            results.push_back(std::move(out));
        }
    }
}

// ============================================================
// 11. SHIPMENT QUERY (Index: shipment.ship_at, expected_at)
// ============================================================
void shipment_query(Database *db, Vector<Record> &results)
{
    shipment_query(db, results, "", Value());
}

void shipment_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    shipment_query(db, results, indexedColumn, val, val);
}

void shipment_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal)
{
    results.clear();
    if (!db) return;

    Table* sTable = db->getTable("shipment");
    Table* otTable = db->getTable("outbound_transaction");
    if (!sTable || !otTable) return;

    Vector<Record> sSource;
    if (!indexedColumn.empty())
    {
        sSource = Query::filterRecords(*sTable, indexedColumn, minVal, maxVal);
    }
    else
    {
        for (size_t i = 0; i < sTable->getRows().size(); ++i)
        {
            if (!sTable->isRowDeleted(i)) sSource.push_back(sTable->getRows()[i]);
        }
    }

    HashMap<Value, Record> outTxnMap;
    const auto& otRows = otTable->getRows();
    for (size_t i = 0; i < otRows.size(); ++i)
    {
        if (otTable->isRowDeleted(i)) continue;
        const auto& ot = otRows[i];
        if (ot.has("outbound_trainsaction_id"))
        {
            outTxnMap[ot.get("outbound_trainsaction_id")] = ot;
        }
        else if (ot.has("outbound_transaction_id"))
        {
            outTxnMap[ot.get("outbound_transaction_id")] = ot;
        }
    }

    for (const auto& s : sSource)
    {
        Record out;
        if (s.has("shipment_id"))
        {
            out.set("shipment.shipment_id", s.get("shipment_id"));
            out.set("shipment_id", s.get("shipment_id"));
        }
        if (s.has("shipment_code")) out.set("shipment.shipment_code", s.get("shipment_code"));

        Value otId;
        if (s.has("sell_id")) otId = s.get("sell_id");
        else if (s.has("outbound_transaction_id")) otId = s.get("outbound_transaction_id");

        if (!holds_alternative<monostate>(otId))
        {
            auto it = outTxnMap.find(otId);
            if (it != outTxnMap.end() && it->second.has("outbound_code"))
            {
                out.set("outbound_transaction.outbound_code", it->second.get("outbound_code"));
            }
        }

        if (s.has("ship_at")) out.set("shipment.ship_at", s.get("ship_at"));
        if (s.has("expected_at")) out.set("shipment.expected_at", s.get("expected_at"));
        if (s.has("status")) out.set("shipment.status", s.get("status"));

        results.push_back(std::move(out));
    }
}

// ============================================================
// 12. SHIPMENT DETAIL QUERY (Index: shipment.ship_at, expected_at)
// ============================================================
void shipment_detail_query(Database *db, Vector<Record> &results)
{
    shipment_detail_query(db, results, "", Value());
}

void shipment_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    shipment_detail_query(db, results, indexedColumn, val, val);
}

void shipment_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal)
{
    results.clear();
    if (!db) return;

    Table* sdTable = db->getTable("shipment_detail");
    Table* sTable = db->getTable("shipment");
    Table* pTable = db->getTable("product");
    if (!sdTable || !sTable || !pTable) return;

    Vector<Record> sSource;
    Vector<Record> pSource;
    bool filterOnProduct = (indexedColumn == "product_code");

    if (!indexedColumn.empty())
    {
        if (filterOnProduct)
        {
            pSource = Query::filterRecords(*pTable, "product_code", minVal);
            for (size_t i = 0; i < sTable->getRows().size(); ++i)
            {
                if (!sTable->isRowDeleted(i)) sSource.push_back(sTable->getRows()[i]);
            }
        }
        else
        {
            sSource = Query::filterRecords(*sTable, indexedColumn, minVal, maxVal);
        }
    }
    else
    {
        for (size_t i = 0; i < sTable->getRows().size(); ++i)
        {
            if (!sTable->isRowDeleted(i)) sSource.push_back(sTable->getRows()[i]);
        }
    }

    HashMap<Value, Record> sMap;
    for (const auto& s : sSource)
    {
        if (s.has("shipment_id"))
        {
            sMap[s.get("shipment_id")] = s;
        }
    }

    HashMap<Value, Record> prodMap;
    if (filterOnProduct)
    {
        for (const auto& p : pSource)
        {
            if (p.has("product_id"))
            {
                prodMap[p.get("product_id")] = p;
            }
        }
    }
    else
    {
        const auto& pRows = pTable->getRows();
        for (size_t i = 0; i < pRows.size(); ++i)
        {
            if (pTable->isRowDeleted(i)) continue;
            if (pRows[i].has("product_id"))
            {
                prodMap[pRows[i].get("product_id")] = pRows[i];
            }
        }
    }

    const auto& sdRows = sdTable->getRows();
    for (size_t i = 0; i < sdRows.size(); ++i)
    {
        if (sdTable->isRowDeleted(i)) continue;
        const auto& sd = sdRows[i];
        if (!sd.has("shipment_id") || !sd.has("product_id")) continue;

        auto sit = sMap.find(sd.get("shipment_id"));
        auto pit = prodMap.find(sd.get("product_id"));
        if (sit != sMap.end() && pit != prodMap.end())
        {
            const auto& s = sit->second;
            const auto& p = pit->second;
            Record out;
            if (sd.has("id"))
            {
                out.set("shipment_detail.id", sd.get("id"));
                out.set("shipment_detail_id", sd.get("id"));
                out.set("id", sd.get("id"));
            }
            if (s.has("shipment_code")) out.set("shipment.shipment_code", s.get("shipment_code"));
            if (p.has("product_code")) out.set("product.product_code", p.get("product_code"));
            if (p.has("product_name")) out.set("product.product_name", p.get("product_name"));
            if (sd.has("received_quantity")) out.set("shipment_detail.received_quantity", sd.get("received_quantity"));
            results.push_back(std::move(out));
        }
    }
}


// ============================================================
// 12. CÁC BẢNG PHÂN LOẠI SẢN PHẨM (CATEGORY HARDWARE QUERIES)
// - Trả về: product_code, product_name, các thuộc tính của danh mục, và is_sold (đang bán)
// - Hỗ trợ lọc theo is_sold và các thuộc tính category có index (HashTree)
// ============================================================
void category_hardware_query(Database *db, const string& categoryName, Vector<Record> &results)
{
    category_hardware_query(db, categoryName, results, "", Value());
}

void category_hardware_query(Database *db, const string& categoryName, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    results.clear();
    if (!db) return;

    string cat = categoryName;
    for (char& c : cat) c = tolower(c);

    Table* catTable = db->getTable(cat);
    Table* prodTable = db->getTable("product");
    if (!catTable || !prodTable) return;

    // Chuẩn hóa tên cột lọc
    string col = indexedColumn;
    if (col.rfind(cat + ".", 0) == 0) col = col.substr(cat.length() + 1);
    else if (col.rfind("product.", 0) == 0) col = col.substr(8);

    bool filterOnSold = (col == "is_sold");

    // Chuẩn bị map sản phẩm (product_id -> product record)
    HashMap<Value, Record> prodMap;
    if (filterOnSold)
    {
        Value soldVal = val;
        if (holds_alternative<string>(val))
        {
            string s = get<string>(val);
            for (char& c : s) c = tolower(c);
            if (s == "true" || s == "1" || s == "đang bán") soldVal = true;
            else if (s == "false" || s == "0" || s == "ngừng bán") soldVal = false;
        }

        Vector<Record> pFiltered = Query::filterRecords(*prodTable, "is_sold", soldVal);
        for (size_t i = 0; i < pFiltered.size(); ++i)
        {
            if (pFiltered[i].has("product_id"))
            {
                prodMap[pFiltered[i].get("product_id")] = pFiltered[i];
            }
        }
    }
    else
    {
        const auto& pRows = prodTable->getRows();
        for (size_t i = 0; i < pRows.size(); ++i)
        {
            if (prodTable->isRowDeleted(i)) continue;
            if (pRows[i].has("product_id"))
            {
                prodMap[pRows[i].get("product_id")] = pRows[i];
            }
        }
    }

    // Lấy danh sách bản ghi category
    Vector<Record> catSource;
    if (!col.empty() && !filterOnSold)
    {
        catSource = Query::filterRecords(*catTable, col, val);
    }
    else
    {
        const auto& cRows = catTable->getRows();
        for (size_t i = 0; i < cRows.size(); ++i)
        {
            if (!catTable->isRowDeleted(i))
            {
                catSource.push_back(cRows[i]);
            }
        }
    }

    // Ghép dữ liệu
    for (size_t i = 0; i < catSource.size(); ++i)
    {
        const auto& crow = catSource[i];
        if (!crow.has("product_id")) continue;

        auto pit = prodMap.find(crow.get("product_id"));
        if (pit == prodMap.end()) continue;
        const auto& prow = pit->second;

        Record out;
        if (crow.has(cat + "_id"))
        {
            out.set(cat + "." + cat + "_id", crow.get(cat + "_id"));
            out.set(cat + "_id", crow.get(cat + "_id"));
        }
        if (prow.has("product_code")) out.set("product.product_code", prow.get("product_code"));
        if (prow.has("product_name")) out.set("product.product_name", prow.get("product_name"));

        const auto& catCols = catTable->getColumns();
        for (size_t c = 0; c < catCols.size(); ++c)
        {
            string colName = catCols[c].getName();
            if (colName == cat + "_id" || colName == "id" || colName == "product_id") continue;
            if (cat == "case" && colName == "product_name") continue;

            if (crow.has(colName))
            {
                out.set(cat + "." + colName, crow.get(colName));
            }
        }

        if (prow.has("is_sold")) out.set("product.is_sold", prow.get("is_sold"));

        results.push_back(std::move(out));
    }
}

void cpu_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val) {
    category_hardware_query(db, "cpu", results, indexedColumn, val);
}
void gpu_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val) {
    category_hardware_query(db, "gpu", results, indexedColumn, val);
}
void mainboard_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val) {
    category_hardware_query(db, "mainboard", results, indexedColumn, val);
}
void ram_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val) {
    category_hardware_query(db, "ram", results, indexedColumn, val);
}
void ssd_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val) {
    category_hardware_query(db, "ssd", results, indexedColumn, val);
}
void psu_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val) {
    category_hardware_query(db, "psu", results, indexedColumn, val);
}
void cooling_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val) {
    category_hardware_query(db, "cooling", results, indexedColumn, val);
}
void case_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val) {
    category_hardware_query(db, "case", results, indexedColumn, val);
}

// ============================================================
// 13. TRUY VẤN PHỤ CHO HIỂN THỊ SPECS, CATEGORY, BRAND
// - Hiển thị danh sách specs của các category (vd: cpu_socket, ssd_capacity,...)
// - Hiển thị bảng category, bảng brand
// - Không cần index vì dữ liệu ít
// ============================================================
void auxiliary_spec_query(Database *db, const string& tableName, Vector<Record> &results, const string& filterColumn, const Value& val)
{
    results.clear();
    if (!db) return;

    string tblName = tableName;
    for (char& c : tblName) c = tolower(c);

    Table* table = db->getTable(tblName);
    if (!table) return;

    string col = filterColumn;
    if (col.rfind(tblName + ".", 0) == 0) col = col.substr(tblName.length() + 1);

    bool hasFilter = !col.empty() && val.index() != 0;

    const auto& rows = table->getRows();
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (table->isRowDeleted(i)) continue;
        const auto& r = rows[i];
        if (hasFilter)
        {
            if (!r.has(col) || !(r.get(col) == val)) continue;
        }

        Record out;
        const auto& tblCols = table->getColumns();
        for (size_t c = 0; c < tblCols.size(); ++c)
        {
            string cname = tblCols[c].getName();
            if (r.has(cname))
            {
                out.set(tblName + "." + cname, r.get(cname));
                out.set(cname, r.get(cname));
            }
        }
        results.push_back(std::move(out));
    }
}

// ============================================================
// 14. TRUY VẤN KHÁCH HÀNG (CUSTOMER QUERIES - Rule 9.4)
// ============================================================
void individual_customer_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    results.clear();
    if (!db) return;

    Table* cTable = db->getTable("customer");
    Table* iTable = db->getTable("individual_info");
    if (!cTable || !iTable) return;

    string col = indexedColumn;
    if (col.rfind("customer.", 0) == 0) col = col.substr(9);
    else if (col.rfind("individual_info.", 0) == 0) col = col.substr(16);

    bool hasFilter = !col.empty() && !holds_alternative<monostate>(val);

    HashMap<Value, Record> infoMap;
    const auto& iRows = iTable->getRows();
    for (size_t i = 0; i < iRows.size(); ++i)
    {
        if (iTable->isRowDeleted(i)) continue;
        if (iRows[i].has("customer_id"))
        {
            infoMap[iRows[i].get("customer_id")] = iRows[i];
        }
    }

    Vector<Record> cSource;
    if (hasFilter && col == "customer_code")
    {
        cSource = Query::filterRecords(*cTable, "customer_code", val);
    }
    else if (hasFilter && col == "customer_id")
    {
        cSource = Query::filterRecords(*cTable, "customer_id", val);
    }
    else
    {
        const auto& cRows = cTable->getRows();
        for (size_t i = 0; i < cRows.size(); ++i)
        {
            if (!cTable->isRowDeleted(i))
            {
                cSource.push_back(cRows[i]);
            }
        }
    }

    for (size_t i = 0; i < cSource.size(); ++i)
    {
        const auto& c = cSource[i];
        if (!c.has("customer_id")) continue;

        if (c.has("type"))
        {
            string t = valueToString(c.get("type"));
            for (char& ch : t) ch = tolower(ch);
            if (t != "individual") continue;
        }

        auto it = infoMap.find(c.get("customer_id"));
        if (it == infoMap.end()) continue;
        const auto& info = it->second;

        if (hasFilter && col != "customer_code" && col != "customer_id")
        {
            Value checkVal;
            if (col == "name" && c.has("name")) checkVal = c.get("name");
            else if (col == "phone_number" && info.has("phone_number")) checkVal = info.get("phone_number");
            else if (col == "email" && info.has("email")) checkVal = info.get("email");
            else if (c.has(col)) checkVal = c.get(col);
            else if (info.has(col)) checkVal = info.get(col);

            if (checkVal != val && valueToString(checkVal) != valueToString(val)) continue;
        }

        Record out;
        out.set("customer.customer_id", c.get("customer_id"));
        out.set("customer_id", c.get("customer_id"));

        if (c.has("customer_code"))
        {
            out.set("customer.customer_code", c.get("customer_code"));
            out.set("customer_code", c.get("customer_code"));
        }
        if (c.has("name"))
        {
            out.set("customer.name", c.get("name"));
            out.set("name", c.get("name"));
        }
        if (info.has("phone_number"))
        {
            out.set("individual_info.phone_number", info.get("phone_number"));
            out.set("phone_number", info.get("phone_number"));
        }
        if (info.has("email"))
        {
            out.set("individual_info.email", info.get("email"));
            out.set("email", info.get("email"));
        }

        results.push_back(std::move(out));
    }
}

void business_customer_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    results.clear();
    if (!db) return;

    Table* cTable = db->getTable("customer");
    Table* bTable = db->getTable("business_info");
    if (!cTable || !bTable) return;

    string col = indexedColumn;
    if (col.rfind("customer.", 0) == 0) col = col.substr(9);
    else if (col.rfind("business_info.", 0) == 0) col = col.substr(14);

    bool hasFilter = !col.empty() && !holds_alternative<monostate>(val);

    HashMap<Value, Record> bMap;
    const auto& bRows = bTable->getRows();
    for (size_t i = 0; i < bRows.size(); ++i)
    {
        if (bTable->isRowDeleted(i)) continue;
        if (bRows[i].has("customer_id"))
        {
            bMap[bRows[i].get("customer_id")] = bRows[i];
        }
    }

    Vector<Record> cSource;
    if (hasFilter && col == "customer_code")
    {
        cSource = Query::filterRecords(*cTable, "customer_code", val);
    }
    else if (hasFilter && col == "customer_id")
    {
        cSource = Query::filterRecords(*cTable, "customer_id", val);
    }
    else
    {
        const auto& cRows = cTable->getRows();
        for (size_t i = 0; i < cRows.size(); ++i)
        {
            if (!cTable->isRowDeleted(i))
            {
                cSource.push_back(cRows[i]);
            }
        }
    }

    for (size_t i = 0; i < cSource.size(); ++i)
    {
        const auto& c = cSource[i];
        if (!c.has("customer_id")) continue;

        if (c.has("type"))
        {
            string t = valueToString(c.get("type"));
            for (char& ch : t) ch = tolower(ch);
            if (t != "business") continue;
        }

        auto it = bMap.find(c.get("customer_id"));
        if (it == bMap.end()) continue;
        const auto& bInfo = it->second;

        if (hasFilter && col != "customer_code" && col != "customer_id")
        {
            Value checkVal;
            if (col == "name" && c.has("name")) checkVal = c.get("name");
            else if (col == "tax_code" && bInfo.has("tax_code")) checkVal = bInfo.get("tax_code");
            else if (col == "phone_number" && bInfo.has("phone_number")) checkVal = bInfo.get("phone_number");
            else if (col == "email" && bInfo.has("email")) checkVal = bInfo.get("email");
            else if (c.has(col)) checkVal = c.get(col);
            else if (bInfo.has(col)) checkVal = bInfo.get(col);

            if (checkVal != val && valueToString(checkVal) != valueToString(val)) continue;
        }

        Record out;
        out.set("customer.customer_id", c.get("customer_id"));
        out.set("customer_id", c.get("customer_id"));

        if (c.has("customer_code"))
        {
            out.set("customer.customer_code", c.get("customer_code"));
            out.set("customer_code", c.get("customer_code"));
        }
        if (c.has("name"))
        {
            out.set("customer.name", c.get("name"));
            out.set("name", c.get("name"));
        }
        if (bInfo.has("tax_code"))
        {
            out.set("business_info.tax_code", bInfo.get("tax_code"));
            out.set("tax_code", bInfo.get("tax_code"));
        }
        if (bInfo.has("phone_number"))
        {
            out.set("business_info.phone_number", bInfo.get("phone_number"));
            out.set("phone_number", bInfo.get("phone_number"));
        }
        if (bInfo.has("email"))
        {
            out.set("business_info.email", bInfo.get("email"));
            out.set("email", bInfo.get("email"));
        }

        results.push_back(std::move(out));
    }
}

} // namespace user_query
