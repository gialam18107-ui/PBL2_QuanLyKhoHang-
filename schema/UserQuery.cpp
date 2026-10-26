#include "UserQuery.h"
#include "datastructure/hashmap/HashMap.h"
#include "query/Query.h"
#include <fstream>
#include <cstdio>
#include <cctype>

using namespace std;

namespace user_query {

namespace {

// Strip table prefix from column name if present (e.g. "product.is_sold" -> "is_sold")
inline string stripPrefix(const string &col, const string &prefix) {
	if (col.rfind(prefix, 0) == 0)
		return col.substr(prefix.length());
	return col;
}

// Convert string to lower case
inline string toLowerStr(string s) {
	for (char &c : s)
		c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
	return s;
}

// Extract outbound transaction ID handling possible schema typo variations
inline Value extractOtId(const Record &r) {
	if (r.has("outbound_trainsaction_id"))
		return r.get("outbound_trainsaction_id");
	if (r.has("outbound_transaction_id"))
		return r.get("outbound_transaction_id");
	if (r.has("sell_id"))
		return r.get("sell_id");
	return Value();
}

inline int safeGetInt(const Record &r, const string &col, int defaultVal = 0) {
	if (!r.has(col))
		return defaultVal;
	const auto &v = r.get(col);
	if (holds_alternative<int>(v))
		return get<int>(v);
	if (holds_alternative<double>(v))
		return static_cast<int>(get<double>(v));
	return defaultVal;
}

inline string safeGetString(const Record &r, const string &col, const string &defaultVal = "") {
	if (!r.has(col))
		return defaultVal;
	return valueToString(r.get(col));
}

} // anonymous namespace

// ============================================================
// 2. PRODUCT QUANTITY QUERY (Index: product.is_sold)
// ============================================================
void product_quantity_query(Database *db, Vector<Record> &results) {
	product_quantity_query(db, results, "", Value());
}

void product_quantity_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	results.clear();
	if (!db)
		return;

	Table *pqTable = db->getTable("product_quantity");
	Table *pTable = db->getTable("product");
	if (!pqTable || !pTable)
		return;

	string col = stripPrefix(indexedColumn, "product_quantity.");
	col = stripPrefix(col, "product.");

	// Dùng con trỏ const Record* để tránh deep-copy toàn bộ Record vào HashMap
	HashMap<Value, const Record*> prodMap;
	Vector<Record> prodSource;

	if (!col.empty()) {
		Value filterVal = val;
		if (col == "is_sold" && holds_alternative<string>(val)) {
			string s = toLowerStr(get<string>(val));
			if (s == "true" || s == "1" || s == "đang bán")
				filterVal = true;
			else if (s == "false" || s == "0" || s == "ngừng bán")
				filterVal = false;
		}
		prodSource = Query::filterRecords(*pTable, col, filterVal);
		for (size_t i = 0; i < prodSource.size(); ++i) {
			if (prodSource[i].has("product_id")) {
				prodMap[prodSource[i].get("product_id")] = &prodSource[i];
			}
		}
	} else {
		const auto &pRows = pTable->getRows();
		for (size_t i = 0; i < pRows.size(); ++i) {
			if (!pTable->isRowDeleted(i) && pRows[i].has("product_id")) {
				prodMap[pRows[i].get("product_id")] = &pRows[i];
			}
		}
	}

	results.reserve(prodMap.size());
	const auto &pqRows = pqTable->getRows();
	for (size_t i = 0; i < pqRows.size(); ++i) {
		if (pqTable->isRowDeleted(i))
			continue;
		const auto &pq = pqRows[i];
		if (!pq.has("product_id"))
			continue;

		auto it = prodMap.find(pq.get("product_id"));
		if (it != prodMap.end()) {
			const Record &p = *(it->second);
			Record out;
			out.set("product.product_id", p.get("product_id"));
			out.set("product_id", p.get("product_id"));

			if (p.has("product_code"))
				out.set("product.product_code", p.get("product_code"));
			if (p.has("barcode"))
				out.set("product.barcode", p.get("barcode"));
			if (p.has("sku"))
				out.set("product.sku", p.get("sku"));
			if (p.has("product_name"))
				out.set("product.product_name", p.get("product_name"));
			if (pq.has("available_quan"))
				out.set("product_quantity.available_quan", pq.get("available_quan"));
			if (pq.has("warranty_quan"))
				out.set("product_quantity.warranty_quan", pq.get("warranty_quan"));
			if (pq.has("scrapped_quan"))
				out.set("product_quantity.scrapped_quan", pq.get("scrapped_quan"));
			if (p.has("is_sold"))
				out.set("product.is_sold", p.get("is_sold"));

			results.push_back(std::move(out));
		}
	}
}

// ============================================================
// 3. UNIT INVENTORY QUERY (Index: serial, status)
// ============================================================
void unit_inventory_query(Database *db, Vector<Record> &results) {
	unit_inventory_query(db, results, "", Value());
}

void unit_inventory_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	results.clear();
	if (!db)
		return;

	Table *uTable = db->getTable("unit_inventory");
	Table *pTable = db->getTable("product");
	if (!uTable || !pTable)
		return;

	string col = stripPrefix(indexedColumn, "unit_inventory.");
	col = stripPrefix(col, "product.");

	// Map product_id -> const Record* (Zero deep copies)
	HashMap<Value, const Record*> prodMap;
	const auto &pRows = pTable->getRows();
	for (size_t i = 0; i < pRows.size(); ++i) {
		if (!pTable->isRowDeleted(i) && pRows[i].has("product_id")) {
			prodMap[pRows[i].get("product_id")] = &pRows[i];
		}
	}

	auto processUnit = [&](const Record &u) {
		if (!u.has("product_id"))
			return;
		auto it = prodMap.find(u.get("product_id"));
		if (it != prodMap.end()) {
			const Record &p = *(it->second);
			Record out;
			if (u.has("unit_id")) {
				out.set("unit_inventory.unit_id", u.get("unit_id"));
				out.set("unit_id", u.get("unit_id"));
			}
			if (p.has("product_name"))
				out.set("product.product_name", p.get("product_name"));
			if (u.has("serial"))
				out.set("unit_inventory.serial", u.get("serial"));
			if (u.has("status"))
				out.set("unit_inventory.status", u.get("status"));
			results.push_back(std::move(out));
		}
	};

	if (!col.empty()) {
		Vector<Record> uSource = Query::filterRecords(*uTable, col, val);
		results.reserve(uSource.size());
		for (size_t i = 0; i < uSource.size(); ++i) {
			processUnit(uSource[i]);
		}
	} else {
		// Duyệt trực tiếp uTable->getRows() mà không sinh thêm vector trung gian
		const auto &uRows = uTable->getRows();
		results.reserve(uRows.size());
		for (size_t i = 0; i < uRows.size(); ++i) {
			if (!uTable->isRowDeleted(i)) {
				processUnit(uRows[i]);
			}
		}
	}
}

// ============================================================
// 4. STATUS HISTORY QUERY
// ============================================================
void status_history_query(Database *db, Vector<Record> &results) {
	status_history_query(db, results, "", Value(), Value());
}

void status_history_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	status_history_query(db, results, indexedColumn, val, Value());
}

void status_history_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &minVal,
	const Value &maxVal) {
	results.clear();
	if (!db)
		return;

	Table *shTable = db->getTable("status_history");
	Table *uTable = db->getTable("unit_inventory");
	Table *dTable = db->getTable("delivery");
	Table *sTable = db->getTable("shipment");
	if (!shTable || !uTable)
		return;

	const Value &val = minVal;
	bool hasRange = (minVal.index() != 0 && maxVal.index() != 0);

	// 1. Map unit_id -> const Record* (Zero deep copies)
	HashMap<Value, const Record*> unitMap;
	const auto &uRows = uTable->getRows();
	for (size_t i = 0; i < uRows.size(); ++i) {
		if (!uTable->isRowDeleted(i) && uRows[i].has("unit_id")) {
			unitMap[uRows[i].get("unit_id")] = &uRows[i];
		}
	}

	// 2. Maps cho delivery và shipment (lưu con trỏ const Record*)
	HashMap<Value, const Record*> delivMap;
	if (dTable) {
		const auto &dRows = dTable->getRows();
		for (size_t i = 0; i < dRows.size(); ++i) {
			if (!dTable->isRowDeleted(i) && dRows[i].has("delivery_id")) {
				delivMap[dRows[i].get("delivery_id")] = &dRows[i];
			}
		}
	}

	HashMap<Value, const Record*> shipMap;
	if (sTable) {
		const auto &sRows = sTable->getRows();
		for (size_t i = 0; i < sRows.size(); ++i) {
			if (!sTable->isRowDeleted(i) && sRows[i].has("shipment_id")) {
				shipMap[sRows[i].get("shipment_id")] = &sRows[i];
			}
		}
	}

	string col = stripPrefix(indexedColumn, "status_history.");
	col = stripPrefix(col, "unit_inventory.");

	bool filterByTxCode = (col == "transaction");
	bool filterByDate = (col == "date");
	bool filterBySerial = (col == "serial");

	HashMap<Value, bool> matchedDelivIds;
	HashMap<Value, bool> matchedShipIds;

	if (filterByTxCode) {
		if (dTable) {
			Vector<Record> matchedD = Query::filterRecords(*dTable, "delivery_code", val);
			for (size_t i = 0; i < matchedD.size(); ++i) {
				if (matchedD[i].has("delivery_id"))
					matchedDelivIds[matchedD[i].get("delivery_id")] = true;
			}
		}
		if (sTable) {
			Vector<Record> matchedS = Query::filterRecords(*sTable, "shipment_code", val);
			for (size_t i = 0; i < matchedS.size(); ++i) {
				if (matchedS[i].has("shipment_id"))
					matchedShipIds[matchedS[i].get("shipment_id")] = true;
			}
		}
	} else if (filterByDate) {
		if (dTable) {
			Vector<Record> matchedD = hasRange
					? Query::filterRecords(*dTable, "delivery_at", minVal, maxVal)
					: Query::filterRecords(*dTable, "delivery_at", val);
			for (size_t i = 0; i < matchedD.size(); ++i) {
				if (matchedD[i].has("delivery_id"))
					matchedDelivIds[matchedD[i].get("delivery_id")] = true;
			}
		}
		if (sTable) {
			Vector<Record> matchedS = hasRange
					? Query::filterRecords(*sTable, "ship_at", minVal, maxVal)
					: Query::filterRecords(*sTable, "ship_at", val);
			for (size_t i = 0; i < matchedS.size(); ++i) {
				if (matchedS[i].has("shipment_id"))
					matchedShipIds[matchedS[i].get("shipment_id")] = true;
			}
		}
	}

	auto processSh = [&](const Record &sh) {
		if (!sh.has("unit_id"))
			return;
		auto uit = unitMap.find(sh.get("unit_id"));
		if (uit == unitMap.end())
			return;
		const Record &u = *(uit->second);

		Record out;
		if (sh.has("history_id")) {
			out.set("status_history.history_id", sh.get("history_id"));
			out.set("history_id", sh.get("history_id"));
		}
		if (u.has("serial"))
			out.set("unit_inventory.serial", u.get("serial"));
		if (sh.has("old_status"))
			out.set("status_history.old_status", sh.get("old_status"));
		if (sh.has("curr_status"))
			out.set("status_history.curr_status", sh.get("curr_status"));

		Value txCodeVal = "";
		Value dateVal;
		string t = "";
		if (sh.has("type") && holds_alternative<string>(sh.get("type"))) {
			t = get<string>(sh.get("type"));
		}
		if (t == "INBOUND" && sh.has("transaction")) {
			auto dit = delivMap.find(sh.get("transaction"));
			if (dit != delivMap.end()) {
				const Record &d = *(dit->second);
				if (d.has("delivery_code"))
					txCodeVal = d.get("delivery_code");
				if (d.has("delivery_at"))
					dateVal = d.get("delivery_at");
			}
		} else if (t == "OUTBOUND" && sh.has("transaction")) {
			auto sit = shipMap.find(sh.get("transaction"));
			if (sit != shipMap.end()) {
				const Record &s = *(sit->second);
				if (s.has("shipment_code"))
					txCodeVal = s.get("shipment_code");
				if (s.has("ship_at"))
					dateVal = s.get("ship_at");
			}
		}

		out.set("status_history.transaction", txCodeVal);
		if (sh.has("type"))
			out.set("status_history.type", sh.get("type"));
		out.set("status_history.date", dateVal);

		results.push_back(std::move(out));
	};

	if (filterByTxCode || filterByDate) {
		const auto &shRows = shTable->getRows();
		for (size_t i = 0; i < shRows.size(); ++i) {
			if (shTable->isRowDeleted(i))
				continue;
			const auto &sh = shRows[i];
			string t = (sh.has("type") && holds_alternative<string>(sh.get("type")))
				? get<string>(sh.get("type"))
				: "";
			if (t == "INBOUND") {
				if (sh.has("transaction") && matchedDelivIds.find(sh.get("transaction")) != matchedDelivIds.end()) {
					processSh(sh);
				}
			} else if (t == "OUTBOUND") {
				if (sh.has("transaction") && matchedShipIds.find(sh.get("transaction")) != matchedShipIds.end()) {
					processSh(sh);
				}
			}
		}
	} else if (filterBySerial) {
		Vector<Record> matchedU = Query::filterRecords(*uTable, "serial", val);
		if (!matchedU.empty() && matchedU[0].has("unit_id")) {
			Vector<Record> shFiltered = Query::filterRecords(*shTable, "unit_id", matchedU[0].get("unit_id"));
			results.reserve(shFiltered.size());
			for (size_t i = 0; i < shFiltered.size(); ++i) {
				processSh(shFiltered[i]);
			}
		}
	} else if (!col.empty()) {
		Vector<Record> shFiltered = hasRange
				? Query::filterRecords(*shTable, col, minVal, maxVal)
				: Query::filterRecords(*shTable, col, val);
		results.reserve(shFiltered.size());
		for (size_t i = 0; i < shFiltered.size(); ++i) {
			processSh(shFiltered[i]);
		}
	} else {
		const auto &shRows = shTable->getRows();
		results.reserve(shRows.size());
		for (size_t i = 0; i < shRows.size(); ++i) {
			if (!shTable->isRowDeleted(i)) {
				processSh(shRows[i]);
			}
		}
	}
}

// ============================================================
// 5. INBOUND TRANSACTION QUERY (Index: inbound_transaction.order_at)
// ============================================================
namespace {

void inbound_transaction_query_impl(Database *db, Vector<Record> &results,
	const string &indexedColumn, bool isRange,
	const Value &val1, const Value &val2) {
	results.clear();
	if (!db)
		return;

	Table *itTable = db->getTable("inbound_transaction");
	Table *cTable = db->getTable("customer");
	if (!itTable || !cTable)
		return;

	string col = stripPrefix(indexedColumn, "inbound_transaction.");
	col = stripPrefix(col, "customer.");
	col = stripPrefix(col, "inbound_type.");

	HashMap<Value, const Record*> custMap;
	const auto &cRows = cTable->getRows();
	for (size_t i = 0; i < cRows.size(); ++i) {
		if (!cTable->isRowDeleted(i) && cRows[i].has("customer_id")) {
			custMap[cRows[i].get("customer_id")] = &cRows[i];
		}
	}

	auto processIt = [&](const Record &it) {
		if (!it.has("customer_id"))
			return;
		auto cit = custMap.find(it.get("customer_id"));
		if (cit != custMap.end()) {
			const Record &c = *(cit->second);
			Record out;
			if (it.has("inbound_transaction_id")) {
				out.set("inbound_transaction.inbound_transaction_id", it.get("inbound_transaction_id"));
				out.set("inbound_transaction_id", it.get("inbound_transaction_id"));
				out.set("inbound_id", it.get("inbound_transaction_id"));
			}
			if (it.has("inbound_code"))
				out.set("inbound_transaction.inbound_code", it.get("inbound_code"));
			if (c.has("customer_code"))
				out.set("customer.customer_code", c.get("customer_code"));
			if (it.has("order_at"))
				out.set("inbound_transaction.order_at", it.get("order_at"));
			if (it.has("type"))
				out.set("inbound_transaction.type", it.get("type"));
			if (it.has("status"))
				out.set("inbound_transaction.status", it.get("status"));
			if (it.has("note"))
				out.set("inbound_transaction.note", it.get("note"));
			results.push_back(std::move(out));
		}
	};

	if (!col.empty()) {
		Vector<Record> itSource;
		if (col == "customer_code") {
			Vector<Record> custs = Query::filterRecords(*cTable, "customer_code", val1);
			if (!custs.empty() && custs[0].has("customer_id")) {
				itSource = Query::filterRecords(*itTable, "customer_id", custs[0].get("customer_id"));
			}
		} else {
			itSource = isRange ? Query::filterRecords(*itTable, col, val1, val2)
				: Query::filterRecords(*itTable, col, val1);
		}
		results.reserve(itSource.size());
		for (size_t i = 0; i < itSource.size(); ++i) {
			processIt(itSource[i]);
		}
	} else {
		const auto &itRows = itTable->getRows();
		results.reserve(itRows.size());
		for (size_t i = 0; i < itRows.size(); ++i) {
			if (!itTable->isRowDeleted(i)) {
				processIt(itRows[i]);
			}
		}
	}
}

} // anonymous namespace

void inbound_transaction_query(Database *db, Vector<Record> &results) {
	inbound_transaction_query_impl(db, results, "", false, Value(), Value());
}

void inbound_transaction_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	inbound_transaction_query_impl(db, results, indexedColumn, false, val, Value());
}

void inbound_transaction_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &minVal,
	const Value &maxVal) {
	inbound_transaction_query_impl(db, results, indexedColumn, true, minVal, maxVal);
}

// ============================================================
// 6. INBOUND DETAIL QUERY
// ============================================================
void inbound_detail_query(Database *db, Vector<Record> &results) {
	inbound_detail_query(db, results, "", Value());
}

void inbound_detail_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	results.clear();
	if (!db)
		return;

	Table *idTable = db->getTable("inbound_detail");
	Table *itTable = db->getTable("inbound_transaction");
	Table *pTable = db->getTable("product");
	if (!idTable || !itTable || !pTable)
		return;

	string col = stripPrefix(indexedColumn, "inbound_detail.");
	col = stripPrefix(col, "inbound_transaction.");
	col = stripPrefix(col, "product.");

	HashMap<Value, const Record*> itMap;
	const auto &itRows = itTable->getRows();
	for (size_t i = 0; i < itRows.size(); ++i) {
		if (!itTable->isRowDeleted(i) && itRows[i].has("inbound_transaction_id")) {
			itMap[itRows[i].get("inbound_transaction_id")] = &itRows[i];
		}
	}

	HashMap<Value, const Record*> prodMap;
	const auto &pRows = pTable->getRows();
	for (size_t i = 0; i < pRows.size(); ++i) {
		if (!pTable->isRowDeleted(i) && pRows[i].has("product_id")) {
			prodMap[pRows[i].get("product_id")] = &pRows[i];
		}
	}

	auto processId = [&](const Record &id) {
		if (!id.has("inbound_transaction_id") || !id.has("product_id"))
			return;
		auto itit = itMap.find(id.get("inbound_transaction_id"));
		auto pit = prodMap.find(id.get("product_id"));
		if (itit != itMap.end() && pit != prodMap.end()) {
			const Record &it = *(itit->second);
			const Record &p = *(pit->second);
			Record out;
			if (id.has("inbound_detail_id")) {
				out.set("inbound_detail.inbound_detail_id", id.get("inbound_detail_id"));
				out.set("inbound_detail_id", id.get("inbound_detail_id"));
			}
			if (it.has("inbound_code"))
				out.set("inbound_transaction.inbound_code", it.get("inbound_code"));
			if (p.has("product_code"))
				out.set("product.product_code", p.get("product_code"));
			if (p.has("product_name"))
				out.set("product.product_name", p.get("product_name"));
			if (id.has("ordered_quantity"))
				out.set("inbound_detail.ordered_quantity", id.get("ordered_quantity"));
			if (id.has("actual_quantity")) {
				out.set("inbound_detail.actual_quantity", id.get("actual_quantity"));
				out.set("actual_quantity", id.get("actual_quantity"));
			}
			results.push_back(std::move(out));
		}
	};

	if (!col.empty()) {
		Vector<Record> idSource;
		if (col == "inbound_code") {
			Vector<Record> txns = Query::filterRecords(*itTable, "inbound_code", val);
			if (!txns.empty() && txns[0].has("inbound_transaction_id")) {
				idSource = Query::filterRecords(*idTable, "inbound_transaction_id", txns[0].get("inbound_transaction_id"));
			}
		} else if (col == "product_code") {
			Vector<Record> prods = Query::filterRecords(*pTable, "product_code", val);
			if (!prods.empty() && prods[0].has("product_id")) {
				idSource = Query::filterRecords(*idTable, "product_id", prods[0].get("product_id"));
			}
		} else {
			idSource = Query::filterRecords(*idTable, col, val);
		}
		results.reserve(idSource.size());
		for (size_t i = 0; i < idSource.size(); ++i) {
			processId(idSource[i]);
		}
	} else {
		const auto &idRows = idTable->getRows();
		results.reserve(idRows.size());
		for (size_t i = 0; i < idRows.size(); ++i) {
			if (!idTable->isRowDeleted(i)) {
				processId(idRows[i]);
			}
		}
	}
}

// ============================================================
// 7. DELIVERY QUERY (Index: delivery.delivery_at, expected_at)
// ============================================================
namespace {

void delivery_query_impl(Database *db, Vector<Record> &results,
	const string &indexedColumn, bool isRange,
	const Value &val1, const Value &val2) {
	results.clear();
	if (!db)
		return;

	Table *dTable = db->getTable("delivery");
	Table *itTable = db->getTable("inbound_transaction");
	if (!dTable || !itTable)
		return;

	string col = stripPrefix(indexedColumn, "delivery.");
	col = stripPrefix(col, "inbound_transaction.");

	HashMap<Value, const Record*> inTxnMap;
	const auto &itRows = itTable->getRows();
	for (size_t i = 0; i < itRows.size(); ++i) {
		if (!itTable->isRowDeleted(i) && itRows[i].has("inbound_transaction_id")) {
			inTxnMap[itRows[i].get("inbound_transaction_id")] = &itRows[i];
		}
	}

	auto processDelivery = [&](const Record &d) {
		Record out;
		if (d.has("delivery_id")) {
			out.set("delivery.delivery_id", d.get("delivery_id"));
			out.set("delivery_id", d.get("delivery_id"));
		}
		if (d.has("delivery_code"))
			out.set("delivery.delivery_code", d.get("delivery_code"));

		if (d.has("inbound_transaction_id")) {
			auto it = inTxnMap.find(d.get("inbound_transaction_id"));
			if (it != inTxnMap.end() && it->second->has("inbound_code")) {
				out.set("inbound_transaction.inbound_code", it->second->get("inbound_code"));
			}
		}

		if (d.has("delivery_at"))
			out.set("delivery.delivery_at", d.get("delivery_at"));
		if (d.has("expected_at"))
			out.set("delivery.expected_at", d.get("expected_at"));
		if (d.has("status"))
			out.set("delivery.status", d.get("status"));

		results.push_back(std::move(out));
	};

	if (!col.empty()) {
		Vector<Record> dSource;
		if (col == "inbound_code") {
			Vector<Record> txns = Query::filterRecords(*itTable, "inbound_code", val1);
			if (!txns.empty() && txns[0].has("inbound_transaction_id")) {
				dSource = Query::filterRecords(*dTable, "inbound_transaction_id", txns[0].get("inbound_transaction_id"));
			}
		} else {
			dSource = isRange ? Query::filterRecords(*dTable, col, val1, val2)
				: Query::filterRecords(*dTable, col, val1);
		}
		results.reserve(dSource.size());
		for (size_t i = 0; i < dSource.size(); ++i) {
			processDelivery(dSource[i]);
		}
	} else {
		const auto &dRows = dTable->getRows();
		results.reserve(dRows.size());
		for (size_t i = 0; i < dRows.size(); ++i) {
			if (!dTable->isRowDeleted(i)) {
				processDelivery(dRows[i]);
			}
		}
	}
}

} // anonymous namespace

void delivery_query(Database *db, Vector<Record> &results) {
	delivery_query_impl(db, results, "", false, Value(), Value());
}

void delivery_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	delivery_query_impl(db, results, indexedColumn, false, val, Value());
}

void delivery_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &minVal,
	const Value &maxVal) {
	delivery_query_impl(db, results, indexedColumn, true, minVal, maxVal);
}

// ============================================================
// 8. DELIVERY DETAIL QUERY (Index: delivery.delivery_at, expected_at)
// ============================================================
namespace {

void delivery_detail_query_impl(Database *db, Vector<Record> &results,
	const string &indexedColumn, bool isRange,
	const Value &val1, const Value &val2) {
	results.clear();
	if (!db)
		return;

	Table *ddTable = db->getTable("delivery_detail");
	Table *dTable = db->getTable("delivery");
	Table *pTable = db->getTable("product");
	if (!ddTable || !dTable || !pTable)
		return;

	string col = stripPrefix(indexedColumn, "delivery_detail.");
	col = stripPrefix(col, "delivery.");
	col = stripPrefix(col, "product.");

	HashMap<Value, const Record*> dMap;
	const auto &dRows = dTable->getRows();
	for (size_t i = 0; i < dRows.size(); ++i) {
		if (!dTable->isRowDeleted(i) && dRows[i].has("delivery_id")) {
			dMap[dRows[i].get("delivery_id")] = &dRows[i];
		}
	}

	HashMap<Value, const Record*> prodMap;
	const auto &pRows = pTable->getRows();
	for (size_t i = 0; i < pRows.size(); ++i) {
		if (!pTable->isRowDeleted(i) && pRows[i].has("product_id")) {
			prodMap[pRows[i].get("product_id")] = &pRows[i];
		}
	}

	auto processDd = [&](const Record &dd) {
		if (!dd.has("delivery_id") || !dd.has("product_id"))
			return;
		auto dit = dMap.find(dd.get("delivery_id"));
		auto pit = prodMap.find(dd.get("product_id"));
		if (dit != dMap.end() && pit != prodMap.end()) {
			const Record &d = *(dit->second);
			const Record &p = *(pit->second);
			Record out;
			if (dd.has("delivery_detail_id")) {
				out.set("delivery_detail.delivery_detail_id", dd.get("delivery_detail_id"));
				out.set("delivery_detail_id", dd.get("delivery_detail_id"));
			}
			if (d.has("delivery_code"))
				out.set("delivery.delivery_code", d.get("delivery_code"));
			if (p.has("product_code"))
				out.set("product.product_code", p.get("product_code"));
			if (p.has("product_name"))
				out.set("product.product_name", p.get("product_name"));
			if (dd.has("received_quantity"))
				out.set("delivery_detail.received_quantity", dd.get("received_quantity"));
			if (dd.has("actual_quantity")) {
				out.set("delivery_detail.actual_quantity", dd.get("actual_quantity"));
				out.set("actual_quantity", dd.get("actual_quantity"));
			}
			results.push_back(std::move(out));
		}
	};

	if (!col.empty()) {
		Vector<Record> ddSource;
		if (col == "delivery_code") {
			Vector<Record> delivs = Query::filterRecords(*dTable, "delivery_code", val1);
			if (!delivs.empty() && delivs[0].has("delivery_id")) {
				ddSource = Query::filterRecords(*ddTable, "delivery_id", delivs[0].get("delivery_id"));
			}
		} else if (col == "product_code") {
			Vector<Record> prods = Query::filterRecords(*pTable, "product_code", val1);
			if (!prods.empty() && prods[0].has("product_id")) {
				ddSource = Query::filterRecords(*ddTable, "product_id", prods[0].get("product_id"));
			}
		} else if (col == "delivery_at" || col == "expected_at") {
			Vector<Record> delivs = isRange ? Query::filterRecords(*dTable, col, val1, val2)
				: Query::filterRecords(*dTable, col, val1);
			for (size_t i = 0; i < delivs.size(); ++i) {
				if (delivs[i].has("delivery_id")) {
					Vector<Record> matched = Query::filterRecords(*ddTable, "delivery_id", delivs[i].get("delivery_id"));
					for (size_t j = 0; j < matched.size(); ++j) {
						ddSource.push_back(std::move(matched[j]));
					}
				}
			}
		} else {
			ddSource = isRange ? Query::filterRecords(*ddTable, col, val1, val2)
				: Query::filterRecords(*ddTable, col, val1);
		}
		results.reserve(ddSource.size());
		for (size_t i = 0; i < ddSource.size(); ++i) {
			processDd(ddSource[i]);
		}
	} else {
		const auto &ddRows = ddTable->getRows();
		results.reserve(ddRows.size());
		for (size_t i = 0; i < ddRows.size(); ++i) {
			if (!ddTable->isRowDeleted(i)) {
				processDd(ddRows[i]);
			}
		}
	}
}

} // anonymous namespace

void delivery_detail_query(Database *db, Vector<Record> &results) {
	delivery_detail_query_impl(db, results, "", false, Value(), Value());
}

void delivery_detail_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	delivery_detail_query_impl(db, results, indexedColumn, false, val, Value());
}

void delivery_detail_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &minVal,
	const Value &maxVal) {
	delivery_detail_query_impl(db, results, indexedColumn, true, minVal, maxVal);
}

// ============================================================
// 9. OUTBOUND TRANSACTION QUERY (Index: outbound_transaction.order_at)
// ============================================================
namespace {

void outbound_transaction_query_impl(Database *db, Vector<Record> &results,
	const string &indexedColumn, bool isRange,
	const Value &val1, const Value &val2) {
	results.clear();
	if (!db)
		return;

	Table *otTable = db->getTable("outbound_transaction");
	Table *cTable = db->getTable("customer");
	if (!otTable || !cTable)
		return;

	string col = stripPrefix(indexedColumn, "outbound_transaction.");
	col = stripPrefix(col, "customer.");
	col = stripPrefix(col, "outbound_type.");

	HashMap<Value, const Record*> custMap;
	const auto &cRows = cTable->getRows();
	for (size_t i = 0; i < cRows.size(); ++i) {
		if (!cTable->isRowDeleted(i) && cRows[i].has("customer_id")) {
			custMap[cRows[i].get("customer_id")] = &cRows[i];
		}
	}

	auto processOt = [&](const Record &ot) {
		if (!ot.has("customer_id"))
			return;
		auto cit = custMap.find(ot.get("customer_id"));
		if (cit != custMap.end()) {
			const Record &c = *(cit->second);
			Record out;
			Value otId = extractOtId(ot);
			if (!holds_alternative<monostate>(otId)) {
				out.set("outbound_transaction.outbound_transaction_id", otId);
				out.set("outbound_transaction_id", otId);
				out.set("outbound_id", otId);
			}
			if (ot.has("outbound_code"))
				out.set("outbound_transaction.outbound_code", ot.get("outbound_code"));
			if (c.has("customer_code"))
				out.set("customer.customer_code", c.get("customer_code"));
			if (ot.has("order_at"))
				out.set("outbound_transaction.order_at", ot.get("order_at"));
			if (ot.has("type"))
				out.set("outbound_transaction.type", ot.get("type"));
			if (ot.has("status"))
				out.set("outbound_transaction.status", ot.get("status"));
			if (ot.has("note"))
				out.set("outbound_transaction.note", ot.get("note"));
			results.push_back(std::move(out));
		}
	};

	if (!col.empty()) {
		Vector<Record> otSource;
		if (col == "customer_code") {
			Vector<Record> custs = Query::filterRecords(*cTable, "customer_code", val1);
			if (!custs.empty() && custs[0].has("customer_id")) {
				otSource = Query::filterRecords(*otTable, "customer_id", custs[0].get("customer_id"));
			}
		} else {
			otSource = isRange ? Query::filterRecords(*otTable, col, val1, val2)
				: Query::filterRecords(*otTable, col, val1);
		}
		results.reserve(otSource.size());
		for (size_t i = 0; i < otSource.size(); ++i) {
			processOt(otSource[i]);
		}
	} else {
		const auto &otRows = otTable->getRows();
		results.reserve(otRows.size());
		for (size_t i = 0; i < otRows.size(); ++i) {
			if (!otTable->isRowDeleted(i)) {
				processOt(otRows[i]);
			}
		}
	}
}

} // anonymous namespace

void outbound_transaction_query(Database *db, Vector<Record> &results) {
	outbound_transaction_query_impl(db, results, "", false, Value(), Value());
}

void outbound_transaction_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	outbound_transaction_query_impl(db, results, indexedColumn, false, val, Value());
}

void outbound_transaction_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &minVal,
	const Value &maxVal) {
	outbound_transaction_query_impl(db, results, indexedColumn, true, minVal, maxVal);
}

// ============================================================
// 10. OUTBOUND DETAIL QUERY
// ============================================================
void outbound_detail_query(Database *db, Vector<Record> &results) {
	outbound_detail_query(db, results, "", Value());
}

void outbound_detail_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	results.clear();
	if (!db)
		return;

	Table *odTable = db->getTable("outbound_detail");
	Table *otTable = db->getTable("outbound_transaction");
	Table *pTable = db->getTable("product");
	if (!odTable || !otTable || !pTable)
		return;

	string col = stripPrefix(indexedColumn, "outbound_detail.");
	col = stripPrefix(col, "outbound_transaction.");
	col = stripPrefix(col, "product.");

	HashMap<Value, const Record*> otMap;
	const auto &otRows = otTable->getRows();
	for (size_t i = 0; i < otRows.size(); ++i) {
		if (otTable->isRowDeleted(i))
			continue;
		Value otId = extractOtId(otRows[i]);
		if (!holds_alternative<monostate>(otId)) {
			otMap[otId] = &otRows[i];
		}
	}

	HashMap<Value, const Record*> prodMap;
	const auto &pRows = pTable->getRows();
	for (size_t i = 0; i < pRows.size(); ++i) {
		if (!pTable->isRowDeleted(i) && pRows[i].has("product_id")) {
			prodMap[pRows[i].get("product_id")] = &pRows[i];
		}
	}

	auto processOd = [&](const Record &od) {
		if (!od.has("outbound_transaction_id") || !od.has("product_id"))
			return;
		auto otit = otMap.find(od.get("outbound_transaction_id"));
		auto pit = prodMap.find(od.get("product_id"));
		if (otit != otMap.end() && pit != prodMap.end()) {
			const Record &ot = *(otit->second);
			const Record &p = *(pit->second);
			Record out;
			if (od.has("outbound_detail_id")) {
				out.set("outbound_detail.outbound_detail_id", od.get("outbound_detail_id"));
				out.set("outbound_detail_id", od.get("outbound_detail_id"));
			}
			if (ot.has("outbound_code"))
				out.set("outbound_transaction.outbound_code", ot.get("outbound_code"));
			if (p.has("product_code"))
				out.set("product.product_code", p.get("product_code"));
			if (p.has("product_name"))
				out.set("product.product_name", p.get("product_name"));
			if (od.has("ordered_quantity"))
				out.set("outbound_detail.ordered_quantity", od.get("ordered_quantity"));
			if (od.has("actual_quantity")) {
				out.set("outbound_detail.actual_quantity", od.get("actual_quantity"));
				out.set("actual_quantity", od.get("actual_quantity"));
			}
			results.push_back(std::move(out));
		}
	};

	if (!col.empty()) {
		Vector<Record> odSource;
		if (col == "outbound_code") {
			Vector<Record> txns = Query::filterRecords(*otTable, "outbound_code", val);
			if (!txns.empty()) {
				Value otId = extractOtId(txns[0]);
				if (!holds_alternative<monostate>(otId)) {
					odSource = Query::filterRecords(*odTable, "outbound_transaction_id", otId);
				}
			}
		} else if (col == "product_code") {
			Vector<Record> prods = Query::filterRecords(*pTable, "product_code", val);
			if (!prods.empty() && prods[0].has("product_id")) {
				odSource = Query::filterRecords(*odTable, "product_id", prods[0].get("product_id"));
			}
		} else {
			odSource = Query::filterRecords(*odTable, col, val);
		}
		results.reserve(odSource.size());
		for (size_t i = 0; i < odSource.size(); ++i) {
			processOd(odSource[i]);
		}
	} else {
		const auto &odRows = odTable->getRows();
		results.reserve(odRows.size());
		for (size_t i = 0; i < odRows.size(); ++i) {
			if (!odTable->isRowDeleted(i)) {
				processOd(odRows[i]);
			}
		}
	}
}

// ============================================================
// 11. SHIPMENT QUERY (Index: shipment.ship_at, expected_at)
// ============================================================
namespace {

void shipment_query_impl(Database *db, Vector<Record> &results,
	const string &indexedColumn, bool isRange,
	const Value &val1, const Value &val2) {
	results.clear();
	if (!db)
		return;

	Table *sTable = db->getTable("shipment");
	Table *otTable = db->getTable("outbound_transaction");
	if (!sTable || !otTable)
		return;

	string col = stripPrefix(indexedColumn, "shipment.");
	col = stripPrefix(col, "outbound_transaction.");

	HashMap<Value, const Record*> outTxnMap;
	const auto &otRows = otTable->getRows();
	for (size_t i = 0; i < otRows.size(); ++i) {
		if (otTable->isRowDeleted(i))
			continue;
		Value otId = extractOtId(otRows[i]);
		if (!holds_alternative<monostate>(otId)) {
			outTxnMap[otId] = &otRows[i];
		}
	}

	auto processShip = [&](const Record &s) {
		Record out;
		if (s.has("shipment_id")) {
			out.set("shipment.shipment_id", s.get("shipment_id"));
			out.set("shipment_id", s.get("shipment_id"));
		}
		if (s.has("shipment_code"))
			out.set("shipment.shipment_code", s.get("shipment_code"));

		Value otId = extractOtId(s);
		if (!holds_alternative<monostate>(otId)) {
			auto it = outTxnMap.find(otId);
			if (it != outTxnMap.end() && it->second->has("outbound_code")) {
				out.set("outbound_transaction.outbound_code", it->second->get("outbound_code"));
			}
		}

		if (s.has("ship_at"))
			out.set("shipment.ship_at", s.get("ship_at"));
		if (s.has("expected_at"))
			out.set("shipment.expected_at", s.get("expected_at"));
		if (s.has("status"))
			out.set("shipment.status", s.get("status"));

		results.push_back(std::move(out));
	};

	if (!col.empty()) {
		Vector<Record> sSource;
		if (col == "outbound_code") {
			Vector<Record> txns = Query::filterRecords(*otTable, "outbound_code", val1);
			if (!txns.empty()) {
				Value otId = extractOtId(txns[0]);
				if (!holds_alternative<monostate>(otId)) {
					sSource = Query::filterRecords(*sTable, "sell_id", otId);
				}
			}
		} else {
			sSource = isRange ? Query::filterRecords(*sTable, col, val1, val2)
				: Query::filterRecords(*sTable, col, val1);
		}
		results.reserve(sSource.size());
		for (size_t i = 0; i < sSource.size(); ++i) {
			processShip(sSource[i]);
		}
	} else {
		const auto &sRows = sTable->getRows();
		results.reserve(sRows.size());
		for (size_t i = 0; i < sRows.size(); ++i) {
			if (!sTable->isRowDeleted(i)) {
				processShip(sRows[i]);
			}
		}
	}
}

} // anonymous namespace

void shipment_query(Database *db, Vector<Record> &results) {
	shipment_query_impl(db, results, "", false, Value(), Value());
}

void shipment_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	shipment_query_impl(db, results, indexedColumn, false, val, Value());
}

void shipment_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &minVal,
	const Value &maxVal) {
	shipment_query_impl(db, results, indexedColumn, true, minVal, maxVal);
}

// ============================================================
// 12. SHIPMENT DETAIL QUERY (Index: shipment.ship_at, expected_at)
// ============================================================
namespace {

void shipment_detail_query_impl(Database *db, Vector<Record> &results,
	const string &indexedColumn, bool isRange,
	const Value &val1, const Value &val2) {
	results.clear();
	if (!db)
		return;

	Table *sdTable = db->getTable("shipment_detail");
	Table *sTable = db->getTable("shipment");
	Table *pTable = db->getTable("product");
	if (!sdTable || !sTable || !pTable)
		return;

	string col = stripPrefix(indexedColumn, "shipment_detail.");
	col = stripPrefix(col, "shipment.");
	col = stripPrefix(col, "product.");

	HashMap<Value, const Record*> sMap;
	const auto &sRows = sTable->getRows();
	for (size_t i = 0; i < sRows.size(); ++i) {
		if (!sTable->isRowDeleted(i) && sRows[i].has("shipment_id")) {
			sMap[sRows[i].get("shipment_id")] = &sRows[i];
		}
	}

	HashMap<Value, const Record*> prodMap;
	const auto &pRows = pTable->getRows();
	for (size_t i = 0; i < pRows.size(); ++i) {
		if (!pTable->isRowDeleted(i) && pRows[i].has("product_id")) {
			prodMap[pRows[i].get("product_id")] = &pRows[i];
		}
	}

	auto processSd = [&](const Record &sd) {
		if (!sd.has("shipment_id") || !sd.has("product_id"))
			return;
		auto sit = sMap.find(sd.get("shipment_id"));
		auto pit = prodMap.find(sd.get("product_id"));
		if (sit != sMap.end() && pit != prodMap.end()) {
			const Record &s = *(sit->second);
			const Record &p = *(pit->second);
			Record out;
			if (sd.has("shipment_detail_id")) {
				out.set("shipment_detail.shipment_detail_id", sd.get("shipment_detail_id"));
				out.set("shipment_detail_id", sd.get("shipment_detail_id"));
			}
			if (s.has("shipment_code"))
				out.set("shipment.shipment_code", s.get("shipment_code"));
			if (p.has("product_code"))
				out.set("product.product_code", p.get("product_code"));
			if (p.has("product_name"))
				out.set("product.product_name", p.get("product_name"));
			if (sd.has("received_quantity"))
				out.set("shipment_detail.received_quantity", sd.get("received_quantity"));
			if (sd.has("actual_quantity")) {
				out.set("shipment_detail.actual_quantity", sd.get("actual_quantity"));
				out.set("actual_quantity", sd.get("actual_quantity"));
			}
			results.push_back(std::move(out));
		}
	};

	if (!col.empty()) {
		Vector<Record> sdSource;
		if (col == "shipment_code") {
			Vector<Record> ships = Query::filterRecords(*sTable, "shipment_code", val1);
			if (!ships.empty() && ships[0].has("shipment_id")) {
				sdSource = Query::filterRecords(*sdTable, "shipment_id", ships[0].get("shipment_id"));
			}
		} else if (col == "product_code") {
			Vector<Record> prods = Query::filterRecords(*pTable, "product_code", val1);
			if (!prods.empty() && prods[0].has("product_id")) {
				sdSource = Query::filterRecords(*sdTable, "product_id", prods[0].get("product_id"));
			}
		} else if (col == "ship_at" || col == "expected_at") {
			Vector<Record> ships = isRange ? Query::filterRecords(*sTable, col, val1, val2)
				: Query::filterRecords(*sTable, col, val1);
			for (size_t i = 0; i < ships.size(); ++i) {
				if (ships[i].has("shipment_id")) {
					Vector<Record> matched = Query::filterRecords(*sdTable, "shipment_id", ships[i].get("shipment_id"));
					for (size_t j = 0; j < matched.size(); ++j) {
						sdSource.push_back(std::move(matched[j]));
					}
				}
			}
		} else {
			sdSource = isRange ? Query::filterRecords(*sdTable, col, val1, val2)
				: Query::filterRecords(*sdTable, col, val1);
		}
		results.reserve(sdSource.size());
		for (size_t i = 0; i < sdSource.size(); ++i) {
			processSd(sdSource[i]);
		}
	} else {
		const auto &sdRows = sdTable->getRows();
		results.reserve(sdRows.size());
		for (size_t i = 0; i < sdRows.size(); ++i) {
			if (!sdTable->isRowDeleted(i)) {
				processSd(sdRows[i]);
			}
		}
	}
}

} // anonymous namespace

void shipment_detail_query(Database *db, Vector<Record> &results) {
	shipment_detail_query_impl(db, results, "", false, Value(), Value());
}

void shipment_detail_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	shipment_detail_query_impl(db, results, indexedColumn, false, val, Value());
}

void shipment_detail_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &minVal,
	const Value &maxVal) {
	shipment_detail_query_impl(db, results, indexedColumn, true, minVal, maxVal);
}

// ============================================================
// 13. CÁC BẢNG PHÂN LOẠI SẢN PHẨM (CATEGORY HARDWARE QUERIES)
// ============================================================
void category_hardware_query(Database *db, const string &categoryName,
	Vector<Record> &results) {
	category_hardware_query(db, categoryName, results, "", Value());
}

void category_hardware_query(Database *db, const string &categoryName,
	Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	results.clear();
	if (!db)
		return;

	string cat = toLowerStr(categoryName);
	Table *catTable = db->getTable(cat);
	Table *prodTable = db->getTable("product");
	if (!catTable || !prodTable)
		return;

	string col = stripPrefix(indexedColumn, cat + ".");
	col = stripPrefix(col, "product.");
	bool filterOnSold = (col == "is_sold");

	HashMap<Value, const Record*> prodMap;
	Vector<Record> pFiltered;

	if (filterOnSold) {
		Value soldVal = val;
		if (holds_alternative<string>(val)) {
			string s = toLowerStr(get<string>(val));
			if (s == "true" || s == "1" || s == "đang bán")
				soldVal = true;
			else if (s == "false" || s == "0" || s == "ngừng bán")
				soldVal = false;
		}
		pFiltered = Query::filterRecords(*prodTable, "is_sold", soldVal);
		for (size_t i = 0; i < pFiltered.size(); ++i) {
			if (pFiltered[i].has("product_id")) {
				prodMap[pFiltered[i].get("product_id")] = &pFiltered[i];
			}
		}
	} else {
		const auto &pRows = prodTable->getRows();
		for (size_t i = 0; i < pRows.size(); ++i) {
			if (!prodTable->isRowDeleted(i) && pRows[i].has("product_id")) {
				prodMap[pRows[i].get("product_id")] = &pRows[i];
			}
		}
	}

	const auto &catCols = catTable->getColumns();
	auto processCatRow = [&](const Record &crow) {
		if (!crow.has("product_id"))
			return;
		auto pit = prodMap.find(crow.get("product_id"));
		if (pit == prodMap.end())
			return;
		const Record &prow = *(pit->second);

		Record out;
		if (crow.has(cat + "_id")) {
			out.set(cat + "." + cat + "_id", crow.get(cat + "_id"));
			out.set(cat + "_id", crow.get(cat + "_id"));
		}
		if (prow.has("product_code"))
			out.set("product.product_code", prow.get("product_code"));
		if (prow.has("product_name"))
			out.set("product.product_name", prow.get("product_name"));

		for (size_t c = 0; c < catCols.size(); ++c) {
			const string &colName = catCols[c].getName();
			if (colName == cat + "_id" || colName == "id" || colName == "product_id")
				continue;
			if (cat == "case" && colName == "product_name")
				continue;
			if (crow.has(colName)) {
				out.set(cat + "." + colName, crow.get(colName));
			}
		}

		if (prow.has("is_sold"))
			out.set("product.is_sold", prow.get("is_sold"));

		results.push_back(std::move(out));
	};

	if (!col.empty() && !filterOnSold) {
		Vector<Record> catSource = Query::filterRecords(*catTable, col, val);
		results.reserve(catSource.size());
		for (size_t i = 0; i < catSource.size(); ++i) {
			processCatRow(catSource[i]);
		}
	} else {
		const auto &cRows = catTable->getRows();
		results.reserve(cRows.size());
		for (size_t i = 0; i < cRows.size(); ++i) {
			if (!catTable->isRowDeleted(i)) {
				processCatRow(cRows[i]);
			}
		}
	}
}

void cpu_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	category_hardware_query(db, "cpu", results, indexedColumn, val);
}
void gpu_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	category_hardware_query(db, "gpu", results, indexedColumn, val);
}
void mainboard_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	category_hardware_query(db, "mainboard", results, indexedColumn, val);
}
void ram_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	category_hardware_query(db, "ram", results, indexedColumn, val);
}
void ssd_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	category_hardware_query(db, "ssd", results, indexedColumn, val);
}
void psu_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	category_hardware_query(db, "psu", results, indexedColumn, val);
}
void cooling_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	category_hardware_query(db, "cooling", results, indexedColumn, val);
}
void case_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	category_hardware_query(db, "case", results, indexedColumn, val);
}

// ============================================================
// 14. TRUY VẤN PHỤ CHO HIỂN THỊ SPECS, CATEGORY, BRAND
// ============================================================
void auxiliary_spec_query(Database *db, const string &tableName,
	Vector<Record> &results, const string &filterColumn,
	const Value &val) {
	results.clear();
	if (!db)
		return;

	string tblName = toLowerStr(tableName);
	Table *table = db->getTable(tblName);
	if (!table)
		return;

	string col = filterColumn;
	if (col.rfind(tblName + ".", 0) == 0)
		col = col.substr(tblName.length() + 1);

	bool hasFilter = (!col.empty() && !holds_alternative<monostate>(val));
	const auto &rows = table->getRows();
	results.reserve(rows.size());
	const auto &cols = table->getColumns();

	for (size_t i = 0; i < rows.size(); ++i) {
		if (table->isRowDeleted(i))
			continue;
		const auto &r = rows[i];
		if (hasFilter) {
			if (!r.has(col) || !(r.get(col) == val))
				continue;
		}
		Record out;
		for (size_t c = 0; c < cols.size(); ++c) {
			const string &colName = cols[c].getName();
			if (r.has(colName)) {
				out.set(tblName + "." + colName, r.get(colName));
				out.set(colName, r.get(colName));
			}
		}
		results.push_back(std::move(out));
	}
}

// ============================================================
// 15. TRUY VẤN KHÁCH HÀNG (CUSTOMER QUERIES - Rule 9.4)
// ============================================================
void individual_customer_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	results.clear();
	if (!db)
		return;

	Table *cTable = db->getTable("customer");
	Table *iTable = db->getTable("individual_info");
	if (!cTable || !iTable)
		return;

	string col = stripPrefix(indexedColumn, "customer.");
	col = stripPrefix(col, "individual_info.");

	bool hasFilter = !col.empty() && !holds_alternative<monostate>(val);

	HashMap<Value, const Record*> infoMap;
	const auto &iRows = iTable->getRows();
	for (size_t i = 0; i < iRows.size(); ++i) {
		if (!iTable->isRowDeleted(i) && iRows[i].has("customer_id")) {
			infoMap[iRows[i].get("customer_id")] = &iRows[i];
		}
	}

	Vector<Record> cSource;
	if (hasFilter && col == "customer_code") {
		cSource = Query::filterRecords(*cTable, "customer_code", val);
	} else if (hasFilter && col == "customer_id") {
		cSource = Query::filterRecords(*cTable, "customer_id", val);
	} else {
		cSource = Query::filterRecords(*cTable, "type", Value(string("individual")));
		if (cSource.empty()) {
			cSource = Query::filterRecords(*cTable, "type", Value(string("INDIVIDUAL")));
		}
	}

	results.reserve(cSource.size());
	for (size_t i = 0; i < cSource.size(); ++i) {
		const auto &c = cSource[i];
		if (!c.has("customer_id"))
			continue;

		if (c.has("type")) {
			string t = toLowerStr(valueToString(c.get("type")));
			if (t != "individual")
				continue;
		}

		auto it = infoMap.find(c.get("customer_id"));
		if (it == infoMap.end())
			continue;
		const Record &info = *(it->second);

		if (hasFilter && col != "customer_code" && col != "customer_id") {
			Value checkVal;
			if (col == "name" && c.has("name"))
				checkVal = c.get("name");
			else if (col == "phone_number" && info.has("phone_number"))
				checkVal = info.get("phone_number");
			else if (col == "email" && info.has("email"))
				checkVal = info.get("email");
			else if (c.has(col))
				checkVal = c.get(col);
			else if (info.has(col))
				checkVal = info.get(col);

			if (checkVal != val && valueToString(checkVal) != valueToString(val))
				continue;
		}

		Record out;
		out.set("customer.customer_id", c.get("customer_id"));
		out.set("customer_id", c.get("customer_id"));

		if (c.has("customer_code")) {
			out.set("customer.customer_code", c.get("customer_code"));
			out.set("customer_code", c.get("customer_code"));
		}
		if (c.has("name")) {
			out.set("customer.name", c.get("name"));
			out.set("name", c.get("name"));
		}
		if (info.has("phone_number")) {
			out.set("individual_info.phone_number", info.get("phone_number"));
			out.set("phone_number", info.get("phone_number"));
		}
		if (info.has("email")) {
			out.set("individual_info.email", info.get("email"));
			out.set("email", info.get("email"));
		}

		results.push_back(std::move(out));
	}
}

void business_customer_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	results.clear();
	if (!db)
		return;

	Table *cTable = db->getTable("customer");
	Table *bTable = db->getTable("business_info");
	if (!cTable || !bTable)
		return;

	string col = stripPrefix(indexedColumn, "customer.");
	col = stripPrefix(col, "business_info.");

	bool hasFilter = !col.empty() && !holds_alternative<monostate>(val);

	HashMap<Value, const Record*> bMap;
	const auto &bRows = bTable->getRows();
	for (size_t i = 0; i < bRows.size(); ++i) {
		if (!bTable->isRowDeleted(i) && bRows[i].has("customer_id")) {
			bMap[bRows[i].get("customer_id")] = &bRows[i];
		}
	}

	Vector<Record> cSource;
	if (hasFilter && col == "customer_code") {
		cSource = Query::filterRecords(*cTable, "customer_code", val);
	} else if (hasFilter && col == "customer_id") {
		cSource = Query::filterRecords(*cTable, "customer_id", val);
	} else {
		cSource = Query::filterRecords(*cTable, "type", Value(string("business")));
		if (cSource.empty()) {
			cSource = Query::filterRecords(*cTable, "type", Value(string("BUSINESS")));
		}
	}

	results.reserve(cSource.size());
	for (size_t i = 0; i < cSource.size(); ++i) {
		const auto &c = cSource[i];
		if (!c.has("customer_id"))
			continue;

		if (c.has("type")) {
			string t = toLowerStr(valueToString(c.get("type")));
			if (t != "business")
				continue;
		}

		auto it = bMap.find(c.get("customer_id"));
		if (it == bMap.end())
			continue;
		const Record &bInfo = *(it->second);

		if (hasFilter && col != "customer_code" && col != "customer_id") {
			Value checkVal;
			if (col == "name" && c.has("name"))
				checkVal = c.get("name");
			else if (col == "tax_code" && bInfo.has("tax_code"))
				checkVal = bInfo.get("tax_code");
			else if (col == "phone_number" && bInfo.has("phone_number"))
				checkVal = bInfo.get("phone_number");
			else if (col == "email" && bInfo.has("email"))
				checkVal = bInfo.get("email");
			else if (c.has(col))
				checkVal = c.get(col);
			else if (bInfo.has(col))
				checkVal = bInfo.get(col);

			if (checkVal != val && valueToString(checkVal) != valueToString(val))
				continue;
		}

		Record out;
		out.set("customer.customer_id", c.get("customer_id"));
		out.set("customer_id", c.get("customer_id"));

		if (c.has("customer_code")) {
			out.set("customer.customer_code", c.get("customer_code"));
			out.set("customer_code", c.get("customer_code"));
		}
		if (c.has("name")) {
			out.set("customer.name", c.get("name"));
			out.set("name", c.get("name"));
		}
		if (bInfo.has("tax_code")) {
			out.set("business_info.tax_code", bInfo.get("tax_code"));
			out.set("tax_code", bInfo.get("tax_code"));
		}
		if (bInfo.has("phone_number")) {
			out.set("business_info.phone_number", bInfo.get("phone_number"));
			out.set("phone_number", bInfo.get("phone_number"));
		}
		if (bInfo.has("email")) {
			out.set("business_info.email", bInfo.get("email"));
			out.set("email", bInfo.get("email"));
		}

		results.push_back(std::move(out));
	}
}

// ============================================================
// 16. BÁO CÁO XUẤT - NHẬP - TỒN ĐỊNH KỲ (PERIODIC INVENTORY REPORT)
// ============================================================
void periodic_inventory_report(Database *db, int month, int year,
	Vector<Record> &results, Record &grandTotal) {
	results.clear();
	grandTotal = Record();
	if (!db || month < 1 || month > 12 || year < 1900)
		return;

	Table *pTable = db->getTable("product");
	Table *pqTable = db->getTable("product_quantity");
	Table *delTable = db->getTable("delivery");
	Table *delDetailTable = db->getTable("delivery_detail");
	Table *shipTable = db->getTable("shipment");
	Table *shipDetailTable = db->getTable("shipment_detail");

	if (!pTable || !delTable || !delDetailTable || !shipTable || !shipDetailTable)
		return;

	char startBuf[32];
	snprintf(startBuf, sizeof(startBuf), "%04d-%02d-01", year, month);
	string startDate(startBuf);

	int daysInMonth = 31;
	if (month == 4 || month == 6 || month == 9 || month == 11) {
		daysInMonth = 30;
	} else if (month == 2) {
		bool isLeap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
		daysInMonth = isLeap ? 29 : 28;
	}
	char endBuf[32];
	snprintf(endBuf, sizeof(endBuf), "%04d-%02d-%02d", year, month, daysInMonth);
	string endDate(endBuf);

	// 1. Phân loại delivery trước tháng và trong tháng
	HashMap<Value, bool> validDelBefore;
	HashMap<Value, bool> validDelMonth;
	const auto &delRows = delTable->getRows();
	for (size_t i = 0; i < delRows.size(); ++i) {
		if (delTable->isRowDeleted(i))
			continue;
		const auto &r = delRows[i];
		if (!r.has("delivery_id") || !r.has("delivery_at") || isNull(r.get("delivery_at")))
			continue;
		string dAt = holds_alternative<string>(r.get("delivery_at")) ? get<string>(r.get("delivery_at")) : "";
		if (dAt.empty())
			continue;
		Value dId = r.get("delivery_id");
		if (dAt < startDate) {
			validDelBefore[dId] = true;
		} else if (dAt <= endDate) {
			validDelMonth[dId] = true;
		}
	}

	// 2. Thu thập bản ghi chi tiết nhập hàng để gọi Query::sum gom theo product_id
	Vector<Record> delRecsBefore;
	Vector<Record> delRecsMonth;
	const auto &delDetailRows = delDetailTable->getRows();
	delRecsBefore.reserve(delDetailRows.size() / 2);
	delRecsMonth.reserve(delDetailRows.size() / 2);

	for (size_t i = 0; i < delDetailRows.size(); ++i) {
		if (delDetailTable->isRowDeleted(i))
			continue;
		const auto &r = delDetailRows[i];
		if (!r.has("delivery_id") || !r.has("product_id") || !r.has("actual_quantity"))
			continue;
		Value dId = r.get("delivery_id");
		bool inBefore = (validDelBefore.find(dId) != validDelBefore.end());
		bool inMonth = (validDelMonth.find(dId) != validDelMonth.end());
		if (!inBefore && !inMonth)
			continue;

		Record rec;
		rec.set("product_id", r.get("product_id"));
		rec.set("actual_quantity", r.get("actual_quantity"));

		if (inBefore)
			delRecsBefore.push_back(rec);
		if (inMonth)
			delRecsMonth.push_back(std::move(rec));
	}

	// Gọi hàm sum của Query
	Vector<Record> summedInBefore = Query::sum(delRecsBefore, "product_id", "actual_quantity");
	Vector<Record> summedInMonth = Query::sum(delRecsMonth, "product_id", "actual_quantity");

	HashMap<Value, int> inBeforeMap;
	for (size_t i = 0; i < summedInBefore.size(); ++i) {
		inBeforeMap[summedInBefore[i].get("product_id")] = safeGetInt(summedInBefore[i], "sum_actual_quantity");
	}

	HashMap<Value, int> inMonthMap;
	for (size_t i = 0; i < summedInMonth.size(); ++i) {
		inMonthMap[summedInMonth[i].get("product_id")] = safeGetInt(summedInMonth[i], "sum_actual_quantity");
	}

	// 3. Phân loại shipment trước tháng và trong tháng
	HashMap<Value, bool> validShipBefore;
	HashMap<Value, bool> validShipMonth;
	const auto &shipRows = shipTable->getRows();
	for (size_t i = 0; i < shipRows.size(); ++i) {
		if (shipTable->isRowDeleted(i))
			continue;
		const auto &r = shipRows[i];
		if (!r.has("shipment_id") || !r.has("ship_at") || isNull(r.get("ship_at")))
			continue;
		string sAt = holds_alternative<string>(r.get("ship_at")) ? get<string>(r.get("ship_at")) : "";
		if (sAt.empty())
			continue;
		Value sId = r.get("shipment_id");
		if (sAt < startDate) {
			validShipBefore[sId] = true;
		} else if (sAt <= endDate) {
			validShipMonth[sId] = true;
		}
	}

	// 4. Thu thập chi tiết xuất hàng để gọi Query::sum gom theo product_id
	Vector<Record> shipRecsBefore;
	Vector<Record> shipRecsMonth;
	const auto &shipDetailRows = shipDetailTable->getRows();
	shipRecsBefore.reserve(shipDetailRows.size() / 2);
	shipRecsMonth.reserve(shipDetailRows.size() / 2);

	for (size_t i = 0; i < shipDetailRows.size(); ++i) {
		if (shipDetailTable->isRowDeleted(i))
			continue;
		const auto &r = shipDetailRows[i];
		if (!r.has("shipment_id") || !r.has("product_id") || !r.has("actual_quantity"))
			continue;
		Value sId = r.get("shipment_id");
		bool inBefore = (validShipBefore.find(sId) != validShipBefore.end());
		bool inMonth = (validShipMonth.find(sId) != validShipMonth.end());
		if (!inBefore && !inMonth)
			continue;

		Record rec;
		rec.set("product_id", r.get("product_id"));
		rec.set("actual_quantity", r.get("actual_quantity"));

		if (inBefore)
			shipRecsBefore.push_back(rec);
		if (inMonth)
			shipRecsMonth.push_back(std::move(rec));
	}

	// Gọi hàm sum của Query
	Vector<Record> summedOutBefore = Query::sum(shipRecsBefore, "product_id", "actual_quantity");
	Vector<Record> summedOutMonth = Query::sum(shipRecsMonth, "product_id", "actual_quantity");

	HashMap<Value, int> outBeforeMap;
	for (size_t i = 0; i < summedOutBefore.size(); ++i) {
		outBeforeMap[summedOutBefore[i].get("product_id")] = safeGetInt(summedOutBefore[i], "sum_actual_quantity");
	}

	HashMap<Value, int> outMonthMap;
	for (size_t i = 0; i < summedOutMonth.size(); ++i) {
		outMonthMap[summedOutMonth[i].get("product_id")] = safeGetInt(summedOutMonth[i], "sum_actual_quantity");
	}

	// 5. Map product_quantity (Zero deep copies)
	HashMap<Value, const Record*> pqMap;
	if (pqTable) {
		const auto &pqRows = pqTable->getRows();
		for (size_t i = 0; i < pqRows.size(); ++i) {
			if (!pqTable->isRowDeleted(i) && pqRows[i].has("product_id")) {
				pqMap[pqRows[i].get("product_id")] = &pqRows[i];
			}
		}
	}

	// 6. Tính toán cho từng sản phẩm
	// Theo Rule 9.1: Duyệt qua tất cả sản phẩm (không phụ thuộc vào is_sold).
	// Loại trừ những sản phẩm mà trong tháng đó:
	// (1) Không phát sinh transaction (inMonth == 0 && outMonth == 0)
	// (2) Tổng tồn kho = 0 (available + warranty + scrapped == 0)
	int sumOpening = 0;
	int sumInbound = 0;
	int sumOutbound = 0;
	int sumClosing = 0;
	int sumAvail = 0;
	int sumWarr = 0;
	int sumScrap = 0;

	const auto &pRows = pTable->getRows();
	results.reserve(pRows.size());

	auto processProduct = [&](const Record &p) {
		Value pId = p.get("product_id");
		int inBefore = inBeforeMap.find(pId) != inBeforeMap.end() ? inBeforeMap[pId] : 0;
		int outBefore = outBeforeMap.find(pId) != outBeforeMap.end() ? outBeforeMap[pId] : 0;
		int openingStock = inBefore - outBefore;

		int inMonth = inMonthMap.find(pId) != inMonthMap.end() ? inMonthMap[pId] : 0;
		int outMonth = outMonthMap.find(pId) != outMonthMap.end() ? outMonthMap[pId] : 0;

		int avail = 0;
		int warr = 0;
		int scrap = 0;
		auto pqIt = pqMap.find(pId);
		if (pqIt != pqMap.end()) {
			const Record &pq = *(pqIt->second);
			avail = safeGetInt(pq, "available_quan");
			warr = safeGetInt(pq, "warranty_quan");
			scrap = safeGetInt(pq, "scrapped_quan");
		}
		int totalCurrent = avail + warr + scrap;

		if (openingStock <= 0 || (openingStock + inMonth - outMonth < 0)) {
			openingStock = totalCurrent - inMonth + outMonth;
			if (openingStock < 0)
				openingStock = 0;
		}

		int closingStock = openingStock + inMonth - outMonth;
		if (closingStock < 0)
			closingStock = 0;

		if (avail + warr + scrap != closingStock) {
			if (closingStock == 0) {
				avail = warr = scrap = 0;
			} else if (closingStock >= (warr + scrap)) {
				avail = closingStock - (warr + scrap);
			} else {
				avail = closingStock;
				warr = 0;
				scrap = 0;
			}
		}

		// Quy tắc loại trừ (Rule 9.1):
		// Loại trừ các sản phẩm không phát sinh transaction và tổng tồn kho = 0 trong tháng báo cáo
		int totalStock = avail + warr + scrap;
		if (inMonth == 0 && outMonth == 0 && totalStock == 0) {
			return;
		}

		Record out;
		out.set("product_id", pId);
		if (p.has("product_code"))
			out.set("product_code", p.get("product_code"));
		if (p.has("product_name"))
			out.set("product_name", p.get("product_name"));
		out.set("opening_stock", Value(openingStock));
		out.set("inbound_quantity", Value(inMonth));
		out.set("outbound_quantity", Value(outMonth));
		out.set("closing_stock", Value(closingStock));
		out.set("available_quan", Value(avail));
		out.set("warranty_quan", Value(warr));
		out.set("scrapped_quan", Value(scrap));

		sumOpening += openingStock;
		sumInbound += inMonth;
		sumOutbound += outMonth;
		sumClosing += closingStock;
		sumAvail += avail;
		sumWarr += warr;
		sumScrap += scrap;

		results.push_back(std::move(out));
	};

	for (size_t i = 0; i < pRows.size(); ++i) {
		if (!pTable->isRowDeleted(i)) {
			processProduct(pRows[i]);
		}
	}

	// 7. Dòng tổng cộng (Grand Total)
	grandTotal.set("product_id", Value(0));
	grandTotal.set("product_code", Value(string("TỔNG CỘNG")));
	grandTotal.set("product_name", Value(string("")));
	grandTotal.set("opening_stock", Value(sumOpening));
	grandTotal.set("inbound_quantity", Value(sumInbound));
	grandTotal.set("outbound_quantity", Value(sumOutbound));
	grandTotal.set("closing_stock", Value(sumClosing));
	grandTotal.set("available_quan", Value(sumAvail));
	grandTotal.set("warranty_quan", Value(sumWarr));
	grandTotal.set("scrapped_quan", Value(sumScrap));
}

} // namespace user_query
