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

// ============================================================
// 1. PRODUCT QUERY (Index: product.is_sold)
// ============================================================
void product_query(Database *db, Vector<Record> &results)
{
    product_query(db, results, "", Value());
}

void product_query(Database *db, Vector<Record> &results, bool is_sold)
{
    product_query(db, results, "is_sold", Value(is_sold));
}

void product_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    results.clear();
    if (!db) return;

    Table* pTable = db->getTable("product");
    if (!pTable) return;

    Vector<Record> sourceRecords;
    if (!indexedColumn.empty())
    {
        sourceRecords = Query::filterRecords(*pTable, indexedColumn, val);
    }
    else
    {
        for (size_t i = 0; i < pTable->getRows().size(); ++i)
        {
            if (!pTable->isRowDeleted(i))
            {
                sourceRecords.push_back(pTable->getRows()[i]);
            }
        }
    }

    for (const auto& r : sourceRecords)
    {
        Record out;
        if (r.has("product_code")) out.set("product.product_code", r.get("product_code"));
        if (r.has("product_name")) out.set("product.product_name", r.get("product_name"));
        if (r.has("sku")) out.set("product.sku", r.get("sku"));
        if (r.has("barcode")) out.set("product.barcode", r.get("barcode"));
        results.push_back(std::move(out));
    }
}

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
            if (p.has("product_name")) out.set("product.product_name", p.get("product_name"));
            if (u.has("serial")) out.set("unit_inventory.serial", u.get("serial"));
            if (u.has("status")) out.set("unit_inventory.status", u.get("status"));
            results.push_back(std::move(out));
        }
    }
}

// ============================================================
// 4. STATUS HISTORY QUERY (Index: status_history.type; có cột 'date')
// ============================================================
void status_history_query(Database *db, Vector<Record> &results)
{
    status_history_query(db, results, "", Value());
}

void status_history_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val)
{
    results.clear();
    if (!db) return;

    Table* shTable = db->getTable("status_history");
    Table* uTable = db->getTable("unit_inventory");
    Table* dTable = db->getTable("delivery");
    Table* sTable = db->getTable("shipment");
    if (!shTable || !uTable) return;

    // Map unit_id -> unit_inventory
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

    // Map delivery_id -> delivery_at (cho INBOUND)
    HashMap<Value, Value> delivDateMap;
    if (dTable)
    {
        for (size_t i = 0; i < dTable->getRows().size(); ++i)
        {
            if (dTable->isRowDeleted(i)) continue;
            const auto& d = dTable->getRows()[i];
            if (d.has("delivery_id") && d.has("delivery_at"))
            {
                delivDateMap[d.get("delivery_id")] = d.get("delivery_at");
            }
        }
    }

    // Map shipment_id -> ship_at (cho OUTBOUND)
    HashMap<Value, Value> shipDateMap;
    if (sTable)
    {
        for (size_t i = 0; i < sTable->getRows().size(); ++i)
        {
            if (sTable->isRowDeleted(i)) continue;
            const auto& s = sTable->getRows()[i];
            if (s.has("shipment_id") && s.has("ship_at"))
            {
                shipDateMap[s.get("shipment_id")] = s.get("ship_at");
            }
        }
    }

    Vector<Record> shSource;
    if (!indexedColumn.empty())
    {
        shSource = Query::filterRecords(*shTable, indexedColumn, val);
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
            if (u.has("serial")) out.set("unit_inventory.serial", u.get("serial"));
            if (sh.has("old_status")) out.set("status_history.old_status", sh.get("old_status"));
            if (sh.has("curr_status")) out.set("status_history.curr_status", sh.get("curr_status"));
            if (sh.has("transaction")) out.set("status_history.transaction", sh.get("transaction"));
            if (sh.has("type")) out.set("status_history.type", sh.get("type"));

            // Lấy cột date theo QuanLyKhoHang_schema.json
            Value dateVal;
            if (sh.has("type") && sh.has("transaction"))
            {
                string t = "";
                if (holds_alternative<string>(sh.get("type")))
                {
                    t = get<string>(sh.get("type"));
                }
                if (t == "INBOUND")
                {
                    auto dit = delivDateMap.find(sh.get("transaction"));
                    if (dit != delivDateMap.end()) dateVal = dit->second;
                }
                else if (t == "OUTBOUND")
                {
                    auto sit = shipDateMap.find(sh.get("transaction"));
                    if (sit != shipDateMap.end()) dateVal = sit->second;
                }
            }
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
            if (it.has("inbound_code")) out.set("inbound_transaction.inbound_code", it.get("inbound_code"));
            if (p.has("product_code")) out.set("product.product_code", p.get("product_code"));
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
            if (ot.has("outbound_code")) out.set("outbound_transaction.outbound_code", ot.get("outbound_code"));
            if (p.has("product_code")) out.set("product.product_code", p.get("product_code"));
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
            if (s.has("shipment_code")) out.set("shipment.shipment_code", s.get("shipment_code"));
            if (p.has("product_code")) out.set("product.product_code", p.get("product_code"));
            if (p.has("product_name")) out.set("product.product_name", p.get("product_name"));
            if (sd.has("received_quantity")) out.set("shipment_detail.received_quantity", sd.get("received_quantity"));
            results.push_back(std::move(out));
        }
    }
}

} // namespace user_query
