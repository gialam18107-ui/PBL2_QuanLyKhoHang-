#include "UserQuery.h"
#include "datastructure/hashmap/HashMap.h"

void user_query::product_query(Database *db, Vector<Record> &results, bool is_sold) {
    if (!db) return;
    Table* t = db->getTable("product");
    if (!t) return;
    results.clear();

    const auto& rows = t->getRows();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (t->isRowDeleted(i)) continue;
        const auto& r = rows[i];
        if (r.has("is_sold") && holds_alternative<bool>(r.get("is_sold")) && get<bool>(r.get("is_sold")) == is_sold) {
            Record out;
            if (r.has("product_code")) out.set("product.product_code", r.get("product_code"));
            if (r.has("product_name")) out.set("product.product_name", r.get("product_name"));
            if (r.has("sku")) out.set("product.sku", r.get("sku"));
            if (r.has("barcode")) out.set("product.barcode", r.get("barcode"));
            results.push_back(std::move(out));
        }
    }
}

void user_query::product_quantity_query(Database *db, Vector<Record> &results) {
    if (!db) return;
    Table* pqTable = db->getTable("product_quantity");
    Table* pTable = db->getTable("product");
    if (!pqTable || !pTable) return;
    results.clear();

    HashMap<int, size_t> prodIndex;
    const auto& pRows = pTable->getRows();
    for (size_t i = 0; i < pRows.size(); ++i) {
        if (pTable->isRowDeleted(i)) continue;
        const auto& r = pRows[i];
        if (r.has("product_id") && holds_alternative<int>(r.get("product_id"))) {
            if (r.has("is_sold") && holds_alternative<bool>(r.get("is_sold")) && get<bool>(r.get("is_sold")) == true) {
                prodIndex[get<int>(r.get("product_id"))] = i;
            }
        }
    }

    const auto& pqRows = pqTable->getRows();
    for (size_t i = 0; i < pqRows.size(); ++i) {
        if (pqTable->isRowDeleted(i)) continue;
        const auto& pq = pqRows[i];
        if (!pq.has("product_id") || !holds_alternative<int>(pq.get("product_id"))) continue;
        int pid = get<int>(pq.get("product_id"));
        auto it = prodIndex.find(pid);
        if (it != prodIndex.end()) {
            const auto& p = pRows[it->second];
            Record out;
            if (p.has("product_code")) out.set("product.product_code", p.get("product_code"));
            if (p.has("product_name")) out.set("product.product_name", p.get("product_name"));
            if (pq.has("available_quan")) out.set("product_quantity.available_quan", pq.get("available_quan"));
            if (pq.has("warranty_quan")) out.set("product_quantity.warranty_quan", pq.get("warranty_quan"));
            if (pq.has("scrapped_quan")) out.set("product_quantity.scrapped_quan", pq.get("scrapped_quan"));
            results.push_back(std::move(out));
        }
    }
}

void user_query::unit_inventory_query(Database *db, Vector<Record> &results) {
    if (!db) return;
    Table* uTable = db->getTable("unit_inventory");
    Table* pTable = db->getTable("product");
    if (!uTable || !pTable) return;
    results.clear();

    HashMap<int, size_t> prodIndex;
    const auto& pRows = pTable->getRows();
    for (size_t i = 0; i < pRows.size(); ++i) {
        if (pTable->isRowDeleted(i)) continue;
        const auto& r = pRows[i];
        if (r.has("product_id") && holds_alternative<int>(r.get("product_id"))) {
            prodIndex[get<int>(r.get("product_id"))] = i;
        }
    }

    const auto& uRows = uTable->getRows();
    for (size_t i = 0; i < uRows.size(); ++i) {
        if (uTable->isRowDeleted(i)) continue;
        const auto& u = uRows[i];
        if (!u.has("product_id") || !holds_alternative<int>(u.get("product_id"))) continue;
        int pid = get<int>(u.get("product_id"));
        auto it = prodIndex.find(pid);
        if (it != prodIndex.end()) {
            const auto& p = pRows[it->second];
            Record out;
            if (p.has("product_name")) out.set("product.product_name", p.get("product_name"));
            if (u.has("serial")) out.set("unit_inventory.serial", u.get("serial"));
            if (u.has("status")) out.set("unit_inventory.status", u.get("status"));
            results.push_back(std::move(out));
        }
    }
}

void user_query::status_history_query(Database *db, Vector<Record> &results) {
    if (!db) return;
    Table* shTable = db->getTable("status_history");
    Table* uTable = db->getTable("unit_inventory");
    if (!shTable || !uTable) return;
    results.clear();

    HashMap<int, size_t> unitIndex;
    const auto& uRows = uTable->getRows();
    for (size_t i = 0; i < uRows.size(); ++i) {
        if (uTable->isRowDeleted(i)) continue;
        const auto& r = uRows[i];
        if (r.has("unit_id") && holds_alternative<int>(r.get("unit_id"))) {
            unitIndex[get<int>(r.get("unit_id"))] = i;
        }
    }

    const auto& shRows = shTable->getRows();
    for (size_t i = 0; i < shRows.size(); ++i) {
        if (shTable->isRowDeleted(i)) continue;
        const auto& sh = shRows[i];
        if (!sh.has("unit_id") || !holds_alternative<int>(sh.get("unit_id"))) continue;
        int uid = get<int>(sh.get("unit_id"));
        auto it = unitIndex.find(uid);
        if (it != unitIndex.end()) {
            const auto& u = uRows[it->second];
            Record out;
            if (u.has("serial")) out.set("unit_inventory.serial", u.get("serial"));
            if (sh.has("old_status")) out.set("status_history.old_status", sh.get("old_status"));
            if (sh.has("curr_status")) out.set("status_history.curr_status", sh.get("curr_status"));
            if (sh.has("transaction")) out.set("status_history.transaction", sh.get("transaction"));
            if (sh.has("type")) out.set("status_history.type", sh.get("type"));
            results.push_back(std::move(out));
        }
    }
}

void user_query::shipment_detail_query(Database *db, Vector<Record> &results) {
    if (!db) return;
    Table* sdTable = db->getTable("shipment_detail");
    Table* sTable = db->getTable("shipment");
    Table* pTable = db->getTable("product");
    if (!sdTable || !sTable || !pTable) return;
    results.clear();

    HashMap<int, size_t> shipmentIndex;
    const auto& sRows = sTable->getRows();
    for (size_t i = 0; i < sRows.size(); ++i) {
        if (sTable->isRowDeleted(i)) continue;
        if (sRows[i].has("shipment_id") && holds_alternative<int>(sRows[i].get("shipment_id"))) {
            shipmentIndex[get<int>(sRows[i].get("shipment_id"))] = i;
        }
    }

    HashMap<int, size_t> prodIndex;
    const auto& pRows = pTable->getRows();
    for (size_t i = 0; i < pRows.size(); ++i) {
        if (pTable->isRowDeleted(i)) continue;
        if (pRows[i].has("product_id") && holds_alternative<int>(pRows[i].get("product_id"))) {
            prodIndex[get<int>(pRows[i].get("product_id"))] = i;
        }
    }

    const auto& sdRows = sdTable->getRows();
    for (size_t i = 0; i < sdRows.size(); ++i) {
        if (sdTable->isRowDeleted(i)) continue;
        const auto& sd = sdRows[i];
        if (!sd.has("shipment_id") || !sd.has("product_id")) continue;
        if (!holds_alternative<int>(sd.get("shipment_id")) || !holds_alternative<int>(sd.get("product_id"))) continue;

        auto sit = shipmentIndex.find(get<int>(sd.get("shipment_id")));
        auto pit = prodIndex.find(get<int>(sd.get("product_id")));
        if (sit != shipmentIndex.end() && pit != prodIndex.end()) {
            const auto& s = sRows[sit->second];
            const auto& p = pRows[pit->second];
            Record out;
            if (s.has("shipment_code")) out.set("shipment.shipment_code", s.get("shipment_code"));
            if (p.has("product_code")) out.set("product.product_code", p.get("product_code"));
            if (p.has("product_name")) out.set("product.product_name", p.get("product_name"));
            if (sd.has("received_quantity")) out.set("shipment_detail.received_quantity", sd.get("received_quantity"));
            results.push_back(std::move(out));
        }
    }
}

void user_query::outbound_transaction_query(Database *db, Vector<Record> &results) {
    if (!db) return;
    Table* otTable = db->getTable("outbound_transaction");
    Table* cTable = db->getTable("customer");
    if (!otTable || !cTable) return;
    results.clear();

    HashMap<int, size_t> custIndex;
    const auto& cRows = cTable->getRows();
    for (size_t i = 0; i < cRows.size(); ++i) {
        if (cTable->isRowDeleted(i)) continue;
        if (cRows[i].has("customer_id") && holds_alternative<int>(cRows[i].get("customer_id"))) {
            custIndex[get<int>(cRows[i].get("customer_id"))] = i;
        }
    }

    const auto& otRows = otTable->getRows();
    for (size_t i = 0; i < otRows.size(); ++i) {
        if (otTable->isRowDeleted(i)) continue;
        const auto& ot = otRows[i];
        if (!ot.has("customer_id") || !holds_alternative<int>(ot.get("customer_id"))) continue;

        auto cit = custIndex.find(get<int>(ot.get("customer_id")));
        if (cit != custIndex.end()) {
            const auto& c = cRows[cit->second];
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

void user_query::outbound_detail_query(Database *db, Vector<Record> &results) {
    if (!db) return;
    Table* odTable = db->getTable("outbound_detail");
    Table* otTable = db->getTable("outbound_transaction");
    Table* pTable = db->getTable("product");
    if (!odTable || !otTable || !pTable) return;
    results.clear();

    HashMap<int, size_t> otIndex;
    const auto& otRows = otTable->getRows();
    for (size_t i = 0; i < otRows.size(); ++i) {
        if (otTable->isRowDeleted(i)) continue;
        if (otRows[i].has("outbound_transaction_id") && holds_alternative<int>(otRows[i].get("outbound_transaction_id"))) {
            otIndex[get<int>(otRows[i].get("outbound_transaction_id"))] = i;
        }
    }

    HashMap<int, size_t> prodIndex;
    const auto& pRows = pTable->getRows();
    for (size_t i = 0; i < pRows.size(); ++i) {
        if (pTable->isRowDeleted(i)) continue;
        if (pRows[i].has("product_id") && holds_alternative<int>(pRows[i].get("product_id"))) {
            prodIndex[get<int>(pRows[i].get("product_id"))] = i;
        }
    }

    const auto& odRows = odTable->getRows();
    for (size_t i = 0; i < odRows.size(); ++i) {
        if (odTable->isRowDeleted(i)) continue;
        const auto& od = odRows[i];
        if (!od.has("outbound_transaction_id") || !od.has("product_id")) continue;
        if (!holds_alternative<int>(od.get("outbound_transaction_id")) || !holds_alternative<int>(od.get("product_id"))) continue;

        auto otit = otIndex.find(get<int>(od.get("outbound_transaction_id")));
        auto pit = prodIndex.find(get<int>(od.get("product_id")));
        if (otit != otIndex.end() && pit != prodIndex.end()) {
            const auto& ot = otRows[otit->second];
            const auto& p = pRows[pit->second];
            Record out;
            if (ot.has("outbound_code")) out.set("outbound_transaction.outbound_code", ot.get("outbound_code"));
            if (p.has("product_code")) out.set("product.product_code", p.get("product_code"));
            if (od.has("ordered_quantity")) out.set("outbound_detail.ordered_quantity", od.get("ordered_quantity"));
            results.push_back(std::move(out));
        }
    }
}

void user_query::delivery_detail_query(Database *db, Vector<Record> &results) {
    if (!db) return;
    Table* ddTable = db->getTable("delivery_detail");
    Table* dTable = db->getTable("delivery");
    Table* pTable = db->getTable("product");
    if (!ddTable || !dTable || !pTable) return;
    results.clear();

    HashMap<int, size_t> dIndex;
    const auto& dRows = dTable->getRows();
    for (size_t i = 0; i < dRows.size(); ++i) {
        if (dTable->isRowDeleted(i)) continue;
        if (dRows[i].has("delivery_id") && holds_alternative<int>(dRows[i].get("delivery_id"))) {
            dIndex[get<int>(dRows[i].get("delivery_id"))] = i;
        }
    }

    HashMap<int, size_t> prodIndex;
    const auto& pRows = pTable->getRows();
    for (size_t i = 0; i < pRows.size(); ++i) {
        if (pTable->isRowDeleted(i)) continue;
        if (pRows[i].has("product_id") && holds_alternative<int>(pRows[i].get("product_id"))) {
            prodIndex[get<int>(pRows[i].get("product_id"))] = i;
        }
    }

    const auto& ddRows = ddTable->getRows();
    for (size_t i = 0; i < ddRows.size(); ++i) {
        if (ddTable->isRowDeleted(i)) continue;
        const auto& dd = ddRows[i];
        if (!dd.has("delivery_id") || !dd.has("product_id")) continue;
        if (!holds_alternative<int>(dd.get("delivery_id")) || !holds_alternative<int>(dd.get("product_id"))) continue;

        auto dit = dIndex.find(get<int>(dd.get("delivery_id")));
        auto pit = prodIndex.find(get<int>(dd.get("product_id")));
        if (dit != dIndex.end() && pit != prodIndex.end()) {
            const auto& d = dRows[dit->second];
            const auto& p = pRows[pit->second];
            Record out;
            if (d.has("delivery_code")) out.set("delivery.delivery_code", d.get("delivery_code"));
            if (p.has("product_code")) out.set("product.product_code", p.get("product_code"));
            if (p.has("product_name")) out.set("product.product_name", p.get("product_name"));
            if (dd.has("received_quantity")) out.set("delivery_detail.received_quantity", dd.get("received_quantity"));
            results.push_back(std::move(out));
        }
    }
}

void user_query::inbound_transaction_query(Database *db, Vector<Record> &results) {
    if (!db) return;
    Table* itTable = db->getTable("inbound_transaction");
    Table* cTable = db->getTable("customer");
    if (!itTable || !cTable) return;
    results.clear();

    HashMap<int, size_t> custIndex;
    const auto& cRows = cTable->getRows();
    for (size_t i = 0; i < cRows.size(); ++i) {
        if (cTable->isRowDeleted(i)) continue;
        if (cRows[i].has("customer_id") && holds_alternative<int>(cRows[i].get("customer_id"))) {
            custIndex[get<int>(cRows[i].get("customer_id"))] = i;
        }
    }

    const auto& itRows = itTable->getRows();
    for (size_t i = 0; i < itRows.size(); ++i) {
        if (itTable->isRowDeleted(i)) continue;
        const auto& it = itRows[i];
        if (!it.has("customer_id") || !holds_alternative<int>(it.get("customer_id"))) continue;

        auto cit = custIndex.find(get<int>(it.get("customer_id")));
        if (cit != custIndex.end()) {
            const auto& c = cRows[cit->second];
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

void user_query::inbound_detail_query(Database *db, Vector<Record> &results) {
    if (!db) return;
    Table* idTable = db->getTable("inbound_detail");
    Table* itTable = db->getTable("inbound_transaction");
    Table* pTable = db->getTable("product");
    if (!idTable || !itTable || !pTable) return;
    results.clear();

    HashMap<int, size_t> itIndex;
    const auto& itRows = itTable->getRows();
    for (size_t i = 0; i < itRows.size(); ++i) {
        if (itTable->isRowDeleted(i)) continue;
        if (itRows[i].has("inbound_transaction_id") && holds_alternative<int>(itRows[i].get("inbound_transaction_id"))) {
            itIndex[get<int>(itRows[i].get("inbound_transaction_id"))] = i;
        }
    }

    HashMap<int, size_t> prodIndex;
    const auto& pRows = pTable->getRows();
    for (size_t i = 0; i < pRows.size(); ++i) {
        if (pTable->isRowDeleted(i)) continue;
        if (pRows[i].has("product_id") && holds_alternative<int>(pRows[i].get("product_id"))) {
            prodIndex[get<int>(pRows[i].get("product_id"))] = i;
        }
    }

    const auto& idRows = idTable->getRows();
    for (size_t i = 0; i < idRows.size(); ++i) {
        if (idTable->isRowDeleted(i)) continue;
        const auto& id = idRows[i];
        if (!id.has("inbound_transaction_id") || !id.has("product_id")) continue;
        if (!holds_alternative<int>(id.get("inbound_transaction_id")) || !holds_alternative<int>(id.get("product_id"))) continue;

        auto itit = itIndex.find(get<int>(id.get("inbound_transaction_id")));
        auto pit = prodIndex.find(get<int>(id.get("product_id")));
        if (itit != itIndex.end() && pit != prodIndex.end()) {
            const auto& it = itRows[itit->second];
            const auto& p = pRows[pit->second];
            Record out;
            if (it.has("inbound_code")) out.set("inbound_transaction.inbound_code", it.get("inbound_code"));
            if (p.has("product_code")) out.set("product.product_code", p.get("product_code"));
            if (id.has("ordered_quantity")) out.set("inbound_detail.ordered_quantity", id.get("ordered_quantity"));
            results.push_back(std::move(out));
        }
    }
}
