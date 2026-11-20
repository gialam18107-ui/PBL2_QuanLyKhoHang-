#include "UserUpdate.h"
#include "UserCreate.h"
#include "UserQuery.h"
#include "SchemaLocalization.h"
#include "query/Query.h"
#include "database/Constraint.h"
#include "datastructure/hashmap/HashMap.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

namespace user_update
{
    static std::string getProductCodeStr(Database* db, int pid)
    {
        if (!db || pid <= 0) return std::to_string(pid);
        const Table* prodTbl = db->getTable("product");
        if (!prodTbl) return std::to_string(pid);
        const Record* pRec = prodTbl->getRecordById(pid);
        if (pRec && pRec->has("product_code") && std::holds_alternative<std::string>(pRec->get("product_code"))) {
            return std::get<std::string>(pRec->get("product_code"));
        }
        return std::to_string(pid);
    }

    static int extractMaxSequenceLocal(const Table* table, const std::string& colName, const std::string& prefix, size_t numOffset)
    {
        if (!table) return 0;
        int maxSeq = 0;
        for (const Record& r : table->view()) {
            if (r.has(colName) && std::holds_alternative<std::string>(r.get(colName))) {
                std::string code = std::get<std::string>(r.get(colName));
                if (code.rfind(prefix, 0) == 0 && code.length() > numOffset) {
                    try {
                        int seq = std::stoi(code.substr(numOffset));
                        if (seq > maxSeq) maxSeq = seq;
                    } catch (...) {}
                }
            }
        }
        return maxSeq;
    }

    static bool hasInventoryCheckScannedProgress(Database* db, int checkId)
    {
        if (!db || checkId <= 0) return false;
        const Table* cuTbl = db->getTable("inventory_check_unit");
        if (cuTbl) {
            for (const auto& u : cuTbl->view("check_id", Value(checkId))) {
                std::string scanSt = (u.has("scanned_status") && std::holds_alternative<std::string>(u.get("scanned_status")))
                                   ? std::get<std::string>(u.get("scanned_status")) : "";
                if (!scanSt.empty() && scanSt != "UNCHECKED") {
                    return true;
                }
                std::string sysSt = (u.has("system_status") && std::holds_alternative<std::string>(u.get("system_status")))
                                  ? std::get<std::string>(u.get("system_status")) : "";
                if (sysSt.empty() || sysSt == "OUT") {
                    return true;
                }
            }
        }
        const Table* cqTbl = db->getTable("inventory_check_quantity");
        if (cqTbl) {
            for (const auto& cq : cqTbl->view("check_id", Value(checkId))) {
                int actA = (cq.has("actual_available_quan") && std::holds_alternative<int>(cq.get("actual_available_quan")))
                         ? std::get<int>(cq.get("actual_available_quan")) : 0;
                int actW = (cq.has("actual_warranty_quan") && std::holds_alternative<int>(cq.get("actual_warranty_quan")))
                         ? std::get<int>(cq.get("actual_warranty_quan")) : 0;
                int actS = (cq.has("actual_scrapped_quan") && std::holds_alternative<int>(cq.get("actual_scrapped_quan")))
                         ? std::get<int>(cq.get("actual_scrapped_quan")) : 0;
                if (actA > 0 || actW > 0 || actS > 0) {
                    return true;
                }
            }
        }
        return false;
    }

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
        const Table* table = db->getTable(tableName);
        if (!table) return OperationResult{false, "Bảng '" + tableName + "' không tồn tại trong CSDL.", 0};
        if (modifiedColumns.size() != modifyingValues.size()) {
            return OperationResult{false, "Số lượng cột và giá trị cần cập nhật không khớp.", 0};
        }

        std::string pkCol = getPrimaryKeyColumn(table);
        if (pkCol.empty()) {
            return OperationResult{false, "Không tìm thấy khóa chính của bảng '" + tableName + "'.", 0};
        }

        const Record* pOld = table->getRecordById(recordId);
        if (!pOld) {
            return OperationResult{false, "Không tìm thấy bản ghi với ID " + std::to_string(recordId) + " trong bảng '" + tableName + "'.", 0};
        }
        const Record& oldRecord = *pOld;

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
            if (table->isColumnNotNull(colName) && isNull(val)) {
                return OperationResult{false, "Trường '" + colName + "' bắt buộc nhập, không thể để trống.", 0};
            }

            // UNIQUE check
            if (table->isColumnUnique(colName)) {
                if (!isNull(val) && !(std::holds_alternative<std::string>(val) && std::get<std::string>(val).empty())) {
                    for (const auto& mRec : table->view(colName, val)) {
                        if (mRec.has(pkCol) && std::holds_alternative<int>(mRec.get(pkCol)) &&
                            std::get<int>(mRec.get(pkCol)) == recordId) {
                            continue;
                        }
                        return OperationResult{false, "Giá trị cập nhật của trường '" + colName + "' đã tồn tại (bị trùng lặp).", 0};
                    }
                }
            }
        }

        // 3. Domain Business Rules & Immutable Fields Validation (Rule 5.3, 5.6 & 6.3.1)
        if (tableName == "inbound_transaction" || tableName == "outbound_transaction") {
            std::string oldTxnSt = oldRecord.has("status") && std::holds_alternative<std::string>(oldRecord.get("status"))
                                 ? std::get<std::string>(oldRecord.get("status")) : "";
            std::string txnType = oldRecord.has("type") && std::holds_alternative<std::string>(oldRecord.get("type"))
                                ? std::get<std::string>(oldRecord.get("type")) : "";
            bool isAdj = (txnType == "ADJUSTMENT_IN" || txnType == "ADJUSTMENT_OUT" || txnType == "ADJ");
            (void)isAdj;

            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& colName = modifiedColumns[i];
                if (colName != "status" && colName != "note") {
                    return OperationResult{false, "Chỉ được phép cập nhật trạng thái hoặc ghi chú của đơn giao dịch.", 0};
                }
                if (colName == "status") {
                    std::string newTxnSt = std::holds_alternative<std::string>(modifyingValues[i])
                                         ? std::get<std::string>(modifyingValues[i]) : "";
                    if ((oldTxnSt == "COMPLETED" || oldTxnSt == "completed") && newTxnSt != oldTxnSt) {
                        return OperationResult{false, "Đơn giao dịch đã ở trạng thái " + schema_i18n::statusVi(oldTxnSt) + ", không thể chuyển ngược trạng thái.", 0};
                    }
                    if (oldTxnSt == "CANCELLED" && newTxnSt != "CANCELLED") {
                        return OperationResult{false, "Đơn giao dịch đã ở trạng thái Đã hủy, trạng thái bị khóa vĩnh viễn.", 0};
                    }

                    if (newTxnSt == "CANCELLED") {
                        if (oldTxnSt != "PENDING") {
                            return OperationResult{false, "Chỉ được phép hủy đơn giao dịch khi đơn đang ở trạng thái Chờ xử lí.", 0};
                        }
                        if (tableName == "inbound_transaction") {
                            const Table* dTable = db->getTable("delivery");
                            const Table* ddTable = db->getTable("delivery_detail");
                            if (dTable) {
                                for (const auto& d : dTable->view("inbound_transaction_id", Value(recordId))) {
                                    std::string dSt = d.has("status") && std::holds_alternative<std::string>(d.get("status")) ? std::get<std::string>(d.get("status")) : "";
                                    if (dSt == "COMPLETED" || dSt == "DELIVERED") {
                                        return OperationResult{false, "Không thể hủy đơn giao dịch: Đã có đợt giao nhận hoàn tất.", 0};
                                    }
                                    if (ddTable && d.has("delivery_id") && std::holds_alternative<int>(d.get("delivery_id"))) {
                                        for (const auto& dd : ddTable->view("delivery_id", Value(std::get<int>(d.get("delivery_id"))))) {
                                            int act = dd.has("actual_quantity") && std::holds_alternative<int>(dd.get("actual_quantity")) ? std::get<int>(dd.get("actual_quantity")) : 0;
                                            if (act > 0) {
                                                return OperationResult{false, "Không thể hủy đơn giao dịch: Đã có linh kiện được ghi nhận thực tế.", 0};
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            const Table* sTable = db->getTable("shipment");
                            const Table* sdTable = db->getTable("shipment_detail");
                            if (sTable) {
                                for (const auto& s : sTable->view("outbound_transaction_id", Value(recordId))) {
                                    std::string sSt = s.has("status") && std::holds_alternative<std::string>(s.get("status")) ? std::get<std::string>(s.get("status")) : "";
                                    if (sSt == "COMPLETED" || sSt == "SHIPPED") {
                                        return OperationResult{false, "Không thể hủy đơn giao dịch: Đã có đợt xuất hàng hoàn tất.", 0};
                                    }
                                    if (sdTable && s.has("shipment_id") && std::holds_alternative<int>(s.get("shipment_id"))) {
                                        for (const auto& sd : sdTable->view("shipment_id", Value(std::get<int>(s.get("shipment_id"))))) {
                                            int act = sd.has("actual_quantity") && std::holds_alternative<int>(sd.get("actual_quantity")) ? std::get<int>(sd.get("actual_quantity")) : 0;
                                            if (act > 0) {
                                                return OperationResult{false, "Không thể hủy đơn giao dịch: Đã có linh kiện được xuất thực tế.", 0};
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    } else if (newTxnSt == "COMPLETED") {
                        const Table* itmTbl = db->getTable(tableName == "inbound_transaction" ? "inbound_detail" : "outbound_detail");
                        std::string fkCol = (tableName == "inbound_transaction") ? "inbound_transaction_id" : "outbound_transaction_id";
                        Vector<Record> itmRecs;
                        if (itmTbl) {
                            for (const auto& r : itmTbl->view(fkCol, Value(recordId))) itmRecs.push_back(r);
                            if (itmRecs.empty()) {
                                fkCol = (tableName == "inbound_transaction") ? "inbound_id" : "outbound_id";
                                for (const auto& r : itmTbl->view(fkCol, Value(recordId))) itmRecs.push_back(r);
                            }
                        }
                        int totalReq = 0;
                        for (const auto& it : itmRecs) {
                            if (it.has("ordered_quantity") && std::holds_alternative<int>(it.get("ordered_quantity"))) {
                                totalReq += std::get<int>(it.get("ordered_quantity"));
                            }
                        }

                        int totalAct = 0;
                        if (tableName == "inbound_transaction") {
                            const Table* dTable = db->getTable("delivery");
                            const Table* ddTable = db->getTable("delivery_detail");
                            if (dTable && ddTable) {
                                for (const auto& d : dTable->view("inbound_transaction_id", Value(recordId))) {
                                    if (d.has("delivery_id") && std::holds_alternative<int>(d.get("delivery_id"))) {
                                        for (const auto& dd : ddTable->view("delivery_id", Value(std::get<int>(d.get("delivery_id"))))) {
                                            if (dd.has("actual_quantity") && std::holds_alternative<int>(dd.get("actual_quantity"))) {
                                                totalAct += std::get<int>(dd.get("actual_quantity"));
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            const Table* sTable = db->getTable("shipment");
                            const Table* sdTable = db->getTable("shipment_detail");
                            if (sTable && sdTable) {
                                for (const auto& s : sTable->view("outbound_transaction_id", Value(recordId))) {
                                    if (s.has("shipment_id") && std::holds_alternative<int>(s.get("shipment_id"))) {
                                        for (const auto& sd : sdTable->view("shipment_id", Value(std::get<int>(s.get("shipment_id"))))) {
                                            if (sd.has("actual_quantity") && std::holds_alternative<int>(sd.get("actual_quantity"))) {
                                                totalAct += std::get<int>(sd.get("actual_quantity"));
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        // Note: Under-fulfilled confirmation to COMPLETED is permitted with UI dialog confirmation & logistics lock (Rule 5.6).
                    } else if (newTxnSt == "PENDING" && oldTxnSt != "PENDING") {
                        return OperationResult{false, "Không thể chuyển ngược trạng thái đơn giao dịch về Chờ xử lí.", 0};
                    }
                }
            }
        }
        else if (tableName == "delivery") {
            // Rule 5.6: Logistics Lock if parent transaction is COMPLETED or CANCELLED
            int inId = -1;
            if (oldRecord.has("inbound_transaction_id") && std::holds_alternative<int>(oldRecord.get("inbound_transaction_id"))) {
                inId = std::get<int>(oldRecord.get("inbound_transaction_id"));
            } else if (oldRecord.has("inbound_id") && std::holds_alternative<int>(oldRecord.get("inbound_id"))) {
                inId = std::get<int>(oldRecord.get("inbound_id"));
            }
            if (inId != -1) {
                const Table* inTab = db->getTable("inbound_transaction");
                if (inTab) {
                    const Record* inRec = inTab->getRecordById(inId);
                    if (inRec && inRec->has("status") && std::holds_alternative<std::string>(inRec->get("status"))) {
                        std::string pSt = std::get<std::string>(inRec->get("status"));
                        if (pSt == "COMPLETED" || pSt == "CANCELLED") {
                            return OperationResult{false, "Đơn hàng (" + schema_i18n::statusVi(pSt) + ") đã hoàn tất hoặc bị hủy. Toàn bộ các đợt nhận hàng đã bị khóa vĩnh viễn.", 0};
                        }
                    }
                }
            }

            std::string oldDelSt = oldRecord.has("status") && std::holds_alternative<std::string>(oldRecord.get("status"))
                                 ? std::get<std::string>(oldRecord.get("status")) : "";
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& colName = modifiedColumns[i];
                if (colName != "status" && colName != "delivery_at") {
                    return OperationResult{false, "Chỉ được phép chỉnh sửa trạng thái và ngày giao nhận thực tế của đợt nhận hàng.", 0};
                }
                if (colName == "status") {
                    std::string newDelSt = std::holds_alternative<std::string>(modifyingValues[i])
                                         ? std::get<std::string>(modifyingValues[i]) : "";
                    if (oldDelSt == "COMPLETED" || oldDelSt == "completed") {
                        if (newDelSt != "COMPLETED") {
                            return OperationResult{false, "Đợt nhận hàng đã ở trạng thái Hoàn thành, không thể chuyển ngược trạng thái.", 0};
                        }
                    }
                    if (oldDelSt == "CANCELLED" && newDelSt != "CANCELLED") {
                        return OperationResult{false, "Đợt nhận hàng đã ở trạng thái Đã hủy, trạng thái bị khóa vĩnh viễn.", 0};
                    }
                    if (newDelSt == "CANCELLED") {
                        const Table* ddTable = db->getTable("delivery_detail");
                        if (ddTable) {
                            for (const auto& dd : ddTable->view("delivery_id", Value(recordId))) {
                                int act = dd.has("actual_quantity") && std::holds_alternative<int>(dd.get("actual_quantity"))
                                        ? std::get<int>(dd.get("actual_quantity")) : 0;
                                if (act > 0) {
                                    return OperationResult{false, "Không thể hủy đợt nhận hàng: Đã có linh kiện được ghi nhận thực tế (" + std::to_string(act) + " cái).", 0};
                                }
                            }
                        }
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
                        const Table* inTab = db->getTable("inbound_transaction");
                        if (inTab) {
                            const Record* inRec = inTab->getRecordById(inId);
                            if (inRec && inRec->has("order_at") && std::holds_alternative<std::string>(inRec->get("order_at"))) {
                                std::string orderAt = std::get<std::string>(inRec->get("order_at"));
                                if (newDeliveryAt < orderAt) {
                                    return OperationResult{false, "Ngày giao nhận thực tế (" + newDeliveryAt + ") không được sớm hơn ngày đặt hàng (" + orderAt + ").", 0};
                                }
                            }
                        }
                    }
                }
            }
        }
        else if (tableName == "shipment") {
            // Rule 5.6: Logistics Lock if parent transaction is COMPLETED or CANCELLED
            int outId = -1;
            if (oldRecord.has("outbound_transaction_id") && std::holds_alternative<int>(oldRecord.get("outbound_transaction_id"))) {
                outId = std::get<int>(oldRecord.get("outbound_transaction_id"));
            } else if (oldRecord.has("outbound_trainsaction_id") && std::holds_alternative<int>(oldRecord.get("outbound_trainsaction_id"))) {
                outId = std::get<int>(oldRecord.get("outbound_trainsaction_id"));
            } else if (oldRecord.has("outbound_id") && std::holds_alternative<int>(oldRecord.get("outbound_id"))) {
                outId = std::get<int>(oldRecord.get("outbound_id"));
            }
            if (outId != -1) {
                const Table* outTab = db->getTable("outbound_transaction");
                if (outTab) {
                    const Record* outRec = outTab->getRecordById(outId);
                    if (outRec && outRec->has("status") && std::holds_alternative<std::string>(outRec->get("status"))) {
                        std::string pSt = std::get<std::string>(outRec->get("status"));
                        if (pSt == "COMPLETED" || pSt == "CANCELLED") {
                            return OperationResult{false, "Đơn hàng (" + schema_i18n::statusVi(pSt) + ") đã hoàn tất hoặc bị hủy. Toàn bộ các đợt xuất hàng đã bị khóa vĩnh viễn.", 0};
                        }
                    }
                }
            }

            std::string oldShipSt = oldRecord.has("status") && std::holds_alternative<std::string>(oldRecord.get("status"))
                                  ? std::get<std::string>(oldRecord.get("status")) : "";
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& colName = modifiedColumns[i];
                if (colName != "status" && colName != "ship_at") {
                    return OperationResult{false, "Chỉ được phép chỉnh sửa trạng thái và ngày xuất thực tế của đợt xuất hàng.", 0};
                }
                if (colName == "status") {
                    std::string newShipSt = std::holds_alternative<std::string>(modifyingValues[i])
                                          ? std::get<std::string>(modifyingValues[i]) : "";
                    if ((oldShipSt == "COMPLETED" || oldShipSt == "completed") && newShipSt != "COMPLETED") {
                        return OperationResult{false, "Đợt xuất hàng đã ở trạng thái Hoàn thành, không thể chuyển ngược trạng thái.", 0};
                    }
                    if (oldShipSt == "CANCELLED" && newShipSt != "CANCELLED") {
                        return OperationResult{false, "Đợt xuất hàng đã ở trạng thái Đã hủy, trạng thái bị khóa vĩnh viễn.", 0};
                    }
                    if (newShipSt == "CANCELLED") {
                        const Table* sdTable = db->getTable("shipment_detail");
                        if (sdTable) {
                            for (const auto& sd : sdTable->view("shipment_id", Value(recordId))) {
                                int act = sd.has("actual_quantity") && std::holds_alternative<int>(sd.get("actual_quantity"))
                                        ? std::get<int>(sd.get("actual_quantity")) : 0;
                                if (act > 0) {
                                    return OperationResult{false, "Không thể hủy đợt xuất hàng: Đã có linh kiện được xuất thực tế (" + std::to_string(act) + " cái).", 0};
                                }
                            }
                        }
                    }
                }

                // Chronological Date Validation: ship_at >= outbound_transaction.order_at
                if (colName == "ship_at" && std::holds_alternative<std::string>(modifyingValues[i])) {
                    std::string newShipAt = std::get<std::string>(modifyingValues[i]);
                    int outId = -1;
                    if (oldRecord.has("outbound_transaction_id") && std::holds_alternative<int>(oldRecord.get("outbound_transaction_id"))) {
                        outId = std::get<int>(oldRecord.get("outbound_transaction_id"));
                    } else if (oldRecord.has("outbound_trainsaction_id") && std::holds_alternative<int>(oldRecord.get("outbound_trainsaction_id"))) {
                        outId = std::get<int>(oldRecord.get("outbound_trainsaction_id"));
                    } else if (oldRecord.has("outbound_id") && std::holds_alternative<int>(oldRecord.get("outbound_id"))) {
                        outId = std::get<int>(oldRecord.get("outbound_id"));
                    }

                    if (outId != -1) {
                        const Table* outTab = db->getTable("outbound_transaction");
                        if (outTab) {
                            const Record* outRec = outTab->getRecordById(outId);
                            if (outRec && outRec->has("order_at") && std::holds_alternative<std::string>(outRec->get("order_at"))) {
                                std::string orderAt = std::get<std::string>(outRec->get("order_at"));
                                if (newShipAt < orderAt) {
                                    return OperationResult{false, "Ngày xuất hàng thực tế (" + newShipAt + ") không được sớm hơn ngày đặt hàng (" + orderAt + ").", 0};
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
                return OperationResult{false, "Đợt kiểm kho đã hoàn thành, không thể chỉnh sửa.", 0};
            }
            if (oldChkSt == "CANCELLED" || oldChkSt == "cancelled") {
                return OperationResult{false, "Đợt kiểm kho đã bị hủy, trạng thái bị khóa vĩnh viễn.", 0};
            }
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& colName = modifiedColumns[i];
                if (colName != "status" && colName != "note") {
                    return OperationResult{false, "Chỉ được phép cập nhật trạng thái hoặc ghi chú của đợt kiểm kho.", 0};
                }
                if (colName == "status") {
                    std::string newChkSt = std::holds_alternative<std::string>(modifyingValues[i])
                                         ? std::get<std::string>(modifyingValues[i]) : "";
                    if (newChkSt != "PENDING" && newChkSt != "PROCESSING" &&
                        newChkSt != "COMPLETED" && newChkSt != "CANCELLED") {
                        return OperationResult{false, "Trạng thái kiểm kho '" + schema_i18n::statusVi(newChkSt) + "' không hợp lệ.", 0};
                    }
                    if (newChkSt == "PENDING" && oldChkSt != "PENDING") {
                        return OperationResult{false, "Không thể chuyển ngược trạng thái đợt kiểm kho về Chờ xử lí.", 0};
                    }
                    if (newChkSt == "CANCELLED") {
                        if (oldChkSt != "PENDING") {
                            return OperationResult{false, "Chỉ được phép hủy đợt kiểm kho khi đang ở trạng thái Chờ xử lí.", 0};
                        }
                        if (hasInventoryCheckScannedProgress(db, recordId)) {
                            return OperationResult{false, "Không thể hủy đợt kiểm kho: Đã có linh kiện hoặc số lượng kiểm đếm được ghi nhận.", 0};
                        }
                    }
                }
            }
        }
        else if (tableName == "adjustment_movement") {
            std::string oldAdjSt = oldRecord.has("status") && std::holds_alternative<std::string>(oldRecord.get("status"))
                                 ? std::get<std::string>(oldRecord.get("status")) : "";
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                const std::string& colName = modifiedColumns[i];
                if (colName != "status" && colName != "note") {
                    return OperationResult{false, "Chỉ được phép cập nhật trạng thái hoặc ghi chú của phiếu điều chuyển.", 0};
                }
                if (colName == "status") {
                    std::string newAdjSt = std::holds_alternative<std::string>(modifyingValues[i])
                                         ? std::get<std::string>(modifyingValues[i]) : "";
                    if (oldAdjSt == "CANCELLED" && newAdjSt != "CANCELLED") {
                        return OperationResult{false, "Phiếu điều chuyển đã ở trạng thái Đã hủy, trạng thái bị khóa vĩnh viễn.", 0};
                    }
                    if ((oldAdjSt == "COMPLETED" || oldAdjSt == "completed") && newAdjSt != oldAdjSt) {
                        return OperationResult{false, "Phiếu điều chuyển đã ở trạng thái Hoàn thành, không thể chuyển ngược trạng thái.", 0};
                    }
                    if (newAdjSt == "CANCELLED") {
                        if (oldAdjSt != "PENDING") {
                            return OperationResult{false, "Chỉ được phép hủy phiếu điều chuyển khi phiếu đang ở trạng thái Chờ xử lí.", 0};
                        }
                        const Table* detTbl = db->getTable("adjust_movement_detail");
                        if (detTbl) {
                            for (const auto& d : detTbl->view("adjust_id", Value(recordId))) {
                                int act = d.has("actual_quantity") && std::holds_alternative<int>(d.get("actual_quantity"))
                                        ? std::get<int>(d.get("actual_quantity")) : 0;
                                if (act > 0) {
                                    return OperationResult{false, "Không thể hủy phiếu điều chuyển: Đã có linh kiện được điều chuyển trạng thái.", 0};
                                }
                            }
                        }
                    } else if (newAdjSt == "COMPLETED") {
                        // Allowed to transition to COMPLETED even if not 100% fulfilled (handled by UI confirmation & checkbox) (Rule 5.6)
                    } else if (newAdjSt == "PENDING" && oldAdjSt != "PENDING") {
                        return OperationResult{false, "Không thể chuyển ngược trạng thái phiếu điều chuyển về Chờ xử lí.", 0};
                    }
                }
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
                    return OperationResult{false, "Chỉ được phép cập nhật trạng thái quét cho linh kiện kiểm kê.", 0};
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
                    return OperationResult{false, "Không được phép thay đổi mã khách hàng, ID hoặc loại khách hàng.", 0};
                }
            }
        }
        else if (tableName == "product") {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                if (modifiedColumns[i] == "is_sold") {
                    bool newSold = std::holds_alternative<bool>(modifyingValues[i]) ? std::get<bool>(modifyingValues[i]) : true;
                    bool oldSold = (oldRecord.has("is_sold") && std::holds_alternative<bool>(oldRecord.get("is_sold"))) ? std::get<bool>(oldRecord.get("is_sold")) : true;
                    if (oldSold != newSold) {
                        std::string activeChk = user_query::find_active_inventory_check_for_product(db, recordId);
                        if (!activeChk.empty()) {
                            return OperationResult{false, "Sản phẩm [" + getProductCodeStr(db, recordId) + "] đang nằm trong đợt kiểm kê [" + activeChk + "] chưa hoàn tất/hủy. Tạm khóa thay đổi trạng thái kinh doanh.", 0};
                        }
                        if (!newSold) {
                            int avail = (oldRecord.has("available_quantity") && std::holds_alternative<int>(oldRecord.get("available_quantity"))) ? std::get<int>(oldRecord.get("available_quantity")) : 0;
                            int warr = (oldRecord.has("warranty_quantity") && std::holds_alternative<int>(oldRecord.get("warranty_quantity"))) ? std::get<int>(oldRecord.get("warranty_quantity")) : 0;
                            int scrap = (oldRecord.has("scrapped_quantity") && std::holds_alternative<int>(oldRecord.get("scrapped_quantity"))) ? std::get<int>(oldRecord.get("scrapped_quantity")) : 0;
                            int total = avail + warr + scrap;
                            if (total > 0) {
                                return OperationResult{false, "Không thể chuyển trạng thái sang Ngừng kinh doanh: Sản phẩm vẫn còn tồn kho (" + std::to_string(total) + " cái).", 0};
                            }
                        }
                    }
                }
            }
        }
        else if (tableName == "brand") {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                if (modifiedColumns[i] == "code") {
                    const Table* prodTable = db->getTable("product");
                    if (prodTable && oldRecord.has("brand") && std::holds_alternative<std::string>(oldRecord.get("brand"))) {
                        std::string bName = std::get<std::string>(oldRecord.get("brand"));
                        if (prodTable->contains("brand", Value(bName))) {
                            return OperationResult{false, "Không được phép sửa mã thương hiệu khi đã có sản phẩm trong danh mục.", 0};
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
                        const Table* histTable = db->getTable("is_sold_changing");
                        if (histTable) {
                            time_t now = time(nullptr);
                            tm* t = localtime(&now);
                            char buf[32];
                            strftime(buf, sizeof(buf), "%Y-%m-%d", t);
                            Record hRec;
                            hRec.set("product_id", recordId);
                            hRec.set("date", std::string(buf));
                            hRec.set("old_status", oldSold);
                            hRec.set("new_status", newSold);
                            db->insertRecord("is_sold_changing", hRec);
                        }
                    }
                    break;
                }
            }
        }

        if (res.success && (tableName == "inbound_transaction" || tableName == "outbound_transaction")) {
            for (size_t i = 0; i < modifiedColumns.size(); ++i) {
                if (modifiedColumns[i] == "status") {
                    std::string newTxnSt = std::holds_alternative<std::string>(modifyingValues[i])
                                         ? std::get<std::string>(modifyingValues[i]) : "";
                    if (newTxnSt == "CANCELLED") {
                        if (tableName == "inbound_transaction") {
                            const Table* dTable = db->getTable("delivery");
                            if (dTable) {
                                for (const auto& d : dTable->view("inbound_transaction_id", Value(recordId))) {
                                    int dId = std::get<int>(d.get("delivery_id"));
                                    std::string dSt = d.has("status") && std::holds_alternative<std::string>(d.get("status")) ? std::get<std::string>(d.get("status")) : "";
                                    if (dSt != "COMPLETED" && dSt != "CANCELLED") {
                                        Record dUp;
                                        dUp.set("status", std::string("CANCELLED"));
                                        db->update("delivery", dId, dUp);
                                    }
                                }
                            }
                        } else {
                            const Table* sTable = db->getTable("shipment");
                            if (sTable) {
                                for (const auto& s : sTable->view("outbound_transaction_id", Value(recordId))) {
                                    int sId = std::get<int>(s.get("shipment_id"));
                                    std::string sSt = s.has("status") && std::holds_alternative<std::string>(s.get("status")) ? std::get<std::string>(s.get("status")) : "";
                                    if (sSt != "COMPLETED" && sSt != "CANCELLED") {
                                        Record sUp;
                                        sUp.set("status", std::string("CANCELLED"));
                                        db->update("shipment", sId, sUp);
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }
        return res;
    }

    OperationResult updateAdjustmentMovementStatus(
        Database* db,
        int adjustId,
        const std::string& newStatus
    ) {
        if (!db || adjustId <= 0) return OperationResult{false, "Tham số không hợp lệ.", 0};
        Vector<std::string> cols;
        cols.push_back("status");
        Vector<Value> vals;
        vals.push_back(Value(newStatus));
        return updateRecord(db, "adjustment_movement", adjustId, cols, vals);
    }

    OperationResult updateInventoryCheckStatus(
        Database* db,
        int checkId,
        const std::string& newStatus
    ) {
        if (!db || checkId <= 0) return OperationResult{false, "Tham số không hợp lệ.", 0};
        Vector<std::string> cols;
        cols.push_back("status");
        Vector<Value> vals;
        vals.push_back(Value(newStatus));
        return updateRecord(db, "inventory_check", checkId, cols, vals);
    }

    OperationResult cancelAdjustmentMovement(
        Database* db,
        int adjustId
    ) {
        return updateAdjustmentMovementStatus(db, adjustId, "CANCELLED");
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
        const Table* prodTable = db->getTable("product");
        const Table* histTable = db->getTable("is_sold_changing");
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

        std::string activeChk = user_query::find_active_inventory_check_for_product(db, productId);
        if (!activeChk.empty()) {
            return OperationResult{false, "Sản phẩm [" + getProductCodeStr(db, productId) + "] đang nằm trong đợt kiểm kê [" + activeChk + "] chưa hoàn tất/hủy. Tạm khóa thay đổi trạng thái kinh doanh.", 0};
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
                return OperationResult{false, "Không thể chuyển trạng thái sang Ngừng kinh doanh: Sản phẩm vẫn còn tồn kho (" + std::to_string(total) + " cái).", 0};
            }
        }

        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu giao dịch CSDL.", 0};
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

            Record hRec;
            hRec.set("product_id", productId);
            hRec.set("date", actualDate);
            hRec.set("old_status", oldIsSold);
            hRec.set("new_status", isSold);

            OperationResult rH = db->insertRecord("is_sold_changing", hRec);
            if (!rH) { db->rollback(); return rH; }
        }

        db->commit();
        return OperationResult{true, "Cập nhật trạng thái kinh doanh và ghi nhật ký lịch sử thành công!", 2};
    }

    OperationResult updateCustomerContact(
        Database* db,
        int customerId,
        const std::string& nameOrCompany,
        const std::string& phone,
        const std::string& email,
        const std::string& taxCode,
        const std::string& address
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        const Table* custTable = db->getTable("customer");
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
            return OperationResult{false, "Không thể bắt đầu giao dịch CSDL.", 0};
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
            if (email.empty()) iUp.set("email", Value{});
            else iUp.set("email", email);
            if (!iUp.getValues().empty()) {
                OperationResult rInfo = db->update("individual_info", customerId, iUp);
                if (!rInfo) {
                    db->rollback();
                    return rInfo;
                }
            }
        } else if (custType == "business") {
            Record bUp;
            if (phone.empty()) bUp.set("phone_number", Value{});
            else bUp.set("phone_number", phone);
            if (email.empty()) bUp.set("email", Value{});
            else bUp.set("email", email);
            if (!taxCode.empty()) bUp.set("tax_code", taxCode);
            if (!address.empty()) bUp.set("address", address);
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
        const Table* brandTable = db->getTable("brand");
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

    static OperationResult dispatchUnitsForShipmentBatchInternal(
        Database* db,
        int shipmentId,
        const Vector<BatchDispatchUnitEntry>& entries,
        bool manageTransaction
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        if (entries.empty()) return OperationResult{false, "Danh sách serial xuất kho không được để trống.", 0};

        const Table* shipTable = db->getTable("shipment");
        const Table* shipDetTable = db->getTable("shipment_detail");
        const Table* outTxnTable = db->getTable("outbound_transaction");
        const Table* outDetTable = db->getTable("outbound_detail");
        const Table* unitTable = db->getTable("unit_inventory");
        const Table* histTable = db->getTable("status_history");
        const Table* prodTable = db->getTable("product");

        if (!shipTable || !shipDetTable || !outTxnTable || !outDetTable || !unitTable || !histTable || !prodTable) {
            return OperationResult{false, "Các bảng dữ liệu xuất kho không đầy đủ.", 0};
        }

        const Record* shipRec = shipTable->getRecordById(shipmentId);
        if (!shipRec) {
            return OperationResult{false, "Không tìm thấy đợt xuất hàng với ID " + std::to_string(shipmentId) + ".", 0};
        }

        std::string shipStatus = shipRec->has("status") && std::holds_alternative<std::string>(shipRec->get("status"))
                               ? std::get<std::string>(shipRec->get("status")) : "";
        if (shipStatus == "COMPLETED" || shipStatus == "completed" || shipStatus == "CANCELLED") {
            return OperationResult{false, "Đợt xuất hàng này đã ở trạng thái Hoàn thành hoặc Đã hủy, không thể xuất thêm linh kiện.", 0};
        }

        int outTxnId = -1;
        if (shipRec->has("outbound_transaction_id") && std::holds_alternative<int>(shipRec->get("outbound_transaction_id"))) {
            outTxnId = std::get<int>(shipRec->get("outbound_transaction_id"));
        } else if (shipRec->has("outbound_trainsaction_id") && std::holds_alternative<int>(shipRec->get("outbound_trainsaction_id"))) {
            outTxnId = std::get<int>(shipRec->get("outbound_trainsaction_id"));
        }
        const Record* outTxnRec = outTxnTable->getRecordById(outTxnId);
        if (!outTxnRec) {
            return OperationResult{false, "Không tìm thấy đơn xuất kho gốc của đợt xuất này.", 0};
        }

        std::string outTxnSt = outTxnRec->has("status") && std::holds_alternative<std::string>(outTxnRec->get("status"))
                             ? std::get<std::string>(outTxnRec->get("status")) : "";
        if (outTxnSt == "COMPLETED" || outTxnSt == "CANCELLED") {
            return OperationResult{false, "Đơn xuất kho (" + schema_i18n::statusVi(outTxnSt) + ") đã hoàn tất hoặc bị hủy. Toàn bộ các đợt xuất hàng đã bị khóa vĩnh viễn.", 0};
        }

        std::string outType = outTxnRec->has("type") && std::holds_alternative<std::string>(outTxnRec->get("type"))
                            ? std::get<std::string>(outTxnRec->get("type")) : "SELLING";

        // Group entry serials by productId
        HashMap<int, Vector<std::string>> serialsByPid;
        Vector<int> orderedPids;
        for (size_t i = 0; i < entries.size(); ++i) {
            int pid = entries[i].productId;
            if (pid <= 0 || entries[i].serial.empty()) {
                return OperationResult{false, "Thông tin sản phẩm hoặc mã serial xuất kho không hợp lệ.", 0};
            }
            if (!serialsByPid.contains(pid)) {
                orderedPids.push_back(pid);
            }
            serialsByPid[pid].push_back(entries[i].serial);
        }

        // Stock Freeze Check
        int conflictPid = 0;
        std::string activeCheckCode = user_query::find_active_inventory_check_for_products(db, orderedPids, conflictPid);
        if (!activeCheckCode.empty()) {
            return OperationResult{
                false,
                "Sản phẩm [" + getProductCodeStr(db, conflictPid) + "] đang nằm trong đợt kiểm kê [" + activeCheckCode +
                "] chưa hoàn tất/hủy. Tạm khóa xuất kho cho tới khi đợt kiểm kê kết thúc.",
                0
            };
        }

        // Pre-index shipment_detail and outbound_detail by product_id
        struct DetailInfo {
            int detailId = -1;
            int planQty = 0;
            int actQty = 0;
        };
        HashMap<int, DetailInfo> shipDetailMap;
        for (const auto& row : shipDetTable->view("shipment_id", Value(shipmentId))) {
            if (row.has("product_id") && std::holds_alternative<int>(row.get("product_id"))) {
                int p = std::get<int>(row.get("product_id"));
                DetailInfo info;
                info.detailId = std::get<int>(row.get("shipment_detail_id"));
                info.planQty = std::get<int>(row.get("received_quantity"));
                info.actQty = std::get<int>(row.get("actual_quantity"));
                shipDetailMap[p] = info;
            }
        }
        HashMap<int, DetailInfo> outDetailMap;
        for (const auto& row : outDetTable->view("outbound_transaction_id", Value(outTxnId))) {
            if (row.has("product_id") && std::holds_alternative<int>(row.get("product_id"))) {
                int p = std::get<int>(row.get("product_id"));
                DetailInfo info;
                info.detailId = std::get<int>(row.get("outbound_detail_id"));
                info.planQty = std::get<int>(row.get("ordered_quantity"));
                info.actQty = std::get<int>(row.get("actual_quantity"));
                outDetailMap[p] = info;
            }
        }

        for (size_t pi = 0; pi < orderedPids.size(); ++pi) {
            int pid = orderedPids[pi];
            int dispatchCount = static_cast<int>(serialsByPid[pid].size());
            if (!shipDetailMap.contains(pid)) {
                return OperationResult{false, "Sản phẩm ID " + std::to_string(pid) + " không thuộc đợt xuất hàng ID " + std::to_string(shipmentId) + ".", 0};
            }
            const auto& sInfo = shipDetailMap[pid];
            if (sInfo.actQty + dispatchCount > sInfo.planQty) {
                return OperationResult{false, "Số lượng xuất của sản phẩm ID " + std::to_string(pid) + " (" + std::to_string(sInfo.actQty + dispatchCount) +
                    ") vượt quá số lượng ấn định (" + std::to_string(sInfo.planQty) + ") của đợt xuất hàng này.", 0};
            }
            if (!outDetailMap.contains(pid)) {
                return OperationResult{false, "Sản phẩm ID " + std::to_string(pid) + " không thuộc đơn xuất kho gốc.", 0};
            }
            const auto& oInfo = outDetailMap[pid];
            if (oInfo.actQty + dispatchCount > oInfo.planQty) {
                return OperationResult{false, "Số lượng xuất vượt quá tổng số lượng đặt hàng (" + std::to_string(oInfo.planQty) + ") của đơn xuất.", 0};
            }
        }

        if (manageTransaction && !db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu giao dịch CSDL.", 0};
        }

        auto failRollback = [&](const OperationResult& err) -> OperationResult {
            if (manageTransaction) db->rollback();
            return err;
        };

        int totalAffected = 0;

        for (size_t pi = 0; pi < orderedPids.size(); ++pi) {
            int productId = orderedPids[pi];
            const Vector<std::string>& prodSerials = serialsByPid[productId];
            int dispatchCount = static_cast<int>(prodSerials.size());

            int decrementedAvail = 0;
            int decrementedWarr = 0;
            int decrementedScrap = 0;

            for (size_t i = 0; i < prodSerials.size(); ++i) {
                const std::string& sn = prodSerials[i];
                int unitId = -1;
                std::string curStatus = "";

                for (const auto& uRow : unitTable->view("serial", Value(sn))) {
                    if (uRow.has("product_id") && std::holds_alternative<int>(uRow.get("product_id")) &&
                        std::get<int>(uRow.get("product_id")) == productId) {
                        unitId = std::get<int>(uRow.get("unit_id"));
                        curStatus = std::get<std::string>(uRow.get("status"));
                        break;
                    }
                }

                if (unitId == -1) {
                    return failRollback(OperationResult{false, "Không tìm thấy linh kiện có serial '" + sn + "' của sản phẩm ID " + std::to_string(productId) + ".", 0});
                }

                if (outType == "SELLING" || outType == "SEL") {
                    if (curStatus != "AVAILABLE") {
                        return failRollback(OperationResult{false, "Linh kiện serial '" + sn + "' đang có trạng thái '" + schema_i18n::statusVi(curStatus) +
                            "', không thỏa mãn điều kiện 'Khả dụng' của đơn bán hàng.", 0});
                    }
                    decrementedAvail++;
                } else if (outType == "WARRANTY_RETURN" || outType == "WRT") {
                    if (curStatus != "SCRAPPED" && curStatus != "WARRANTY") {
                        return failRollback(OperationResult{false, "Linh kiện serial '" + sn + "' đang có trạng thái '" + schema_i18n::statusVi(curStatus) +
                            "', không thỏa mãn điều kiện 'Lỗi/hỏng' hoặc 'Bảo hành' của đơn gửi bảo hành hãng.", 0});
                    }
                    if (curStatus == "SCRAPPED") decrementedScrap++;
                    else decrementedWarr++;
                } else if (outType == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || outType == "WRC") {
                    if (curStatus != "AVAILABLE" && curStatus != "WARRANTY") {
                        return failRollback(OperationResult{false, "Linh kiện serial '" + sn + "' đang có trạng thái '" + schema_i18n::statusVi(curStatus) +
                            "', không thỏa mãn điều kiện 'Khả dụng' hoặc 'Bảo hành' của đơn đổi bảo hành khách.", 0});
                    }
                    if (curStatus == "AVAILABLE") decrementedAvail++;
                    else decrementedWarr++;
                } else {
                    if (curStatus == "OUT") {
                        return failRollback(OperationResult{false, "Linh kiện serial '" + sn + "' đã ở trạng thái 'Đã xuất'.", 0});
                    }
                    if (curStatus == "AVAILABLE") decrementedAvail++;
                    else if (curStatus == "WARRANTY") decrementedWarr++;
                    else if (curStatus == "SCRAPPED") decrementedScrap++;
                }

                Record uUp;
                uUp.set("status", std::string("OUT"));
                OperationResult rU = db->update("unit_inventory", unitId, uUp);
                if (!rU) return failRollback(rU);
                totalAffected++;

                Record hRec;
                hRec.set("unit_id", unitId);
                hRec.set("old_status", curStatus);
                hRec.set("curr_status", std::string("OUT"));
                hRec.set("transaction", shipmentId);
                hRec.set("type", std::string("OUTBOUND"));

                OperationResult rH = db->insertRecord("status_history", hRec);
                if (!rH) return failRollback(rH);
                totalAffected++;
            }

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
            if (!rPq) return failRollback(rPq);
            totalAffected++;

            const auto& sInfo = shipDetailMap[productId];
            Record sdUp;
            sdUp.set("actual_quantity", sInfo.actQty + dispatchCount);
            OperationResult rSUp = db->update("shipment_detail", sInfo.detailId, sdUp);
            if (!rSUp) return failRollback(rSUp);
            totalAffected++;

            const auto& oInfo = outDetailMap[productId];
            Record odUp;
            odUp.set("actual_quantity", oInfo.actQty + dispatchCount);
            OperationResult rOUp = db->update("outbound_detail", oInfo.detailId, odUp);
            if (!rOUp) return failRollback(rOUp);
            totalAffected++;
        }

        if (shipStatus != "COMPLETED" && shipStatus != "completed") {
            Record shipStatusUp;
            shipStatusUp.set("status", std::string("PICKING"));
            OperationResult rShipSt = db->update("shipment", shipmentId, shipStatusUp);
            if (!rShipSt) return failRollback(rShipSt);
        }

        std::string curOutTxnSt = outTxnRec->has("status") && std::holds_alternative<std::string>(outTxnRec->get("status"))
            ? std::get<std::string>(outTxnRec->get("status")) : "";
        if (curOutTxnSt != "COMPLETED" && curOutTxnSt != "completed") {
            Record outTxnStatusUp;
            outTxnStatusUp.set("status", std::string("PROCESSING"));
            OperationResult rTxnSt = db->update("outbound_transaction", outTxnId, outTxnStatusUp);
            if (!rTxnSt) return failRollback(rTxnSt);
        }

        if (manageTransaction) {
            db->commit();
        }
        return OperationResult{true, "Đã xuất " + std::to_string(entries.size()) + " linh kiện cho đợt xuất " + std::to_string(shipmentId) + " thành công!", totalAffected};
    }

    OperationResult dispatchUnitsForShipmentBatch(
        Database* db,
        int shipmentId,
        const Vector<BatchDispatchUnitEntry>& entries
    ) {
        return dispatchUnitsForShipmentBatchInternal(db, shipmentId, entries, true);
    }

    OperationResult dispatchUnitsForShipment(
        Database* db,
        int shipmentId,
        int productId,
        const Vector<std::string>& serials
    ) {
        if (serials.empty()) return OperationResult{false, "Danh sách serial xuất kho không được để trống.", 0};
        Vector<BatchDispatchUnitEntry> entries;
        entries.reserve(serials.size());
        for (size_t i = 0; i < serials.size(); ++i) {
            BatchDispatchUnitEntry e;
            e.productId = productId;
            e.serial = serials[i];
            entries.push_back(e);
        }
        return dispatchUnitsForShipmentBatch(db, shipmentId, entries);
    }

    OperationResult createAndDispatchAdjustmentOutbound(
        Database* db,
        const std::string& checkCode,
        const std::string& orderDate,
        const Vector<BatchDispatchUnitEntry>& entries,
        const std::string& note
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        if (checkCode.empty()) return OperationResult{false, "Mã đợt kiểm kho tham chiếu không được để trống.", 0};
        if (orderDate.empty()) return OperationResult{false, "Ngày lập đơn xuất điều chỉnh không được để trống.", 0};
        if (entries.empty()) return OperationResult{false, "Danh sách linh kiện cần xuất điều chỉnh không được để trống.", 0};

        const Table* chkTbl = db->getTable("inventory_check");
        const Table* cqTbl = db->getTable("inventory_check_quantity");
        const Table* outTxnTable = db->getTable("outbound_transaction");
        const Table* shipTable = db->getTable("shipment");
        const Table* prodTable = db->getTable("product");
        if (!chkTbl || !cqTbl || !outTxnTable || !shipTable || !prodTable) {
            return OperationResult{false, "Các bảng dữ liệu liên quan đến xuất điều chỉnh không đầy đủ.", 0};
        }

        TableView chkRecs = chkTbl->view("check_code", Value(checkCode));
        if (chkRecs.empty()) {
            return OperationResult{false, "Mã đợt kiểm kho '" + checkCode + "' không tồn tại trong hệ thống.", 0};
        }
        const Record& chkRec = *chkRecs.begin();
        std::string chkSt = chkRec.has("status") && std::holds_alternative<std::string>(chkRec.get("status"))
                          ? std::get<std::string>(chkRec.get("status")) : "";
        if (chkSt != "COMPLETED") {
            return OperationResult{false, "Chỉ được tạo đơn xuất điều chỉnh sau khi đợt kiểm kho [" + checkCode + "] đã hoàn thành.", 0};
        }
        if (!outTxnTable->view("source_code", Value(checkCode)).empty()) {
            return OperationResult{false, "Đợt kiểm kho [" + checkCode + "] đã được tạo đơn xuất điều chỉnh trước đó.", 0};
        }

        int chkId = std::get<int>(chkRec.get("check_id"));
        HashMap<int, bool> allowedPids;
        for (const auto& cq : cqTbl->view("check_id", Value(chkId))) {
            if (cq.has("product_id") && std::holds_alternative<int>(cq.get("product_id"))) {
                allowedPids[std::get<int>(cq.get("product_id"))] = true;
            }
        }

        HashMap<int, int> qtyByPid;
        Vector<int> orderedPids;
        for (size_t i = 0; i < entries.size(); ++i) {
            int pid = entries[i].productId;
            if (pid <= 0 || !prodTable->getRecordById(pid)) {
                return OperationResult{false, "Sản phẩm ID " + std::to_string(pid) + " không tồn tại trong danh mục.", 0};
            }
            if (!allowedPids.contains(pid)) {
                return OperationResult{false, "Sản phẩm [" + getProductCodeStr(db, pid) + "] không thuộc phạm vi đợt kiểm kho " + checkCode + ".", 0};
            }
            if (entries[i].serial.empty()) {
                return OperationResult{false, "Mã serial cần xuất điều chỉnh không được để trống.", 0};
            }
            if (!qtyByPid.contains(pid)) {
                orderedPids.push_back(pid);
                qtyByPid[pid] = 0;
            }
            qtyByPid[pid] = qtyByPid[pid] + 1;
        }

        int conflictPid = 0;
        std::string activeChk = user_query::find_active_inventory_check_for_products(db, orderedPids, conflictPid);
        if (!activeChk.empty()) {
            return OperationResult{false, "Sản phẩm [" + getProductCodeStr(db, conflictPid) + "] đang nằm trong đợt kiểm kê [" + activeChk + "] chưa hoàn tất/hủy.", 0};
        }

        int maxSeq = extractMaxSequenceLocal(outTxnTable, "outbound_code", "OUT-ADJ-", 8);
        int newSeq = maxSeq + 1;
        std::ostringstream outOss;
        outOss << "OUT-ADJ-" << std::setfill('0') << std::setw(6) << newSeq;
        std::string outCode = outOss.str();
        std::string shipCode = outCode + "-S01";

        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu giao dịch CSDL.", 0};
        }

        Record outTxn;
        outTxn.set("outbound_code", outCode);
        outTxn.set("customer_id", Value{});
        outTxn.set("source_code", checkCode);
        outTxn.set("order_at", orderDate);
        outTxn.set("type", std::string("ADJUSTMENT_OUT"));
        outTxn.set("status", std::string("PENDING"));
        outTxn.set("next_ship_no", 2);
        outTxn.set("note", note);

        OperationResult r1 = db->insertRecord("outbound_transaction", outTxn);
        if (!r1) { db->rollback(); return r1; }

        int outTxnId = 0;
        for (const auto& r : outTxnTable->view("outbound_code", Value(outCode))) {
            if (r.has("outbound_trainsaction_id") && std::holds_alternative<int>(r.get("outbound_trainsaction_id"))) {
                outTxnId = std::get<int>(r.get("outbound_trainsaction_id"));
                break;
            } else if (r.has("outbound_transaction_id") && std::holds_alternative<int>(r.get("outbound_transaction_id"))) {
                outTxnId = std::get<int>(r.get("outbound_transaction_id"));
                break;
            }
        }
        if (outTxnId <= 0) {
            db->rollback();
            return OperationResult{false, "Không thể truy xuất ID đơn xuất điều chỉnh vừa tạo.", 0};
        }

        for (size_t i = 0; i < orderedPids.size(); ++i) {
            int pid = orderedPids[i];
            Record outDet;
            outDet.set("outbound_transaction_id", outTxnId);
            outDet.set("product_id", pid);
            outDet.set("ordered_quantity", qtyByPid[pid]);
            outDet.set("actual_quantity", 0);
            OperationResult rd = db->insertRecord("outbound_detail", outDet);
            if (!rd) { db->rollback(); return rd; }
        }

        Record ship;
        ship.set("shipment_code", shipCode);
        ship.set("outbound_transaction_id", outTxnId);
        ship.set("expected_at", orderDate);
        ship.set("ship_at", Value{});
        ship.set("status", std::string("PENDING"));

        OperationResult r2 = db->insertRecord("shipment", ship);
        if (!r2) { db->rollback(); return r2; }

        int shipId = 0;
        for (const auto& r : shipTable->view("shipment_code", Value(shipCode))) {
            if (r.has("shipment_id") && std::holds_alternative<int>(r.get("shipment_id"))) {
                shipId = std::get<int>(r.get("shipment_id"));
                break;
            }
        }
        if (shipId <= 0) {
            db->rollback();
            return OperationResult{false, "Không thể truy xuất ID đợt xuất hàng vừa tạo.", 0};
        }

        for (size_t i = 0; i < orderedPids.size(); ++i) {
            int pid = orderedPids[i];
            Record shipDet;
            shipDet.set("shipment_id", shipId);
            shipDet.set("product_id", pid);
            shipDet.set("received_quantity", qtyByPid[pid]);
            shipDet.set("actual_quantity", 0);
            OperationResult rsd = db->insertRecord("shipment_detail", shipDet);
            if (!rsd) { db->rollback(); return rsd; }
        }

        OperationResult rDisp = dispatchUnitsForShipmentBatchInternal(db, shipId, entries, false);
        if (!rDisp.success) {
            db->rollback();
            return rDisp;
        }

        db->commit();
        return OperationResult{
            true,
            "Đã tạo đơn xuất điều chỉnh [" + outCode + "] và tự động xuất kho thành công " + std::to_string(entries.size()) + " linh kiện!",
            rDisp.affectedCount
        };
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
        const Table* logTable = db->getTable(tbl);
        const Table* unitTable = db->getTable("unit_inventory");
        const Table* histTable = db->getTable("status_history");
        const Table* prodTable = db->getTable("product");
        if (!logTable || !unitTable || !histTable || !prodTable) {
            return OperationResult{false, "Các bảng dữ liệu giao vận hoặc linh kiện không đầy đủ.", 0};
        }

        TableView logMatches = logTable->view(codeCol, Value(logisticsCode));
        if (logMatches.empty()) {
            return OperationResult{false, "Không tìm thấy đợt giao vận với mã " + logisticsCode, 0};
        }
        const Record& logRec = *logMatches.begin();

        // 1. Completion Lock check (Rule 4.5 & Rule 5.6)
        std::string currentLogStatus = "";
        if (logRec.has("status") && std::holds_alternative<std::string>(logRec.get("status"))) {
            currentLogStatus = std::get<std::string>(logRec.get("status"));
        }
        if (currentLogStatus == "COMPLETED" || currentLogStatus == "completed") {
            return OperationResult{false, "Đợt giao nhận này đã ở trạng thái Hoàn thành. Toàn bộ thao tác thay đổi trạng thái linh kiện đã bị khóa vĩnh viễn.", 0};
        }
        if (currentLogStatus == "CANCELLED" || currentLogStatus == "cancelled") {
            return OperationResult{false, "Đợt giao nhận này đã ở trạng thái Đã hủy. Toàn bộ thao tác thay đổi trạng thái linh kiện đã bị khóa vĩnh viễn.", 0};
        }

        // 2. Resolve transaction type and parent status for business rule constraints (Rule 5.6 Logistics Lock)
        std::string txnType = "";
        std::string txnStatus = "";
        if (isInbound) {
            int inTxnId = -1;
            if (logRec.has("inbound_transaction_id") && std::holds_alternative<int>(logRec.get("inbound_transaction_id"))) {
                inTxnId = std::get<int>(logRec.get("inbound_transaction_id"));
            } else if (logRec.has("inbound_id") && std::holds_alternative<int>(logRec.get("inbound_id"))) {
                inTxnId = std::get<int>(logRec.get("inbound_id"));
            }
            const Table* inTxnTable = db->getTable("inbound_transaction");
            if (inTxnTable && inTxnId > 0) {
                const Record* inTxnRec = inTxnTable->getRecordById(inTxnId);
                if (inTxnRec) {
                    if (inTxnRec->has("type") && std::holds_alternative<std::string>(inTxnRec->get("type"))) {
                        txnType = std::get<std::string>(inTxnRec->get("type"));
                    }
                    if (inTxnRec->has("status") && std::holds_alternative<std::string>(inTxnRec->get("status"))) {
                        txnStatus = std::get<std::string>(inTxnRec->get("status"));
                    }
                }
            }
        } else {
            int outTxnId = -1;
            if (logRec.has("outbound_transaction_id") && std::holds_alternative<int>(logRec.get("outbound_transaction_id"))) {
                outTxnId = std::get<int>(logRec.get("outbound_transaction_id"));
            } else if (logRec.has("outbound_trainsaction_id") && std::holds_alternative<int>(logRec.get("outbound_trainsaction_id"))) {
                outTxnId = std::get<int>(logRec.get("outbound_trainsaction_id"));
            } else if (logRec.has("outbound_id") && std::holds_alternative<int>(logRec.get("outbound_id"))) {
                outTxnId = std::get<int>(logRec.get("outbound_id"));
            }
            const Table* outTxnTable = db->getTable("outbound_transaction");
            if (outTxnTable && outTxnId > 0) {
                const Record* outTxnRec = outTxnTable->getRecordById(outTxnId);
                if (outTxnRec) {
                    if (outTxnRec->has("type") && std::holds_alternative<std::string>(outTxnRec->get("type"))) {
                        txnType = std::get<std::string>(outTxnRec->get("type"));
                    }
                    if (outTxnRec->has("status") && std::holds_alternative<std::string>(outTxnRec->get("status"))) {
                        txnStatus = std::get<std::string>(outTxnRec->get("status"));
                    }
                }
            }
        }

        if (txnStatus == "COMPLETED" || txnStatus == "CANCELLED") {
            return OperationResult{false, "Đơn hàng (" + schema_i18n::statusVi(txnStatus) + ") đã hoàn tất hoặc bị hủy. Toàn bộ các đợt vận chuyển liên quan đã bị khóa vĩnh viễn.", 0};
        }

        // 3. Validate permitted target statuses (Rule 4.4, 4.5)
        if (!isInbound) {
            if (newStatus != "OUT") {
                return OperationResult{false, "Đơn xuất hàng chỉ cho phép trạng thái kết thúc là 'Đã xuất'.", 0};
            }
        } else {
            if (newStatus == "OUT") {
                return OperationResult{false, "Đơn nhập kho không cho phép linh kiện có trạng thái 'Đã xuất'.", 0};
            }
            if (txnType == "PURCHASING" || txnType == "PUR") {
                if (newStatus != "AVAILABLE" && newStatus != "SCRAPPED") {
                    return OperationResult{false, "Đơn mua mới chỉ cho phép chuyển đổi giữa 'Khả dụng' và 'Lỗi/hỏng'.", 0};
                }
            } else if (txnType == "WARRANTY_REPLACEMENT" || txnType == "WRP" ||
                       txnType == "WARRANTY_CHECKING" || txnType == "WCK") {
                if (newStatus != "WARRANTY") {
                    return OperationResult{false, "Đơn " + schema_i18n::orderTypeVi(txnType) + " chỉ cho phép trạng thái 'Bảo hành'.", 0};
                }
            } else if (txnType == "CUSTOMER_RETURN" || txnType == "CRT" ||
                       txnType == "ADJUSTMENT_IN" || txnType == "ADJ") {
                if (newStatus != "AVAILABLE" && newStatus != "WARRANTY" && newStatus != "SCRAPPED") {
                    return OperationResult{false, "Đơn " + schema_i18n::orderTypeVi(txnType) + " chỉ cho phép chọn trạng thái 'Khả dụng', 'Bảo hành' hoặc 'Lỗi/hỏng'.", 0};
                }
            }
        }

        // 4. Find unit record
        TableView matchedUnits = unitTable->view("serial", Value(serial));
        if (matchedUnits.empty()) {
            return OperationResult{false, "Không tìm thấy linh kiện với mã serial '" + serial + "' trong kho.", 0};
        }
        const Record& uRec = *matchedUnits.begin();
        int unitId = std::get<int>(uRec.get("unit_id"));
        int productId = std::get<int>(uRec.get("product_id"));
        std::string oldStatus = std::get<std::string>(uRec.get("status"));

        if (oldStatus == newStatus) {
            return OperationResult{true, "Linh kiện " + serial + " hiện tại đã có trạng thái '" + schema_i18n::statusVi(newStatus) + "'.", 0};
        }

        // Stock Freeze Check
        std::string activeCheckCode = user_query::find_active_inventory_check_for_product(db, productId);
        if (!activeCheckCode.empty()) {
            return OperationResult{
                false,
                "Sản phẩm [" + getProductCodeStr(db, productId) + "] đang nằm trong đợt kiểm kê [" + activeCheckCode +
                "] chưa hoàn tất/hủy. Tạm khóa cập nhật trạng thái linh kiện cho tới khi đợt kiểm kê kết thúc.",
                0
            };
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
        int logPk = isInbound ? std::get<int>(logRec.get("delivery_id")) : std::get<int>(logRec.get("shipment_id"));

        for (const auto& hRow : histTable->view("transaction", Value(logPk))) {
            if (hRow.has("unit_id") && std::holds_alternative<int>(hRow.get("unit_id"))) {
                if (std::get<int>(hRow.get("unit_id")) == unitId) {
                    existingHistId = (hRow.has("history_id") && std::holds_alternative<int>(hRow.get("history_id")))
                                   ? std::get<int>(hRow.get("history_id")) : -1;
                    break;
                }
            }
        }

        if (existingHistId > 0) {
            Record hUp;
            hUp.set("curr_status", newStatus);
            OperationResult rH = db->update("status_history", existingHistId, hUp);
            if (!rH) { db->rollback(); return rH; }
        } else {
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
        return OperationResult{true, "Đã cập nhật trạng thái linh kiện " + serial + " từ '" + schema_i18n::statusVi(oldStatus) + "' sang '" + schema_i18n::statusVi(newStatus) + "' thành công!", 3};
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
        const Table* cqTbl = db->getTable("inventory_check_quantity");
        if (!cqTbl) return OperationResult{false, "Bảng inventory_check_quantity không tồn tại.", 0};

        const Record* cqRec = cqTbl->getRecordById(checkQuantityId);
        if (!cqRec) return OperationResult{false, "Không tìm thấy bản ghi số lượng kiểm kho với ID " + std::to_string(checkQuantityId) + ".", 0};

        Record up;
        up.set("actual_available_quan", std::max(0, actualAvail));
        up.set("actual_warranty_quan", std::max(0, actualWarr));
        up.set("actual_scrapped_quan", std::max(0, actualScrap));
        up.set("note", note);

        OperationResult res = db->update("inventory_check_quantity", checkQuantityId, up);
        if (res.success) {
            int checkId = cqRec->has("check_id") && std::holds_alternative<int>(cqRec->get("check_id")) ? std::get<int>(cqRec->get("check_id")) : 0;
            const Table* chkTbl = db->getTable("inventory_check");
            if (chkTbl && checkId > 0) {
                const Record* chk = chkTbl->getRecordById(checkId);
                if (chk && chk->has("status") && std::holds_alternative<std::string>(chk->get("status")) && std::get<std::string>(chk->get("status")) == "PENDING") {
                    Record chkUp;
                    chkUp.set("status", std::string("PROCESSING"));
                    db->update("inventory_check", checkId, chkUp);
                }
            }
        }
        return res;
    }

    OperationResult updateInventoryCheckUnit(
        Database* db,
        int checkUnitId,
        const std::string& scannedStatus
    ) {
        if (!db || checkUnitId <= 0) return OperationResult{false, "Tham số không hợp lệ.", 0};
        const Table* cuTbl = db->getTable("inventory_check_unit");
        const Table* cqTbl = db->getTable("inventory_check_quantity");
        if (!cuTbl || !cqTbl) return OperationResult{false, "Bảng kiểm kê không tồn tại.", 0};

        const Record* cuRec = cuTbl->getRecordById(checkUnitId);
        if (!cuRec) return OperationResult{false, "Không tìm thấy linh kiện kiểm kê với ID " + std::to_string(checkUnitId) + ".", 0};

        int cqId = std::get<int>(cuRec->get("check_quantity_id"));

        if (!db->beginTransaction()) return OperationResult{false, "Không thể bắt đầu giao dịch CSDL.", 0};

        Record cuUp;
        cuUp.set("scanned_status", scannedStatus);
        OperationResult rCu = db->update("inventory_check_unit", checkUnitId, cuUp);
        if (!rCu) { db->rollback(); return rCu; }

        // Recalculate actual counts for this check_quantity
        int actAvail = 0, actWarr = 0, actScrap = 0;
        for (const auto& u : cuTbl->view("check_quantity_id", Value(cqId))) {
            int uPk = std::get<int>(u.get("check_unit_id"));
            std::string st = (uPk == checkUnitId) ? scannedStatus :
                ((u.has("scanned_status") && std::holds_alternative<std::string>(u.get("scanned_status")))
                    ? std::get<std::string>(u.get("scanned_status")) : "");
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

        int checkId = cuRec->has("check_id") && std::holds_alternative<int>(cuRec->get("check_id")) ? std::get<int>(cuRec->get("check_id")) : 0;
        const Table* chkTbl = db->getTable("inventory_check");
        if (chkTbl && checkId > 0) {
            const Record* chk = chkTbl->getRecordById(checkId);
            if (chk && chk->has("status") && std::holds_alternative<std::string>(chk->get("status")) && std::get<std::string>(chk->get("status")) == "PENDING") {
                Record chkUp;
                chkUp.set("status", std::string("PROCESSING"));
                db->update("inventory_check", checkId, chkUp);
            }
        }

        db->commit();
        return OperationResult{true, "Cập nhật trạng thái quét linh kiện và đồng bộ số lượng kiểm kho thành công!", 2};
    }

    OperationResult recordSurplusCheckUnit(
        Database* db,
        int checkId,
        int checkQuantityId,
        const std::string& serial,
        const std::string& systemStatus,
        const std::string& scannedStatus
    ) {
        if (!db || checkId <= 0 || checkQuantityId <= 0 || serial.empty()) {
            return OperationResult{false, "Tham số không hợp lệ.", 0};
        }
        const Table* cuTbl = db->getTable("inventory_check_unit");
        const Table* cqTbl = db->getTable("inventory_check_quantity");
        if (!cuTbl || !cqTbl) return OperationResult{false, "Bảng kiểm kê không tồn tại.", 0};

        // Format check
        if (!user_create::isValidSerialFormat(serial)) {
            return OperationResult{false, "Mã serial '" + serial + "' không đúng định dạng chuẩn SN-XXXXXXX.", 0};
        }

        // Composite uniqueness check: cannot have duplicate (check_id, serial)
        // O(1) lookup via hash tree index on serial
        for (const auto& cu : cuTbl->view("serial", Value(serial))) {
            if (cu.has("check_id") && std::holds_alternative<int>(cu.get("check_id")) &&
                std::get<int>(cu.get("check_id")) == checkId) {
                return OperationResult{false, "Mã serial '" + serial + "' đã tồn tại trong đợt kiểm kê này rồi.", 0};
            }
        }

        if (!db->beginTransaction()) return OperationResult{false, "Không thể bắt đầu giao dịch CSDL.", 0};

        Record cuRec;
        cuRec.set("check_id", checkId);
        cuRec.set("check_quantity_id", checkQuantityId);
        cuRec.set("serial", serial);
        if (systemStatus.empty()) {
            cuRec.set("system_status", Value{}); // NULL for brand new unrecorded unit
        } else {
            cuRec.set("system_status", systemStatus);
        }
        cuRec.set("scanned_status", scannedStatus);

        OperationResult rIns = db->insertRecord("inventory_check_unit", cuRec);
        if (!rIns) { db->rollback(); return rIns; }

        // Recalculate actual counts for this check_quantity
        int actAvail = 0, actWarr = 0, actScrap = 0;
        for (const auto& u : cuTbl->view("check_quantity_id", Value(checkQuantityId))) {
            std::string st = (u.has("scanned_status") && std::holds_alternative<std::string>(u.get("scanned_status")))
                                ? std::get<std::string>(u.get("scanned_status")) : "";
            if (st == "AVAILABLE") actAvail++;
            else if (st == "WARRANTY") actWarr++;
            else if (st == "SCRAPPED") actScrap++;
        }

        Record cqUp;
        cqUp.set("actual_available_quan", actAvail);
        cqUp.set("actual_warranty_quan", actWarr);
        cqUp.set("actual_scrapped_quan", actScrap);

        OperationResult rCq = db->update("inventory_check_quantity", checkQuantityId, cqUp);
        if (!rCq) { db->rollback(); return rCq; }

        const Table* chkTbl = db->getTable("inventory_check");
        if (chkTbl) {
            const Record* chk = chkTbl->getRecordById(checkId);
            if (chk && chk->has("status") && std::holds_alternative<std::string>(chk->get("status")) && std::get<std::string>(chk->get("status")) == "PENDING") {
                Record chkUp;
                chkUp.set("status", std::string("PROCESSING"));
                db->update("inventory_check", checkId, chkUp);
            }
        }

        db->commit();
        return OperationResult{true, "Ghi nhận linh kiện thừa vào đợt kiểm kho thành công!", 2};
    }

    OperationResult saveInventoryCheckBatch(
        Database* db,
        int checkId,
        const Vector<BatchCheckUnitUpdate>& modifiedUnits,
        const Vector<BatchCheckUnitSurplus>& surplusUnits
    ) {
        if (!db || checkId <= 0) return OperationResult{false, "Tham số không hợp lệ.", 0};
        if (modifiedUnits.empty() && surplusUnits.empty()) {
            return OperationResult{true, "Không có dữ liệu thay đổi cần lưu.", 0};
        }

        const Table* chkTbl = db->getTable("inventory_check");
        const Table* cqTbl = db->getTable("inventory_check_quantity");
        const Table* cuTbl = db->getTable("inventory_check_unit");
        if (!chkTbl || !cqTbl || !cuTbl) {
            return OperationResult{false, "Các bảng dữ liệu kiểm kê không tồn tại.", 0};
        }

        const Record* chkRec = chkTbl->getRecordById(checkId);
        if (!chkRec) {
            return OperationResult{false, "Không tìm thấy đợt kiểm kê với ID " + std::to_string(checkId) + ".", 0};
        }
        std::string checkSt = chkRec->has("status") && std::holds_alternative<std::string>(chkRec->get("status"))
            ? std::get<std::string>(chkRec->get("status")) : "";
        if (checkSt == "COMPLETED") {
            return OperationResult{false, "Đợt kiểm kê đã Hoàn thành, không thể chỉnh sửa dữ liệu.", 0};
        }
        if (checkSt == "CANCELLED") {
            return OperationResult{false, "Đợt kiểm kê đã bị Hủy, không thể chỉnh sửa dữ liệu.", 0};
        }

        // Validate serials for surplus units upfront
        for (size_t i = 0; i < surplusUnits.size(); ++i) {
            const auto& s = surplusUnits[i];
            if (!user_create::isValidSerialFormat(s.serial)) {
                return OperationResult{false, "Mã serial '" + s.serial + "' không đúng định dạng chuẩn SN-XXXXXXX.", 0};
            }
        }

        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu giao dịch CSDL.", 0};
        }

        // Track affected check_quantity_ids using custom HashMap
        HashMap<int, bool> affectedCqIds;
        int totalAffected = 0;

        // 1. Process modified units
        for (size_t i = 0; i < modifiedUnits.size(); ++i) {
            const auto& m = modifiedUnits[i];
            const Record* r = cuTbl->getRecordById(m.checkUnitId);
            if (!r) continue;

            if (r->has("check_quantity_id") && std::holds_alternative<int>(r->get("check_quantity_id"))) {
                affectedCqIds[std::get<int>(r->get("check_quantity_id"))] = true;
            }

            Record updatedRec;
            updatedRec.set("scanned_status", m.scannedStatus);
            if (db->update("inventory_check_unit", m.checkUnitId, updatedRec).success) {
                totalAffected++;
            }
        }

        // 2. Process surplus units
        for (size_t i = 0; i < surplusUnits.size(); ++i) {
            const auto& s = surplusUnits[i];
            affectedCqIds[s.checkQuantityId] = true;

            Record cuRec;
            cuRec.set("check_id", checkId);
            cuRec.set("check_quantity_id", s.checkQuantityId);
            cuRec.set("serial", s.serial);
            if (s.systemStatus.empty()) {
                cuRec.set("system_status", Value{}); // NULL for brand new unrecorded unit
            } else {
                cuRec.set("system_status", s.systemStatus);
            }
            cuRec.set("scanned_status", s.scannedStatus);

            if (db->insert("inventory_check_unit", cuRec)) {
                totalAffected++;
            }
        }

        // 3. Recalculate actual counts for affected check_quantities in a single pass using custom HashMap
        struct QuanCount {
            int avail = 0;
            int warr = 0;
            int scrap = 0;
        };
        HashMap<int, QuanCount> counts;
        Vector<int> affectedList = affectedCqIds.getKeys();
        for (size_t i = 0; i < affectedList.size(); ++i) {
            counts[affectedList[i]] = QuanCount{0, 0, 0};
        }

        // Optimize: scan only the units belonging to affected check_quantity_id (FK index lookup)
        for (size_t i = 0; i < affectedList.size(); ++i) {
            int cqId = affectedList[i];
            for (const auto& u : cuTbl->view("check_quantity_id", Value(cqId))) {
                std::string st = (u.has("scanned_status") && std::holds_alternative<std::string>(u.get("scanned_status")))
                                    ? std::get<std::string>(u.get("scanned_status")) : "";
                if (st == "AVAILABLE") counts[cqId].avail++;
                else if (st == "WARRANTY") counts[cqId].warr++;
                else if (st == "SCRAPPED") counts[cqId].scrap++;
            }
        }

        for (size_t i = 0; i < affectedList.size(); ++i) {
            int cqId = affectedList[i];
            const Record* cqRec = cqTbl->getRecordById(cqId);
            if (!cqRec) continue;
            const auto& c = counts[cqId];
            Record updatedCq;
            updatedCq.set("actual_available_quan", c.avail);
            updatedCq.set("actual_warranty_quan", c.warr);
            updatedCq.set("actual_scrapped_quan", c.scrap);
            if (db->update("inventory_check_quantity", cqId, updatedCq).success) {
                totalAffected++;
            }
        }

        // 4. Update check status to PROCESSING if it was PENDING
        if (checkSt == "PENDING") {
            const Record* chkRec = chkTbl->getRecordById(checkId);
            if (chkRec) {
                Record updatedChk = *chkRec;
                updatedChk.set("status", std::string("PROCESSING"));
                db->update("inventory_check", checkId, updatedChk);
            }
        }

        if (db->isInTransaction()) {
            db->commit();
        }
        return OperationResult{true, "Đã lưu thành công dữ liệu kiểm kê vào CSDL!", totalAffected};
    }

    OperationResult cancelInventoryCheck(
        Database* db,
        int checkId
    ) {
        if (!db || checkId <= 0) return OperationResult{false, "Tham số không hợp lệ.", 0};
        const Table* chkTbl = db->getTable("inventory_check");
        if (!chkTbl) return OperationResult{false, "Bảng inventory_check không tồn tại.", 0};

        const Record* chkRec = chkTbl->getRecordById(checkId);
        if (!chkRec) return OperationResult{false, "Không tìm thấy đợt kiểm kho với ID " + std::to_string(checkId) + ".", 0};

        std::string curSt = chkRec->has("status") && std::holds_alternative<std::string>(chkRec->get("status"))
            ? std::get<std::string>(chkRec->get("status")) : "";
        if (curSt == "COMPLETED" || curSt == "completed") {
            return OperationResult{false, "Đợt kiểm kê đã hoàn thành, không thể hủy.", 0};
        }
        if (curSt == "CANCELLED" || curSt == "cancelled") {
            return OperationResult{false, "Đợt kiểm kê đã ở trạng thái Đã hủy.", 0};
        }
        if (curSt != "PENDING") {
            return OperationResult{false, "Chỉ được phép hủy đợt kiểm kê khi đang ở trạng thái Chờ xử lí.", 0};
        }
        if (hasInventoryCheckScannedProgress(db, checkId)) {
            return OperationResult{false, "Không thể hủy đợt kiểm kê: Đã có linh kiện hoặc số lượng kiểm đếm được ghi nhận.", 0};
        }

        Record up;
        up.set("status", std::string("CANCELLED"));
        return db->update("inventory_check", checkId, up);
    }

    OperationResult completeInventoryCheck(
        Database* db,
        int checkId
    ) {
        if (!db || checkId <= 0) return OperationResult{false, "Tham số không hợp lệ.", 0};
        const Table* chkTbl = db->getTable("inventory_check");
        if (!chkTbl) return OperationResult{false, "Bảng inventory_check không tồn tại.", 0};

        const Record* chkRec = chkTbl->getRecordById(checkId);
        if (!chkRec) return OperationResult{false, "Không tìm thấy đợt kiểm kho với ID " + std::to_string(checkId) + ".", 0};

        std::string curSt = chkRec->has("status") && std::holds_alternative<std::string>(chkRec->get("status"))
            ? std::get<std::string>(chkRec->get("status")) : "";
        if (curSt == "COMPLETED" || curSt == "completed") {
            return OperationResult{false, "Đợt kiểm kê này đã ở trạng thái Hoàn thành.", 0};
        }
        if (curSt == "CANCELLED" || curSt == "cancelled") {
            return OperationResult{false, "Đợt kiểm kê đã bị hủy, không thể chuyển sang Hoàn thành.", 0};
        }

        Record up;
        up.set("status", std::string("COMPLETED"));
        return db->update("inventory_check", checkId, up);
    }

    OperationResult executeAdjustmentMovement(
        Database* db,
        int adjustId
    ) {
        if (!db || adjustId <= 0) return OperationResult{false, "Tham số không hợp lệ.", 0};
        const Table* adjTbl = db->getTable("adjustment_movement");
        const Table* detTbl = db->getTable("adjust_movement_detail");
        const Table* prodTbl = db->getTable("product");
        const Table* unitTbl = db->getTable("unit_inventory");
        const Table* histTbl = db->getTable("status_history");

        if (!adjTbl || !detTbl || !prodTbl || !unitTbl || !histTbl) {
            return OperationResult{false, "Các bảng dữ liệu điều chuyển không đầy đủ.", 0};
        }

        const Record* adjRec = adjTbl->getRecordById(adjustId);
        if (!adjRec) return OperationResult{false, "Không tìm thấy phiếu điều chuyển với ID " + std::to_string(adjustId) + ".", 0};

        std::string adjStatus = std::get<std::string>(adjRec->get("status"));
        if (adjStatus == "COMPLETED" || adjStatus == "CANCELLED") {
            return OperationResult{false, "Phiếu điều chuyển này đã ở trạng thái " + schema_i18n::statusVi(adjStatus) + ".", 0};
        }

        TableView details = detTbl->view("adjust_id", Value(adjustId));
        if (details.empty()) {
            return OperationResult{false, "Phiếu điều chuyển không có mặt hàng chi tiết nào.", 0};
        }

        Vector<int> pidsToMove;
        for (const auto& det : details) {
            pidsToMove.push_back(std::get<int>(det.get("product_id")));
        }
        int conflictPid = 0;
        std::string activeCheckCode = user_query::find_active_inventory_check_for_products(db, pidsToMove, conflictPid);
        if (!activeCheckCode.empty()) {
            return OperationResult{
                false,
                "Sản phẩm [" + getProductCodeStr(db, conflictPid) + "] đang nằm trong đợt kiểm kê [" + activeCheckCode +
                "] chưa hoàn tất/hủy. Tạm khóa điều chuyển trạng thái cho tới khi đợt kiểm kê kết thúc.",
                0
            };
        }

        if (!db->beginTransaction()) return OperationResult{false, "Không thể bắt đầu giao dịch CSDL.", 0};

        std::string sourceCode = (adjRec->has("source_code") && std::holds_alternative<std::string>(adjRec->get("source_code")))
            ? std::get<std::string>(adjRec->get("source_code")) : "";
        int checkId = 0;
        const Table* chkTbl = db->getTable("inventory_check");
        const Table* chkUnitTbl = db->getTable("inventory_check_unit");
        if (chkTbl && !sourceCode.empty()) {
            for (const auto& c : chkTbl->view("check_code", Value(sourceCode))) {
                if (c.has("check_id") && std::holds_alternative<int>(c.get("check_id"))) {
                    checkId = std::get<int>(c.get("check_id"));
                }
                break;
            }
        }

        int totalAffected = 0;
        for (const auto& det : details) {
            int detId = std::get<int>(det.get("adjust_detail_id"));
            int pid = std::get<int>(det.get("product_id"));
            std::string fromSt = std::get<std::string>(det.get("from_status"));
            std::string toSt = std::get<std::string>(det.get("to_status"));
            int reqQty = std::get<int>(det.get("adjust_quantity"));

            // Prioritize units recorded with this exact status shift in inventory_check_unit (CHK-UNIT)
            Vector<Record> candidates;
            HashMap<int, bool> pickedUnitIds;

            if (checkId > 0 && chkUnitTbl) {
                for (const auto& cu : chkUnitTbl->view("check_id", Value(checkId))) {
                    std::string cuSys = cu.has("system_status") && std::holds_alternative<std::string>(cu.get("system_status")) ? std::get<std::string>(cu.get("system_status")) : "";
                    std::string cuScan = cu.has("scanned_status") && std::holds_alternative<std::string>(cu.get("scanned_status")) ? std::get<std::string>(cu.get("scanned_status")) : "";
                    std::string sn = cu.has("serial") && std::holds_alternative<std::string>(cu.get("serial")) ? std::get<std::string>(cu.get("serial")) : "";
                    if (cuSys == fromSt && cuScan == toSt && !sn.empty()) {
                        for (const auto& uRec : unitTbl->view("serial", Value(sn))) {
                            int uPid = uRec.has("product_id") && std::holds_alternative<int>(uRec.get("product_id")) ? std::get<int>(uRec.get("product_id")) : 0;
                            std::string uSt = uRec.has("status") && std::holds_alternative<std::string>(uRec.get("status")) ? std::get<std::string>(uRec.get("status")) : "";
                            int uId = std::get<int>(uRec.get("unit_id"));
                            if (uPid == pid && uSt == fromSt && !pickedUnitIds.contains(uId)) {
                                candidates.push_back(uRec);
                                pickedUnitIds[uId] = true;
                                if (static_cast<int>(candidates.size()) == reqQty) break;
                            }
                            break;
                        }
                    }
                }
            }

            // Fill remainder if needed from unit_inventory
            if (static_cast<int>(candidates.size()) < reqQty) {
                for (const auto& u : unitTbl->view("product_id", Value(pid))) {
                    int uId = std::get<int>(u.get("unit_id"));
                    if (!pickedUnitIds.contains(uId) && std::get<std::string>(u.get("status")) == fromSt) {
                        candidates.push_back(u);
                        pickedUnitIds[uId] = true;
                        if (static_cast<int>(candidates.size()) == reqQty) break;
                    }
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
                Record hRec;
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

    static OperationResult executeAdjustmentMovementUnitsInternal(
        Database* db,
        int adjustId,
        const Vector<AdjustUnitEntry>& stagedUnits,
        bool manageTransaction
    ) {
        if (!db || adjustId <= 0 || stagedUnits.empty()) {
            return OperationResult{false, "Danh sách linh kiện điều chuyển trống hoặc tham số không hợp lệ.", 0};
        }
        const Table* adjTbl = db->getTable("adjustment_movement");
        const Table* detTbl = db->getTable("adjust_movement_detail");
        const Table* prodTbl = db->getTable("product");
        const Table* unitTbl = db->getTable("unit_inventory");
        const Table* histTbl = db->getTable("status_history");

        if (!adjTbl || !detTbl || !prodTbl || !unitTbl || !histTbl) {
            return OperationResult{false, "Các bảng dữ liệu điều chuyển không đầy đủ.", 0};
        }

        const Record* adjRec = adjTbl->getRecordById(adjustId);
        if (!adjRec) return OperationResult{false, "Không tìm thấy phiếu điều chuyển ID " + std::to_string(adjustId), 0};

        std::string adjStatus = std::get<std::string>(adjRec->get("status"));
        if (adjStatus == "COMPLETED" || adjStatus == "CANCELLED") {
            return OperationResult{false, "Phiếu điều chuyển đã ở trạng thái " + schema_i18n::statusVi(adjStatus) + ", không thể thực hiện.", 0};
        }

        // Group staged units by detailId
        HashMap<int, Vector<std::string>> unitsByDetail;
        for (size_t i = 0; i < stagedUnits.size(); ++i) {
            unitsByDetail[stagedUnits[i].adjustDetailId].push_back(stagedUnits[i].serial);
        }

        TableView allDetails = detTbl->view("adjust_id", Value(adjustId));
        Vector<int> affectedPids;
        for (const auto& det : allDetails) {
            int detId = std::get<int>(det.get("adjust_detail_id"));
            if (!unitsByDetail.contains(detId)) continue;
            affectedPids.push_back(std::get<int>(det.get("product_id")));
        }

        int conflictPid = 0;
        std::string activeCheckCode = user_query::find_active_inventory_check_for_products(db, affectedPids, conflictPid);
        if (!activeCheckCode.empty()) {
            return OperationResult{
                false,
                "Sản phẩm [" + getProductCodeStr(db, conflictPid) + "] đang nằm trong đợt kiểm kê [" + activeCheckCode +
                "] chưa hoàn tất/hủy. Tạm khóa điều chuyển trạng thái cho tới khi đợt kiểm kê kết thúc.",
                0
            };
        }

        if (manageTransaction && !db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu giao dịch CSDL.", 0};
        }

        auto failRollback = [&](const OperationResult& err) -> OperationResult {
            if (manageTransaction) db->rollback();
            return err;
        };

        int totalAffected = 0;

        for (const auto& det : allDetails) {
            int detId = std::get<int>(det.get("adjust_detail_id"));
            if (!unitsByDetail.contains(detId)) continue;

            const auto& serials = unitsByDetail[detId];
            int pid = std::get<int>(det.get("product_id"));
            std::string fromSt = std::get<std::string>(det.get("from_status"));
            std::string toSt = std::get<std::string>(det.get("to_status"));
            int adjQty = std::get<int>(det.get("adjust_quantity"));
            int curAct = det.has("actual_quantity") && std::holds_alternative<int>(det.get("actual_quantity"))
                         ? std::get<int>(det.get("actual_quantity")) : 0;

            if (curAct + static_cast<int>(serials.size()) > adjQty) {
                return failRollback(OperationResult{false, "Số lượng linh kiện chuyển vượt quá kế hoạch đã khai báo cho sản phẩm ID " + std::to_string(pid), 0});
            }

            int movedForLine = 0;
            for (size_t k = 0; k < serials.size(); ++k) {
                const std::string& sn = serials[k];
                int uId = -1;
                int uPid = -1;
                std::string uSt = "";
                for (const auto& u : unitTbl->view("serial", Value(sn))) {
                    uId = std::get<int>(u.get("unit_id"));
                    uPid = std::get<int>(u.get("product_id"));
                    uSt = std::get<std::string>(u.get("status"));
                    break;
                }
                if (uId == -1) {
                    return failRollback(OperationResult{false, "Không tìm thấy linh kiện có mã serial " + sn, 0});
                }

                if (uPid != pid) {
                    return failRollback(OperationResult{false, "Linh kiện " + sn + " không thuộc sản phẩm ID " + std::to_string(pid), 0});
                }
                if (uSt != fromSt) {
                    return failRollback(OperationResult{false, "Linh kiện " + sn + " hiện có trạng thái '" + schema_i18n::statusVi(uSt) + "' thay vì '" + schema_i18n::statusVi(fromSt) + "'.", 0});
                }

                Record uUp;
                uUp.set("status", toSt);
                OperationResult rU = db->update("unit_inventory", uId, uUp);
                if (!rU) return failRollback(rU);

                Record hRec;
                hRec.set("unit_id", uId);
                hRec.set("old_status", fromSt);
                hRec.set("curr_status", toSt);
                hRec.set("transaction", adjustId);
                hRec.set("type", std::string("ADJUSTMENT"));
                OperationResult rH = db->insertRecord("status_history", hRec);
                if (!rH) return failRollback(rH);

                movedForLine++;
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

                if (fromSt == "AVAILABLE") avail = std::max(0, avail - movedForLine);
                else if (fromSt == "WARRANTY") warr = std::max(0, warr - movedForLine);
                else if (fromSt == "SCRAPPED") scrap = std::max(0, scrap - movedForLine);

                if (toSt == "AVAILABLE") avail += movedForLine;
                else if (toSt == "WARRANTY") warr += movedForLine;
                else if (toSt == "SCRAPPED") scrap += movedForLine;

                Record pUp;
                pUp.set("available_quantity", avail);
                pUp.set("warranty_quantity", warr);
                pUp.set("scrapped_quantity", scrap);
                OperationResult rP = db->update("product", pid, pUp);
                if (!rP) return failRollback(rP);
                totalAffected++;
            }

            Record detUp;
            detUp.set("actual_quantity", curAct + movedForLine);
            OperationResult rDetUp = db->update("adjust_movement_detail", detId, detUp);
            if (!rDetUp) return failRollback(rDetUp);
        }

        // Refresh detail records to check completion
        TableView updatedDetails = detTbl->view("adjust_id", Value(adjustId));
        bool allCompleted = true;
        for (const auto& det : updatedDetails) {
            int adjQ = std::get<int>(det.get("adjust_quantity"));
            int actQ = det.has("actual_quantity") && std::holds_alternative<int>(det.get("actual_quantity"))
                       ? std::get<int>(det.get("actual_quantity")) : 0;
            if (actQ < adjQ) {
                allCompleted = false;
                break;
            }
        }

        Record adjUp;
        adjUp.set("status", std::string(allCompleted ? "COMPLETED" : "PROCESSING"));
        OperationResult rAdj = db->update("adjustment_movement", adjustId, adjUp);
        if (!rAdj) return failRollback(rAdj);
        totalAffected++;

        if (manageTransaction) {
            db->commit();
        }
        return OperationResult{true, "Đã điều chuyển trạng thái cho " + std::to_string(stagedUnits.size()) + " linh kiện thành công!", totalAffected};
    }

    OperationResult executeAdjustmentMovementUnits(
        Database* db,
        int adjustId,
        const Vector<AdjustUnitEntry>& stagedUnits
    ) {
        return executeAdjustmentMovementUnitsInternal(db, adjustId, stagedUnits, true);
    }

    OperationResult createAndExecuteAdjustmentMovement(
        Database* db,
        const std::string& checkCode,
        const std::string& adjustedAt,
        const Vector<AdjustMovementUnitInput>& entries,
        const std::string& note
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        if (checkCode.empty()) return OperationResult{false, "Mã đợt kiểm kho tham chiếu không được để trống.", 0};
        if (adjustedAt.empty()) return OperationResult{false, "Ngày điều chuyển không được để trống.", 0};
        if (entries.empty()) return OperationResult{false, "Danh sách linh kiện cần điều chuyển trạng thái không được để trống.", 0};

        const Table* chkTbl = db->getTable("inventory_check");
        const Table* cqTbl = db->getTable("inventory_check_quantity");
        const Table* adjTbl = db->getTable("adjustment_movement");
        const Table* detTbl = db->getTable("adjust_movement_detail");
        const Table* prodTbl = db->getTable("product");
        if (!chkTbl || !cqTbl || !adjTbl || !detTbl || !prodTbl) {
            return OperationResult{false, "Các bảng dữ liệu liên quan đến điều chuyển không đầy đủ.", 0};
        }

        TableView chkRecs = chkTbl->view("check_code", Value(checkCode));
        if (chkRecs.empty()) {
            return OperationResult{false, "Mã đợt kiểm kho '" + checkCode + "' không tồn tại trong hệ thống.", 0};
        }
        const Record& chkRec = *chkRecs.begin();
        std::string chkSt = chkRec.has("status") && std::holds_alternative<std::string>(chkRec.get("status"))
                          ? std::get<std::string>(chkRec.get("status")) : "";
        if (chkSt != "COMPLETED") {
            return OperationResult{false, "Chỉ được tạo phiếu điều chuyển sau khi đợt kiểm kho [" + checkCode + "] đã hoàn thành.", 0};
        }
        if (!adjTbl->view("source_code", Value(checkCode)).empty()) {
            return OperationResult{false, "Đợt kiểm kho [" + checkCode + "] đã được tạo phiếu điều chuyển trước đó.", 0};
        }

        int chkId = std::get<int>(chkRec.get("check_id"));
        HashMap<int, bool> allowedPids;
        for (const auto& cq : cqTbl->view("check_id", Value(chkId))) {
            if (cq.has("product_id") && std::holds_alternative<int>(cq.get("product_id"))) {
                allowedPids[std::get<int>(cq.get("product_id"))] = true;
            }
        }

        struct MovGroup {
            int productId = 0;
            std::string fromStatus;
            std::string toStatus;
            Vector<std::string> serials;
        };
        Vector<MovGroup> groups;
        HashMap<std::string, size_t> keyToGroupIdx;
        Vector<int> orderedPids;
        HashMap<int, bool> seenPids;

        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& ent = entries[i];
            if (ent.productId <= 0 || !prodTbl->getRecordById(ent.productId)) {
                return OperationResult{false, "Sản phẩm ID " + std::to_string(ent.productId) + " không tồn tại trong danh mục.", 0};
            }
            if (!allowedPids.contains(ent.productId)) {
                return OperationResult{false, "Sản phẩm [" + getProductCodeStr(db, ent.productId) + "] không thuộc phạm vi đợt kiểm kho " + checkCode + ".", 0};
            }
            if (ent.fromStatus == ent.toStatus) {
                return OperationResult{false, "Trạng thái nguồn và trạng thái đích không được trùng nhau.", 0};
            }
            if (ent.fromStatus != "AVAILABLE" && ent.fromStatus != "WARRANTY" && ent.fromStatus != "SCRAPPED") {
                return OperationResult{false, "Trạng thái nguồn '" + schema_i18n::statusVi(ent.fromStatus) + "' không hợp lệ.", 0};
            }
            if (ent.toStatus != "AVAILABLE" && ent.toStatus != "WARRANTY" && ent.toStatus != "SCRAPPED") {
                return OperationResult{false, "Trạng thái đích '" + schema_i18n::statusVi(ent.toStatus) + "' không hợp lệ.", 0};
            }
            if (ent.serial.empty()) {
                return OperationResult{false, "Mã serial cần điều chuyển không được để trống.", 0};
            }

            if (!seenPids.contains(ent.productId)) {
                seenPids[ent.productId] = true;
                orderedPids.push_back(ent.productId);
            }

            std::string gKey = std::to_string(ent.productId) + "|" + ent.fromStatus + "|" + ent.toStatus;
            if (!keyToGroupIdx.contains(gKey)) {
                MovGroup g;
                g.productId = ent.productId;
                g.fromStatus = ent.fromStatus;
                g.toStatus = ent.toStatus;
                keyToGroupIdx[gKey] = groups.size();
                groups.push_back(g);
            }
            groups[keyToGroupIdx[gKey]].serials.push_back(ent.serial);
        }

        int conflictPid = 0;
        std::string activeChk = user_query::find_active_inventory_check_for_products(db, orderedPids, conflictPid);
        if (!activeChk.empty()) {
            return OperationResult{false, "Sản phẩm [" + getProductCodeStr(db, conflictPid) + "] đang nằm trong đợt kiểm kê [" + activeChk + "] chưa hoàn tất/hủy.", 0};
        }

        std::string adjCode = user_create::generateUniqueAdjustCode(db, "STOCKTAKE_DIFF");

        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu giao dịch CSDL.", 0};
        }

        Record adjRec;
        adjRec.set("adjust_code", adjCode);
        adjRec.set("source_code", checkCode);
        adjRec.set("reason", std::string("STOCKTAKE_DIFF"));
        adjRec.set("adjusted_at", adjustedAt);
        adjRec.set("status", std::string("PENDING"));
        adjRec.set("note", note);

        OperationResult rAdj = db->insertRecord("adjustment_movement", adjRec);
        if (!rAdj) { db->rollback(); return rAdj; }

        auto adjView = adjTbl->view("adjust_code", Value(adjCode));
        if (adjView.empty()) { db->rollback(); return OperationResult{false, "Không thể tìm lại phiếu điều chuyển vừa tạo.", 0}; }
        int adjId = std::get<int>((*adjView.begin()).get("adjust_id"));

        for (size_t gIdx = 0; gIdx < groups.size(); ++gIdx) {
            const auto& grp = groups[gIdx];
            Record dRec;
            dRec.set("adjust_id", adjId);
            dRec.set("product_id", grp.productId);
            dRec.set("from_status", grp.fromStatus);
            dRec.set("to_status", grp.toStatus);
            dRec.set("adjust_quantity", static_cast<int>(grp.serials.size()));
            dRec.set("actual_quantity", 0);

            OperationResult rDet = db->insertRecord("adjust_movement_detail", dRec);
            if (!rDet) { db->rollback(); return rDet; }
        }

        Vector<AdjustUnitEntry> unitEntries;
        unitEntries.reserve(entries.size());
        for (const auto& det : detTbl->view("adjust_id", Value(adjId))) {
            int detId = std::get<int>(det.get("adjust_detail_id"));
            int pid = std::get<int>(det.get("product_id"));
            std::string fromSt = std::get<std::string>(det.get("from_status"));
            std::string toSt = std::get<std::string>(det.get("to_status"));
            std::string gKey = std::to_string(pid) + "|" + fromSt + "|" + toSt;
            if (keyToGroupIdx.contains(gKey)) {
                const auto& grp = groups[keyToGroupIdx[gKey]];
                for (size_t k = 0; k < grp.serials.size(); ++k) {
                    unitEntries.push_back({detId, grp.serials[k]});
                }
            }
        }

        OperationResult rExec = executeAdjustmentMovementUnitsInternal(db, adjId, unitEntries, false);
        if (!rExec.success) {
            db->rollback();
            return rExec;
        }

        db->commit();
        return OperationResult{
            true,
            "Đã tạo phiếu điều chuyển trạng thái [" + adjCode + "] và cập nhật kho thành công " + std::to_string(entries.size()) + " linh kiện!",
            rExec.affectedCount
        };
    }
}
