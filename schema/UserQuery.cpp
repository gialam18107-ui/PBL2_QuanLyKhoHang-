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

	const Table* pTable = db->getTable("product");
	if (!pTable)
		return;

	string col = stripPrefix(indexedColumn, "product_quantity.");
	col = stripPrefix(col, "product.");

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
	} else {
		for (const auto &r : pTable->view()) {
			prodSource.push_back(r);
		}
	}

	for (const Record &p : prodSource) {
		Record out;
		if (p.has("product_id")) {
			out.set("product.product_id", p.get("product_id"));
			out.set("product_id", p.get("product_id"));
		}
		if (p.has("product_code"))
			out.set("product.product_code", p.get("product_code"));
		if (p.has("barcode"))
			out.set("product.barcode", p.get("barcode"));
		if (p.has("sku"))
			out.set("product.sku", p.get("sku"));
		if (p.has("product_name"))
			out.set("product.product_name", p.get("product_name"));
		if (p.has("brand"))
			out.set("product.brand", p.get("brand"));
		if (p.has("category"))
			out.set("product.category", p.get("category"));

		int avail = safeGetInt(p, "available_quantity");
		int warr = safeGetInt(p, "warranty_quantity");
		int scrap = safeGetInt(p, "scrapped_quantity");

		out.set("product.available_quantity", avail);
		out.set("available_quantity", avail);
		out.set("product_quantity.available_quan", avail);
		out.set("available_quan", avail);

		out.set("product.warranty_quantity", warr);
		out.set("warranty_quantity", warr);
		out.set("product_quantity.warranty_quan", warr);
		out.set("warranty_quan", warr);

		out.set("product.scrapped_quantity", scrap);
		out.set("scrapped_quantity", scrap);
		out.set("product_quantity.scrapped_quan", scrap);
		out.set("scrapped_quan", scrap);

		if (p.has("is_sold"))
			out.set("product.is_sold", p.get("is_sold"));

		results.push_back(std::move(out));
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

	const Table* uTable = db->getTable("unit_inventory");
	const Table* pTable = db->getTable("product");
	if (!uTable || !pTable)
		return;

	string col = stripPrefix(indexedColumn, "unit_inventory.");
	col = stripPrefix(col, "product.");

	// Map product_id -> const Record* (Zero deep copies)
	HashMap<Value, const Record*> prodMap;
	for (const auto &r : pTable->view()) {
		if (r.has("product_id")) {
			prodMap[r.get("product_id")] = &r;
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
		for (const auto &r : uTable->view()) {
		processUnit(r);
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

	const Table* shTable = db->getTable("status_history");
	const Table* uTable = db->getTable("unit_inventory");
	const Table* dTable = db->getTable("delivery");
	const Table* sTable = db->getTable("shipment");
	const Table* aTable = db->getTable("adjustment_movement");
	if (!shTable || !uTable)
		return;

	const Value &val = minVal;
	bool hasRange = (minVal.index() != 0 && maxVal.index() != 0);

	// 1. Map unit_id -> const Record* (Zero deep copies)
	HashMap<Value, const Record*> unitMap;
	for (const auto &r : uTable->view()) {
		if (r.has("unit_id")) {
			unitMap[r.get("unit_id")] = &r;
		}
	}

	// 2. Maps cho delivery, shipment và adjustment_movement (lưu con trỏ const Record*)
	HashMap<Value, const Record*> delivMap;
	if (dTable) {
		for (const auto &r : dTable->view()) {
		if (r.has("delivery_id")) {
			delivMap[r.get("delivery_id")] = &r;
		}
	}
	}

	HashMap<Value, const Record*> shipMap;
	if (sTable) {
		for (const auto &r : sTable->view()) {
		if (r.has("shipment_id")) {
			shipMap[r.get("shipment_id")] = &r;
		}
	}
	}

	HashMap<Value, const Record*> adjMap;
	if (aTable) {
		for (const auto &r : aTable->view()) {
		if (r.has("adjust_id")) {
			adjMap[r.get("adjust_id")] = &r;
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
	HashMap<Value, bool> matchedAdjIds;

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
		if (aTable) {
			Vector<Record> matchedA = Query::filterRecords(*aTable, "adjust_code", val);
			for (size_t i = 0; i < matchedA.size(); ++i) {
				if (matchedA[i].has("adjust_id"))
					matchedAdjIds[matchedA[i].get("adjust_id")] = true;
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
		if (aTable) {
			Vector<Record> matchedA = hasRange
					? Query::filterRecords(*aTable, "adjusted_at", minVal, maxVal)
					: Query::filterRecords(*aTable, "adjusted_at", val);
			for (size_t i = 0; i < matchedA.size(); ++i) {
				if (matchedA[i].has("adjust_id"))
					matchedAdjIds[matchedA[i].get("adjust_id")] = true;
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
		} else if (t == "ADJUSTMENT" && sh.has("transaction")) {
			auto ait = adjMap.find(sh.get("transaction"));
			if (ait != adjMap.end()) {
				const Record &a = *(ait->second);
				if (a.has("adjust_code"))
					txCodeVal = a.get("adjust_code");
				if (a.has("adjusted_at"))
					dateVal = a.get("adjusted_at");
			}
		}

		out.set("status_history.transaction", txCodeVal);
		if (sh.has("type"))
			out.set("status_history.type", sh.get("type"));
		out.set("status_history.date", dateVal);

		results.push_back(std::move(out));
	};

	if (filterByTxCode || filterByDate) {
		for (const auto &sh : shTable->view()) {
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
			} else if (t == "ADJUSTMENT") {
				if (sh.has("transaction") && matchedAdjIds.find(sh.get("transaction")) != matchedAdjIds.end()) {
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
		for (const auto &r : shTable->view()) {
		processSh(r);
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

	const Table* itTable = db->getTable("inbound_transaction");
	const Table* cTable = db->getTable("customer");
	if (!itTable || !cTable)
		return;

	string col = stripPrefix(indexedColumn, "inbound_transaction.");
	col = stripPrefix(col, "customer.");
	col = stripPrefix(col, "inbound_type.");

	HashMap<Value, const Record*> custMap;
	for (const auto &r : cTable->view()) {
		if (r.has("customer_id")) {
			custMap[r.get("customer_id")] = &r;
		}
	}

	auto processIt = [&](const Record &it) {
		Record out;
		if (it.has("inbound_transaction_id")) {
			out.set("inbound_transaction.inbound_transaction_id", it.get("inbound_transaction_id"));
			out.set("inbound_transaction_id", it.get("inbound_transaction_id"));
			out.set("inbound_id", it.get("inbound_transaction_id"));
		}
		if (it.has("inbound_code"))
			out.set("inbound_transaction.inbound_code", it.get("inbound_code"));
		if (it.has("source_code") && !isNull(it.get("source_code"))) {
			out.set("inbound_transaction.source_code", it.get("source_code"));
			out.set("source_code", it.get("source_code"));
		} else {
			out.set("inbound_transaction.source_code", std::string(""));
			out.set("source_code", std::string(""));
		}

		std::string custCode = "";
		if (it.has("customer_id") && !isNull(it.get("customer_id"))) {
			auto cit = custMap.find(it.get("customer_id"));
			if (cit != custMap.end() && cit->second && cit->second->has("customer_code")) {
				custCode = std::get<std::string>(cit->second->get("customer_code"));
			}
		}
		out.set("customer.customer_code", custCode);
		out.set("customer_code", custCode);

		if (it.has("order_at"))
			out.set("inbound_transaction.order_at", it.get("order_at"));
		if (it.has("type"))
			out.set("inbound_transaction.type", it.get("type"));
		if (it.has("status"))
			out.set("inbound_transaction.status", it.get("status"));
		if (it.has("note"))
			out.set("inbound_transaction.note", it.get("note"));
		results.push_back(std::move(out));
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
		for (const auto &r : itTable->view()) {
		processIt(r);
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

	const Table* idTable = db->getTable("inbound_detail");
	const Table* itTable = db->getTable("inbound_transaction");
	const Table* pTable = db->getTable("product");
	if (!idTable || !itTable || !pTable)
		return;

	string col = stripPrefix(indexedColumn, "inbound_detail.");
	col = stripPrefix(col, "inbound_transaction.");
	col = stripPrefix(col, "product.");

	HashMap<Value, const Record*> itMap;
	for (const auto &r : itTable->view()) {
		if (r.has("inbound_transaction_id")) {
			itMap[r.get("inbound_transaction_id")] = &r;
		}
	}

	HashMap<Value, const Record*> prodMap;
	for (const auto &r : pTable->view()) {
		if (r.has("product_id")) {
			prodMap[r.get("product_id")] = &r;
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
			if (id.has("product_id")) {
				out.set("inbound_detail.product_id", id.get("product_id"));
				out.set("product.product_id", id.get("product_id"));
				out.set("product_id", id.get("product_id"));
			}
			if (p.has("product_code")) {
				out.set("product.product_code", p.get("product_code"));
				out.set("product_code", p.get("product_code"));
			}
			if (p.has("product_name")) {
				out.set("product.product_name", p.get("product_name"));
				out.set("product_name", p.get("product_name"));
			}
			if (id.has("ordered_quantity")) {
				out.set("inbound_detail.ordered_quantity", id.get("ordered_quantity"));
				out.set("ordered_quantity", id.get("ordered_quantity"));
			}
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
		for (const auto &r : idTable->view()) {
		processId(r);
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

	const Table* dTable = db->getTable("delivery");
	const Table* itTable = db->getTable("inbound_transaction");
	if (!dTable || !itTable)
		return;

	string col = stripPrefix(indexedColumn, "delivery.");
	col = stripPrefix(col, "inbound_transaction.");

	HashMap<Value, const Record*> inTxnMap;
	for (const auto &r : itTable->view()) {
		if (r.has("inbound_transaction_id")) {
			inTxnMap[r.get("inbound_transaction_id")] = &r;
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
		for (const auto &r : dTable->view()) {
		processDelivery(r);
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

	const Table* ddTable = db->getTable("delivery_detail");
	const Table* dTable = db->getTable("delivery");
	const Table* pTable = db->getTable("product");
	if (!ddTable || !dTable || !pTable)
		return;

	string col = stripPrefix(indexedColumn, "delivery_detail.");
	col = stripPrefix(col, "delivery.");
	col = stripPrefix(col, "product.");

	HashMap<Value, const Record*> dMap;
	for (const auto &r : dTable->view()) {
		if (r.has("delivery_id")) {
			dMap[r.get("delivery_id")] = &r;
		}
	}

	HashMap<Value, const Record*> prodMap;
	for (const auto &r : pTable->view()) {
		if (r.has("product_id")) {
			prodMap[r.get("product_id")] = &r;
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
			if (dd.has("product_id")) {
				out.set("delivery_detail.product_id", dd.get("product_id"));
				out.set("product.product_id", dd.get("product_id"));
				out.set("product_id", dd.get("product_id"));
			}
			if (p.has("product_code")) {
				out.set("product.product_code", p.get("product_code"));
				out.set("product_code", p.get("product_code"));
			}
			if (p.has("product_name")) {
				out.set("product.product_name", p.get("product_name"));
				out.set("product_name", p.get("product_name"));
			}
			if (dd.has("received_quantity")) {
				out.set("delivery_detail.received_quantity", dd.get("received_quantity"));
				out.set("received_quantity", dd.get("received_quantity"));
			}
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
		for (const auto &r : ddTable->view()) {
		processDd(r);
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

	const Table* otTable = db->getTable("outbound_transaction");
	const Table* cTable = db->getTable("customer");
	if (!otTable || !cTable)
		return;

	string col = stripPrefix(indexedColumn, "outbound_transaction.");
	col = stripPrefix(col, "customer.");
	col = stripPrefix(col, "outbound_type.");

	HashMap<Value, const Record*> custMap;
	for (const auto &r : cTable->view()) {
		if (r.has("customer_id")) {
			custMap[r.get("customer_id")] = &r;
		}
	}

	auto processOt = [&](const Record &ot) {
		Record out;
		Value otId = extractOtId(ot);
		if (!holds_alternative<monostate>(otId)) {
			out.set("outbound_transaction.outbound_transaction_id", otId);
			out.set("outbound_transaction_id", otId);
			out.set("outbound_id", otId);
		}
		if (ot.has("outbound_code"))
			out.set("outbound_transaction.outbound_code", ot.get("outbound_code"));
		if (ot.has("source_code") && !isNull(ot.get("source_code"))) {
			out.set("outbound_transaction.source_code", ot.get("source_code"));
			out.set("source_code", ot.get("source_code"));
		} else {
			out.set("outbound_transaction.source_code", std::string(""));
			out.set("source_code", std::string(""));
		}

		std::string custCode = "";
		if (ot.has("customer_id") && !isNull(ot.get("customer_id"))) {
			auto cit = custMap.find(ot.get("customer_id"));
			if (cit != custMap.end() && cit->second && cit->second->has("customer_code")) {
				custCode = std::get<std::string>(cit->second->get("customer_code"));
			}
		}
		out.set("customer.customer_code", custCode);
		out.set("customer_code", custCode);

		if (ot.has("order_at"))
			out.set("outbound_transaction.order_at", ot.get("order_at"));
		if (ot.has("type"))
			out.set("outbound_transaction.type", ot.get("type"));
		if (ot.has("status"))
			out.set("outbound_transaction.status", ot.get("status"));
		if (ot.has("note"))
			out.set("outbound_transaction.note", ot.get("note"));
		results.push_back(std::move(out));
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
		for (const auto &r : otTable->view()) {
		processOt(r);
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

	const Table* odTable = db->getTable("outbound_detail");
	const Table* otTable = db->getTable("outbound_transaction");
	const Table* pTable = db->getTable("product");
	if (!odTable || !otTable || !pTable)
		return;

	string col = stripPrefix(indexedColumn, "outbound_detail.");
	col = stripPrefix(col, "outbound_transaction.");
	col = stripPrefix(col, "product.");

	HashMap<Value, const Record*> otMap;
	for (const auto &ot : otTable->view()) {
		Value otId = extractOtId(ot);
		if (!holds_alternative<monostate>(otId)) {
			otMap[otId] = &ot;
		}
	}

	HashMap<Value, const Record*> prodMap;
	for (const auto &r : pTable->view()) {
		if (r.has("product_id")) {
			prodMap[r.get("product_id")] = &r;
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
			if (od.has("product_id")) {
				out.set("outbound_detail.product_id", od.get("product_id"));
				out.set("product.product_id", od.get("product_id"));
				out.set("product_id", od.get("product_id"));
			}
			if (p.has("product_code")) {
				out.set("product.product_code", p.get("product_code"));
				out.set("product_code", p.get("product_code"));
			}
			if (p.has("product_name")) {
				out.set("product.product_name", p.get("product_name"));
				out.set("product_name", p.get("product_name"));
			}
			if (od.has("ordered_quantity")) {
				out.set("outbound_detail.ordered_quantity", od.get("ordered_quantity"));
				out.set("ordered_quantity", od.get("ordered_quantity"));
			}
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
		for (const auto &r : odTable->view()) {
		processOd(r);
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

	const Table* sTable = db->getTable("shipment");
	const Table* otTable = db->getTable("outbound_transaction");
	if (!sTable || !otTable)
		return;

	string col = stripPrefix(indexedColumn, "shipment.");
	col = stripPrefix(col, "outbound_transaction.");

	HashMap<Value, const Record*> outTxnMap;
	for (const auto &ot : otTable->view()) {
		Value otId = extractOtId(ot);
		if (!holds_alternative<monostate>(otId)) {
			outTxnMap[otId] = &ot;
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
		for (const auto &r : sTable->view()) {
		processShip(r);
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

	const Table* sdTable = db->getTable("shipment_detail");
	const Table* sTable = db->getTable("shipment");
	const Table* pTable = db->getTable("product");
	if (!sdTable || !sTable || !pTable)
		return;

	string col = stripPrefix(indexedColumn, "shipment_detail.");
	col = stripPrefix(col, "shipment.");
	col = stripPrefix(col, "product.");

	HashMap<Value, const Record*> sMap;
	for (const auto &r : sTable->view()) {
		if (r.has("shipment_id")) {
			sMap[r.get("shipment_id")] = &r;
		}
	}

	HashMap<Value, const Record*> prodMap;
	for (const auto &r : pTable->view()) {
		if (r.has("product_id")) {
			prodMap[r.get("product_id")] = &r;
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
			if (sd.has("product_id")) {
				out.set("shipment_detail.product_id", sd.get("product_id"));
				out.set("product.product_id", sd.get("product_id"));
				out.set("product_id", sd.get("product_id"));
			}
			if (p.has("product_code")) {
				out.set("product.product_code", p.get("product_code"));
				out.set("product_code", p.get("product_code"));
			}
			if (p.has("product_name")) {
				out.set("product.product_name", p.get("product_name"));
				out.set("product_name", p.get("product_name"));
			}
			if (sd.has("received_quantity")) {
				out.set("shipment_detail.received_quantity", sd.get("received_quantity"));
				out.set("received_quantity", sd.get("received_quantity"));
			}
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
		for (const auto &r : sdTable->view()) {
		processSd(r);
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
	const Table* catTable = db->getTable(cat);
	const Table* prodTable = db->getTable("product");
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
		for (const auto &r : prodTable->view()) {
		if (r.has("product_id")) {
			prodMap[r.get("product_id")] = &r;
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
		for (const auto &r : catTable->view()) {
		processCatRow(r);
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
	const Table* table = db->getTable(tblName);
	if (!table)
		return;

	string col = filterColumn;
	if (col.rfind(tblName + ".", 0) == 0)
		col = col.substr(tblName.length() + 1);

	bool hasFilter = (!col.empty() && !holds_alternative<monostate>(val));
	const auto &cols = table->getColumns();
	for (const auto &r : table->view()) {
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

	const Table* cTable = db->getTable("customer");
	const Table* iTable = db->getTable("individual_info");
	if (!cTable || !iTable)
		return;

	string col = stripPrefix(indexedColumn, "customer.");
	col = stripPrefix(col, "individual_info.");

	bool hasFilter = !col.empty() && !holds_alternative<monostate>(val);

	HashMap<Value, const Record*> infoMap;
	for (const auto &r : iTable->view()) {
		if (r.has("customer_id")) {
			infoMap[r.get("customer_id")] = &r;
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

	const Table* cTable = db->getTable("customer");
	const Table* bTable = db->getTable("business_info");
	if (!cTable || !bTable)
		return;

	string col = stripPrefix(indexedColumn, "customer.");
	col = stripPrefix(col, "business_info.");

	bool hasFilter = !col.empty() && !holds_alternative<monostate>(val);

	HashMap<Value, const Record*> bMap;
	for (const auto &r : bTable->view()) {
		if (r.has("customer_id")) {
			bMap[r.get("customer_id")] = &r;
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
		if (bInfo.has("address")) {
			out.set("business_info.address", bInfo.get("address"));
			out.set("address", bInfo.get("address"));
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

	const Table* pTable = db->getTable("product");
		const Table* delTable = db->getTable("delivery");
	const Table* delDetailTable = db->getTable("delivery_detail");
	const Table* shipTable = db->getTable("shipment");
	const Table* shipDetailTable = db->getTable("shipment_detail");

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

	// 1. Phân loại delivery trước tháng, trong tháng và sau tháng
	HashMap<Value, bool> validDelBefore;
	HashMap<Value, bool> validDelMonth;
	HashMap<Value, bool> validDelAfter;
	for (const auto &r : delTable->view()) {
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
		} else {
			validDelAfter[dId] = true;
		}
	}

	// 2. Thu thập bản ghi chi tiết nhập hàng để gọi Query::sum gom theo product_id
	Vector<Record> delRecsBefore;
	Vector<Record> delRecsMonth;
	Vector<Record> delRecsAfter;
	for (const auto &r : delDetailTable->view()) {
		if (!r.has("delivery_id") || !r.has("product_id") || !r.has("actual_quantity"))
			continue;
		Value dId = r.get("delivery_id");
		bool inBefore = (validDelBefore.find(dId) != validDelBefore.end());
		bool inMonth = (validDelMonth.find(dId) != validDelMonth.end());
		bool inAfter = (validDelAfter.find(dId) != validDelAfter.end());
		if (!inBefore && !inMonth && !inAfter)
			continue;

		Record rec;
		rec.set("product_id", r.get("product_id"));
		rec.set("actual_quantity", r.get("actual_quantity"));

		if (inBefore)
			delRecsBefore.push_back(rec);
		if (inMonth)
			delRecsMonth.push_back(rec);
		if (inAfter)
			delRecsAfter.push_back(std::move(rec));
	}

	Vector<Record> summedInBefore = Query::sum(delRecsBefore, "product_id", "actual_quantity");
	Vector<Record> summedInMonth = Query::sum(delRecsMonth, "product_id", "actual_quantity");
	Vector<Record> summedInAfter = Query::sum(delRecsAfter, "product_id", "actual_quantity");

	HashMap<Value, int> inBeforeMap;
	for (size_t i = 0; i < summedInBefore.size(); ++i) {
		inBeforeMap[summedInBefore[i].get("product_id")] = safeGetInt(summedInBefore[i], "sum_actual_quantity");
	}

	HashMap<Value, int> inMonthMap;
	for (size_t i = 0; i < summedInMonth.size(); ++i) {
		inMonthMap[summedInMonth[i].get("product_id")] = safeGetInt(summedInMonth[i], "sum_actual_quantity");
	}

	HashMap<Value, int> inAfterMap;
	for (size_t i = 0; i < summedInAfter.size(); ++i) {
		inAfterMap[summedInAfter[i].get("product_id")] = safeGetInt(summedInAfter[i], "sum_actual_quantity");
	}

	// 3. Phân loại shipment trước tháng, trong tháng và sau tháng
	HashMap<Value, bool> validShipBefore;
	HashMap<Value, bool> validShipMonth;
	HashMap<Value, bool> validShipAfter;
	for (const auto &r : shipTable->view()) {
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
		} else {
			validShipAfter[sId] = true;
		}
	}

	// 4. Thu thập chi tiết xuất hàng để gọi Query::sum gom theo product_id
	Vector<Record> shipRecsBefore;
	Vector<Record> shipRecsMonth;
	Vector<Record> shipRecsAfter;
	for (const auto &r : shipDetailTable->view()) {
		if (!r.has("shipment_id") || !r.has("product_id") || !r.has("actual_quantity"))
			continue;
		Value sId = r.get("shipment_id");
		bool inBefore = (validShipBefore.find(sId) != validShipBefore.end());
		bool inMonth = (validShipMonth.find(sId) != validShipMonth.end());
		bool inAfter = (validShipAfter.find(sId) != validShipAfter.end());
		if (!inBefore && !inMonth && !inAfter)
			continue;

		Record rec;
		rec.set("product_id", r.get("product_id"));
		rec.set("actual_quantity", r.get("actual_quantity"));

		if (inBefore)
			shipRecsBefore.push_back(rec);
		if (inMonth)
			shipRecsMonth.push_back(rec);
		if (inAfter)
			shipRecsAfter.push_back(std::move(rec));
	}

	Vector<Record> summedOutBefore = Query::sum(shipRecsBefore, "product_id", "actual_quantity");
	Vector<Record> summedOutMonth = Query::sum(shipRecsMonth, "product_id", "actual_quantity");
	Vector<Record> summedOutAfter = Query::sum(shipRecsAfter, "product_id", "actual_quantity");

	HashMap<Value, int> outBeforeMap;
	for (size_t i = 0; i < summedOutBefore.size(); ++i) {
		outBeforeMap[summedOutBefore[i].get("product_id")] = safeGetInt(summedOutBefore[i], "sum_actual_quantity");
	}

	HashMap<Value, int> outMonthMap;
	for (size_t i = 0; i < summedOutMonth.size(); ++i) {
		outMonthMap[summedOutMonth[i].get("product_id")] = safeGetInt(summedOutMonth[i], "sum_actual_quantity");
	}

	HashMap<Value, int> outAfterMap;
	for (size_t i = 0; i < summedOutAfter.size(); ++i) {
		outAfterMap[summedOutAfter[i].get("product_id")] = safeGetInt(summedOutAfter[i], "sum_actual_quantity");
	}

	// 5. Tính toán cho từng sản phẩm (Rule 10.1)
	int sumOpening = 0;
	int sumInbound = 0;
	int sumOutbound = 0;
	int sumClosing = 0;
	int sumAvail = 0;
	int sumWarr = 0;
	int sumScrap = 0;

	auto processProduct = [&](const Record &p) {
		Value pId = p.get("product_id");
		int inBefore = inBeforeMap.find(pId) != inBeforeMap.end() ? inBeforeMap[pId] : 0;
		int outBefore = outBeforeMap.find(pId) != outBeforeMap.end() ? outBeforeMap[pId] : 0;
		int inMonth = inMonthMap.find(pId) != inMonthMap.end() ? inMonthMap[pId] : 0;
		int outMonth = outMonthMap.find(pId) != outMonthMap.end() ? outMonthMap[pId] : 0;
		int inAfter = inAfterMap.find(pId) != inAfterMap.end() ? inAfterMap[pId] : 0;
		int outAfter = outAfterMap.find(pId) != outAfterMap.end() ? outAfterMap[pId] : 0;

		int curAvail = safeGetInt(p, "available_quantity");
		int curWarr = safeGetInt(p, "warranty_quantity");
		int curScrap = safeGetInt(p, "scrapped_quantity");
		int totalCurrent = curAvail + curWarr + curScrap;

		// Tính ngược tồn cuối kỳ từ tồn hiện tại trừ các biến động phát sinh sau kỳ
		int closingStock = totalCurrent - inAfter + outAfter;
		if (closingStock < 0)
			closingStock = 0;

		// Tồn đầu kỳ = Tồn cuối kỳ - Nhập trong kỳ + Xuất trong kỳ
		int openingStock = closingStock - inMonth + outMonth;
		if (openingStock < 0)
			openingStock = 0;

		// Nếu không có phát sinh nào trước và trong kỳ
		if (inBefore == 0 && inMonth == 0 && outBefore == 0 && outMonth == 0 && closingStock == 0) {
			openingStock = 0;
		}

		// Quy tắc loại trừ (Rule 10.1):
		// Ẩn các sản phẩm mà cả đầu kỳ, trong kỳ và cuối kỳ đều bằng 0
		if (openingStock == 0 && inMonth == 0 && outMonth == 0 && closingStock == 0) {
			return;
		}

		int avail = curAvail;
		int warr = curWarr;
		int scrap = curScrap;
		if (closingStock == 0) {
			avail = 0;
			warr = 0;
			scrap = 0;
		} else if (totalCurrent > 0 && closingStock != totalCurrent) {
			double factor = static_cast<double>(closingStock) / totalCurrent;
			warr = static_cast<int>(curWarr * factor);
			scrap = static_cast<int>(curScrap * factor);
			avail = closingStock - warr - scrap;
			if (avail < 0) {
				avail = closingStock;
				warr = 0;
				scrap = 0;
			}
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

	for (const auto &p : pTable->view()) {
		processProduct(p);
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

// ============================================================
// 16. TRUY VẤN VÀ TỔNG KẾT LINH KIỆN GIAO VẬN (DELIVERY / SHIPMENT UNITS & LOGISTICS QUERIES)
// ============================================================

void logistics_units_history_query(Database *db, const string& logisticsCode, Vector<Record> &results) {
	results.clear();
	if (!db || logisticsCode.empty())
		return;

	const Table* shTable = db->getTable("status_history");
	const Table* uTable = db->getTable("unit_inventory");
	const Table* pTable = db->getTable("product");
	const Table* dTable = db->getTable("delivery");
	const Table* sTable = db->getTable("shipment");
	if (!shTable || !uTable || !pTable)
		return;

	// Resolve logistics ID and type
	int targetLogisticsId = -1;
	string expectedType = "";

	if (dTable) {
		Vector<Record> matchedD = Query::filterRecords(*dTable, "delivery_code", Value(logisticsCode));
		if (!matchedD.empty() && matchedD[0].has("delivery_id")) {
			targetLogisticsId = safeGetInt(matchedD[0], "delivery_id", -1);
			expectedType = "INBOUND";
		}
	}
	if (targetLogisticsId <= 0 && sTable) {
		Vector<Record> matchedS = Query::filterRecords(*sTable, "shipment_code", Value(logisticsCode));
		if (!matchedS.empty() && matchedS[0].has("shipment_id")) {
			targetLogisticsId = safeGetInt(matchedS[0], "shipment_id", -1);
			expectedType = "OUTBOUND";
		}
	}

	// Maps for fast lookups
	HashMap<Value, const Record*> unitMap;
	for (const auto &r : uTable->view()) {
		if (r.has("unit_id")) {
			unitMap[r.get("unit_id")] = &r;
		}
	}

	HashMap<Value, const Record*> prodMap;
	for (const auto &r : pTable->view()) {
		if (r.has("product_id")) {
			prodMap[r.get("product_id")] = &r;
		}
	}

	struct UnitHistoryTrack {
		int unitId = 0;
		string serial;
		string initialOldStatus;
		string latestCurrStatus;
		string type;
		string date;
		int productId = 0;
		string productName;
		string productCode;
	};

	Vector<int> unitOrder;
	HashMap<int, UnitHistoryTrack> unitTracker;

	for (const auto &r : shTable->view()) {

		if (!r.has("transaction") || !r.has("unit_id")) continue;

		bool match = false;
		const auto &txVal = r.get("transaction");
		if (holds_alternative<int>(txVal) && targetLogisticsId > 0 && get<int>(txVal) == targetLogisticsId) {
			match = true;
		} else if (holds_alternative<string>(txVal) && get<string>(txVal) == logisticsCode) {
			match = true;
		}

		if (!match) continue;

		if (!expectedType.empty() && r.has("type")) {
			string t = safeGetString(r, "type");
			if (!t.empty() && t != expectedType) continue;
		}

		int uid = safeGetInt(r, "unit_id", 0);
		if (uid <= 0) continue;

		string oldSt = safeGetString(r, "old_status", "");
		string currSt = safeGetString(r, "curr_status", "");
		string tType = safeGetString(r, "type", expectedType);
		string tDate = safeGetString(r, "date", "");

		if (!unitTracker.contains(uid)) {
			UnitHistoryTrack tr;
			tr.unitId = uid;
			tr.initialOldStatus = oldSt;
			tr.latestCurrStatus = currSt;
			tr.type = tType;
			tr.date = tDate;

			if (unitMap.contains(Value(uid))) {
				const Record *uRec = unitMap[Value(uid)];
				tr.serial = safeGetString(*uRec, "serial");
				tr.productId = safeGetInt(*uRec, "product_id", 0);
				if (prodMap.contains(Value(tr.productId))) {
					const Record *pRec = prodMap[Value(tr.productId)];
					tr.productName = safeGetString(*pRec, "product_name");
					tr.productCode = safeGetString(*pRec, "product_code");
				}
			}

			unitOrder.push_back(uid);
			unitTracker[uid] = std::move(tr);
		} else {
			// Update latest status resulting from subsequent in-batch transition
			unitTracker[uid].latestCurrStatus = currSt;
			if (!tDate.empty()) unitTracker[uid].date = tDate;
		}
	}

	for (size_t i = 0; i < unitOrder.size(); ++i) {
		int uid = unitOrder[i];
		const auto &tr = unitTracker[uid];

		Record rec;
		rec.set("unit_id", Value(tr.unitId));
		rec.set("serial", Value(tr.serial));
		rec.set("product_id", Value(tr.productId));
		rec.set("product_name", Value(tr.productName));
		rec.set("product_code", Value(tr.productCode));
		rec.set("old_status", tr.initialOldStatus.empty() ? Value{} : Value(tr.initialOldStatus));
		rec.set("curr_status", Value(tr.latestCurrStatus));
		rec.set("type", Value(tr.type));
		rec.set("date", Value(tr.date));
		results.push_back(std::move(rec));
	}
}

void logistics_unit_status_summary(Database *db, const string& logisticsCode, Vector<Pair<string, int>> &summary) {
	summary.clear();
	if (!db || logisticsCode.empty()) return;

	Vector<Record> records;
	logistics_units_history_query(db, logisticsCode, records);

	int avail = 0, warr = 0, scrap = 0, out = 0, other = 0;
	for (size_t i = 0; i < records.size(); ++i) {
		string st = safeGetString(records[i], "curr_status");
		if (st == "AVAILABLE") avail++;
		else if (st == "WARRANTY") warr++;
		else if (st == "SCRAPPED") scrap++;
		else if (st == "OUT") out++;
		else other++;
	}

	summary.push_back(Pair<string, int>("AVAILABLE", avail));
	summary.push_back(Pair<string, int>("WARRANTY", warr));
	summary.push_back(Pair<string, int>("SCRAPPED", scrap));
	summary.push_back(Pair<string, int>("OUT", out));
	if (other > 0) {
		summary.push_back(Pair<string, int>("OTHER", other));
	}
}

string get_transaction_type_by_order_code(Database *db, const string& orderCode, bool isInbound) {
	if (!db || orderCode.empty()) return "";
	string tbl = isInbound ? "inbound_transaction" : "outbound_transaction";
	string codeCol = isInbound ? "inbound_code" : "outbound_code";
	const Table* t = db->getTable(tbl);
	if (!t) return "";

	Vector<Record> matched = Query::filterRecords(*t, codeCol, Value(orderCode));
	if (!matched.empty() && matched[0].has("type")) {
		return safeGetString(matched[0], "type");
	}
	return "";
}

string get_logistics_status_by_code(Database *db, const string& logisticsCode, bool isInbound) {
	if (!db || logisticsCode.empty()) return "";
	string tbl = isInbound ? "delivery" : "shipment";
	string codeCol = isInbound ? "delivery_code" : "shipment_code";
	const Table* t = db->getTable(tbl);
	if (!t) return "";

	Vector<Record> matched = Query::filterRecords(*t, codeCol, Value(logisticsCode));
	if (!matched.empty() && matched[0].has("status")) {
		return safeGetString(matched[0], "status");
	}
	return "";
}

bool is_logistics_completed(Database *db, const string& logisticsCode, bool isInbound) {
	string st = get_logistics_status_by_code(db, logisticsCode, isInbound);
	return (st == "COMPLETED" || st == "completed" || st == "Completed");
}

int get_product_id_by_serial(Database *db, const string& serial) {
	if (!db || serial.empty()) return -1;
	const Table* uTable = db->getTable("unit_inventory");
	if (!uTable) return -1;

	Vector<Record> matched = Query::filterRecords(*uTable, "serial", Value(serial));
	if (!matched.empty() && matched[0].has("product_id")) {
		return safeGetInt(matched[0], "product_id", -1);
	}
	return -1;
}

string get_unit_status_by_serial(Database *db, const string& serial) {
	if (!db || serial.empty()) return "";
	const Table* uTable = db->getTable("unit_inventory");
	if (!uTable) return "";

	Vector<Record> matched = Query::filterRecords(*uTable, "serial", Value(serial));
	if (!matched.empty() && matched[0].has("status")) {
		return safeGetString(matched[0], "status");
	}
	return "";
}

bool is_unit_in_inventory(Database *db, const string& serial) {
	return get_product_id_by_serial(db, serial) > 0;
}

void get_candidate_units_for_order(Database *db, const Vector<int>& productIds, Vector<Record>& results) {
	results.clear();
	if (!db || productIds.empty()) return;

	const Table* uTable = db->getTable("unit_inventory");
	const Table* pTable = db->getTable("product");
	if (!uTable || !pTable) return;

	HashMap<Value, const Record*> pMap;
	for (const auto &r : pTable->view()) {
		if (r.has("product_id")) {
			pMap[r.get("product_id")] = &r;
		}
	}

	HashMap<int, bool> allowedPids;
	for (size_t i = 0; i < productIds.size(); ++i) {
		allowedPids[productIds[i]] = true;
	}

	for (const auto &uRow : uTable->view()) {
		int pid = safeGetInt(uRow, "product_id", -1);
		if (pid > 0 && allowedPids.contains(pid) && allowedPids[pid]) {
			Record r;
			r.set("unit_id", uRow.get("unit_id"));
			r.set("serial", uRow.get("serial"));
			r.set("product_id", Value(pid));
			r.set("status", uRow.get("status"));
			if (pMap.contains(Value(pid))) {
				const Record *pRec = pMap[Value(pid)];
				r.set("product_name", pRec->get("product_name"));
				r.set("product_code", pRec->get("product_code"));
			}
			results.push_back(std::move(r));
		}
	}
}

void all_customers_query(Database *db, Vector<Record> &results) {
	results.clear();
	if (!db) return;
	const Table* custTable = db->getTable("customer");
	if (!custTable) return;

	for (const auto &r : custTable->view()) {
		results.push_back(r);
	}
}

void get_all_statuses_for_table(Database *db, const string& tableName, Vector<string> &statuses) {
	statuses.clear();
	if (!db || tableName.empty()) return;
	const Table* t = db->getTable(tableName);
	if (!t) return;

	for (const auto &r : t->view()) {
		if (r.has("status") && holds_alternative<string>(r.get("status"))) {
			statuses.push_back(get<string>(r.get("status")));
		}
	}
}

void get_logistics_batch_allocation_info(Database* db, int txnId, bool isInbound, Vector<LogisticsBatchItemInfo>& items) {
	items.clear();
	if (!db || txnId <= 0) return;

	const Table* prodTable = db->getTable("product");
		if (!prodTable) return;

	HashMap<Value, const Record*> prodMap;
	for (const auto &r : prodTable->view()) {
		if (r.has("product_id")) {
			prodMap[r.get("product_id")] = &r;
		}
	}



	if (isInbound) {
		const Table* inDetTable = db->getTable("inbound_detail");
		const Table* delTable = db->getTable("delivery");
		const Table* delDetTable = db->getTable("delivery_detail");
		if (!inDetTable || !delTable || !delDetTable) return;

		HashMap<int, int> allocatedMap;
		for (const auto& del : delTable->view("inbound_transaction_id", Value(txnId))) {
			int delId = safeGetInt(del, "delivery_id", 0);
			for (const auto& dd : delDetTable->view("delivery_id", Value(delId))) {
				int pid = safeGetInt(dd, "product_id", 0);
				int rq = safeGetInt(dd, "received_quantity", 0);
				allocatedMap[pid] = (allocatedMap.contains(pid) ? allocatedMap[pid] : 0) + rq;
			}
		}

		for (const auto& idet : inDetTable->view("inbound_transaction_id", Value(txnId))) {
				int pid = safeGetInt(idet, "product_id", 0);
				int ordQty = safeGetInt(idet, "ordered_quantity", 0);
				int allocated = allocatedMap.contains(pid) ? allocatedMap[pid] : 0;
				int remaining = std::max(0, ordQty - allocated);

				LogisticsBatchItemInfo info;
				info.productId = pid;
				info.orderedQty = ordQty;
				info.alreadyAllocatedQty = allocated;
				info.remainingToAllocate = remaining;

				if (prodMap.contains(Value(pid))) {
					const Record* p = prodMap[Value(pid)];
					info.productCode = safeGetString(*p, "product_code");
					info.productName = safeGetString(*p, "product_name");
				} else {
					info.productCode = to_string(pid);
					info.productName = "Product " + to_string(pid);
				}

				if (prodMap.contains(Value(pid))) {
					const Record* p = prodMap[Value(pid)];
					info.currentStockAvail = safeGetInt(*p, "available_quantity", 0);
					info.currentStockWrt = safeGetInt(*p, "warranty_quantity", 0);
					info.currentStockScrap = safeGetInt(*p, "scrapped_quantity", 0);
				}

				items.push_back(std::move(info));
			}
	} else {
		const Table* outDetTable = db->getTable("outbound_detail");
		const Table* shipTable = db->getTable("shipment");
		const Table* shipDetTable = db->getTable("shipment_detail");
		if (!outDetTable || !shipTable || !shipDetTable) return;

		HashMap<int, int> allocatedMap;
		for (const auto& ship : shipTable->view("outbound_transaction_id", Value(txnId))) {
			int shipId = safeGetInt(ship, "shipment_id", 0);
			for (const auto& sd : shipDetTable->view("shipment_id", Value(shipId))) {
				int pid = safeGetInt(sd, "product_id", 0);
				int rq = safeGetInt(sd, "received_quantity", 0);
				allocatedMap[pid] = (allocatedMap.contains(pid) ? allocatedMap[pid] : 0) + rq;
			}
		}

		for (const auto& odet : outDetTable->view("outbound_transaction_id", Value(txnId))) {
				int pid = safeGetInt(odet, "product_id", 0);
				int ordQty = safeGetInt(odet, "ordered_quantity", 0);
				int allocated = allocatedMap.contains(pid) ? allocatedMap[pid] : 0;
				int remaining = std::max(0, ordQty - allocated);

				LogisticsBatchItemInfo info;
				info.productId = pid;
				info.orderedQty = ordQty;
				info.alreadyAllocatedQty = allocated;
				info.remainingToAllocate = remaining;

				if (prodMap.contains(Value(pid))) {
					const Record* p = prodMap[Value(pid)];
					info.productCode = safeGetString(*p, "product_code");
					info.productName = safeGetString(*p, "product_name");
				} else {
					info.productCode = to_string(pid);
					info.productName = "Product " + to_string(pid);
				}

				if (prodMap.contains(Value(pid))) {
					const Record* p = prodMap[Value(pid)];
					info.currentStockAvail = safeGetInt(*p, "available_quantity", 0);
					info.currentStockWrt = safeGetInt(*p, "warranty_quantity", 0);
					info.currentStockScrap = safeGetInt(*p, "scrapped_quantity", 0);
				}

				items.push_back(std::move(info));
			}
	}
}

void logistics_product_status_summary(
	Database* db,
	const string& logisticsCode,
	bool isInbound,
	Vector<ProductLogisticsStatusSummary>& summary
) {
	summary.clear();
	if (!db || logisticsCode.empty()) return;

	Vector<Record> batchDetails;
	if (isInbound) {
		delivery_detail_query(db, batchDetails, "delivery_code", Value(logisticsCode));
	} else {
		shipment_detail_query(db, batchDetails, "shipment_code", Value(logisticsCode));
	}

	HashMap<int, size_t> pidToIndex;
	for (size_t i = 0; i < batchDetails.size(); ++i) {
		const auto& r = batchDetails[i];
		int pid = safeGetInt(r, "product_id", 0);
		if (pid <= 0) continue;

		ProductLogisticsStatusSummary item;
		item.productId = pid;
		item.productCode = safeGetString(r, "product.product_code");
		if (item.productCode.empty()) item.productCode = safeGetString(r, "product_code");
		item.productName = safeGetString(r, "product.product_name");
		if (item.productName.empty()) item.productName = safeGetString(r, "product_name");

		if (isInbound) {
			item.batchQty = safeGetInt(r, "delivery_detail.received_quantity", safeGetInt(r, "received_quantity", 0));
			item.actualQty = safeGetInt(r, "delivery_detail.actual_quantity", safeGetInt(r, "actual_quantity", 0));
		} else {
			item.batchQty = safeGetInt(r, "shipment_detail.received_quantity", safeGetInt(r, "received_quantity", 0));
			item.actualQty = safeGetInt(r, "shipment_detail.actual_quantity", safeGetInt(r, "actual_quantity", 0));
		}

		pidToIndex[pid] = summary.size();
		summary.push_back(std::move(item));
	}

	Vector<Record> unitHistories;
	logistics_units_history_query(db, logisticsCode, unitHistories);

	for (size_t i = 0; i < unitHistories.size(); ++i) {
		const auto& uRec = unitHistories[i];
		int pid = safeGetInt(uRec, "product_id", 0);
		string st = safeGetString(uRec, "curr_status");
		string oldSt = safeGetString(uRec, "old_status");

		if (pidToIndex.contains(pid)) {
			size_t idx = pidToIndex[pid];
			if (st == "AVAILABLE") summary[idx].availableQty++;
			else if (st == "WARRANTY") summary[idx].warrantyQty++;
			else if (st == "SCRAPPED") summary[idx].scrappedQty++;
			else if (st == "OUT") {
				summary[idx].outQty++;
				if (oldSt == "AVAILABLE") summary[idx].availableQty++;
				else if (oldSt == "WARRANTY") summary[idx].warrantyQty++;
				else if (oldSt == "SCRAPPED") summary[idx].scrappedQty++;
				else summary[idx].availableQty++; // Default outbound unit was available stock
			}
		}
	}
}


// ========================================================
// 17. TRUY VẤN KIỂM KHO VÀ ĐIỀU CHUYỂN
// ========================================================

void inventory_check_query(Database *db, Vector<Record> &results) {
	inventory_check_query(db, results, "", Value());
}

void inventory_check_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	results.clear();
	if (!db) return;
	const Table* chkTable = db->getTable("inventory_check");
	if (!chkTable) return;

	string col = stripPrefix(indexedColumn, "inventory_check.");
	Vector<Record> raw;
	if (!col.empty()) {
		raw = Query::filterRecords(*chkTable, col, val);
	} else {
		for (const auto &r : chkTable->view()) {
			raw.push_back(r);
		}
	}

	results.reserve(raw.size());
	for (size_t i = 0; i < raw.size(); ++i) {
		Record r = raw[i];
		if (r.has("check_at") && !r.has("check_date")) {
			r.set("check_date", r.get("check_at"));
		} else if (r.has("check_date") && !r.has("check_at")) {
			r.set("check_at", r.get("check_date"));
		}
		results.push_back(std::move(r));
	}
}

void inventory_check_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &minVal, const Value &maxVal) {
	results.clear();
	if (!db) return;
	const Table* chkTable = db->getTable("inventory_check");
	if (!chkTable) return;

	string col = stripPrefix(indexedColumn, "inventory_check.");
	results = Query::filterRecords(*chkTable, col, minVal, maxVal);
}

void inventory_check_quantity_query(Database *db, int checkId, Vector<Record> &results) {
	results.clear();
	if (!db) return;
	const Table* cqTable = db->getTable("inventory_check_quantity");
	const Table* chkTable = db->getTable("inventory_check");
	const Table* pTable = db->getTable("product");
	if (!cqTable) return;

	HashMap<Value, const Record*> pMap;
	if (pTable) {
		for (const auto &r : pTable->view()) {
		if (r.has("product_id")) {
			pMap[r.get("product_id")] = &r;
		}
	}
	}

	HashMap<Value, const Record*> chkMap;
	if (chkTable) {
		for (const auto &r : chkTable->view()) {
		if (r.has("check_id")) {
			chkMap[r.get("check_id")] = &r;
		}
	}
	}

	Vector<Record> cqRows;
	if (checkId > 0) {
		cqRows = Query::filterRecords(*cqTable, "check_id", Value(checkId));
	} else {
		for (const auto &r : cqTable->view()) cqRows.push_back(r);
	}

	results.reserve(cqRows.size());
	for (size_t i = 0; i < cqRows.size(); ++i) {
		const auto &cq = cqRows[i];
		Record r = cq;
		int cId = safeGetInt(cq, "check_id");
		auto chkIt = chkMap.find(Value(cId));
		if (chkIt != chkMap.end()) {
			const Record &chk = *(chkIt->second);
			if (chk.has("check_code")) r.set("check_code", chk.get("check_code"));
			if (chk.has("check_type")) r.set("check_type", chk.get("check_type"));
			if (chk.has("status")) r.set("check_status", chk.get("status"));
		}

		int pid = safeGetInt(cq, "product_id");
		auto it = pMap.find(Value(pid));
		if (it != pMap.end()) {
			const Record &p = *(it->second);
			if (p.has("product_code")) r.set("product_code", p.get("product_code"));
			if (p.has("product_name")) r.set("product_name", p.get("product_name"));
			if (p.has("category")) r.set("category", p.get("category"));
			if (p.has("brand")) r.set("brand", p.get("brand"));
		}

		int sysAvail = safeGetInt(cq, "system_available_quan");
		int actAvail = safeGetInt(cq, "actual_available_quan");
		int sysWarr = safeGetInt(cq, "system_warranty_quan");
		int actWarr = safeGetInt(cq, "actual_warranty_quan");
		int sysScrap = safeGetInt(cq, "system_scrapped_quan");
		int actScrap = safeGetInt(cq, "actual_scrapped_quan");

		int totalSys = sysAvail + sysWarr + sysScrap;
		int totalAct = actAvail + actWarr + actScrap;
		int totalDiff = totalAct - totalSys;

		r.set("diff_available", actAvail - sysAvail);
		r.set("diff_warranty", actWarr - sysWarr);
		r.set("diff_scrapped", actScrap - sysScrap);

		r.set("total_system_quan", totalSys);
		r.set("total_actual_quan", totalAct);
		r.set("total_diff_quan", totalDiff);

		results.push_back(std::move(r));
	}
}

void inventory_check_unit_query(Database *db, int checkId, Vector<Record> &results) {
	results.clear();
	if (!db) return;
	const Table* cuTable = db->getTable("inventory_check_unit");
	const Table* chkTable = db->getTable("inventory_check");
	const Table* cqTable = db->getTable("inventory_check_quantity");
	const Table* pTable = db->getTable("product");
	if (!cuTable) return;

	HashMap<Value, const Record*> chkMap;
	if (chkTable) {
		for (const auto &r : chkTable->view()) {
		if (r.has("check_id")) {
			chkMap[r.get("check_id")] = &r;
		}
	}
	}

	HashMap<Value, const Record*> cqMap;
	if (cqTable) {
		for (const auto &r : cqTable->view()) {
		if (r.has("check_quantity_id")) {
			cqMap[r.get("check_quantity_id")] = &r;
		}
	}
	}

	HashMap<Value, const Record*> pMap;
	if (pTable) {
		for (const auto &r : pTable->view()) {
		if (r.has("product_id")) {
			pMap[r.get("product_id")] = &r;
		}
	}
	}

	Vector<Record> cuRows;
	if (checkId > 0) {
		cuRows = Query::filterRecords(*cuTable, "check_id", Value(checkId));
	} else {
		for (const auto &r : cuTable->view()) cuRows.push_back(r);
	}

	results.reserve(cuRows.size());
	for (size_t i = 0; i < cuRows.size(); ++i) {
		const auto &cu = cuRows[i];
		Record r = cu;
		int cId = safeGetInt(cu, "check_id");
		auto chkIt = chkMap.find(Value(cId));
		if (chkIt != chkMap.end()) {
			const Record &chk = *(chkIt->second);
			if (chk.has("check_code")) r.set("check_code", chk.get("check_code"));
			if (chk.has("check_type")) r.set("check_type", chk.get("check_type"));
		}

		int cqId = safeGetInt(cu, "check_quantity_id");
		auto cqIt = cqMap.find(Value(cqId));
		if (cqIt != cqMap.end()) {
			const Record &cq = *(cqIt->second);
			if (cq.has("product_id")) {
				int pid = safeGetInt(cq, "product_id");
				r.set("product_id", pid);
				auto pIt = pMap.find(Value(pid));
				if (pIt != pMap.end()) {
					const Record &p = *(pIt->second);
					if (p.has("product_code")) r.set("product_code", p.get("product_code"));
					if (p.has("product_name")) r.set("product_name", p.get("product_name"));
				}
			}
		}

		std::string sysSt = cu.has("system_status") && std::holds_alternative<std::string>(cu.get("system_status"))
			? std::get<std::string>(cu.get("system_status")) : "";
		std::string scanSt = cu.has("scanned_status") && std::holds_alternative<std::string>(cu.get("scanned_status"))
			? std::get<std::string>(cu.get("scanned_status")) : "";

		if (sysSt.empty()) {
			r.set("match_result", std::string("Thừa mới (" + scanSt + ")"));
		} else if (sysSt == "OUT") {
			r.set("match_result", std::string("Tái phát hiện (" + scanSt + ")"));
		} else if (scanSt.empty() || scanSt == "UNCHECKED") {
			r.set("match_result", std::string("Chưa quét"));
		} else if (scanSt == "OUT") {
			r.set("match_result", std::string("Mất/Thiếu"));
		} else if (scanSt == sysSt) {
			r.set("match_result", std::string("Khớp (" + sysSt + ")"));
		} else {
			r.set("match_result", std::string("Lệch (" + sysSt + " -> " + scanSt + ")"));
		}

		results.push_back(std::move(r));
	}
}

void adjustment_movement_query(Database *db, Vector<Record> &results) {
	adjustment_movement_query(db, results, "", Value());
}

void adjustment_movement_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &val) {
	results.clear();
	if (!db) return;
	const Table* adjTable = db->getTable("adjustment_movement");
	if (!adjTable) return;

	string col = stripPrefix(indexedColumn, "adjustment_movement.");
	if (!col.empty()) {
		results = Query::filterRecords(*adjTable, col, val);
	} else {
		for (const auto &r : adjTable->view()) {
			results.push_back(r);
		}
	}
}

void adjustment_movement_query(Database *db, Vector<Record> &results,
	const string &indexedColumn, const Value &minVal, const Value &maxVal) {
	results.clear();
	if (!db) return;
	const Table* adjTable = db->getTable("adjustment_movement");
	if (!adjTable) return;

	string col = stripPrefix(indexedColumn, "adjustment_movement.");
	results = Query::filterRecords(*adjTable, col, minVal, maxVal);
}

void adjust_movement_detail_query(Database *db, int adjustId, Vector<Record> &results) {
	results.clear();
	if (!db) return;
	const Table* amdTable = db->getTable("adjust_movement_detail");
	const Table* adjTable = db->getTable("adjustment_movement");
	const Table* pTable = db->getTable("product");
	if (!amdTable) return;

	HashMap<Value, const Record*> adjMap;
	if (adjTable) {
		for (const auto &r : adjTable->view()) {
		if (r.has("adjust_id")) {
			adjMap[r.get("adjust_id")] = &r;
		}
	}
	}

	HashMap<Value, const Record*> pMap;
	if (pTable) {
		for (const auto &r : pTable->view()) {
		if (r.has("product_id")) {
			pMap[r.get("product_id")] = &r;
		}
	}
	}

	Vector<Record> amdRows;
	if (adjustId > 0) {
		amdRows = Query::filterRecords(*amdTable, "adjust_id", Value(adjustId));
	} else {
		for (const auto &r : amdTable->view()) amdRows.push_back(r);
	}

	results.reserve(amdRows.size());
	for (size_t i = 0; i < amdRows.size(); ++i) {
		const auto &amd = amdRows[i];
		Record r = amd;
		int aId = safeGetInt(amd, "adjust_id");
		auto adjIt = adjMap.find(Value(aId));
		if (adjIt != adjMap.end()) {
			const Record &adj = *(adjIt->second);
			if (adj.has("adjust_code")) r.set("adjust_code", adj.get("adjust_code"));
			if (adj.has("source_code")) r.set("source_code", adj.get("source_code"));
			if (adj.has("status")) r.set("adjust_status", adj.get("status"));
		}

		int pid = safeGetInt(amd, "product_id");
		auto it = pMap.find(Value(pid));
		if (it != pMap.end()) {
			const Record &p = *(it->second);
			if (p.has("product_code")) r.set("product_code", p.get("product_code"));
			if (p.has("product_name")) r.set("product_name", p.get("product_name"));
		}
		results.push_back(std::move(r));
	}
}

void is_sold_changing_query(Database *db, Vector<Record> &results, int productId) {
	results.clear();
	if (!db) return;
	const Table* iscTable = db->getTable("is_sold_changing");
	const Table* pTable = db->getTable("product");
	if (!iscTable) return;

	HashMap<Value, const Record*> pMap;
	if (pTable) {
		for (const auto &r : pTable->view()) {
		if (r.has("product_id")) {
			pMap[r.get("product_id")] = &r;
		}
	}
	}

	Vector<Record> source;
	if (productId > 0) {
		source = Query::filterRecords(*iscTable, "product_id", Value(productId));
	} else {
		for (const auto &r : iscTable->view()) source.push_back(r);
	}

	results.reserve(source.size());
	for (size_t i = 0; i < source.size(); ++i) {
		Record r = source[i];
		int pid = safeGetInt(r, "product_id");
		auto it = pMap.find(Value(pid));
		if (it != pMap.end()) {
			const Record &p = *(it->second);
			if (p.has("product_code")) r.set("product_code", p.get("product_code"));
			if (p.has("product_name")) r.set("product_name", p.get("product_name"));
		}
		results.push_back(std::move(r));
	}
}

} // namespace user_query
