#ifndef USERQUERY_H
#define USERQUERY_H

#include "database/Database.h"
#include "database/Record.h"
#include "datastructure/vector/Vector.h"
#include "datastructure/hashmap/Pair.h"
#include "query/Query.h"
#include <string>

using namespace std;

namespace user_query {
	// ========================================================
	// SORT TRÊN KẾT QUẢ VECTOR<RECORD>
	// ========================================================
	inline void sort_query_results(Vector<Record>& records, const string& chosenColumn, bool ascending = true) {
		Query::querySort(records, chosenColumn, ascending);
	}

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

	// ========================================================
	// 15. BÁO CÁO XUẤT - NHẬP - TỒN ĐỊNH KỲ (PERIODIC INVENTORY REPORT - Section 9)
	// - Tính toán số lượng biến động và tồn kho theo từng tháng cho sản phẩm đang bán (is_sold == TRUE)
	// - Tồn đầu kỳ: tính dồn mọi giao dịch trước ngày 01 của tháng báo cáo
	// - Nhập/Xuất trong tháng: tính theo actual_quantity của delivery_detail và shipment_detail trong tháng
	// - Trả về danh sách kết quả sản phẩm và Record grandTotal chứa dòng tổng cộng
	// ========================================================
	void periodic_inventory_report(Database *db, int month, int year, Vector<Record> &results, Record &grandTotal);

	// ========================================================
	// 16. TRUY VẤN VÀ TỔNG KẾT LINH KIỆN GIAO VẬN (DELIVERY / SHIPMENT UNITS & LOGISTICS QUERIES)
	// ========================================================
	// Lấy danh sách các unit bị thay đổi trạng thái theo mã đợt giao nhận (delivery_code / shipment_code)
	// Trả về Record chứa: serial, product_id, product_name, product_code, old_status, curr_status, type, date
	void logistics_units_history_query(Database *db, const string& logisticsCode, Vector<Record> &results);

	// Tổng kết số lượng linh kiện theo từng trạng thái cho một đợt giao nhận
	// Trả về Vector các cặp (status_name, count)
	void logistics_unit_status_summary(Database *db, const string& logisticsCode, Vector<Pair<string, int>> &summary);

	// Lấy loại giao dịch (type) từ mã đơn (inbound_code / outbound_code)
	string get_transaction_type_by_order_code(Database *db, const string& orderCode, bool isInbound);

	// Lấy trạng thái của đợt giao nhận (delivery_code / shipment_code)
	string get_logistics_status_by_code(Database *db, const string& logisticsCode, bool isInbound);

	// Kiểm tra đợt giao nhận đã hoàn thành (COMPLETED) hay chưa
	bool is_logistics_completed(Database *db, const string& logisticsCode, bool isInbound);

	// Tra cứu product_id từ serial
	int get_product_id_by_serial(Database *db, const string& serial);

	// Tra cứu trạng thái hiện tại từ serial
	string get_unit_status_by_serial(Database *db, const string& serial);

	// Kiểm tra serial có tồn tại trong unit_inventory hay không
	bool is_unit_in_inventory(Database *db, const string& serial);

	// Lấy danh sách các unit ứng viên trong kho cho các sản phẩm của đợt giao nhận
	// Record chứa: unit_id, serial, product_id, product_name, product_code, status
	void get_candidate_units_for_order(Database *db, const Vector<int>& productIds, Vector<Record>& results);

	// Lấy danh sách toàn bộ khách hàng (cá nhân và doanh nghiệp)
	void all_customers_query(Database *db, Vector<Record> &results);

	// Lấy danh sách các giá trị trạng thái từ một bảng status (vd: delivery_status, shipment_status, transaction_status)
	void get_all_statuses_for_table(Database *db, const string& tableName, Vector<string> &statuses);

	// Thông tin phân bổ hàng hóa cho các đợt giao nhận (delivery / shipment)
	struct LogisticsBatchItemInfo {
		int productId = 0;
		string productCode;
		string productName;
		int orderedQty = 0;
		int alreadyAllocatedQty = 0;
		int remainingToAllocate = 0;
		int currentStockAvail = 0;
		int currentStockWrt = 0;
		int currentStockScrap = 0;
	};
	void get_logistics_batch_allocation_info(Database* db, int txnId, bool isInbound, Vector<LogisticsBatchItemInfo>& items);

	// Tổng kết số lượng theo từng trạng thái của từng sản phẩm trong một đợt giao nhận
	struct ProductLogisticsStatusSummary {
		int productId = 0;
		string productCode;
		string productName;
		int batchQty = 0;
		int actualQty = 0;
		int availableQty = 0;
		int warrantyQty = 0;
		int scrappedQty = 0;
		int outQty = 0;
	};
	void logistics_product_status_summary(
		Database* db,
		const string& logisticsCode,
		bool isInbound,
		Vector<ProductLogisticsStatusSummary>& summary
	);

	// ========================================================
	// 17. TRUY VẤN KIỂM KHO VÀ ĐIỀU CHUYỂN (INVENTORY CHECK & ADJUSTMENT QUERIES)
	// ========================================================
	// 17.1 Truy vấn đợt kiểm kho (inventory_check)
	void inventory_check_query(Database *db, Vector<Record> &results);
	void inventory_check_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
	void inventory_check_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);

	// 17.2 Chi tiết số lượng kiểm kê theo đợt (inventory_check_quantity)
	void inventory_check_quantity_query(Database *db, int checkId, Vector<Record> &results);

	// 17.3 Chi tiết linh kiện kiểm kê theo đợt (inventory_check_unit)
	void inventory_check_unit_query(Database *db, int checkId, Vector<Record> &results);

	// 17.4 Truy vấn phiếu điều chuyển trạng thái (adjustment_movement)
	void adjustment_movement_query(Database *db, Vector<Record> &results);
	void adjustment_movement_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& val);
	void adjustment_movement_query(Database *db, Vector<Record> &results, const string& indexedColumn, const Value& minVal, const Value& maxVal);

	// 17.5 Chi tiết điều chuyển (adjust_movement_detail)
	void adjust_movement_detail_query(Database *db, int adjustId, Vector<Record> &results);

	// 17.6 Lịch sử thay đổi trạng thái bán is_sold (is_sold_changing)
	void is_sold_changing_query(Database *db, Vector<Record> &results, int productId = 0);
}

#endif // USERQUERY_H
