#include "UserUpdate.h"
#include "query/Query.h"
#include "database/Constraint.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

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
                Vector<Record> matches = Query::filterRecords(*table, colName, val);
                for (size_t m = 0; m < matches.size(); ++m) {
                    const Record& mRec = matches[m];
                    if (mRec.has(pkCol) && std::holds_alternative<int>(mRec.get(pkCol)) &&
                        std::get<int>(mRec.get(pkCol)) == recordId) {
                        continue;
                    }
                    return OperationResult{false, "Giá trị cập nhật của cột '" + colName + "' đã tồn tại (vi phạm ràng buộc UNIQUE).", 0};
                }
            }
        }

        // 3. Domain Business Rules & Immutable Fields Validation (Rule 5.3 & 6.3.1)
        if (tableName == "inbound_transaction" || tableName == "outbound_transaction") {
            std::string oldTxnSt = oldRecord.has("status") && std::holds_alternative<std::string>(oldRecord.get("status"))
                                 ? std::get<std::string>(oldRecord.get("status")) : "";
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& colName = modifiedColumns[i];
                if (colName != "status" && colName != "note") {
                    return OperationResult{false, "Chỉ được phép cập nhật trạng thái (status) hoặc ghi chú (note) của đơn giao dịch.", 0};
                }
                if (colName == "status") {
                    std::string newTxnSt = std::holds_alternative<std::string>(modifyingValues[i])
                                         ? std::get<std::string>(modifyingValues[i]) : "";
                    if ((oldTxnSt == "COMPLETED" || oldTxnSt == "completed") && newTxnSt != "COMPLETED") {
                        return OperationResult{false, "Đơn giao dịch đã ở trạng thái HOÀN THÀNH (COMPLETED), không thể chuyển ngược trạng thái (Rule 4.5).", 0};
                    }
                }
            }
        }
        else if (tableName == "delivery") {
            std::string oldDelSt = oldRecord.has("status") && std::holds_alternative<std::string>(oldRecord.get("status"))
                                 ? std::get<std::string>(oldRecord.get("status")) : "";
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& colName = modifiedColumns[i];
                if (colName != "status" && colName != "delivery_at") {
                    return OperationResult{false, "Chỉ được phép chỉnh sửa 'status' và 'delivery_at' của đợt nhận hàng.", 0};
                }
                if (colName == "status") {
                    std::string newDelSt = std::holds_alternative<std::string>(modifyingValues[i])
                                         ? std::get<std::string>(modifyingValues[i]) : "";
                    if ((oldDelSt == "COMPLETED" || oldDelSt == "completed") && newDelSt != "COMPLETED") {
                        return OperationResult{false, "Đợt giao nhận đã ở trạng thái HOÀN THÀNH (COMPLETED), không thể chuyển ngược trạng thái (Rule 4.5).", 0};
                    }
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
            std::string oldShipSt = oldRecord.has("status") && std::holds_alternative<std::string>(oldRecord.get("status"))
                                  ? std::get<std::string>(oldRecord.get("status")) : "";
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& colName = modifiedColumns[i];
                if (colName != "status" && colName != "ship_at") {
                    return OperationResult{false, "Chỉ được phép chỉnh sửa 'status' và 'ship_at' của đợt xuất hàng.", 0};
                }
                if (colName == "status") {
                    std::string newShipSt = std::holds_alternative<std::string>(modifyingValues[i])
                                          ? std::get<std::string>(modifyingValues[i]) : "";
                    if ((oldShipSt == "COMPLETED" || oldShipSt == "completed") && newShipSt != "COMPLETED") {
                        return OperationResult{false, "Đợt xuất hàng đã ở trạng thái HOÀN THÀNH (COMPLETED), không thể chuyển ngược trạng thái (Rule 4.5).", 0};
                    }
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
        else if (tableName == "inventory_check") {
            std::string oldChkSt = oldRecord.has("status") && std::holds_alternative<std::string>(oldRecord.get("status"))
                                 ? std::get<std::string>(oldRecord.get("status")) : "";
            if (oldChkSt == "COMPLETED" || oldChkSt == "completed") {
                return OperationResult{false, "Đợt kiểm kho đã ở trạng thái HOÀN THÀNH (COMPLETED), không thể chỉnh sửa.", 0};
            }
        }
        else if (tableName == "adjustment_movement") {
            std::string oldAdjSt = oldRecord.has("status") && std::holds_alternative<std::string>(oldRecord.get("status"))
                                 ? std::get<std::string>(oldRecord.get("status")) : "";
            if (oldAdjSt == "COMPLETED" || oldAdjSt == "completed") {
                return OperationResult{false, "Phiếu điều chuyển đã ở trạng thái HOÀN THÀNH (COMPLETED), không thể chỉnh sửa.", 0};
            }
        }
        else if (tableName == "inventory_check_quantity") {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& col = modifiedColumns[i];
                if (col == "check_id" || col == "product_id" || col == "system_available_quan" ||
                    col == "system_warranty_quan" || col == "system_scrapped_quan") {
                    return OperationResult{false, "Không thể sửa đổi phạm vi hoặc số lượng hệ thống của đợt kiểm kho (cột '" + col + "' bị khóa).", 0};
                }
            }
        }
        else if (tableName == "inventory_check_unit") {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& col = modifiedColumns[i];
                if (col != "scanned_status") {
                    return OperationResult{false, "Chỉ được phép cập nhật trạng thái quét 'scanned_status' cho linh kiện kiểm kê.", 0};
                }
            }
        }
        else if (tableName == "adjust_movement_detail") {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& col = modifiedColumns[i];
                if (col != "actual_quantity") {
                    return OperationResult{false, "Không thể sửa đổi thông tin chi tiết điều chuyển đã khai báo (cột '" + col + "' bị khóa).", 0};
                }
            }
        }
        else if (tableName == "inbound_detail" || tableName == "delivery_detail" ||
                 tableName == "outbound_detail" || tableName == "shipment_detail" ||
                 tableName == "status_history") {
            return OperationResult{false, "Bảng chi tiết giao dịch / lịch sử không được phép sửa đổi trực tiếp.", 0};
        }
        else if (tableName == "customer") {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& colName = modifiedColumns[i];
                if (colName == "customer_code" || colName == "customer_id" || colName == "type") {
                    return OperationResult{false, "Không được phép thay đổi mã khách hàng, ID hoặc loại khách hàng (Rule 4.1).", 0};
                }
            }
        }
        else if (tableName == "product") {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                if (modifiedColumns[i] == "is_sold") {
                    bool newSold = std::holds_alternative<bool>(modifyingValues[i]) ? std::get<bool>(modifyingValues[i]) : true;
                    bool oldSold = (oldRecord.has("is_sold") && std::holds_alternative<bool>(oldRecord.get("is_sold"))) ? std::get<bool>(oldRecord.get("is_sold")) : true;
                    if (oldSold != newSold && !newSold) {
                        int avail = (oldRecord.has("available_quantity") && std::holds_alternative<int>(oldRecord.get("available_quantity"))) ? std::get<int>(oldRecord.get("available_quantity")) : 0;
                        int warr = (oldRecord.has("warranty_quantity") && std::holds_alternative<int>(oldRecord.get("warranty_quantity"))) ? std::get<int>(oldRecord.get("warranty_quantity")) : 0;
                        int scrap = (oldRecord.has("scrapped_quantity") && std::holds_alternative<int>(oldRecord.get("scrapped_quantity"))) ? std::get<int>(oldRecord.get("scrapped_quantity")) : 0;
                        int total = avail + warr + scrap;
                        if (total > 0) {
                            return OperationResult{false, "Không thể chuyển trạng thái ngừng bán (is_sold = FALSE): Sản phẩm vẫn còn tồn kho (" + std::to_string(total) + " cái).", 0};
                        }
                    }
                }
            }
        }
        else if (tableName == "brand") {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                if (modifiedColumns[i] == "code") {
                    Table* prodTable = db->getTable("product");
                    if (prodTable && oldRecord.has("brand") && std::holds_alternative<std::string>(oldRecord.get("brand"))) {
                        std::string bName = std::get<std::string>(oldRecord.get("brand"));
                        Vector<Record> pRecs = Query::filterRecords(*prodTable, "brand", Value(bName));
                        if (!pRecs.empty()) {
                            return OperationResult{false, "Không được phép sửa mã thương hiệu (code) khi đã có sản phẩm trong danh mục.", 0};
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

        OperationResult res = db->update(tableName, recordId, newValuesRecord);
        if (res.success && tableName == "product") {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                if (modifiedColumns[i] == "is_sold") {
                    bool newSold = std::holds_alternative<bool>(modifyingValues[i]) ? std::get<bool>(modifyingValues[i]) : true;
                    bool oldSold = (oldRecord.has("is_sold") && std::holds_alternative<bool>(oldRecord.get("is_sold"))) ? std::get<bool>(oldRecord.get("is_sold")) : true;
                    if (oldSold != newSold) {
                        Table* histTable = db->getTable("is_sold_changing");
                        if (histTable) {
                            time_t now = time(nullptr);
                            tm* t = localtime(&now);
                            char buf[32];
                            strftime(buf, sizeof(buf), "%Y-%m-%d", t);
                            int hId = 1;
                            const auto& hRows = histTable->getRows();
                            for (size_t h = 0; h < hRows.size(); ++h) {
                                if (hRows[h].has("history_id") && std::holds_alternative<int>(hRows[h].get("history_id"))) {
                                    int hidVal = std::get<int>(hRows[h].get("history_id"));
                                    if (hidVal >= hId) hId = hidVal + 1;
                                }
                            }
                            Record hRec;
                            hRec.set("history_id", hId);
                            hRec.set("product_id", recordId);
                            hRec.set("old_is_sold", oldSold);
                            hRec.set("new_is_sold", newSold);
                            hRec.set("changed_at", std::string(buf));
                            hRec.set("reason", std::string("Cập nhật thông tin sản phẩm"));
                            db->insertRecord("is_sold_changing", hRec);
                        }
                    }
                    break;
                }
            }
        }
        return res;
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

    OperationResult updateTransactionNote(
        Database* db,
        const std::string& tableName,
        int transactionId,
        const std::string& newNote
    ) {
        if (!db || transactionId <= 0) return OperationResult{false, "Tham số không hợp lệ.", 0};
        if (tableName != "inbound_transaction" && tableName != "outbound_transaction") {
            return OperationResult{false, "Bảng giao dịch không hợp lệ.", 0};
        }
        Vector<std::string> cols;
        cols.push_back("note");
        Vector<Value> vals;
        vals.push_back(Value(newNote));
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
        bool isSold,
        const std::string& dateStr
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        Table* prodTable = db->getTable("product");
        Table* histTable = db->getTable("is_sold_changing");
        if (!prodTable) return OperationResult{false, "Bảng product không tồn tại.", 0};

        const Record* pRec = prodTable->getRecordById(productId);
        if (!pRec) {
            return OperationResult{false, "Không tìm thấy sản phẩm với ID " + std::to_string(productId) + ".", 0};
        }

        bool oldIsSold = (pRec->has("is_sold") && std::holds_alternative<bool>(pRec->get("is_sold")))
                       ? std::get<bool>(pRec->get("is_sold")) : true;

        if (oldIsSold == isSold) {
            return OperationResult{true, "Trạng thái kinh doanh không thay đổi.", 0};
        }

        // Rule: is_sold chỉ có thể đổi sang FALSE nếu tổng số lượng sản phẩm là 0
        if (!isSold) {
            int avail = (pRec->has("available_quantity") && std::holds_alternative<int>(pRec->get("available_quantity")))
                      ? std::get<int>(pRec->get("available_quantity")) : 0;
            int warr = (pRec->has("warranty_quantity") && std::holds_alternative<int>(pRec->get("warranty_quantity")))
                     ? std::get<int>(pRec->get("warranty_quantity")) : 0;
            int scrap = (pRec->has("scrapped_quantity") && std::holds_alternative<int>(pRec->get("scrapped_quantity")))
                      ? std::get<int>(pRec->get("scrapped_quantity")) : 0;
            int total = avail + warr + scrap;
            if (total > 0) {
                return OperationResult{false, "Không thể chuyển trạng thái ngừng bán (is_sold = FALSE): Sản phẩm vẫn còn tồn kho (" + std::to_string(total) + " cái).", 0};
            }
        }

        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu Transaction CSDL.", 0};
        }

        Record pUp;
        pUp.set("is_sold", isSold);
        OperationResult rUp = db->update("product", productId, pUp);
        if (!rUp) { db->rollback(); return rUp; }

        if (histTable) {
            std::string actualDate = dateStr;
            if (actualDate.empty()) {
                time_t now = time(nullptr);
                tm* t = localtime(&now);
                char buf[32];
                strftime(buf, sizeof(buf), "%Y-%m-%d", t);
                actualDate = buf;
            }

            int hId = 1;
            for (const auto& r : histTable->getRows()) {
                if (r.has("history_id") && std::holds_alternative<int>(r.get("history_id"))) {
                    int id = std::get<int>(r.get("history_id"));
                    if (id >= hId) hId = id + 1;
                }
            }

            Record hRec;
            hRec.set("history_id", hId);
            hRec.set("product_id", productId);
            hRec.set("date", actualDate);
            hRec.set("old_status", oldIsSold);
            hRec.set("new_status", isSold);

            OperationResult rH = db->insertRecord("is_sold_changing", hRec);
            if (!rH) { db->rollback(); return rH; }
        }

        db->commit();
        return OperationResult{true, "Cập nhật trạng thái bán và ghi nhật ký lịch sử thành công!", 2};
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

        if (customerId == 0 || (cRec->has("customer_code") && std::holds_alternative<std::string>(cRec->get("customer_code")) && std::get<std::string>(cRec->get("customer_code")) == "BS-0000000")) {
            return OperationResult{false, "Không thể sửa thông tin của khách hàng hệ thống mặc định (BS-0000000).", 0};
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

    // ========================================================
    // SHIPMENT UNIT DISPATCHING (Rule 4.4.1)
    // ========================================================

    OperationResult dispatchUnitsForShipment(
        Database* db,
        int shipmentId,
        int productId,
        const Vector<std::string>& serials
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        if (serials.empty()) return OperationResult{false, "Danh sách serial xuất kho không được để trống.", 0};

        Table* shipTable = db->getTable("shipment");
        Table* shipDetTable = db->getTable("shipment_detail");
        Table* outTxnTable = db->getTable("outbound_transaction");
        Table* outDetTable = db->getTable("outbound_detail");
        Table* unitTable = db->getTable("unit_inventory");
        Table* histTable = db->getTable("status_history");
                Table* prodTable = db->getTable("product");

        if (!shipTable || !shipDetTable || !outTxnTable || !outDetTable || !unitTable || !histTable || !prodTable) {
            return OperationResult{false, "Các bảng dữ liệu xuất kho không đầy đủ.", 0};
        }

        const Record* shipRec = shipTable->getRecordById(shipmentId);
        if (!shipRec) {
            return OperationResult{false, "Không tìm thấy đợt xuất hàng với ID " + std::to_string(shipmentId) + ".", 0};
        }

        std::string shipStatus = shipRec->has("status") && std::holds_alternative<std::string>(shipRec->get("status"))
                               ? std::get<std::string>(shipRec->get("status")) : "";
        if (shipStatus == "COMPLETED" || shipStatus == "completed") {
            return OperationResult{false, "Đợt xuất hàng này đã ở trạng thái HOÀN THÀNH (COMPLETED), không thể xuất thêm linh kiện.", 0};
        }

        int outTxnId = -1;
        if (shipRec->has("sell_id") && std::holds_alternative<int>(shipRec->get("sell_id"))) {
            outTxnId = std::get<int>(shipRec->get("sell_id"));
        }
        const Record* outTxnRec = outTxnTable->getRecordById(outTxnId);
        if (!outTxnRec) {
            return OperationResult{false, "Không tìm thấy đơn xuất kho gốc của đợt xuất này.", 0};
        }

        std::string outType = outTxnRec->has("type") && std::holds_alternative<std::string>(outTxnRec->get("type"))
                            ? std::get<std::string>(outTxnRec->get("type")) : "SELLING";

        // Find shipment_detail
        int shipDetId = -1;
        int shipRecQty = 0;
        int shipActQty = 0;
        Vector<Record> sdRecords = Query::filterRecords(*shipDetTable, "shipment_id", Value(shipmentId));
        for (const auto& row : sdRecords) {
            if (row.has("product_id") && std::holds_alternative<int>(row.get("product_id")) &&
                std::get<int>(row.get("product_id")) == productId) {
                shipDetId = std::get<int>(row.get("shipment_detail_id"));
                shipRecQty = std::get<int>(row.get("received_quantity"));
                shipActQty = std::get<int>(row.get("actual_quantity"));
                break;
            }
        }
        if (shipDetId == -1) {
            return OperationResult{false, "Sản phẩm ID " + std::to_string(productId) + " không thuộc đợt xuất hàng ID " + std::to_string(shipmentId) + ".", 0};
        }

        int dispatchCount = static_cast<int>(serials.size());
        if (shipActQty + dispatchCount > shipRecQty) {
            return OperationResult{false, "Số lượng xuất (" + std::to_string(shipActQty + dispatchCount) +
                ") vượt quá số lượng ấn định (" + std::to_string(shipRecQty) + ") của đợt xuất hàng này.", 0};
        }

        // Find outbound_detail
        int outDetId = -1;
        int outOrdQty = 0;
        int outActQty = 0;
        Vector<Record> odRecords = Query::filterRecords(*outDetTable, "outbound_transaction_id", Value(outTxnId));
        for (const auto& row : odRecords) {
            if (row.has("product_id") && std::holds_alternative<int>(row.get("product_id")) &&
                std::get<int>(row.get("product_id")) == productId) {
                outDetId = std::get<int>(row.get("outbound_detail_id"));
                outOrdQty = std::get<int>(row.get("ordered_quantity"));
                outActQty = std::get<int>(row.get("actual_quantity"));
                break;
            }
        }
        if (outDetId == -1) {
            return OperationResult{false, "Sản phẩm ID " + std::to_string(productId) + " không thuộc đơn xuất kho gốc.", 0};
        }

        if (outActQty + dispatchCount > outOrdQty) {
            return OperationResult{false, "Số lượng xuất vượt quá tổng số lượng đặt hàng (" + std::to_string(outOrdQty) + ") của đơn xuất.", 0};
        }

        // Precondition rules (Rule 4.4.1):
        // SEL: AVAILABLE -> OUT
        // WRT: SCRAPPED/WARRANTY -> OUT
        // WRC: AVAILABLE/WARRANTY -> OUT
        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu Transaction CSDL.", 0};
        }

        int totalAffected = 0;
        int decrementedAvail = 0;
        int decrementedWarr = 0;
        int decrementedScrap = 0;

        for (size_t i = 0; i < serials.size(); ++i) {
            const std::string& sn = serials[i];
            int unitId = -1;
            std::string curStatus = "";

            Vector<Record> uRecs = Query::filterRecords(*unitTable, "serial", Value(sn));
            if (!uRecs.empty()) {
                const auto& uRow = uRecs[0];
                if (uRow.has("product_id") && std::holds_alternative<int>(uRow.get("product_id")) &&
                    std::get<int>(uRow.get("product_id")) == productId) {
                    unitId = std::get<int>(uRow.get("unit_id"));
                    curStatus = std::get<std::string>(uRow.get("status"));
                }
            }

            if (unitId == -1) {
                db->rollback();
                return OperationResult{false, "Không tìm thấy linh kiện có serial '" + sn + "' của sản phẩm ID " + std::to_string(productId) + ".", 0};
            }

            // Verify precondition
            if (outType == "SELLING" || outType == "SEL") {
                if (curStatus != "AVAILABLE") {
                    db->rollback();
                    return OperationResult{false, "Linh kiện serial '" + sn + "' có trạng thái '" + curStatus +
                        "', không thỏa mãn điều kiện AVAILABLE của đơn bán hàng (SEL) (Rule 4.4.1).", 0};
                }
                decrementedAvail++;
            } else if (outType == "WARRANTY_RETURN" || outType == "WRT") {
                if (curStatus != "SCRAPPED" && curStatus != "WARRANTY") {
                    db->rollback();
                    return OperationResult{false, "Linh kiện serial '" + sn + "' có trạng thái '" + curStatus +
                        "', không thỏa mãn điều kiện SCRAPPED/WARRANTY của đơn xuất trả bảo hành (WRT) (Rule 4.4.1).", 0};
                }
                if (curStatus == "SCRAPPED") decrementedScrap++;
                else decrementedWarr++;
            } else if (outType == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || outType == "WRC") {
                if (curStatus != "AVAILABLE" && curStatus != "WARRANTY") {
                    db->rollback();
                    return OperationResult{false, "Linh kiện serial '" + sn + "' có trạng thái '" + curStatus +
                        "', không thỏa mãn điều kiện AVAILABLE/WARRANTY của đơn đổi bảo hành (WRC) (Rule 4.4.1).", 0};
                }
                if (curStatus == "AVAILABLE") decrementedAvail++;
                else decrementedWarr++;
            } else {
                if (curStatus == "OUT") {
                    db->rollback();
                    return OperationResult{false, "Linh kiện serial '" + sn + "' đã ở trạng thái OUT.", 0};
                }
                if (curStatus == "AVAILABLE") decrementedAvail++;
                else if (curStatus == "WARRANTY") decrementedWarr++;
                else if (curStatus == "SCRAPPED") decrementedScrap++;
            }

            // Update unit status to OUT (Rule 4.4.1: Target status of all outbound units must be OUT)
            Record uUp;
            uUp.set("status", std::string("OUT"));
            OperationResult rU = db->update("unit_inventory", unitId, uUp);
            if (!rU) { db->rollback(); return rU; }
            totalAffected++;

            // Insert into status_history
            int histId = 1;
            std::string pkHist = histTable->getPrimaryKeyColumn();
            for (const auto& hRow : histTable->getRows()) {
                if (hRow.has(pkHist) && std::holds_alternative<int>(hRow.get(pkHist))) {
                    int hid = std::get<int>(hRow.get(pkHist));
                    if (hid >= histId) histId = hid + 1;
                }
            }

            Record hRec;
            hRec.set("history_id", histId);
            hRec.set("unit_id", unitId);
            hRec.set("old_status", curStatus);
            hRec.set("curr_status", std::string("OUT"));
            hRec.set("transaction", shipmentId);
            hRec.set("type", std::string("OUTBOUND"));

            OperationResult rH = db->insertRecord("status_history", hRec);
            if (!rH) { db->rollback(); return rH; }
            totalAffected++;
        }

        // Decrement product_quantity
        const Record* pRec = prodTable->getRecordById(productId);
        int curAvail = 0, curWarr = 0, curScrap = 0;
        if (pRec) {
            if (pRec->has("available_quantity") && std::holds_alternative<int>(pRec->get("available_quantity")))
                curAvail = std::get<int>(pRec->get("available_quantity"));
            if (pRec->has("warranty_quantity") && std::holds_alternative<int>(pRec->get("warranty_quantity")))
                curWarr = std::get<int>(pRec->get("warranty_quantity"));
            if (pRec->has("scrapped_quantity") && std::holds_alternative<int>(pRec->get("scrapped_quantity")))
                curScrap = std::get<int>(pRec->get("scrapped_quantity"));
        }

        Record pUp;
        pUp.set("available_quantity", std::max(0, curAvail - decrementedAvail));
        pUp.set("warranty_quantity", std::max(0, curWarr - decrementedWarr));
        pUp.set("scrapped_quantity", std::max(0, curScrap - decrementedScrap));
        OperationResult rPq = db->update("product", productId, pUp);
        if (!rPq) { db->rollback(); return rPq; }
        totalAffected++;

        // Update actual_quantity in shipment_detail and outbound_detail
        Record sdUp;
        sdUp.set("actual_quantity", shipActQty + dispatchCount);
        OperationResult rSUp = db->update("shipment_detail", shipDetId, sdUp);
        if (!rSUp) { db->rollback(); return rSUp; }
        totalAffected++;

        Record odUp;
        odUp.set("actual_quantity", outActQty + dispatchCount);
        OperationResult rOUp = db->update("outbound_detail", outDetId, odUp);
        if (!rOUp) { db->rollback(); return rOUp; }
        totalAffected++;

        // Update shipment status to PICKING if not already COMPLETED (Rule 4.5: do not auto-complete)
        if (shipStatus != "COMPLETED" && shipStatus != "completed") {
            Record shipStatusUp;
            shipStatusUp.set("status", std::string("PICKING"));
            db->update("shipment", shipmentId, shipStatusUp);
        }

        // Update outbound_transaction status to PROCESSING if not already COMPLETED/CONFIRMED
        std::string curOutTxnSt = outTxnRec->has("status") && std::holds_alternative<std::string>(outTxnRec->get("status"))
            ? std::get<std::string>(outTxnRec->get("status")) : "";
        if (curOutTxnSt != "COMPLETED" && curOutTxnSt != "completed" && curOutTxnSt != "CONFIRMED") {
            Record outTxnStatusUp;
            outTxnStatusUp.set("status", std::string("PROCESSING"));
            db->update("outbound_transaction", outTxnId, outTxnStatusUp);
        }

        db->commit();
        return OperationResult{true, "Đã xuất " + std::to_string(dispatchCount) + " unit cho đợt xuất " + std::to_string(shipmentId) + " thành công!", totalAffected};
    }

    OperationResult dispatchUnitForShipment(
        Database* db,
        int shipmentId,
        int productId,
        const std::string& serial
    ) {
        Vector<std::string> serials;
        serials.push_back(serial);
        return dispatchUnitsForShipment(db, shipmentId, productId, serials);
    }

    OperationResult changeUnitStatusInLogistics(
        Database* db,
        const std::string& logisticsCode,
        bool isInbound,
        const std::string& serial,
        const std::string& newStatus
    ) {
        if (!db) return OperationResult{false, "CSDL không hợp lệ.", 0};
        if (logisticsCode.empty() || serial.empty() || newStatus.empty()) {
            return OperationResult{false, "Thông tin mã đợt, serial hoặc trạng thái mới không được để trống.", 0};
        }

        std::string tbl = isInbound ? "delivery" : "shipment";
        std::string codeCol = isInbound ? "delivery_code" : "shipment_code";
        Table* logTable = db->getTable(tbl);
        Table* unitTable = db->getTable("unit_inventory");
        Table* histTable = db->getTable("status_history");
        Table* prodTable = db->getTable("product");
        if (!logTable || !unitTable || !histTable || !prodTable) {
            return OperationResult{false, "Các bảng dữ liệu giao vận hoặc linh kiện không đầy đủ.", 0};
        }

        Vector<Record> logMatches = Query::filterRecords(*logTable, codeCol, Value(logisticsCode));
        if (logMatches.empty()) {
            return OperationResult{false, "Không tìm thấy đợt giao vận với mã " + logisticsCode, 0};
        }
        const Record& logRec = logMatches[0];

        // 1. Completion Lock check (Rule 4.5)
        std::string currentLogStatus = "";
        if (logRec.has("status") && std::holds_alternative<std::string>(logRec.get("status"))) {
            currentLogStatus = std::get<std::string>(logRec.get("status"));
        }
        if (currentLogStatus == "COMPLETED" || currentLogStatus == "completed") {
            return OperationResult{false, "Đợt giao nhận này đã ở trạng thái HOÀN THÀNH (COMPLETED). Toàn bộ thao tác thay đổi trạng thái linh kiện đã bị khóa vĩnh viễn (Rule 4.5).", 0};
        }

        // 2. Resolve transaction type for business rule constraints
        std::string txnType = "";
        if (isInbound) {
            int inTxnId = -1;
            if (logRec.has("inbound_transaction_id") && std::holds_alternative<int>(logRec.get("inbound_transaction_id"))) {
                inTxnId = std::get<int>(logRec.get("inbound_transaction_id"));
            }
            Table* inTxnTable = db->getTable("inbound_transaction");
            if (inTxnTable && inTxnId > 0) {
                const Record* inTxnRec = inTxnTable->getRecordById(inTxnId);
                if (inTxnRec && inTxnRec->has("type") && std::holds_alternative<std::string>(inTxnRec->get("type"))) {
                    txnType = std::get<std::string>(inTxnRec->get("type"));
                }
            }
        } else {
            int outTxnId = -1;
            if (logRec.has("sell_id") && std::holds_alternative<int>(logRec.get("sell_id"))) {
                outTxnId = std::get<int>(logRec.get("sell_id"));
            }
            Table* outTxnTable = db->getTable("outbound_transaction");
            if (outTxnTable && outTxnId > 0) {
                const Record* outTxnRec = outTxnTable->getRecordById(outTxnId);
                if (outTxnRec && outTxnRec->has("type") && std::holds_alternative<std::string>(outTxnRec->get("type"))) {
                    txnType = std::get<std::string>(outTxnRec->get("type"));
                }
            }
        }

        // 3. Validate permitted target statuses (Rule 4.4, 4.5)
        if (!isInbound) {
            if (newStatus != "OUT") {
                return OperationResult{false, "Đơn xuất hàng chỉ cho phép trạng thái kết thúc là OUT (Rule 4.4.1).", 0};
            }
        } else {
            if (newStatus == "OUT") {
                return OperationResult{false, "Đơn nhập kho nghiêm cấm linh kiện có trạng thái OUT (Rule 4.4.2).", 0};
            }
            if (txnType == "PURCHASING" || txnType == "PUR") {
                if (newStatus != "AVAILABLE" && newStatus != "SCRAPPED") {
                    return OperationResult{false, "Đơn mua hàng mới (PUR) chỉ cho phép chuyển đổi giữa AVAILABLE và SCRAPPED (Rule 4.4.2, 4.5).", 0};
                }
            } else if (txnType == "WARRANTY_REPLACEMENT" || txnType == "WRP" ||
                       txnType == "WARRANTY_CHECKING" || txnType == "WCK") {
                if (newStatus != "WARRANTY") {
                    return OperationResult{false, "Đơn bảo hành (" + txnType + ") chỉ cho phép trạng thái WARRANTY (Rule 4.4.2).", 0};
                }
            } else if (txnType == "CUSTOMER_RETURN" || txnType == "CRT" ||
                       txnType == "ADJUSTMENT_IN" || txnType == "ADJ") {
                if (newStatus != "AVAILABLE" && newStatus != "WARRANTY" && newStatus != "SCRAPPED") {
                    return OperationResult{false, "Đơn " + txnType + " chỉ cho phép chọn trạng thái AVAILABLE, WARRANTY hoặc SCRAPPED (Rule 4.4.2).", 0};
                }
            }
        }

        // 4. Find unit record
        Vector<Record> matchedUnits = Query::filterRecords(*unitTable, "serial", Value(serial));
        if (matchedUnits.empty()) {
            return OperationResult{false, "Không tìm thấy linh kiện với mã serial '" + serial + "' trong kho.", 0};
        }
        const Record& uRec = matchedUnits[0];
        int unitId = std::get<int>(uRec.get("unit_id"));
        int productId = std::get<int>(uRec.get("product_id"));
        std::string oldStatus = std::get<std::string>(uRec.get("status"));

        if (oldStatus == newStatus) {
            return OperationResult{true, "Linh kiện " + serial + " hiện tại đã có trạng thái '" + newStatus + "'.", 0};
        }

        // 5. Atomic update transaction
        db->beginTransaction();

        Record uUp;
        uUp.set("status", newStatus);
        OperationResult rU = db->update("unit_inventory", unitId, uUp);
        if (!rU) { db->rollback(); return rU; }

        // Update product buckets directly
        const Record* pRec = prodTable ? prodTable->getRecordById(productId) : nullptr;
        int curAvail = 0, curWarr = 0, curScrap = 0;
        if (pRec) {
            if (pRec->has("available_quantity") && std::holds_alternative<int>(pRec->get("available_quantity")))
                curAvail = std::get<int>(pRec->get("available_quantity"));
            if (pRec->has("warranty_quantity") && std::holds_alternative<int>(pRec->get("warranty_quantity")))
                curWarr = std::get<int>(pRec->get("warranty_quantity"));
            if (pRec->has("scrapped_quantity") && std::holds_alternative<int>(pRec->get("scrapped_quantity")))
                curScrap = std::get<int>(pRec->get("scrapped_quantity"));
        }

        if (oldStatus == "AVAILABLE") curAvail = std::max(0, curAvail - 1);
        else if (oldStatus == "WARRANTY") curWarr = std::max(0, curWarr - 1);
        else if (oldStatus == "SCRAPPED") curScrap = std::max(0, curScrap - 1);

        if (newStatus == "AVAILABLE") curAvail++;
        else if (newStatus == "WARRANTY") curWarr++;
        else if (newStatus == "SCRAPPED") curScrap++;

        Record pUp;
        pUp.set("available_quantity", curAvail);
        pUp.set("warranty_quantity", curWarr);
        pUp.set("scrapped_quantity", curScrap);
        OperationResult rPq = db->update("product", productId, pUp);
        if (!rPq) { db->rollback(); return rPq; }

        // Update existing record in status_history if present, else insert new
        int existingHistId = -1;
        std::string pkHist = histTable->getPrimaryKeyColumn();
        int maxHistId = 0;
        int logPk = isInbound ? std::get<int>(logRec.get("delivery_id")) : std::get<int>(logRec.get("shipment_id"));

        const auto& hRows = histTable->getRows();
        for (size_t i = 0; i < hRows.size(); ++i) {
            if (histTable->isRowDeleted(i)) continue;
            const auto& hRow = hRows[i];
            if (hRow.has(pkHist) && std::holds_alternative<int>(hRow.get(pkHist))) {
                int hid = std::get<int>(hRow.get(pkHist));
                if (hid > maxHistId) maxHistId = hid;
            }
            if (hRow.has("transaction") && hRow.has("unit_id")) {
                int tx = std::holds_alternative<int>(hRow.get("transaction")) ? std::get<int>(hRow.get("transaction")) : -1;
                int uId = std::holds_alternative<int>(hRow.get("unit_id")) ? std::get<int>(hRow.get("unit_id")) : -1;
                if (tx == logPk && uId == unitId) {
                    existingHistId = (hRow.has("history_id") && std::holds_alternative<int>(hRow.get("history_id")))
                                   ? std::get<int>(hRow.get("history_id")) : -1;
                }
            }
        }

        if (existingHistId > 0) {
            Record hUp;
            hUp.set("curr_status", newStatus);
            OperationResult rH = db->update("status_history", existingHistId, hUp);
            if (!rH) { db->rollback(); return rH; }
        } else {
            int histId = maxHistId + 1;
            std::time_t tNow = std::time(nullptr);
            std::tm tmNow;
#if defined(_WIN32) || defined(_WIN64)
            localtime_s(&tmNow, &tNow);
#else
            localtime_r(&tNow, &tmNow);
#endif
            std::ostringstream dtOss;
            dtOss << std::setfill('0')
                  << (tmNow.tm_year + 1900) << "-"
                  << std::setw(2) << (tmNow.tm_mon + 1) << "-"
                  << std::setw(2) << tmNow.tm_mday;

            Record hRec;
            hRec.set("history_id", histId);
            hRec.set("unit_id", unitId);
            hRec.set("old_status", oldStatus);
            hRec.set("curr_status", newStatus);
            hRec.set("transaction", logPk);
            hRec.set("type", std::string(isInbound ? "INBOUND" : "OUTBOUND"));
            hRec.set("date", dtOss.str());

            OperationResult rH = db->insertRecord("status_history", hRec);
            if (!rH) { db->rollback(); return rH; }
        }

        db->commit();
        return OperationResult{true, "Đã cập nhật trạng thái linh kiện " + serial + " từ '" + oldStatus + "' sang '" + newStatus + "' thành công!", 3};
    }


// ========================================================
    // INVENTORY CHECK & ADJUSTMENT MOVEMENT UPDATE IMPLEMENTATION
    // ========================================================

    OperationResult updateInventoryCheckQuantity(
        Database* db,
        int checkQuantityId,
        int actualAvail,
        int actualWarr,
        int actualScrap,
        const std::string& note
    ) {
        if (!db || checkQuantityId <= 0) return OperationResult{false, "Tham số không hợp lệ.", 0};
        Table* cqTbl = db->getTable("inventory_check_quantity");
        if (!cqTbl) return OperationResult{false, "Bảng inventory_check_quantity không tồn tại.", 0};

        const Record* cqRec = cqTbl->getRecordById(checkQuantityId);
        if (!cqRec) return OperationResult{false, "Không tìm thấy bản ghi số lượng kiểm kho với ID " + std::to_string(checkQuantityId) + ".", 0};

        Record up;
        up.set("actual_available_quan", std::max(0, actualAvail));
        up.set("actual_warranty_quan", std::max(0, actualWarr));
        up.set("actual_scrapped_quan", std::max(0, actualScrap));
        up.set("note", note);

        return db->update("inventory_check_quantity", checkQuantityId, up);
    }

    OperationResult updateInventoryCheckUnit(
        Database* db,
        int checkUnitId,
        const std::string& scannedStatus
    ) {
        if (!db || checkUnitId <= 0) return OperationResult{false, "Tham số không hợp lệ.", 0};
        Table* cuTbl = db->getTable("inventory_check_unit");
        Table* cqTbl = db->getTable("inventory_check_quantity");
        if (!cuTbl || !cqTbl) return OperationResult{false, "Bảng kiểm kê không tồn tại.", 0};

        const Record* cuRec = cuTbl->getRecordById(checkUnitId);
        if (!cuRec) return OperationResult{false, "Không tìm thấy linh kiện kiểm kê với ID " + std::to_string(checkUnitId) + ".", 0};

        int cqId = std::get<int>(cuRec->get("check_quantity_id"));

        if (!db->beginTransaction()) return OperationResult{false, "Không thể bắt đầu Transaction CSDL.", 0};

        Record cuUp;
        cuUp.set("scanned_status", scannedStatus);
        OperationResult rCu = db->update("inventory_check_unit", checkUnitId, cuUp);
        if (!rCu) { db->rollback(); return rCu; }

        // Recalculate actual counts for this check_quantity
        Vector<Record> allUnitsForCq = Query::filterRecords(*cuTbl, "check_quantity_id", Value(cqId));
        int actAvail = 0, actWarr = 0, actScrap = 0;
        for (size_t i = 0; i < allUnitsForCq.size(); ++i) {
            const auto& u = allUnitsForCq[i];
            int uPk = std::get<int>(u.get("check_unit_id"));
            std::string st = (uPk == checkUnitId) ? scannedStatus : std::get<std::string>(u.get("scanned_status"));
            if (st == "AVAILABLE") actAvail++;
            else if (st == "WARRANTY") actWarr++;
            else if (st == "SCRAPPED") actScrap++;
        }

        Record cqUp;
        cqUp.set("actual_available_quan", actAvail);
        cqUp.set("actual_warranty_quan", actWarr);
        cqUp.set("actual_scrapped_quan", actScrap);

        OperationResult rCq = db->update("inventory_check_quantity", cqId, cqUp);
        if (!rCq) { db->rollback(); return rCq; }

        db->commit();
        return OperationResult{true, "Cập nhật trạng thái quét linh kiện và đồng bộ số lượng kiểm kho thành công!", 2};
    }

    OperationResult completeInventoryCheck(
        Database* db,
        int checkId
    ) {
        if (!db || checkId <= 0) return OperationResult{false, "Tham số không hợp lệ.", 0};
        Table* chkTbl = db->getTable("inventory_check");
        if (!chkTbl) return OperationResult{false, "Bảng inventory_check không tồn tại.", 0};

        const Record* chkRec = chkTbl->getRecordById(checkId);
        if (!chkRec) return OperationResult{false, "Không tìm thấy đợt kiểm kho với ID " + std::to_string(checkId) + ".", 0};

        Record up;
        up.set("status", std::string("COMPLETED"));
        return db->update("inventory_check", checkId, up);
    }

    OperationResult executeAdjustmentMovement(
        Database* db,
        int adjustId
    ) {
        if (!db || adjustId <= 0) return OperationResult{false, "Tham số không hợp lệ.", 0};
        Table* adjTbl = db->getTable("adjustment_movement");
        Table* detTbl = db->getTable("adjust_movement_detail");
        Table* prodTbl = db->getTable("product");
        Table* unitTbl = db->getTable("unit_inventory");
        Table* histTbl = db->getTable("status_history");

        if (!adjTbl || !detTbl || !prodTbl || !unitTbl || !histTbl) {
            return OperationResult{false, "Các bảng dữ liệu điều chuyển không đầy đủ.", 0};
        }

        const Record* adjRec = adjTbl->getRecordById(adjustId);
        if (!adjRec) return OperationResult{false, "Không tìm thấy phiếu điều chuyển với ID " + std::to_string(adjustId) + ".", 0};

        std::string adjStatus = std::get<std::string>(adjRec->get("status"));
        if (adjStatus == "COMPLETED") {
            return OperationResult{false, "Phiếu điều chuyển này đã hoàn thành (COMPLETED).", 0};
        }

        Vector<Record> details = Query::filterRecords(*detTbl, "adjust_id", Value(adjustId));
        if (details.empty()) {
            return OperationResult{false, "Phiếu điều chuyển không có mặt hàng chi tiết nào.", 0};
        }

        if (!db->beginTransaction()) return OperationResult{false, "Không thể bắt đầu Transaction CSDL.", 0};

        int totalAffected = 0;
        for (size_t i = 0; i < details.size(); ++i) {
            const auto& det = details[i];
            int detId = std::get<int>(det.get("adjust_detail_id"));
            int pid = std::get<int>(det.get("product_id"));
            std::string fromSt = std::get<std::string>(det.get("from_status"));
            std::string toSt = std::get<std::string>(det.get("to_status"));
            int reqQty = std::get<int>(det.get("adjust_quantity"));

            // Find candidates in unit_inventory
            Vector<Record> candidates;
            for (const auto& u : unitTbl->getRows()) {
                if (std::get<int>(u.get("product_id")) == pid && std::get<std::string>(u.get("status")) == fromSt) {
                    candidates.push_back(u);
                    if (static_cast<int>(candidates.size()) == reqQty) break;
                }
            }

            int moved = 0;
            for (size_t k = 0; k < candidates.size(); ++k) {
                int uId = std::get<int>(candidates[k].get("unit_id"));
                Record uUp;
                uUp.set("status", toSt);
                OperationResult rU = db->update("unit_inventory", uId, uUp);
                if (!rU) { db->rollback(); return rU; }

                // Log into status_history
                int maxHId = 1;
                for (const auto& h : histTbl->getRows()) {
                    if (h.has("history_id") && std::holds_alternative<int>(h.get("history_id"))) {
                        int hid = std::get<int>(h.get("history_id"));
                        if (hid >= maxHId) maxHId = hid + 1;
                    }
                }
                Record hRec;
                hRec.set("history_id", maxHId);
                hRec.set("unit_id", uId);
                hRec.set("old_status", fromSt);
                hRec.set("curr_status", toSt);
                hRec.set("transaction", adjustId);
                hRec.set("type", std::string("ADJUSTMENT"));
                OperationResult rH = db->insertRecord("status_history", hRec);
                if (!rH) { db->rollback(); return rH; }
                moved++;
                totalAffected++;
            }

            // Update product stock counters
            const Record* pRec = prodTbl->getRecordById(pid);
            if (pRec) {
                int avail = (pRec->has("available_quantity") && std::holds_alternative<int>(pRec->get("available_quantity")))
                          ? std::get<int>(pRec->get("available_quantity")) : 0;
                int warr = (pRec->has("warranty_quantity") && std::holds_alternative<int>(pRec->get("warranty_quantity")))
                         ? std::get<int>(pRec->get("warranty_quantity")) : 0;
                int scrap = (pRec->has("scrapped_quantity") && std::holds_alternative<int>(pRec->get("scrapped_quantity")))
                          ? std::get<int>(pRec->get("scrapped_quantity")) : 0;

                if (fromSt == "AVAILABLE") avail = std::max(0, avail - moved);
                else if (fromSt == "WARRANTY") warr = std::max(0, warr - moved);
                else if (fromSt == "SCRAPPED") scrap = std::max(0, scrap - moved);

                if (toSt == "AVAILABLE") avail += moved;
                else if (toSt == "WARRANTY") warr += moved;
                else if (toSt == "SCRAPPED") scrap += moved;

                Record pUp;
                pUp.set("available_quantity", avail);
                pUp.set("warranty_quantity", warr);
                pUp.set("scrapped_quantity", scrap);
                OperationResult rP = db->update("product", pid, pUp);
                if (!rP) { db->rollback(); return rP; }
                totalAffected++;
            }

            Record detUp;
            detUp.set("actual_quantity", moved);
            db->update("adjust_movement_detail", detId, detUp);
        }

        Record adjUp;
        adjUp.set("status", std::string("COMPLETED"));
        OperationResult rAdj = db->update("adjustment_movement", adjustId, adjUp);
        if (!rAdj) { db->rollback(); return rAdj; }
        totalAffected++;

        db->commit();
        return OperationResult{true, "Thực hiện điều chuyển trạng thái linh kiện thành công!", totalAffected};
    }
}
