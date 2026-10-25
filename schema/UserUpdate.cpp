#include "UserUpdate.h"
#include "database/Constraint.h"
#include <iostream>

namespace user_update
{
    std::string getPrimaryKeyColumn(const Table* table)
    {
        if (!table) return "";

        // Check columns directly
        for (const auto& col : table->getColumns()) {
            if (col.isPrimaryKey()) {
                return col.getName();
            }
        }

        // Check constraints
        for (const auto* c : table->getConstraints()) {
            if (c && c->getType() == ConstraintType::PRIMARY_KEY) {
                const auto* pk = dynamic_cast<const PrimaryKeyConstraint*>(c);
                if (pk) return pk->getColumnName();
            }
        }

        // Fallbacks
        const std::string& name = table->getName();
        if (table->getColumn(name + "_id")) return name + "_id";
        if (table->getColumn("id")) return "id";
        if (table->getColumn("status_id")) return "status_id";

        return "";
    }

    OperationResult updateRecord(
        Database* db,
        const std::string& tableName,
        int recordId,
        const Vector<std::string>& modifiedColumns,
        const Vector<Value>& modifyingValues
    ) {
        // 1. Schema & Existence Validation
        if (!db) {
            return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        }
        Table* table = db->getTable(tableName);
        if (!table) {
            return OperationResult{false, "Bảng '" + tableName + "' không tồn tại trong CSDL.", 0};
        }
        if (modifiedColumns.size() != modifyingValues.size()) {
            return OperationResult{false, "Số lượng cột và giá trị cần cập nhật không khớp.", 0};
        }

        std::string pkCol = getPrimaryKeyColumn(table);
        if (pkCol.empty()) {
            return OperationResult{false, "Không tìm thấy khóa chính của bảng '" + tableName + "'.", 0};
        }

        // Locate existing record
        int targetRowIdx = -1;
        Record oldRecord;
        const auto& rows = table->getRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (table->isRowDeleted(i)) continue;
            const Record& r = rows[i];
            if (r.has(pkCol) && holds_alternative<int>(r.get(pkCol))) {
                if (get<int>(r.get(pkCol)) == recordId) {
                    targetRowIdx = static_cast<int>(i);
                    oldRecord = r;
                    break;
                }
            }
        }

        if (targetRowIdx == -1) {
            return OperationResult{false, "Không tìm thấy bản ghi với ID " + std::to_string(recordId) + " trong bảng '" + tableName + "'.", 0};
        }

        // 2. Validate columns, types, and constraints
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
                const std::string& colName = modifiedColumns[i];
                if (colName != "status") {
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
                if (colName == "delivery_at" && holds_alternative<std::string>(modifyingValues[i])) {
                    std::string newDeliveryAt = get<std::string>(modifyingValues[i]);
                    int inId = -1;
                    if (oldRecord.has("inbound_transaction_id") && holds_alternative<int>(oldRecord.get("inbound_transaction_id"))) {
                        inId = get<int>(oldRecord.get("inbound_transaction_id"));
                    } else if (oldRecord.has("inbound_id") && holds_alternative<int>(oldRecord.get("inbound_id"))) {
                        inId = get<int>(oldRecord.get("inbound_id"));
                    }

                    if (inId != -1) {
                        Table* inTab = db->getTable("inbound_transaction");
                        if (inTab) {
                            for (size_t r = 0; r < inTab->getRows().size(); ++r) {
                                if (inTab->isRowDeleted(r)) continue;
                                const Record& inRec = inTab->getRows()[r];
                                int curId = -1;
                                if (inRec.has("inbound_transaction_id") && holds_alternative<int>(inRec.get("inbound_transaction_id"))) {
                                    curId = get<int>(inRec.get("inbound_transaction_id"));
                                } else if (inRec.has("inbound_id") && holds_alternative<int>(inRec.get("inbound_id"))) {
                                    curId = get<int>(inRec.get("inbound_id"));
                                }

                                if (curId == inId) {
                                    if (inRec.has("order_at") && holds_alternative<std::string>(inRec.get("order_at"))) {
                                        std::string orderAt = get<std::string>(inRec.get("order_at"));
                                        if (newDeliveryAt < orderAt) {
                                            return OperationResult{false, "Ngày giao nhận thực tế (delivery_at: " + newDeliveryAt + ") không được sớm hơn ngày đặt hàng (order_at: " + orderAt + ").", 0};
                                        }
                                    }
                                    break;
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
                if (colName == "ship_at" && holds_alternative<std::string>(modifyingValues[i])) {
                    std::string newShipAt = get<std::string>(modifyingValues[i]);
                    int outId = -1;
                    if (oldRecord.has("sell_id") && holds_alternative<int>(oldRecord.get("sell_id"))) {
                        outId = get<int>(oldRecord.get("sell_id"));
                    } else if (oldRecord.has("outbound_trainsaction_id") && holds_alternative<int>(oldRecord.get("outbound_trainsaction_id"))) {
                        outId = get<int>(oldRecord.get("outbound_trainsaction_id"));
                    } else if (oldRecord.has("outbound_id") && holds_alternative<int>(oldRecord.get("outbound_id"))) {
                        outId = get<int>(oldRecord.get("outbound_id"));
                    }

                    if (outId != -1) {
                        Table* outTab = db->getTable("outbound_transaction");
                        if (outTab) {
                            for (size_t r = 0; r < outTab->getRows().size(); ++r) {
                                if (outTab->isRowDeleted(r)) continue;
                                const Record& outRec = outTab->getRows()[r];
                                int curId = -1;
                                if (outRec.has("outbound_trainsaction_id") && holds_alternative<int>(outRec.get("outbound_trainsaction_id"))) {
                                    curId = get<int>(outRec.get("outbound_trainsaction_id"));
                                } else if (outRec.has("outbound_id") && holds_alternative<int>(outRec.get("outbound_id"))) {
                                    curId = get<int>(outRec.get("outbound_id"));
                                }

                                if (curId == outId) {
                                    if (outRec.has("order_at") && holds_alternative<std::string>(outRec.get("order_at"))) {
                                        std::string orderAt = get<std::string>(outRec.get("order_at"));
                                        if (newShipAt < orderAt) {
                                            return OperationResult{false, "Ngày xuất hàng thực tế (ship_at: " + newShipAt + ") không được sớm hơn ngày đặt hàng (order_at: " + orderAt + ").", 0};
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (tableName == "inbound_detail" || tableName == "delivery_detail" ||
                 tableName == "outbound_detail" || tableName == "shipment_detail" ||
                 tableName == "status_history") {
            return OperationResult{false, "Bảng chi tiết giao dịch / lịch sử không được phép sửa đổi trực tiếp.", 0};
        }

        // 4. Perform Update
        Record newValuesRecord;
        for (size_t i = 0; i < modifiedColumns.size(); ++i) {
            newValuesRecord.set(modifiedColumns[i], modifyingValues[i]);
        }

        Condition cond(pkCol, CompareOperator::EQUAL, Value(recordId));
        bool updated = table->update(cond, newValuesRecord);
        if (!updated) {
            return OperationResult{false, "Cập nhật dữ liệu thất bại do vi phạm ràng buộc bảng.", 0};
        }

        return OperationResult{true, "Cập nhật bản ghi thành công.", 1};
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
}
