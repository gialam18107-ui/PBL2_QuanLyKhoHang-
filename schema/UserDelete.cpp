#include "UserDelete.h"
#include "UserUpdate.h"
#include <iostream>

namespace user_delete
{
    OperationResult deleteProduct(Database* db, int productId)
    {
        if (!db) {
            return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        }
        Table* prodTable = db->getTable("product");
        if (!prodTable) {
            return OperationResult{false, "Bảng 'product' không tồn tại.", 0};
        }

        // 1. Locate product and get its category
        int prodRowIdx = -1;
        std::string categoryName;
        const auto& pRows = prodTable->getRows();
        for (size_t i = 0; i < pRows.size(); ++i) {
            if (prodTable->isRowDeleted(i)) continue;
            const Record& r = pRows[i];
            if (r.has("product_id") && holds_alternative<int>(r.get("product_id"))) {
                if (get<int>(r.get("product_id")) == productId) {
                    prodRowIdx = static_cast<int>(i);
                    if (r.has("category") && holds_alternative<std::string>(r.get("category"))) {
                        categoryName = get<std::string>(r.get("category"));
                    }
                    break;
                }
            }
        }

        if (prodRowIdx == -1) {
            return OperationResult{false, "Không tìm thấy sản phẩm với ID " + std::to_string(productId) + ".", 0};
        }

        // 2. Stock Precondition Check: Product cannot be deleted if active stock exists
        Table* qtyTable = db->getTable("product_quantity");
        if (qtyTable) {
            const auto& qRows = qtyTable->getRows();
            for (size_t i = 0; i < qRows.size(); ++i) {
                if (qtyTable->isRowDeleted(i)) continue;
                const Record& r = qRows[i];
                if (r.has("product_id") && holds_alternative<int>(r.get("product_id")) && get<int>(r.get("product_id")) == productId) {
                    int avail = r.has("available_quan") && holds_alternative<int>(r.get("available_quan")) ? get<int>(r.get("available_quan")) : 0;
                    int wrt = r.has("warranty_quan") && holds_alternative<int>(r.get("warranty_quan")) ? get<int>(r.get("warranty_quan")) : 0;
                    int scr = r.has("scrapped_quan") && holds_alternative<int>(r.get("scrapped_quan")) ? get<int>(r.get("scrapped_quan")) : 0;
                    if (avail > 0 || wrt > 0 || scr > 0) {
                        return OperationResult{false, "Không thể xóa sản phẩm: Vẫn còn hàng tồn trong kho (Khả dụng: " + std::to_string(avail) + ", Bảo hành: " + std::to_string(wrt) + ", Phế phẩm: " + std::to_string(scr) + ").", 0};
                    }
                    break;
                }
            }
        }

        // 3. Transaction History Check
        Table* inDetailTab = db->getTable("inbound_detail");
        if (inDetailTab) {
            const auto& rows = inDetailTab->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (inDetailTab->isRowDeleted(i)) continue;
                if (rows[i].has("product_id") && holds_alternative<int>(rows[i].get("product_id")) && get<int>(rows[i].get("product_id")) == productId) {
                    return OperationResult{false, "Không thể xóa sản phẩm: Đã có lịch sử trong Đơn nhập kho (inbound_detail).", 0};
                }
            }
        }

        Table* outDetailTab = db->getTable("outbound_detail");
        if (outDetailTab) {
            const auto& rows = outDetailTab->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (outDetailTab->isRowDeleted(i)) continue;
                if (rows[i].has("product_id") && holds_alternative<int>(rows[i].get("product_id")) && get<int>(rows[i].get("product_id")) == productId) {
                    return OperationResult{false, "Không thể xóa sản phẩm: Đã có lịch sử trong Đơn xuất kho (outbound_detail).", 0};
                }
            }
        }

        Table* unitTab = db->getTable("unit_inventory");
        if (unitTab) {
            const auto& rows = unitTab->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (unitTab->isRowDeleted(i)) continue;
                if (rows[i].has("product_id") && holds_alternative<int>(rows[i].get("product_id")) && get<int>(rows[i].get("product_id")) == productId) {
                    return OperationResult{false, "Không thể xóa sản phẩm: Đang có Unit / Serial tồn tại cho sản phẩm này.", 0};
                }
            }
        }

        // Determine specific hardware category table
        std::string catUpper = categoryName;
        for (auto& c : catUpper) c = std::toupper(static_cast<unsigned char>(c));

        std::string specTable;
        if (catUpper == "CPU") specTable = "cpu";
        else if (catUpper == "GPU") specTable = "gpu";
        else if (catUpper == "MAINBOARD") specTable = "mainboard";
        else if (catUpper == "RAM") specTable = "ram";
        else if (catUpper == "SSD") specTable = "ssd";
        else if (catUpper == "PSU") specTable = "psu";
        else if (catUpper == "COOLING") specTable = "cooling";
        else if (catUpper == "CASE") specTable = "case";

        // 4. Atomic Multi-table Deletion
        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu Transaction CSDL.", 0};
        }

        Condition cond("product_id", CompareOperator::EQUAL, Value(productId));

        // Delete from hardware spec table first (child)
        if (!specTable.empty() && db->hasTable(specTable)) {
            Table* sTab = db->getTable(specTable);
            sTab->remove(cond);
        }

        // Delete from product_quantity (child)
        if (qtyTable) {
            qtyTable->remove(cond);
        }

        // Delete from product (parent)
        if (!prodTable->remove(cond)) {
            db->rollback();
            return OperationResult{false, "Không thể xóa sản phẩm khỏi bảng 'product'.", 0};
        }

        db->commit();
        return OperationResult{true, "Xóa sản phẩm thành công!", 3};
    }

    OperationResult deleteRecord(Database* db, const std::string& tableName, int recordId)
    {
        if (!db) {
            return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        }
        Table* table = db->getTable(tableName);
        if (!table) {
            return OperationResult{false, "Bảng '" + tableName + "' không tồn tại trong CSDL.", 0};
        }

        // 1. Transaction & History Immutability Protection (Rule 5.3 & 6.3.1)
        if (tableName == "inbound_transaction" || tableName == "inbound_detail" ||
            tableName == "delivery" || tableName == "delivery_detail" ||
            tableName == "outbound_transaction" || tableName == "outbound_detail" ||
            tableName == "shipment" || tableName == "shipment_detail" ||
            tableName == "unit_inventory" || tableName == "status_history") {
            return OperationResult{false, "Dữ liệu giao dịch và lịch sử vòng đời đã xác nhận, không được phép xóa (Rule 5.3).", 0};
        }

        // 2. Special handler for product
        if (tableName == "product") {
            return deleteProduct(db, recordId);
        }

        // 3. Standard single-table deletion with Foreign Key Protection
        std::string pkCol = user_update::getPrimaryKeyColumn(table);
        if (pkCol.empty()) {
            return OperationResult{false, "Không tìm thấy khóa chính của bảng '" + tableName + "'.", 0};
        }

        // Locate existing record
        bool found = false;
        const auto& rows = table->getRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (table->isRowDeleted(i)) continue;
            const Record& r = rows[i];
            if (r.has(pkCol) && holds_alternative<int>(r.get(pkCol))) {
                if (get<int>(r.get(pkCol)) == recordId) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            return OperationResult{false, "Không tìm thấy bản ghi với ID " + std::to_string(recordId) + " trong bảng '" + tableName + "'.", 0};
        }

        Condition cond(pkCol, CompareOperator::EQUAL, Value(recordId));
        bool removed = db->remove(tableName, cond);
        if (!removed) {
            return OperationResult{false, "Không thể xóa bản ghi: Đang có dữ liệu ở các bảng khác tham chiếu đến bản ghi này (vi phạm toàn vẹn tham chiếu).", 0};
        }

        return OperationResult{true, "Xóa bản ghi thành công.", 1};
    }
}
