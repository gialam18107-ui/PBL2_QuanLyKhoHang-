#include "UserUpdate.h"
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
        Table* qtyTable = db->getTable("product_quantity");
        Table* prodTable = db->getTable("product");

        if (!shipTable || !shipDetTable || !outTxnTable || !outDetTable || !unitTable || !histTable || !qtyTable || !prodTable) {
            return OperationResult{false, "Các bảng dữ liệu xuất kho không đầy đủ.", 0};
        }

        const Record* shipRec = shipTable->getRecordById(shipmentId);
        if (!shipRec) {
            return OperationResult{false, "Không tìm thấy đợt xuất hàng với ID " + std::to_string(shipmentId) + ".", 0};
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
        for (const auto& row : shipDetTable->getRows()) {
            if (row.has("shipment_id") && std::holds_alternative<int>(row.get("shipment_id")) &&
                std::get<int>(row.get("shipment_id")) == shipmentId &&
                row.has("product_id") && std::holds_alternative<int>(row.get("product_id")) &&
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
        for (const auto& row : outDetTable->getRows()) {
            if (row.has("outbound_transaction_id") && std::holds_alternative<int>(row.get("outbound_transaction_id")) &&
                std::get<int>(row.get("outbound_transaction_id")) == outTxnId &&
                row.has("product_id") && std::holds_alternative<int>(row.get("product_id")) &&
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

            for (const auto& uRow : unitTable->getRows()) {
                if (uRow.has("serial") && std::holds_alternative<std::string>(uRow.get("serial")) &&
                    std::get<std::string>(uRow.get("serial")) == sn &&
                    uRow.has("product_id") && std::holds_alternative<int>(uRow.get("product_id")) &&
                    std::get<int>(uRow.get("product_id")) == productId) {
                    unitId = std::get<int>(uRow.get("unit_id"));
                    curStatus = std::get<std::string>(uRow.get("status"));
                    break;
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
        const Record* pqRec = qtyTable->getRecordById(productId);
        int curAvail = 0, curWarr = 0, curScrap = 0;
        if (pqRec) {
            if (pqRec->has("available_quan") && std::holds_alternative<int>(pqRec->get("available_quan")))
                curAvail = std::get<int>(pqRec->get("available_quan"));
            if (pqRec->has("warranty_quan") && std::holds_alternative<int>(pqRec->get("warranty_quan")))
                curWarr = std::get<int>(pqRec->get("warranty_quan"));
            if (pqRec->has("scrapped_quan") && std::holds_alternative<int>(pqRec->get("scrapped_quan")))
                curScrap = std::get<int>(pqRec->get("scrapped_quan"));
        }

        Record pqUp;
        pqUp.set("available_quan", std::max(0, curAvail - decrementedAvail));
        pqUp.set("warranty_quan", std::max(0, curWarr - decrementedWarr));
        pqUp.set("scrapped_quan", std::max(0, curScrap - decrementedScrap));

        OperationResult rPq = db->update("product_quantity", productId, pqUp);
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

        // Check if shipment is fully dispatched -> COMPLETED
        bool shipAllDone = true;
        for (const auto& row : shipDetTable->getRows()) {
            if (row.has("shipment_id") && std::holds_alternative<int>(row.get("shipment_id")) &&
                std::get<int>(row.get("shipment_id")) == shipmentId) {
                int rQty = std::get<int>(row.get("received_quantity"));
                int aQty = std::get<int>(row.get("actual_quantity"));
                if (std::get<int>(row.get("shipment_detail_id")) == shipDetId) aQty += dispatchCount;
                if (aQty < rQty) { shipAllDone = false; break; }
            }
        }

        // Get current date
        std::time_t t = std::time(nullptr);
        std::tm tmVal;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tmVal, &t);
#else
        localtime_r(&t, &tmVal);
#endif
        std::ostringstream dtOss;
        dtOss << std::setfill('0')
              << (tmVal.tm_year + 1900) << "-"
              << std::setw(2) << (tmVal.tm_mon + 1) << "-"
              << std::setw(2) << tmVal.tm_mday;
        std::string todayStr = dtOss.str();

        Record shipStatusUp;
        if (shipAllDone) {
            shipStatusUp.set("status", std::string("COMPLETED"));
            if (isNull(shipRec->get("ship_at"))) {
                shipStatusUp.set("ship_at", todayStr);
            }
        } else {
            shipStatusUp.set("status", std::string("PICKING"));
        }
        db->update("shipment", shipmentId, shipStatusUp);

        // Check if outbound_transaction is fully dispatched -> CONFIRMED
        bool outTxnAllDone = true;
        for (const auto& row : outDetTable->getRows()) {
            if (row.has("outbound_transaction_id") && std::holds_alternative<int>(row.get("outbound_transaction_id")) &&
                std::get<int>(row.get("outbound_transaction_id")) == outTxnId) {
                int ordQ = std::get<int>(row.get("ordered_quantity"));
                int actQ = std::get<int>(row.get("actual_quantity"));
                if (std::get<int>(row.get("outbound_detail_id")) == outDetId) actQ += dispatchCount;
                if (actQ < ordQ) { outTxnAllDone = false; break; }
            }
        }

        Record outTxnStatusUp;
        if (outTxnAllDone) {
            outTxnStatusUp.set("status", std::string("CONFIRMED"));
        } else {
            outTxnStatusUp.set("status", std::string("PROCESSING"));
        }
        db->update("outbound_transaction", outTxnId, outTxnStatusUp);

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
}
