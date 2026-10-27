#include "UserUpdate.h"
#include "database/Constraint.h"
#include <iostream>

namespace user_update
{
    std::string getPrimaryKeyColumn(const Table* table)
    {
        return table ? table->getPrimaryKeyColumn() : "";
    }

    OperationResult updateRecord(
        Database* db,
        const std::string& tableName,
        int recordId,
        const Vector<std::string>& modifiedColumns,
        const Vector<Value>& modifyingValues
    ) {
        // 1. Schema & Existence Validation
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        Table* table = db->getTable(tableName);
        if (!table) return OperationResult{false, "Bảng '" + tableName + "' không tồn tại trong CSDL.", 0};
        if (modifiedColumns.size() != modifyingValues.size()) {
            return OperationResult{false, "Số lượng cột và giá trị cần cập nhật không khớp.", 0};
        }

        std::string pkCol = getPrimaryKeyColumn(table);
        if (pkCol.empty()) {
            return OperationResult{false, "Không tìm thấy khóa chính của bảng '" + tableName + "'.", 0};
        }

        int targetRowIdx = table->findRowIndexById(recordId);
        if (targetRowIdx < 0) {
            return OperationResult{false, "Không tìm thấy bản ghi với ID " + std::to_string(recordId) + " trong bảng '" + tableName + "'.", 0};
        }
        const Record& oldRecord = table->getRows()[targetRowIdx];

        // 2. Validate columns, types, and constraints
        const auto& rows = table->getRows();
        for (size_t i = 0; i < modifiedColumns.size(); ++i) {
            const std::string& colName = modifiedColumns[i];
            const Value& val = modifyingValues[i];

            const Column* col = table->getColumn(colName);
            if (!col) {
                return OperationResult{false, "Cột '" + colName + "' không tồn tại trong bảng '" + tableName + "'.", 0};
            }

            // Cannot modify primary key column directly
            if (colName == pkCol) {
                return OperationResult{false, "Không được phép chỉnh sửa cột khóa chính '" + pkCol + "'.", 0};
            }

            // NOT NULL check
            if (col->isNotNull() && isNull(val)) {
                return OperationResult{false, "Cột '" + colName + "' có thuộc tính NOT NULL, không thể nhận giá trị rỗng.", 0};
            }

            // UNIQUE check
            if (col->isUnique()) {
                for (size_t r = 0; r < rows.size(); ++r) {
                    if (table->isRowDeleted(r) || static_cast<int>(r) == targetRowIdx) continue;
                    if (rows[r].has(colName) && rows[r].get(colName) == val) {
                        return OperationResult{false, "Giá trị cập nhật của cột '" + colName + "' đã tồn tại (vi phạm ràng buộc UNIQUE).", 0};
                    }
                }
            }
        }

        // 3. Domain Business Rules & Immutable Fields Validation (Rule 5.3 & 6.3.1)
        if (tableName == "inbound_transaction" || tableName == "outbound_transaction") {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                if (modifiedColumns[i] != "status") {
                    return OperationResult{false, "Chỉ được phép cập nhật trạng thái (status) của đơn giao dịch đã xác nhận.", 0};
                }
            }
        }
        else if (tableName == "delivery") {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& colName = modifiedColumns[i];
                if (colName != "status" && colName != "delivery_at") {
                    return OperationResult{false, "Chỉ được phép chỉnh sửa 'status' và 'delivery_at' của đợt nhận hàng.", 0};
                }

                // Chronological Date Validation: delivery_at >= inbound_transaction.order_at
                if (colName == "delivery_at" && std::holds_alternative<std::string>(modifyingValues[i])) {
                    std::string newDeliveryAt = std::get<std::string>(modifyingValues[i]);
                    int inId = -1;
                    if (oldRecord.has("inbound_transaction_id") && std::holds_alternative<int>(oldRecord.get("inbound_transaction_id"))) {
                        inId = std::get<int>(oldRecord.get("inbound_transaction_id"));
                    } else if (oldRecord.has("inbound_id") && std::holds_alternative<int>(oldRecord.get("inbound_id"))) {
                        inId = std::get<int>(oldRecord.get("inbound_id"));
                    }

                    if (inId != -1) {
                        Table* inTab = db->getTable("inbound_transaction");
                        if (inTab) {
                            const Record* inRec = inTab->getRecordById(inId);
                            if (inRec && inRec->has("order_at") && std::holds_alternative<std::string>(inRec->get("order_at"))) {
                                std::string orderAt = std::get<std::string>(inRec->get("order_at"));
                                if (newDeliveryAt < orderAt) {
                                    return OperationResult{false, "Ngày giao nhận thực tế (delivery_at: " + newDeliveryAt + ") không được sớm hơn ngày đặt hàng (order_at: " + orderAt + ").", 0};
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (tableName == "shipment") {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& colName = modifiedColumns[i];
                if (colName != "status" && colName != "ship_at") {
                    return OperationResult{false, "Chỉ được phép chỉnh sửa 'status' và 'ship_at' của đợt xuất hàng.", 0};
                }

                // Chronological Date Validation: ship_at >= outbound_transaction.order_at
                if (colName == "ship_at" && std::holds_alternative<std::string>(modifyingValues[i])) {
                    std::string newShipAt = std::get<std::string>(modifyingValues[i]);
                    int outId = -1;
                    if (oldRecord.has("sell_id") && std::holds_alternative<int>(oldRecord.get("sell_id"))) {
                        outId = std::get<int>(oldRecord.get("sell_id"));
                    } else if (oldRecord.has("outbound_trainsaction_id") && std::holds_alternative<int>(oldRecord.get("outbound_trainsaction_id"))) {
                        outId = std::get<int>(oldRecord.get("outbound_trainsaction_id"));
                    } else if (oldRecord.has("outbound_id") && std::holds_alternative<int>(oldRecord.get("outbound_id"))) {
                        outId = std::get<int>(oldRecord.get("outbound_id"));
                    }

                    if (outId != -1) {
                        Table* outTab = db->getTable("outbound_transaction");
                        if (outTab) {
                            const Record* outRec = outTab->getRecordById(outId);
                            if (outRec && outRec->has("order_at") && std::holds_alternative<std::string>(outRec->get("order_at"))) {
                                std::string orderAt = std::get<std::string>(outRec->get("order_at"));
                                if (newShipAt < orderAt) {
                                    return OperationResult{false, "Ngày xuất hàng thực tế (ship_at: " + newShipAt + ") không được sớm hơn ngày đặt hàng (order_at: " + orderAt + ").", 0};
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (tableName == "inbound_detail" || tableName == "delivery_detail" ||
                 tableName == "outbound_detail" || tableName == "shipment_detail" ||
                 tableName == "status_history" || tableName == "product_quantity") {
            return OperationResult{false, "Bảng chi tiết giao dịch / lịch sử / số lượng tồn kho không được phép sửa đổi trực tiếp.", 0};
        }
        else if (tableName == "customer") {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& colName = modifiedColumns[i];
                if (colName == "customer_code" || colName == "customer_id" || colName == "type") {
                    return OperationResult{false, "Không được phép thay đổi mã khách hàng, ID hoặc loại khách hàng (Rule 4.1).", 0};
                }
            }
        }
        else if (tableName == "brand") {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                if (modifiedColumns[i] == "code") {
                    Table* prodTable = db->getTable("product");
                    if (prodTable && oldRecord.has("brand") && std::holds_alternative<std::string>(oldRecord.get("brand"))) {
                        std::string bName = std::get<std::string>(oldRecord.get("brand"));
                        for (size_t r = 0; r < prodTable->getRows().size(); ++r) {
                            if (prodTable->isRowDeleted(r)) continue;
                            const Record& pRec = prodTable->getRows()[r];
                            if (pRec.has("brand") && std::holds_alternative<std::string>(pRec.get("brand"))) {
                                if (std::get<std::string>(pRec.get("brand")) == bName) {
                                    return OperationResult{false, "Không được phép sửa mã thương hiệu (code) khi đã có sản phẩm trong danh mục.", 0};
                                }
                            }
                        }
                    }
                }
            }
        }

        // 4. Dispatch directly to Database
        Record newValuesRecord;
        for (size_t i = 0; i < modifiedColumns.size(); ++i) {
            newValuesRecord.set(modifiedColumns[i], modifyingValues[i]);
        }

        return db->update(tableName, recordId, newValuesRecord);
    }

    OperationResult updateTransactionStatus(
        Database* db,
        const std::string& tableName,
        int transactionId,
        const std::string& newStatus
    ) {
        Vector<std::string> cols;
        cols.push_back("status");
        Vector<Value> vals;
        vals.push_back(Value(newStatus));
        return updateRecord(db, tableName, transactionId, cols, vals);
    }

    OperationResult updateLogisticsActualDate(
        Database* db,
        const std::string& tableName,
        int logisticsId,
        const std::string& actualDateStr
    ) {
        Vector<std::string> cols;
        if (tableName == "delivery") {
            cols.push_back("delivery_at");
        } else if (tableName == "shipment") {
            cols.push_back("ship_at");
        } else {
            return OperationResult{false, "Bảng vận chuyển không hợp lệ: " + tableName, 0};
        }
        Vector<Value> vals;
        vals.push_back(Value(actualDateStr));
        return updateRecord(db, tableName, logisticsId, cols, vals);
    }

    OperationResult updateProductSellingStatus(
        Database* db,
        int productId,
        bool isSold
    ) {
        Vector<std::string> cols;
        cols.push_back("is_sold");
        Vector<Value> vals;
        vals.push_back(Value(isSold));
        return updateRecord(db, "product", productId, cols, vals);
    }

    OperationResult updateCustomerContact(
        Database* db,
        int customerId,
        const std::string& nameOrCompany,
        const std::string& phone,
        const std::string& email,
        const std::string& taxCode
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        Table* custTable = db->getTable("customer");
        if (!custTable) return OperationResult{false, "Bảng customer không tồn tại.", 0};

        const Record* cRec = custTable->getRecordById(customerId);
        if (!cRec) {
            return OperationResult{false, "Không tìm thấy khách hàng với ID " + std::to_string(customerId) + ".", 0};
        }

        std::string custType = cRec->has("type") && std::holds_alternative<std::string>(cRec->get("type"))
                             ? std::get<std::string>(cRec->get("type")) : "";

        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu Transaction CSDL.", 0};
        }

        // Update name in customer table if provided
        if (!nameOrCompany.empty()) {
            Record cUp;
            cUp.set("name", nameOrCompany);
            OperationResult rName = db->update("customer", customerId, cUp);
            if (!rName) {
                db->rollback();
                return rName;
            }
        }

        // Update contact info in individual_info or business_info
        if (custType == "individual") {
            Record iUp;
            if (!phone.empty()) iUp.set("phone_number", phone);
            if (!email.empty()) iUp.set("email", email);
            if (!iUp.getValues().empty()) {
                OperationResult rInfo = db->update("individual_info", customerId, iUp);
                if (!rInfo) {
                    db->rollback();
                    return rInfo;
                }
            }
        } else if (custType == "business") {
            Record bUp;
            if (!phone.empty()) bUp.set("phone_number", phone);
            if (!email.empty()) bUp.set("email", email);
            if (!taxCode.empty()) bUp.set("tax_code", taxCode);
            if (!bUp.getValues().empty()) {
                OperationResult rInfo = db->update("business_info", customerId, bUp);
                if (!rInfo) {
                    db->rollback();
                    return rInfo;
                }
            }
        }

        db->commit();
        return OperationResult{true, "Cập nhật thông tin khách hàng thành công.", 1};
    }

    OperationResult updateBrand(
        Database* db,
        int brandId,
        const std::string& newBrandName
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        if (newBrandName.empty()) {
            return OperationResult{false, "Tên thương hiệu không được để trống.", 0};
        }
        Table* brandTable = db->getTable("brand");
        if (!brandTable) return OperationResult{false, "Bảng brand không tồn tại.", 0};

        const Record* bRec = brandTable->getRecordById(brandId);
        if (!bRec) {
            return OperationResult{false, "Không tìm thấy thương hiệu với ID " + std::to_string(brandId) + ".", 0};
        }

        Record bUp;
        bUp.set("brand", newBrandName);
        return db->update("brand", brandId, bUp);
    }
}
