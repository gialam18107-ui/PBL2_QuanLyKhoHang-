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
    // GIAO DIỆN TRUY VẤN (USER QUERY)
    // - Chỉ cho phép lọc trên CỘT CÓ INDEX (qua Query::filterRecords)
    // - Chỉ chọn đúng 1 cột làm tiêu chí tại một thời điểm
    // - Khi đổi sang cột khác, gọi với cột mới sẽ tự động RESET tiêu chí cũ
    // - Dữ liệu trả về luôn có ĐẦY ĐỦ TẤT CẢ CÁC CỘT của view
    // ========================================================

    // 1. PRODUCT_QUANTITY_QUERY (Index: product.is_sold)
    void product_quantity_query(Database *db, Vector<Record> &results);
    void product_quantity_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);

    // 2. UNIT_INVENTORY_QUERY (Index: serial, status)
    void unit_inventory_query(Database *db, Vector<Record> &results);
    void unit_inventory_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);

    // 3. STATUS_HISTORY_QUERY
    // - Cột transaction: hiển thị delivery_code (INBOUND) hoặc shipment_code (OUTBOUND)
    // - Cột date: ngày giao hàng / xuất hàng (delivery_at hoặc ship_at)
    // - Khi tìm kiếm transaction: tìm kiếm trên delivery.delivery_code và shipment.shipment_code (Hash) trước
    // - Khi tìm kiếm date: tìm kiếm trên delivery.delivery_at và shipment.ship_at (AVL tree range) trước
    // - Tìm kiếm type: tìm kiếm trên status_history.type (HashTree)
    void status_history_query(Database *db, Vector<Record> &results);
    void status_history_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
    void status_history_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);

    // 4. INBOUND_TRANSACTION_QUERY (Index: inbound_transaction.order_at, type, status, inbound_code)
    void inbound_transaction_query(Database *db, Vector<Record> &results);
    void inbound_transaction_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
    void inbound_transaction_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);

    // 5. INBOUND_DETAIL_QUERY
    void inbound_detail_query(Database *db, Vector<Record> &results);
    void inbound_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);

    // 6. DELIVERY_QUERY (Index: delivery.delivery_at, expected_at, status, delivery_code)
    void delivery_query(Database *db, Vector<Record> &results);
    void delivery_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
    void delivery_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);

    // 7. DELIVERY_DETAIL_QUERY (Index: delivery.delivery_at, expected_at, delivery_code, product.product_code)
    void delivery_detail_query(Database *db, Vector<Record> &results);
    void delivery_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
    void delivery_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);

    // 8. OUTBOUND_TRANSACTION_QUERY (Index: outbound_transaction.order_at, type, status, outbound_code)
    void outbound_transaction_query(Database *db, Vector<Record> &results);
    void outbound_transaction_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
    void outbound_transaction_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);

    // 9. OUTBOUND_DETAIL_QUERY
    void outbound_detail_query(Database *db, Vector<Record> &results);
    void outbound_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);

    // 10. SHIPMENT_QUERY (Index: shipment.ship_at, expected_at, status, shipment_code)
    void shipment_query(Database *db, Vector<Record> &results);
    void shipment_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
    void shipment_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);

    // 11. SHIPMENT_DETAIL_QUERY (Index: shipment.ship_at, expected_at, shipment_code, product.product_code)
    void shipment_detail_query(Database *db, Vector<Record> &results);
    void shipment_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
    void shipment_detail_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);

    // ========================================================
    // 12. CÁC BẢNG PHÂN LOẠI SẢN PHẨM (CATEGORY HARDWARE QUERIES)
    // - Trả về: product_code, product_name, các thuộc tính của danh mục, và is_sold (đang bán hay không)
    // - Cho phép lọc theo is_sold và các thuộc tính cấu hình có index (HashTree)
    // ========================================================
    void category_hardware_query(Database *db, const string& categoryName, Vector<Record> &results);
    void category_hardware_query(Database *db, const string& categoryName, Vector<Record> &results, const string& indexedColumn, const Value& val);

    void cpu_query(Database *db, Vector<Record> &results, const string& indexedColumn = "", const Value& val = Value());
    void gpu_query(Database *db, Vector<Record> &results, const string& indexedColumn = "", const Value& val = Value());
    void mainboard_query(Database *db, Vector<Record> &results, const string& indexedColumn = "", const Value& val = Value());
    void ram_query(Database *db, Vector<Record> &results, const string& indexedColumn = "", const Value& val = Value());
    void ssd_query(Database *db, Vector<Record> &results, const string& indexedColumn = "", const Value& val = Value());
    void psu_query(Database *db, Vector<Record> &results, const string& indexedColumn = "", const Value& val = Value());
    void cooling_query(Database *db, Vector<Record> &results, const string& indexedColumn = "", const Value& val = Value());
    void case_query(Database *db, Vector<Record> &results, const string& indexedColumn = "", const Value& val = Value());

    // ========================================================
    // 13. TRUY VẤN PHỤ CHO HIỂN THỊ SPECS, CATEGORY, BRAND
    // - Hiển thị danh sách specs của các category (vd: cpu_socket, ssd_capacity,...)
    // - Hiển thị bảng category, bảng brand
    // - Không cần index vì dữ liệu ít
    // ========================================================
    void auxiliary_spec_query(Database *db, const string& tableName, Vector<Record> &results, const string& filterColumn = "", const Value& val = Value());

    // ========================================================
    // 14. TRUY VẤN KHÁCH HÀNG (CUSTOMER QUERIES - Rule 9.4)
    // - Tách riêng Khách hàng cá nhân (individual) và Khách hàng doanh nghiệp (business)
    // - Giữ customer_id trong Record để phục vụ CRUD nhưng ẩn trên giao diện UI
    // - Hỗ trợ lọc theo mã khách hàng (Index), tên, số điện thoại, email, mã số thuế
    // ========================================================
    void individual_customer_query(Database *db, Vector<Record> &results, const string& indexedColumn = "", const Value& val = Value());
    void business_customer_query(Database *db, Vector<Record> &results, const string& indexedColumn = "", const Value& val = Value());
}

#endif // USERQUERY_H
