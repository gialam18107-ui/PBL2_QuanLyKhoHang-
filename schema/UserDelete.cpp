#include "UserDelete.h"
#include "query/Query.h"
#include <iostream>
#include <cctype>

namespace user_delete
{
    static std::string getSpecTableName(const std::string& categoryName)
    {
        std::string catUpper = categoryName;
        for (auto& c : catUpper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (catUpper == "CPU") return "cpu";
        if (catUpper == "GPU") return "gpu";
        if (catUpper == "MAINBOARD") return "mainboard";
        if (catUpper == "RAM") return "ram";
        if (catUpper == "SSD") return "ssd";
        if (catUpper == "PSU") return "psu";
        if (catUpper == "COOLING") return "cooling";
        if (catUpper == "CASE") return "case";
        return "";
    }

    static bool hasMatchingRecord(const Table* table, const std::string& colName, int value)
    {
        if (!table) return false;
        return table->contains(colName, Value(value));
    }

    OperationResult deleteProduct(Database* db, int productId)
    {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        const Table* prodTable = db->getTable("product");
        if (!prodTable) return OperationResult{false, "Bảng 'product' không tồn tại.", 0};

        // 1. Locate product and get category
        const Record* pRec = prodTable->getRecordById(productId);
        if (!pRec) {
            return OperationResult{false, "Không tìm thấy sản phẩm với ID " + std::to_string(productId) + ".", 0};
        }
        std::string categoryName = pRec->has("category") && std::holds_alternative<std::string>(pRec->get("category"))
                                 ? std::get<std::string>(pRec->get("category")) : "";

        // 2. Stock Precondition Check: Product cannot be deleted if active stock exists
        int avail = (pRec->has("available_quantity") && std::holds_alternative<int>(pRec->get("available_quantity"))) ? std::get<int>(pRec->get("available_quantity")) : 0;
        int wrt = (pRec->has("warranty_quantity") && std::holds_alternative<int>(pRec->get("warranty_quantity"))) ? std::get<int>(pRec->get("warranty_quantity")) : 0;
        int scr = (pRec->has("scrapped_quantity") && std::holds_alternative<int>(pRec->get("scrapped_quantity"))) ? std::get<int>(pRec->get("scrapped_quantity")) : 0;
        if (avail > 0 || wrt > 0 || scr > 0) {
            return OperationResult{false, "Không thể xóa sản phẩm: Vẫn còn hàng tồn trong kho (Khả dụng: " + std::to_string(avail) + ", Bảo hành: " + std::to_string(wrt) + ", Lỗi/hỏng: " + std::to_string(scr) + ").", 0};
        }

        // 3. History Checks (Transactions, Units, Inventory Checks, Movements, Status Changing)
        if (hasMatchingRecord(db->getTable("inbound_detail"), "product_id", productId)) {
            return OperationResult{false, "Không thể xóa sản phẩm: Đã có lịch sử trong Đơn nhập kho (inbound_detail).", 0};
        }
        if (hasMatchingRecord(db->getTable("outbound_detail"), "product_id", productId)) {
            return OperationResult{false, "Không thể xóa sản phẩm: Đã có lịch sử trong Đơn xuất kho (outbound_detail).", 0};
        }
        if (hasMatchingRecord(db->getTable("unit_inventory"), "product_id", productId)) {
            return OperationResult{false, "Không thể xóa sản phẩm: Đang có Unit / Serial tồn tại cho sản phẩm này.", 0};
        }
        if (hasMatchingRecord(db->getTable("inventory_check_quantity"), "product_id", productId)) {
            return OperationResult{false, "Không thể xóa sản phẩm: Đã có lịch sử trong Đợt kiểm kho (inventory_check_quantity).", 0};
        }
        if (hasMatchingRecord(db->getTable("adjust_movement_detail"), "product_id", productId)) {
            return OperationResult{false, "Không thể xóa sản phẩm: Đã có lịch sử trong Phiếu điều chuyển (adjust_movement_detail).", 0};
        }
        if (hasMatchingRecord(db->getTable("is_sold_changing"), "product_id", productId)) {
            return OperationResult{false, "Không thể xóa sản phẩm: Đã có lịch sử thay đổi trạng thái bán (is_sold_changing).", 0};
        }

        std::string specTable = getSpecTableName(categoryName);

        // 4. Atomic Multi-table Deletion
        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu Transaction CSDL.", 0};
        }

        // Delete from hardware spec table first (child)
        if (!specTable.empty() && db->hasTable(specTable)) {
            const Table* sTab = db->getTable(specTable);
            if (sTab) {
                std::string pkCol = sTab->getPrimaryKeyColumn();
                Vector<int> specIds;
                for (const auto& r : sTab->view("product_id", Value(productId))) {
                    if (!pkCol.empty() && r.has(pkCol) && std::holds_alternative<int>(r.get(pkCol))) {
                        specIds.push_back(std::get<int>(r.get(pkCol)));
                    }
                }
                for (size_t i = 0; i < specIds.size(); ++i) {
                    OperationResult specRes = db->remove(specTable, specIds[i]);
                    if (!specRes && specRes.message.find("tham chiếu") != std::string::npos) {
                        db->rollback();
                        return specRes;
                    }
                }
                // Spec records are removed by primary key above

            }
        }



        // Delete from product (parent)
        OperationResult prodRes = db->remove("product", productId);
        if (!prodRes) {
            db->rollback();
            return OperationResult{false, "Không thể xóa sản phẩm khỏi bảng 'product': " + prodRes.message, 0};
        }

        db->commit();
        return OperationResult{true, "Xóa sản phẩm thành công!", 3};
    }

    OperationResult deleteCustomer(Database* db, int customerId)
    {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        const Table* custTable = db->getTable("customer");
        if (!custTable) return OperationResult{false, "Bảng customer không tồn tại.", 0};

        const Record* cRec = custTable->getRecordById(customerId);
        if (!cRec) {
            return OperationResult{false, "Không tìm thấy khách hàng với ID " + std::to_string(customerId) + ".", 0};
        }

        if (customerId == 0 || (cRec->has("customer_code") && std::holds_alternative<std::string>(cRec->get("customer_code")) && std::get<std::string>(cRec->get("customer_code")) == "BS-0000000")) {
            return OperationResult{false, "Không thể xóa khách hàng hệ thống mặc định (BS-0000000).", 0};
        }

        // 1. Transaction History Precondition Check (Accounting / Economic Rule)
        if (hasMatchingRecord(db->getTable("inbound_transaction"), "customer_id", customerId)) {
            return OperationResult{false, "Không thể xóa khách hàng: Đã có lịch sử trong Đơn nhập kho (inbound_transaction). Quy định kế toán yêu cầu bảo lưu chứng từ.", 0};
        }
        if (hasMatchingRecord(db->getTable("outbound_transaction"), "customer_id", customerId)) {
            return OperationResult{false, "Không thể xóa khách hàng: Đã có lịch sử trong Đơn xuất kho (outbound_transaction). Quy định kế toán yêu cầu bảo lưu chứng từ.", 0};
        }

        // 2. Safe deletion in transaction
        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu Transaction CSDL.", 0};
        }

        // Remove from individual_info or business_info first
        if (db->hasTable("individual_info")) {
            const Table* iTab = db->getTable("individual_info");
            if (iTab && iTab->getRecordById(customerId)) {
                db->remove("individual_info", customerId);
            }
        }
        if (db->hasTable("business_info")) {
            const Table* bTab = db->getTable("business_info");
            if (bTab && bTab->getRecordById(customerId)) {
                db->remove("business_info", customerId);
            }
        }

        OperationResult resCust = db->remove("customer", customerId);
        if (!resCust) {
            db->rollback();
            return OperationResult{false, "Lỗi khi xóa khách hàng khỏi bảng customer: " + resCust.message, 0};
        }

        db->commit();
        return OperationResult{true, "Xóa khách hàng thành công!", 2};
    }

    OperationResult deleteBrand(Database* db, int brandId)
    {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        const Table* brandTable = db->getTable("brand");
        if (!brandTable) return OperationResult{false, "Bảng brand không tồn tại.", 0};

        const Record* bRec = brandTable->getRecordById(brandId);
        if (!bRec) {
            return OperationResult{false, "Không tìm thấy thương hiệu với ID " + std::to_string(brandId) + ".", 0};
        }

        std::string brandName = bRec->has("brand") && std::holds_alternative<std::string>(bRec->get("brand"))
                              ? std::get<std::string>(bRec->get("brand")) : "";

        // Check if referenced by product
        const Table* prodTable = db->getTable("product");
        if (prodTable && !brandName.empty()) {
            if (prodTable->contains("brand", Value(brandName))) {
                return OperationResult{false, "Không thể xóa thương hiệu '" + brandName + "': Đang có sản phẩm trong danh mục thuộc thương hiệu này.", 0};
            }
        }

        return db->remove("brand", brandId);
    }

    OperationResult deleteRecord(Database* db, const std::string& tableName, int recordId)
    {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};

        // 1. Transaction & History Immutability Protection (Rule 5.3 & 6.3.1)
        if (tableName == "inbound_transaction" || tableName == "inbound_detail" ||
            tableName == "delivery" || tableName == "delivery_detail" ||
            tableName == "outbound_transaction" || tableName == "outbound_detail" ||
            tableName == "shipment" || tableName == "shipment_detail" ||
            tableName == "unit_inventory" || tableName == "status_history" ||
            tableName == "category" || tableName == "is_sold_changing" ||
            tableName == "inventory_check" || tableName == "inventory_check_quantity" ||
            tableName == "inventory_check_unit" || tableName == "adjustment_movement" ||
            tableName == "adjust_movement_detail") {
            return OperationResult{false, "Bảng '" + tableName + "' là dữ liệu hệ thống / chứng từ giao dịch đã xác nhận, không được phép xóa.", 0};
        }

        // 2. Handlers for coupled / special tables
        if (tableName == "product") return deleteProduct(db, recordId);
        if (tableName == "customer") return deleteCustomer(db, recordId);
        if (tableName == "brand") return deleteBrand(db, recordId);

        // 3. Dispatch directly to Database
        return db->remove(tableName, recordId);
    }
}
