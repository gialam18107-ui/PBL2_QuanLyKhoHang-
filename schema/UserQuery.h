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

	// ========================================================
	// 18. CÁC HÀM TIỆN ÍCH TRUY VẤN CHO GIAO DIỆN & NGHIỆP VỤ (UI & BUSINESS HELPERS)
	// ========================================================

	// 18.1 Tra cứu ID đơn hàng, trạng thái, ngày đặt và ghi chú từ mã đơn
	bool resolve_order_info_by_code(
		Database* db,
		const string& orderCode,
		bool isInbound,
		int& txnId,
		string& status,
		string& orderDate,
		string& note
	);

	// 18.2 Lấy trạng thái đơn hàng từ mã đơn
	string get_order_status_by_code(Database* db, const string& orderCode, bool isInbound);

	// 18.3 Lấy mã nguồn (source_code) của đơn hàng từ mã đơn
	string get_order_source_code_by_code(Database* db, const string& orderCode, bool isInbound);

	// 18.4 Tra cứu ID đợt vận chuyển từ mã giao nhận (delivery_code / shipment_code)
	int resolve_logistics_id_by_code(Database* db, const string& logisticsCode, bool isInbound);

	// 18.5 Tìm kiếm nhanh 1 sản phẩm theo mã (product_code / barcode / sku) hoặc từ khóa tên
	int search_single_product(Database* db, const string& keyword, bool& isSoldOut);

	// 18.6 Tra cứu thông tin chi tiết một linh kiện từ serial
	bool get_unit_details_by_serial(
		Database* db,
		const string& serial,
		int& unitId,
		int& productId,
		string& status,
		bool& isSold
	);

	// 18.7 Tra cứu thông tin cơ bản của sản phẩm phục vụ hiển thị
	bool get_product_display_info(
		Database* db,
		int productId,
		string& productCode,
		string& productName,
		bool& isSold
	);

	// 18.8 Tra cứu ID sản phẩm từ mã sản phẩm
	int resolve_product_id_by_code(Database* db, const string& productCode);

	// 18.9 Tra cứu ID khách hàng từ mã khách hàng
	int resolve_customer_id_by_code(Database* db, const string& customerCode);

	// 18.10 Kiểm tra thương hiệu có đang được sản phẩm nào tham chiếu không
	bool is_brand_referenced_by_product(Database* db, const string& brandName);

	// 18.11 Kiểm tra tiến độ đợt giao nhận (đã có linh kiện nhận/xuất thực tế > 0 hay chưa)
	bool has_logistics_batch_progress(Database* db, int logId, bool isInbound);

	// 18.12 Tổng hợp tiến độ của đơn hàng (tổng số lượng kế hoạch, thực tế, đã phát sinh tiến độ hay chưa)
	void get_order_progress_summary(
		Database* db,
		int txnId,
		bool isInbound,
		int& totalPlanQty,
		int& totalActQty,
		bool& hasProgress
	);

	// 18.13 Lấy ID đợt xuất đầu tiên của một đơn xuất kho
	int get_first_shipment_id_for_order(Database* db, const string& outboundCode);

	// 18.14 Lấy danh sách ID sản phẩm được kiểm kê trong một đợt kiểm kho
	void get_check_product_ids(Database* db, const string& checkCode, Vector<int>& productIds);

	// 18.15 Lấy danh sách serial ứng viên để điều chuyển trạng thái
	void get_candidate_units_for_adjustment(
		Database* db,
		int productId,
		const string& fromStatus,
		const string& toStatus,
		const string& sourceCheckCode,
		const Vector<string>& excludeList,
		Vector<Record>& results
	);

	// 18.16 Lấy lịch sử các linh kiện đã được điều chuyển trong một phiếu điều chuyển
	void adjustment_units_history_query(Database* db, int adjustId, Vector<Record>& results);

	// 18.17 Tra cứu thông tin đơn giao nhận đầu tiên phát sinh từ đợt kiểm kho
	int get_first_logistics_id_for_check_source(Database* db, const string& checkCode, bool isInbound);

	// 18.18 Lấy danh sách serial ứng viên để xuất kho / phân bổ cho đơn hàng
	void get_candidate_units_for_order_dispatch(
		Database* db,
		int productId,
		const string& sourceCheckCode,
		const Vector<string>& excludeList,
		Vector<Record>& results
	);

	// 18.19 Kiểm tra xem đợt kiểm kho đã phát sinh đơn nhập/xuất/điều chuyển chênh lệch chưa
	bool get_check_consequential_codes(
		Database* db,
		const string& checkCode,
		string& inboundCode,
		string& outboundCode,
		string& adjustCode
	);
}

#endif // USERQUERY_H
