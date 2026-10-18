#ifndef USERQUERY_H
#define USERQUERY_H

#include "database/Database.h"
#include "database/Record.h"
#include "datastructure/vector/Vector.h"
#include <string>

using namespace std;

namespace user_query {
    // ========================================================
    // SORT TRÊN KẾT QUẢ VECTOR<RECORD>
    // ========================================================
    void sort_query_results(Vector<Record>& records, const string& chosenColumn, bool ascending = true);

    // ========================================================
    // 12 GIAO DIỆN TRUY VẤN (USER QUERY)
    // - Chỉ cho phép lọc trên CỘT CÓ INDEX (qua Query::filterRecords)
    // - Chỉ chọn đúng 1 cột làm tiêu chí tại một thời điểm
    // - Khi đổi sang cột khác, gọi với cột mới sẽ tự động RESET tiêu chí cũ
    // - Dữ liệu trả về luôn có ĐẦY ĐỦ TẤT CẢ CÁC CỘT của view
    // ========================================================

    // 1. PRODUCT_QUERY (Index: hash_tree_is_sold)
    void product_query(Database *db, Vector<Record> &results);
    void product_query(Database *db, Vector<Record> &results, bool is_sold);
    void product_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);

    // 2. PRODUCT_QUANTITY_QUERY (Index: product.is_sold)
    void product_quantity_query(Database *db, Vector<Record> &results);
    void product_quantity_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);

    // 3. UNIT_INVENTORY_QUERY (Index: serial, status)
    void unit_inventory_query(Database *db, Vector<Record> &results);
    void unit_inventory_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);

    // 4. STATUS_HISTORY_QUERY (Index: status_history.type; Kèm cột 'date')
    void status_history_query(Database *db, Vector<Record> &results);
    void status_history_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);

    // 5. INBOUND_TRANSACTION_QUERY (Index: inbound_transaction.order_at)
    void inbound_transaction_query(Database *db, Vector<Record> &results);
    void inbound_transaction_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
    void inbound_transaction_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);

    // 6. INBOUND_DETAIL_QUERY
    void inbound_detail_query(Database *db, Vector<Record> &results);
    void inbound_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);

    // 7. DELIVERY_QUERY (Index: delivery.delivery_at, expected_at)
    void delivery_query(Database *db, Vector<Record> &results);
    void delivery_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
    void delivery_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);

    // 8. DELIVERY_DETAIL_QUERY (Index: delivery.delivery_at, expected_at)
    void delivery_detail_query(Database *db, Vector<Record> &results);
    void delivery_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
    void delivery_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);

    // 9. OUTBOUND_TRANSACTION_QUERY (Index: outbound_transaction.order_at)
    void outbound_transaction_query(Database *db, Vector<Record> &results);
    void outbound_transaction_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
    void outbound_transaction_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);

    // 10. OUTBOUND_DETAIL_QUERY
    void outbound_detail_query(Database *db, Vector<Record> &results);
    void outbound_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);

    // 11. SHIPMENT_QUERY (Index: shipment.ship_at, expected_at)
    void shipment_query(Database *db, Vector<Record> &results);
    void shipment_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
    void shipment_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);

    // 12. SHIPMENT_DETAIL_QUERY (Index: shipment.ship_at, expected_at)
    void shipment_detail_query(Database *db, Vector<Record> &results);
    void shipment_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
    void shipment_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);
}

#endif // USERQUERY_H
