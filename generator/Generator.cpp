#include "Generator.h"
#include "data_io/DatabaseIO.h"
#include "schema/Schema.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace Generator
{
    DataGenerator::DataGenerator(Database& database, unsigned int seed)
        : db(database),
          nextUnitId(1),
          nextInboundTxnId(1),
          nextInboundDetailId(1),
          nextDeliveryId(1),
          nextDeliveryDetailId(1),
          nextOutboundTxnId(1),
          nextOutboundDetailId(1),
          nextShipmentId(1),
          nextShipmentDetailId(1),
          nextStatusHistoryId(1),
          nextInventoryCheckId(1),
          nextCheckQuantityId(1),
          nextCheckUnitId(1),
          nextAdjustmentMovementId(1),
          nextAdjustDetailId(1),
          nextIsSoldChangingId(1),
          nextCheckSeq(1),
          nextAdjustSeq(1),
          rngSeed(seed == 0 ? static_cast<unsigned int>(time(nullptr)) : seed)
    {
        initialize();
    }

    unsigned int DataGenerator::nextRandom()
    {
        // 32-bit xorshift PRNG (avoids low-bit periodicity of 32-bit LCG mod 2^k)
        if (rngSeed == 0) rngSeed = 0x9E3779B9u;
        rngSeed ^= rngSeed << 13;
        rngSeed ^= rngSeed >> 17;
        rngSeed ^= rngSeed << 5;
        return rngSeed;
    }

    string DataGenerator::formatNumber(int num, int digits) const
    {
        string s = to_string(num);
        while (static_cast<int>(s.length()) < digits)
        {
            s = "0" + s;
        }
        return s;
    }

    string DataGenerator::getCurrentDate() const
    {
        return "2026-09-29";
    }

    string DataGenerator::advanceDate(const string& dateStr, int days) const
    {
        if (dateStr.length() < 10) return "2026-09-29";
        int y = stoi(dateStr.substr(0, 4));
        int m = stoi(dateStr.substr(5, 2));
        int d = stoi(dateStr.substr(8, 2));

        d += days;
        while (d > 28)
        {
            d -= 28;
            m += 1;
            if (m > 12)
            {
                m = 1;
                y += 1;
            }
        }
        return formatNumber(y, 4) + "-" + formatNumber(m, 2) + "-" + formatNumber(d, 2);
    }

    string DataGenerator::generateUniqueSerial()
    {
        static const char chars[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
        string s;
        do
        {
            s = "SN-";
            s += chars[nextRandom() % 32];
            s += chars[nextRandom() % 32];
            s += formatNumber(nextUnitId % 100000, 5);
        } while (existingSerials.contains(s));
        existingSerials[s] = true;
        return s;
    }

    bool DataGenerator::isValidProduct(int productId, bool requireActive) const
    {
        const Table* prodTbl = db.getTable("product");
        if (!prodTbl || productId <= 0) return false;
        const Record* pRec = prodTbl->getRecordById(productId);
        if (!pRec) return false;
        if (requireActive)
        {
            bool isSold = true;
            if (pRec->has("is_sold") && holds_alternative<bool>(pRec->get("is_sold")))
            {
                isSold = get<bool>(pRec->get("is_sold"));
            }
            if (!isSold) return false;
        }
        return true;
    }

    bool DataGenerator::isProductFrozen(int productId) const
    {
        const Table* chkTbl = db.getTable("inventory_check");
        const Table* cqTbl = db.getTable("inventory_check_quantity");
        if (!chkTbl || !cqTbl || productId <= 0) return false;

        HashMap<int, bool> activeChecks;
        for (const auto& c : chkTbl->view("status", Value(string("PENDING"))))
        {
            if (c.has("check_id") && holds_alternative<int>(c.get("check_id")))
            {
                activeChecks[get<int>(c.get("check_id"))] = true;
            }
        }
        for (const auto& c : chkTbl->view("status", Value(string("PROCESSING"))))
        {
            if (c.has("check_id") && holds_alternative<int>(c.get("check_id")))
            {
                activeChecks[get<int>(c.get("check_id"))] = true;
            }
        }
        if (activeChecks.empty()) return false;

        for (const auto& cq : cqTbl->view("product_id", Value(productId)))
        {
            if (cq.has("check_id") && holds_alternative<int>(cq.get("check_id")))
            {
                if (activeChecks.contains(get<int>(cq.get("check_id"))))
                {
                    return true;
                }
            }
        }
        return false;
    }

    int DataGenerator::countUnitsByStatus(int productId, const string& status) const
    {
        const Table* unitTbl = db.getTable("unit_inventory");
        if (!unitTbl || productId <= 0) return 0;

        int count = 0;
        for (const auto& u : unitTbl->view("product_id", Value(productId)))
        {
            if (u.has("status") && holds_alternative<string>(u.get("status")) &&
                get<string>(u.get("status")) == status)
            {
                count++;
            }
        }
        return count;
    }

    int DataGenerator::countEligibleOutboundUnits(int productId, const string& outboundType) const
    {
        const Table* unitTbl = db.getTable("unit_inventory");
        if (!unitTbl || productId <= 0) return 0;

        int count = 0;
        for (const auto& u : unitTbl->view("product_id", Value(productId)))
        {
            if (!u.has("status") || !holds_alternative<string>(u.get("status"))) continue;
            string st = get<string>(u.get("status"));
            if (outboundType == "WARRANTY_RETURN" || outboundType == "WRT")
            {
                if (st == "WARRANTY" || st == "SCRAPPED") count++;
            }
            else if (outboundType == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || outboundType == "WRC")
            {
                if (st == "AVAILABLE" || st == "WARRANTY") count++;
            }
            else if (outboundType == "ADJUSTMENT_OUT" || outboundType == "ADJ")
            {
                if (st == "AVAILABLE" || st == "WARRANTY" || st == "SCRAPPED") count++;
            }
            else if (outboundType == "SELLING" || outboundType == "SEL")
            {
                if (st == "AVAILABLE") count++;
            }
        }
        return count;
    }

    bool DataGenerator::validateCompletedCheckScope(const string& checkCode,
                                                    const Vector<int>& productIds,
                                                    const string& uniquenessTable) const
    {
        if (checkCode.empty())
        {
            cerr << "[Generator Error] Missing required source_code referencing an inventory check.\n";
            return false;
        }
        const Table* chkTbl = db.getTable("inventory_check");
        const Table* cqTbl = db.getTable("inventory_check_quantity");
        if (!chkTbl || !cqTbl)
        {
            cerr << "[Generator Error] Missing inventory_check tables for source_code validation.\n";
            return false;
        }

        TableView matches = chkTbl->view("check_code", Value(checkCode));
        if (matches.empty())
        {
            cerr << "[Generator Error] source_code '" << checkCode << "' does not exist in inventory_check.\n";
            return false;
        }

        const Record& chkRec = *matches.begin();
        string st = (chkRec.has("status") && holds_alternative<string>(chkRec.get("status")))
                  ? get<string>(chkRec.get("status")) : "";
        if (st != "COMPLETED")
        {
            cerr << "[Generator Error] Referenced inventory_check '" << checkCode
                 << "' is in status '" << st << "' (must be COMPLETED).\n";
            return false;
        }

        if (!uniquenessTable.empty())
        {
            const Table* uniqTbl = db.getTable(uniquenessTable);
            if (uniqTbl && !uniqTbl->view("source_code", Value(checkCode)).empty())
            {
                cerr << "[Generator Error] source_code '" << checkCode
                     << "' is already linked to an existing record in '" << uniquenessTable << "'.\n";
                return false;
            }
        }

        int chkId = get<int>(chkRec.get("check_id"));
        HashMap<int, bool> allowedPids;
        for (const auto& cq : cqTbl->view("check_id", Value(chkId)))
        {
            if (cq.has("product_id") && holds_alternative<int>(cq.get("product_id")))
            {
                allowedPids[get<int>(cq.get("product_id"))] = true;
            }
        }

        for (size_t i = 0; i < productIds.size(); ++i)
        {
            if (!allowedPids.contains(productIds[i]))
            {
                cerr << "[Generator Error] Product ID " << productIds[i]
                     << " is outside the audited scope of check '" << checkCode << "'.\n";
                return false;
            }
        }
        return true;
    }

    void DataGenerator::updateLookupNextCodeNo(const string& tableName,
                                               const string& keyCol,
                                               const string& keyVal,
                                               int nextSeq)
    {
        const Table* tbl = db.getTable(tableName);
        if (!tbl) return;
        string pkCol = tbl->getPrimaryKeyColumn();
        if (pkCol.empty()) return;

        for (const auto& r : tbl->view(keyCol, Value(keyVal)))
        {
            if (!r.has(pkCol) || !holds_alternative<int>(r.get(pkCol))) continue;
            int pkId = get<int>(r.get(pkCol));
            int curNext = (r.has("next_code_no") && holds_alternative<int>(r.get("next_code_no")))
                        ? get<int>(r.get("next_code_no")) : 0;
            if (nextSeq > curNext)
            {
                Record upd;
                upd.set("next_code_no", nextSeq);
                db.update(tableName, pkId, upd);
            }
            break;
        }
    }

        void DataGenerator::scanExistingDatabase()
    {
        // 1. Scan Unit Inventory
        const Table* unitTbl = db.getTable("unit_inventory");
        if (unitTbl != nullptr)
        {
            for (const auto& row : unitTbl->view())
            {
                int id = get<int>(row.get("unit_id"));
                if (id >= nextUnitId) nextUnitId = id + 1;
                if (holds_alternative<string>(row.get("serial")))
                {
                    existingSerials[get<string>(row.get("serial"))] = true;
                }
            }
        }

        // 2. Scan Inbound Transaction
        const Table* inTxnTbl = db.getTable("inbound_transaction");
        if (inTxnTbl != nullptr)
        {
            for (const auto& row : inTxnTbl->view())
            {
                int id = get<int>(row.get("inbound_transaction_id"));
                if (id >= nextInboundTxnId) nextInboundTxnId = id + 1;

                if (holds_alternative<string>(row.get("inbound_code")))
                {
                    string code = get<string>(row.get("inbound_code"));
                    if (code.length() >= 11)
                    {
                        string typeCode = code.substr(3, 3);
                        size_t lastDash = code.rfind('-');
                        if (lastDash != string::npos && lastDash + 1 < code.length())
                        {
                            try
                            {
                                int seq = stoi(code.substr(lastDash + 1));
                                int cur = inboundTypeCounters.contains(typeCode) ? inboundTypeCounters[typeCode] : 0;
                                if (seq > cur) inboundTypeCounters[typeCode] = seq;
                            }
                            catch (...) {}
                        }
                    }
                }
            }
        }

        // 3. Scan Inbound Detail
        const Table* inDetTbl = db.getTable("inbound_detail");
        if (inDetTbl != nullptr)
        {
            for (const auto& row : inDetTbl->view())
            {
                int id = get<int>(row.get("inbound_detail_id"));
                if (id >= nextInboundDetailId) nextInboundDetailId = id + 1;
            }
        }

        // 4. Scan Delivery
        const Table* delTbl = db.getTable("delivery");
        if (delTbl != nullptr)
        {
            for (const auto& row : delTbl->view())
            {
                int id = get<int>(row.get("delivery_id"));
                if (id >= nextDeliveryId) nextDeliveryId = id + 1;
            }
        }

        // 4b. Scan Delivery Detail
        const Table* delDetTbl = db.getTable("delivery_detail");
        if (delDetTbl != nullptr)
        {
            for (const auto& row : delDetTbl->view())
            {
                int id = get<int>(row.get("delivery_detail_id"));
                if (id >= nextDeliveryDetailId) nextDeliveryDetailId = id + 1;
            }
        }

        // 5. Scan Outbound Transaction
        const Table* outTxnTbl = db.getTable("outbound_transaction");
        if (outTxnTbl != nullptr)
        {
            for (const auto& row : outTxnTbl->view())
            {
                int id = get<int>(row.get("outbound_trainsaction_id"));
                if (id >= nextOutboundTxnId) nextOutboundTxnId = id + 1;

                if (holds_alternative<string>(row.get("outbound_code")))
                {
                    string code = get<string>(row.get("outbound_code"));
                    if (code.length() >= 12)
                    {
                        string typeCode = code.substr(4, 3);
                        size_t lastDash = code.rfind('-');
                        if (lastDash != string::npos && lastDash + 1 < code.length())
                        {
                            try
                            {
                                int seq = stoi(code.substr(lastDash + 1));
                                int cur = outboundTypeCounters.contains(typeCode) ? outboundTypeCounters[typeCode] : 0;
                                if (seq > cur) outboundTypeCounters[typeCode] = seq;
                            }
                            catch (...) {}
                        }
                    }
                }
            }
        }

        // 6. Scan Outbound Detail
        const Table* outDetTbl = db.getTable("outbound_detail");
        if (outDetTbl != nullptr)
        {
            for (const auto& row : outDetTbl->view())
            {
                int id = get<int>(row.get("outbound_detail_id"));
                if (id >= nextOutboundDetailId) nextOutboundDetailId = id + 1;
            }
        }

        // 7. Scan Shipment
        const Table* shipTbl = db.getTable("shipment");
        if (shipTbl != nullptr)
        {
            for (const auto& row : shipTbl->view())
            {
                int id = get<int>(row.get("shipment_id"));
                if (id >= nextShipmentId) nextShipmentId = id + 1;
            }
        }

        // 7b. Scan Shipment Detail
        const Table* shipDetTbl = db.getTable("shipment_detail");
        if (shipDetTbl != nullptr)
        {
            for (const auto& row : shipDetTbl->view())
            {
                int id = get<int>(row.get("shipment_detail_id"));
                if (id >= nextShipmentDetailId) nextShipmentDetailId = id + 1;
            }
        }

        // 8. Scan Status History
        const Table* histTbl = db.getTable("status_history");
        if (histTbl != nullptr)
        {
            for (const auto& row : histTbl->view())
            {
                int id = get<int>(row.get("history_id"));
                if (id >= nextStatusHistoryId) nextStatusHistoryId = id + 1;
            }
        }

        // 9. Scan Inventory Check
        const Table* chkTbl = db.getTable("inventory_check");
        if (chkTbl != nullptr)
        {
            for (const auto& row : chkTbl->view())
            {
                int id = get<int>(row.get("check_id"));
                if (id >= nextInventoryCheckId) nextInventoryCheckId = id + 1;

                if (holds_alternative<string>(row.get("check_code")))
                {
                    string code = get<string>(row.get("check_code"));
                    size_t lastDash = code.rfind('-');
                    if (lastDash != string::npos && lastDash + 1 < code.length())
                    {
                        try
                        {
                            int seq = stoi(code.substr(lastDash + 1));
                            if (seq >= nextCheckSeq) nextCheckSeq = seq + 1;
                        }
                        catch (...) {}
                    }
                }
            }
        }

        // 10. Scan Inventory Check Quantity
        const Table* chkQuanTbl = db.getTable("inventory_check_quantity");
        if (chkQuanTbl != nullptr)
        {
            for (const auto& row : chkQuanTbl->view())
            {
                int id = get<int>(row.get("check_quantity_id"));
                if (id >= nextCheckQuantityId) nextCheckQuantityId = id + 1;
            }
        }

        // 11. Scan Inventory Check Unit
        const Table* chkUnitTbl = db.getTable("inventory_check_unit");
        if (chkUnitTbl != nullptr)
        {
            for (const auto& row : chkUnitTbl->view())
            {
                int id = get<int>(row.get("check_unit_id"));
                if (id >= nextCheckUnitId) nextCheckUnitId = id + 1;
            }
        }

        // 12. Scan Adjustment Movement
        const Table* adjTbl = db.getTable("adjustment_movement");
        if (adjTbl != nullptr)
        {
            for (const auto& row : adjTbl->view())
            {
                int id = get<int>(row.get("adjust_id"));
                if (id >= nextAdjustmentMovementId) nextAdjustmentMovementId = id + 1;

                if (holds_alternative<string>(row.get("adjust_code")))
                {
                    string code = get<string>(row.get("adjust_code"));
                    size_t lastDash = code.rfind('-');
                    if (lastDash != string::npos && lastDash + 1 < code.length())
                    {
                        try
                        {
                            int seq = stoi(code.substr(lastDash + 1));
                            if (seq >= nextAdjustSeq) nextAdjustSeq = seq + 1;
                        }
                        catch (...) {}
                    }
                }
            }
        }

        // 13. Scan Adjustment Movement Detail
        const Table* adjDetTbl = db.getTable("adjust_movement_detail");
        if (adjDetTbl != nullptr)
        {
            for (const auto& row : adjDetTbl->view())
            {
                int id = get<int>(row.get("adjust_detail_id"));
                if (id >= nextAdjustDetailId) nextAdjustDetailId = id + 1;
            }
        }

        // 14. Scan is_sold_changing
        const Table* isSoldTbl = db.getTable("is_sold_changing");
        if (isSoldTbl != nullptr)
        {
            for (const auto& row : isSoldTbl->view())
            {
                int id = get<int>(row.get("history_id"));
                if (id >= nextIsSoldChangingId) nextIsSoldChangingId = id + 1;
            }
        }
    }

    void DataGenerator::initialize()
    {
        scanExistingDatabase();
    }

    int DataGenerator::getAvailableStock(int productId) const
    {
        const Table* unitTbl = db.getTable("unit_inventory");
        if (unitTbl == nullptr) return 0;

        int count = 0;
        for (const auto& r : unitTbl->view())
        {
            if (get<int>(r.get("product_id")) == productId &&
                holds_alternative<string>(r.get("status")) &&
                get<string>(r.get("status")) == "AVAILABLE")
            {
                count++;
            }
        }
        return count;
    }

        void DataGenerator::syncProductQuantities()
    {
        const Table* prodTbl = db.getTable("product");
        const Table* unitTbl = db.getTable("unit_inventory");
        if (prodTbl == nullptr || unitTbl == nullptr) return;

        HashMap<int, int> availableCounts;
        HashMap<int, int> warrantyCounts;
        HashMap<int, int> scrappedCounts;

        for (const auto& u : unitTbl->view())
        {
            int pid = get<int>(u.get("product_id"));
            if (!holds_alternative<string>(u.get("status"))) continue;
            string st = get<string>(u.get("status"));
            if (st == "AVAILABLE")
            {
                int cur = availableCounts.contains(pid) ? availableCounts[pid] : 0;
                availableCounts[pid] = cur + 1;
            }
            else if (st == "WARRANTY")
            {
                int cur = warrantyCounts.contains(pid) ? warrantyCounts[pid] : 0;
                warrantyCounts[pid] = cur + 1;
            }
            else if (st == "SCRAPPED")
            {
                int cur = scrappedCounts.contains(pid) ? scrappedCounts[pid] : 0;
                scrappedCounts[pid] = cur + 1;
            }
        }

        for (const auto& prod : prodTbl->view())
        {
            int pid = get<int>(prod.get("product_id"));
            int avail = availableCounts.contains(pid) ? availableCounts[pid] : 0;
            int warr = warrantyCounts.contains(pid) ? warrantyCounts[pid] : 0;
            int scrap = scrappedCounts.contains(pid) ? scrappedCounts[pid] : 0;

            Record updateRec;
            updateRec.set("available_quantity", avail);
            updateRec.set("warranty_quantity", warr);
            updateRec.set("scrapped_quantity", scrap);
            db.update("product", pid, updateRec);
        }
    }

        bool DataGenerator::updateProductIsSold(int productId, bool newIsSold, int changedBy, const string& reason)
    {
        (void)changedBy;
        (void)reason;
        const Table* prodTbl = db.getTable("product");
        if (!prodTbl) return false;

        for (const auto& row : prodTbl->view())
        {
            if (get<int>(row.get("product_id")) == productId)
            {
                bool curIsSold = true;
                if (holds_alternative<bool>(row.get("is_sold")))
                {
                    curIsSold = get<bool>(row.get("is_sold"));
                }

                if (curIsSold == newIsSold) return true; // No change needed

                if (!newIsSold)
                {
                    int avail = get<int>(row.get("available_quantity"));
                    int warr = get<int>(row.get("warranty_quantity"));
                    int scrap = get<int>(row.get("scrapped_quantity"));
                    int totalStock = avail + warr + scrap;

                    if (totalStock > 0)
                    {
                        cerr << "[Generator Error] Cannot set is_sold = FALSE for product " << productId
                             << " because stock remains in warehouse (" << totalStock << " units).\n";
                        return false;
                    }
                }

                Record updateRec;
                updateRec.set("is_sold", newIsSold);
                db.update("product", productId, updateRec);

                Record hist;
                hist.set("history_id", nextIsSoldChangingId++);
                hist.set("product_id", productId);
                hist.set("date", getCurrentDate());
                hist.set("old_status", curIsSold);
                hist.set("new_status", newIsSold);
                db.insert("is_sold_changing", hist);

                cout << "[+] Product ID " << productId << " is_sold changed to "
                     << (newIsSold ? "TRUE" : "FALSE") << "\n";
                return true;
            }
        }

        cerr << "[Generator Error] Product ID " << productId << " not found.\n";
        return false;
    }

    string DataGenerator::generateAuditCheckWithDiscrepancies(const string& checkDate,
                                                              const Vector<int>& productIds,
                                                              const HashMap<int, int>& surplusUnrecordedAvail,
                                                              const HashMap<int, bool>& rediscoveredOutUnits,
                                                              const HashMap<int, bool>& missingUnitIds,
                                                              const HashMap<int, string>& shiftedUnitTargets)
    {
        const Table* chkTbl = db.getTable("inventory_check");
        const Table* chkQuanTbl = db.getTable("inventory_check_quantity");
        const Table* chkUnitTbl = db.getTable("inventory_check_unit");
        const Table* unitTbl = db.getTable("unit_inventory");
        const Table* prodTbl = db.getTable("product");
        if (!chkTbl) return "";

        string date = checkDate.empty() ? getCurrentDate() : checkDate;
        int chkId = nextInventoryCheckId++;
        int seq = nextCheckSeq++;
        string code = "CHK-UNIT-" + formatNumber(seq, 6);

        Record chkRec;
        chkRec.set("check_id", chkId);
        chkRec.set("check_code", code);
        chkRec.set("check_type", string("UNIT"));
        chkRec.set("check_at", date);
        chkRec.set("status", string("COMPLETED"));
        chkRec.set("note", string("Inventory check audit (UNIT) completed"));
        db.insert("inventory_check", chkRec);
        updateLookupNextCodeNo("check_type", "check_type", "UNIT", seq + 1);

        Vector<int> dedupPids;
        HashMap<int, bool> seenPids;
        if (productIds.empty() && prodTbl)
        {
            for (const auto& r : prodTbl->view())
            {
                int pid = get<int>(r.get("product_id"));
                if (!seenPids.contains(pid))
                {
                    seenPids[pid] = true;
                    dedupPids.push_back(pid);
                }
            }
        }
        else
        {
            for (size_t i = 0; i < productIds.size(); ++i)
            {
                int pid = productIds[i];
                if (pid > 0 && !seenPids.contains(pid))
                {
                    seenPids[pid] = true;
                    dedupPids.push_back(pid);
                }
            }
        }

        struct UnitAuditRow
        {
            string serial;
            string systemStatus;
            Value scannedStatus;
        };

        for (size_t i = 0; i < dedupPids.size(); ++i)
        {
            int pid = dedupPids[i];
            int sysAvail = 0, sysWarr = 0, sysScrap = 0;
            int actAvail = 0, actWarr = 0, actScrap = 0;
            Vector<UnitAuditRow> auditRows;

            if (unitTbl)
            {
                for (const auto& u : unitTbl->view("product_id", Value(pid)))
                {
                    if (!u.has("status") || !holds_alternative<string>(u.get("status"))) continue;
                    int uId = get<int>(u.get("unit_id"));
                    string st = get<string>(u.get("status"));
                    string sn = get<string>(u.get("serial"));

                    if (st == "OUT")
                    {
                        if (rediscoveredOutUnits.contains(uId))
                        {
                            actAvail++;
                            auditRows.push_back({sn, "OUT", Value(string("AVAILABLE"))});
                        }
                        continue;
                    }

                    if (st == "AVAILABLE") sysAvail++;
                    else if (st == "WARRANTY") sysWarr++;
                    else if (st == "SCRAPPED") sysScrap++;

                    Value scanVal;
                    if (missingUnitIds.contains(uId))
                    {
                        scanVal = Value{}; // NULL -> Missing
                    }
                    else if (shiftedUnitTargets.contains(uId))
                    {
                        string targetSt = shiftedUnitTargets.find(uId)->second;
                        scanVal = Value(targetSt);
                        if (targetSt == "AVAILABLE") actAvail++;
                        else if (targetSt == "WARRANTY") actWarr++;
                        else if (targetSt == "SCRAPPED") actScrap++;
                    }
                    else
                    {
                        scanVal = Value(st);
                        if (st == "AVAILABLE") actAvail++;
                        else if (st == "WARRANTY") actWarr++;
                        else if (st == "SCRAPPED") actScrap++;
                    }
                    auditRows.push_back({sn, st, scanVal});
                }
            }

            if (surplusUnrecordedAvail.contains(pid))
            {
                actAvail += surplusUnrecordedAvail.find(pid)->second;
            }

            int cqId = nextCheckQuantityId++;
            if (chkQuanTbl)
            {
                Record qRec;
                qRec.set("check_quantity_id", cqId);
                qRec.set("check_id", chkId);
                qRec.set("product_id", pid);
                qRec.set("system_available_quan", sysAvail);
                qRec.set("system_warranty_quan", sysWarr);
                qRec.set("system_scrapped_quan", sysScrap);
                qRec.set("actual_available_quan", actAvail);
                qRec.set("actual_warranty_quan", actWarr);
                qRec.set("actual_scrapped_quan", actScrap);
                qRec.set("note", string(""));
                db.insert("inventory_check_quantity", qRec);
            }

            if (chkUnitTbl)
            {
                for (size_t k = 0; k < auditRows.size(); ++k)
                {
                    Record uRec;
                    uRec.set("check_unit_id", nextCheckUnitId++);
                    uRec.set("check_id", chkId);
                    uRec.set("check_quantity_id", cqId);
                    uRec.set("serial", auditRows[k].serial);
                    uRec.set("system_status", auditRows[k].systemStatus);
                    uRec.set("scanned_status", auditRows[k].scannedStatus);
                    db.insert("inventory_check_unit", uRec);
                }
            }
        }

        cout << "[+] Generated Inventory Check: " << code << " (ID: " << chkId << ")\n";
        return code;
    }

    string DataGenerator::generateInventoryCheck(const string& checkType,
                                                 const string& checkDate,
                                                 const Vector<int>& productIds)
    {
        (void)checkType;
        for (size_t i = 0; i < productIds.size(); ++i)
        {
            if (!isValidProduct(productIds[i], false))
            {
                cerr << "[Generator Error] Invalid product ID " << productIds[i] << " in inventory check.\n";
                return "";
            }
            if (isProductFrozen(productIds[i]))
            {
                cerr << "[Generator Error] Product ID " << productIds[i]
                     << " is already frozen in an open inventory check.\n";
                return "";
            }
        }
        HashMap<int, int> emptySurplus;
        HashMap<int, bool> emptyRedisc;
        HashMap<int, bool> emptyMissing;
        HashMap<int, string> emptyShift;
        return generateAuditCheckWithDiscrepancies(checkDate, productIds, emptySurplus, emptyRedisc, emptyMissing, emptyShift);
    }

    int DataGenerator::generateInbound(const string& inboundType,
                                       const Vector<Pair<int, int>>& items,
                                       int numDeliveries,
                                       const string& orderDate,
                                       const string& note,
                                       const string& sourceCode)
    {
        if (items.empty())
        {
            cerr << "[Generator Error] Inbound transaction requires at least one product.\n";
            return -1;
        }

        string normType = inboundType;
        if (normType == "PUR") normType = "PURCHASING";
        else if (normType == "ADJ") normType = "ADJUSTMENT_IN";
        else if (normType == "WCK") normType = "WARRANTY_CHECKING";
        else if (normType == "WRP") normType = "WARRANTY_REPLACEMENT";
        else if (normType == "CRT") normType = "CUSTOMER_RETURN";

        if (normType != "PURCHASING" && normType != "ADJUSTMENT_IN" &&
            normType != "WARRANTY_CHECKING" && normType != "WARRANTY_REPLACEMENT" &&
            normType != "CUSTOMER_RETURN")
        {
            cerr << "[Generator Error] Invalid inbound type: " << inboundType << "\n";
            return -1;
        }

        const Table* inTxnTbl = db.getTable("inbound_transaction");
        const Table* inDetTbl = db.getTable("inbound_detail");
        const Table* delTbl = db.getTable("delivery");
        const Table* delDetTbl = db.getTable("delivery_detail");
        const Table* unitTbl = db.getTable("unit_inventory");
        const Table* histTbl = db.getTable("status_history");
        const Table* prodTbl = db.getTable("product");

        if (!inTxnTbl || !inDetTbl || !delTbl || !delDetTbl || !unitTbl)
        {
            cerr << "[Generator Error] Missing required database tables for inbound generation.\n";
            return -1;
        }

        // Merge duplicate product entries and validate quantities
        Vector<Pair<int, int>> mergedItems;
        HashMap<int, size_t> pidToIdx;
        for (size_t i = 0; i < items.size(); ++i)
        {
            int pid = items[i].first;
            int qty = items[i].second;
            if (qty <= 0)
            {
                cerr << "[Generator Error] Inbound quantity for product ID " << pid << " must be > 0.\n";
                return -1;
            }
            if (!pidToIdx.contains(pid))
            {
                pidToIdx[pid] = mergedItems.size();
                mergedItems.push_back(Pair<int, int>(pid, qty));
            }
            else
            {
                mergedItems[pidToIdx[pid]].second += qty;
            }
        }

        Vector<int> pids;
        HashMap<int, Vector<int>> outUnitPoolByPid;
        for (size_t i = 0; i < mergedItems.size(); ++i)
        {
            int pid = mergedItems[i].first;
            int qty = mergedItems[i].second;
            if (!isValidProduct(pid, true))
            {
                cerr << "[Generator Error] Product ID " << pid << " does not exist or has is_sold = FALSE.\n";
                return -1;
            }
            if (isProductFrozen(pid))
            {
                cerr << "[Generator Error] Product ID " << pid << " is frozen in an active inventory check.\n";
                return -1;
            }

            if (normType == "WARRANTY_CHECKING" || normType == "CUSTOMER_RETURN" || normType == "ADJUSTMENT_IN")
            {
                Vector<int> outPool;
                for (const auto& uRec : unitTbl->view("product_id", Value(pid)))
                {
                    if (uRec.has("status") && holds_alternative<string>(uRec.get("status")) &&
                        get<string>(uRec.get("status")) == "OUT")
                    {
                        outPool.push_back(get<int>(uRec.get("unit_id")));
                    }
                }
                if ((normType == "WARRANTY_CHECKING" || normType == "CUSTOMER_RETURN") &&
                    static_cast<int>(outPool.size()) < qty)
                {
                    cerr << "[Generator Error] Inbound type " << normType << " requires OUT units. Product ID "
                         << pid << " requested " << qty << ", available OUT units: " << outPool.size() << ".\n";
                    return -1;
                }
                outUnitPoolByPid[pid] = outPool;
            }
            pids.push_back(pid);
        }

        string date = orderDate.empty() ? getCurrentDate() : orderDate;

        // Validate / prepare source_code for ADJUSTMENT_IN before committing counter changes
        Value sourceCodeVal;
        HashMap<int, int> surplusUnrecordedAvail;
        HashMap<int, bool> rediscoveredOutUnits;
        if (normType == "ADJUSTMENT_IN")
        {
            if (!sourceCode.empty())
            {
                if (!validateCompletedCheckScope(sourceCode, pids, "inbound_transaction"))
                {
                    return -1;
                }
                sourceCodeVal = sourceCode;
            }
            else
            {
                for (size_t i = 0; i < mergedItems.size(); ++i)
                {
                    int pid = mergedItems[i].first;
                    int qty = mergedItems[i].second;
                    const Vector<int>& outPool = outUnitPoolByPid[pid];
                    int takeOut = static_cast<int>(outPool.size()) < qty ? static_cast<int>(outPool.size()) : qty;
                    for (int k = 0; k < takeOut; ++k)
                    {
                        rediscoveredOutUnits[outPool[k]] = true;
                    }
                    int newUnrecorded = qty - takeOut;
                    if (newUnrecorded > 0)
                    {
                        surplusUnrecordedAvail[pid] = newUnrecorded;
                    }
                }
                HashMap<int, bool> emptyMissing;
                HashMap<int, string> emptyShift;
                string genCode = generateAuditCheckWithDiscrepancies(
                    date, pids, surplusUnrecordedAvail, rediscoveredOutUnits, emptyMissing, emptyShift
                );
                if (genCode.empty()) return -1;
                sourceCodeVal = genCode;
            }
        }
        else
        {
            if (!sourceCode.empty())
            {
                if (!inTxnTbl->view("source_code", Value(sourceCode)).empty())
                {
                    cerr << "[Generator Error] Duplicate source_code '" << sourceCode << "' in inbound_transaction.\n";
                    return -1;
                }
                sourceCodeVal = sourceCode;
            }
            else
            {
                sourceCodeVal = Value{}; // NULL
            }
        }

        // Determine Type Code
        string typeCode = "PUR";
        if (normType == "ADJUSTMENT_IN") typeCode = "ADJ";
        else if (normType == "WARRANTY_CHECKING") typeCode = "WCK";
        else if (normType == "WARRANTY_REPLACEMENT") typeCode = "WRP";
        else if (normType == "CUSTOMER_RETURN") typeCode = "CRT";

        int seq = (inboundTypeCounters.contains(typeCode) ? inboundTypeCounters[typeCode] : 0) + 1;
        inboundTypeCounters[typeCode] = seq;
        string inboundCode = "IN-" + typeCode + "-" + formatNumber(seq, 6);
        updateLookupNextCodeNo("inbound_type", "code", typeCode, seq + 1);

        if (numDeliveries < 1) numDeliveries = 1;

        // Rule 4.1: For ADJUSTMENT_IN, customer_id MUST be NULL; otherwise valid customer
        Value customerVal;
        if (normType == "ADJUSTMENT_IN")
        {
            customerVal = Value{}; // NULL
        }
        else if (normType == "WARRANTY_CHECKING" || normType == "CUSTOMER_RETURN")
        {
            customerVal = 1; // Individual customer
        }
        else
        {
            customerVal = 4; // Supplier / Business
        }

        // 1. Create InboundTransaction record
        int txnId = nextInboundTxnId++;
        Record inTxn;
        inTxn.set("inbound_transaction_id", txnId);
        inTxn.set("inbound_code", inboundCode);
        inTxn.set("source_code", sourceCodeVal);
        inTxn.set("customer_id", customerVal);
        inTxn.set("order_at", date);
        inTxn.set("type", normType);
        inTxn.set("status", string("COMPLETED"));
        inTxn.set("next_delivery_no", numDeliveries + 1);
        inTxn.set("note", note.empty() ? ("Inbound " + normType + " received successfully") : note);

        db.insert("inbound_transaction", inTxn);

        // 2. Create InboundDetail records
        HashMap<int, int> inDetIdByPid;
        for (size_t i = 0; i < mergedItems.size(); ++i)
        {
            int pid = mergedItems[i].first;
            int qty = mergedItems[i].second;
            int detId = nextInboundDetailId++;
            inDetIdByPid[pid] = detId;

            Record inDet;
            inDet.set("inbound_detail_id", detId);
            inDet.set("inbound_transaction_id", txnId);
            inDet.set("product_id", pid);
            inDet.set("ordered_quantity", qty);
            inDet.set("actual_quantity", 0);
            db.insert("inbound_detail", inDet);
        }

        HashMap<int, size_t> outCursorByPid;
        HashMap<int, int> inActualByPid;

        // 3. Create Delivery and DeliveryDetail records
        for (int d = 1; d <= numDeliveries; d++)
        {
            int delId = nextDeliveryId++;
            string delCode = inboundCode + "-D" + formatNumber(d, 2);
            string delDate = advanceDate(date, d);

            Record delRecord;
            delRecord.set("delivery_id", delId);
            delRecord.set("delivery_code", delCode);
            delRecord.set("inbound_transaction_id", txnId);
            delRecord.set("delivery_at", delDate);
            delRecord.set("expected_at", delDate);
            delRecord.set("status", string("COMPLETED"));
            db.insert("delivery", delRecord);

            for (size_t i = 0; i < mergedItems.size(); ++i)
            {
                int pid = mergedItems[i].first;
                int totalQty = mergedItems[i].second;
                int baseQty = totalQty / numDeliveries;
                int rem = totalQty % numDeliveries;
                int deliveryQty = baseQty + (d <= rem ? 1 : 0);

                int delDetId = 0;
                if (delDetTbl != nullptr && deliveryQty > 0)
                {
                    delDetId = nextDeliveryDetailId++;
                    Record delDet;
                    delDet.set("delivery_detail_id", delDetId);
                    delDet.set("delivery_id", delId);
                    delDet.set("product_id", pid);
                    delDet.set("received_quantity", deliveryQty);
                    delDet.set("actual_quantity", 0);
                    db.insert("delivery_detail", delDet);
                }

                // Inbound Unit Processing (Rule 4.4.1)
                for (int u = 0; u < deliveryQty; u++)
                {
                    int uId = 0;
                    string serial;
                    bool fromNull = false;
                    string oldStatus = "";
                    string targetStatus = "AVAILABLE";

                    if (normType == "PURCHASING")
                    {
                        fromNull = true;
                        targetStatus = (nextRandom() % 25 == 0) ? "SCRAPPED" : "AVAILABLE";
                    }
                    else if (normType == "WARRANTY_REPLACEMENT")
                    {
                        fromNull = true;
                        targetStatus = "WARRANTY";
                    }
                    else if (normType == "WARRANTY_CHECKING")
                    {
                        oldStatus = "OUT";
                        targetStatus = "WARRANTY";
                        const Vector<int>& outPool = outUnitPoolByPid[pid];
                        size_t curIdx = outCursorByPid.contains(pid) ? outCursorByPid[pid] : 0;
                        uId = outPool[curIdx];
                        outCursorByPid[pid] = curIdx + 1;
                    }
                    else if (normType == "CUSTOMER_RETURN")
                    {
                        oldStatus = "OUT";
                        targetStatus = "AVAILABLE";
                        const Vector<int>& outPool = outUnitPoolByPid[pid];
                        size_t curIdx = outCursorByPid.contains(pid) ? outCursorByPid[pid] : 0;
                        uId = outPool[curIdx];
                        outCursorByPid[pid] = curIdx + 1;
                    }
                    else // ADJUSTMENT_IN
                    {
                        const Vector<int>& outPool = outUnitPoolByPid[pid];
                        size_t curIdx = outCursorByPid.contains(pid) ? outCursorByPid[pid] : 0;
                        if (curIdx < outPool.size())
                        {
                            oldStatus = "OUT";
                            targetStatus = "AVAILABLE";
                            uId = outPool[curIdx];
                            outCursorByPid[pid] = curIdx + 1;
                        }
                        else
                        {
                            fromNull = true;
                            targetStatus = "AVAILABLE";
                        }
                    }

                    if (fromNull)
                    {
                        uId = nextUnitId++;
                        serial = generateUniqueSerial();

                        Record unitRecord;
                        unitRecord.set("unit_id", uId);
                        unitRecord.set("product_id", pid);
                        unitRecord.set("serial", serial);
                        unitRecord.set("status", targetStatus);
                        db.insert("unit_inventory", unitRecord);
                    }
                    else
                    {
                        Record uUpd;
                        uUpd.set("status", targetStatus);
                        db.update("unit_inventory", uId, uUpd);
                    }

                    // Insert Status History (Rule 6.8: NULL via monostate when fromNull)
                    if (histTbl != nullptr)
                    {
                        Record histRecord;
                        histRecord.set("history_id", nextStatusHistoryId++);
                        histRecord.set("unit_id", uId);
                        histRecord.set("old_status", fromNull ? Value{} : Value(oldStatus));
                        histRecord.set("curr_status", targetStatus);
                        histRecord.set("transaction", delId);
                        histRecord.set("type", string("INBOUND"));
                        db.insert("status_history", histRecord);
                    }
                }

                if (deliveryQty > 0)
                {
                    if (delDetTbl != nullptr && delDetId > 0)
                    {
                        Record ddUpd;
                        ddUpd.set("actual_quantity", deliveryQty);
                        db.update("delivery_detail", delDetId, ddUpd);
                    }
                    inActualByPid[pid] = (inActualByPid.contains(pid) ? inActualByPid[pid] : 0) + deliveryQty;
                }
            }
        }

        for (size_t i = 0; i < mergedItems.size(); ++i)
        {
            int pid = mergedItems[i].first;
            int detId = inDetIdByPid[pid];
            int actQty = inActualByPid.contains(pid) ? inActualByPid[pid] : 0;
            Record idUpd;
            idUpd.set("actual_quantity", actQty);
            db.update("inbound_detail", detId, idUpd);
        }

        // 4. Update Product stock quantities
        if (prodTbl != nullptr)
        {
            syncProductQuantities();
        }

        cout << "[+] Generated Inbound Transaction: " << inboundCode << " (ID: " << txnId << ")\n";
        cout << "    - Deliveries created: " << numDeliveries << "\n";
        return txnId;
    }

    int DataGenerator::generateOutbound(const string& outboundType,
                                        const Vector<Pair<int, int>>& items,
                                        int numShipments,
                                        const string& orderDate,
                                        const string& note,
                                        const string& sourceCode)
    {
        if (items.empty())
        {
            cerr << "[Generator Error] Outbound transaction requires at least one product.\n";
            return -1;
        }

        string normType = outboundType;
        if (normType == "SEL") normType = "SELLING";
        else if (normType == "ADJ") normType = "ADJUSTMENT_OUT";
        else if (normType == "WRT") normType = "WARRANTY_RETURN";
        else if (normType == "WRC") normType = "WARRANTY_REPLACEMENT_TO_CUSTOMER";

        if (normType == "RETURN_TO_SUPPLIER" || normType == "RTS")
        {
            cerr << "[Generator Error] Outbound type RETURN_TO_SUPPLIER (RTS) has been removed from the system.\n";
            return -1;
        }
        if (normType != "SELLING" && normType != "ADJUSTMENT_OUT" &&
            normType != "WARRANTY_RETURN" && normType != "WARRANTY_REPLACEMENT_TO_CUSTOMER")
        {
            cerr << "[Generator Error] Invalid outbound type: " << outboundType << "\n";
            return -1;
        }

        const Table* outTxnTbl = db.getTable("outbound_transaction");
        const Table* outDetTbl = db.getTable("outbound_detail");
        const Table* shipTbl = db.getTable("shipment");
        const Table* shipDetTbl = db.getTable("shipment_detail");
        const Table* unitTbl = db.getTable("unit_inventory");
        const Table* histTbl = db.getTable("status_history");
        const Table* prodTbl = db.getTable("product");

        if (!outTxnTbl || !outDetTbl || !shipTbl || !shipDetTbl || !unitTbl)
        {
            cerr << "[Generator Error] Missing required database tables for outbound generation.\n";
            return -1;
        }

        // Merge duplicate product entries and validate quantities
        Vector<Pair<int, int>> mergedItems;
        HashMap<int, size_t> pidToIdx;
        for (size_t i = 0; i < items.size(); ++i)
        {
            int pid = items[i].first;
            int qty = items[i].second;
            if (qty <= 0)
            {
                cerr << "[Generator Error] Outbound quantity for product ID " << pid << " must be > 0.\n";
                return -1;
            }
            if (!pidToIdx.contains(pid))
            {
                pidToIdx[pid] = mergedItems.size();
                mergedItems.push_back(Pair<int, int>(pid, qty));
            }
            else
            {
                mergedItems[pidToIdx[pid]].second += qty;
            }
        }

        // Validate product existence, active status, stock freeze, and eligible stock availability
        Vector<int> pids;
        HashMap<int, Vector<Pair<int, string>>> eligibleUnitsByPid;
        for (size_t i = 0; i < mergedItems.size(); ++i)
        {
            int pid = mergedItems[i].first;
            int requested = mergedItems[i].second;

            if (!isValidProduct(pid, true))
            {
                cerr << "[Generator Error] Product ID " << pid << " does not exist or has is_sold = FALSE.\n";
                return -1;
            }
            if (isProductFrozen(pid))
            {
                cerr << "[Generator Error] Product ID " << pid << " is frozen in an active inventory check.\n";
                return -1;
            }

            Vector<Pair<int, string>> pool;
            for (const auto& u : unitTbl->view("product_id", Value(pid)))
            {
                if (!u.has("status") || !holds_alternative<string>(u.get("status"))) continue;
                string st = get<string>(u.get("status"));
                bool eligible = false;
                if (normType == "WARRANTY_RETURN")
                {
                    eligible = (st == "WARRANTY" || st == "SCRAPPED");
                }
                else if (normType == "WARRANTY_REPLACEMENT_TO_CUSTOMER")
                {
                    eligible = (st == "AVAILABLE" || st == "WARRANTY");
                }
                else if (normType == "ADJUSTMENT_OUT")
                {
                    eligible = (st == "AVAILABLE" || st == "WARRANTY" || st == "SCRAPPED");
                }
                else
                {
                    eligible = (st == "AVAILABLE");
                }

                if (eligible)
                {
                    pool.push_back(Pair<int, string>(get<int>(u.get("unit_id")), st));
                }
            }

            if (static_cast<int>(pool.size()) < requested)
            {
                cerr << "[Generator Error] Insufficient eligible stock for product ID " << pid
                     << ". Requested: " << requested << ", Eligible in stock: " << pool.size() << ".\n";
                return -1;
            }

            eligibleUnitsByPid[pid] = pool;
            pids.push_back(pid);
        }

        string date = orderDate.empty() ? getCurrentDate() : orderDate;

        // Validate / prepare source_code for ADJUSTMENT_OUT before mutating DB
        Value sourceCodeVal;
        if (normType == "ADJUSTMENT_OUT")
        {
            if (!sourceCode.empty())
            {
                if (!validateCompletedCheckScope(sourceCode, pids, "outbound_transaction"))
                {
                    return -1;
                }
                sourceCodeVal = sourceCode;
            }
            else
            {
                HashMap<int, bool> missingUnitIds;
                for (size_t i = 0; i < mergedItems.size(); ++i)
                {
                    int pid = mergedItems[i].first;
                    int qty = mergedItems[i].second;
                    const auto& pool = eligibleUnitsByPid[pid];
                    for (int k = 0; k < qty; ++k)
                    {
                        missingUnitIds[pool[k].first] = true;
                    }
                }
                HashMap<int, int> emptySurplus;
                HashMap<int, bool> emptyRedisc;
                HashMap<int, string> emptyShift;
                string genCode = generateAuditCheckWithDiscrepancies(
                    date, pids, emptySurplus, emptyRedisc, missingUnitIds, emptyShift
                );
                if (genCode.empty()) return -1;
                sourceCodeVal = genCode;
            }
        }
        else
        {
            if (!sourceCode.empty())
            {
                if (!outTxnTbl->view("source_code", Value(sourceCode)).empty())
                {
                    cerr << "[Generator Error] Duplicate source_code '" << sourceCode << "' in outbound_transaction.\n";
                    return -1;
                }
                sourceCodeVal = sourceCode;
            }
            else
            {
                sourceCodeVal = Value{}; // NULL
            }
        }

        // Determine Type Code
        string typeCode = "SEL";
        if (normType == "ADJUSTMENT_OUT") typeCode = "ADJ";
        else if (normType == "WARRANTY_RETURN") typeCode = "WRT";
        else if (normType == "WARRANTY_REPLACEMENT_TO_CUSTOMER") typeCode = "WRC";

        int seq = (outboundTypeCounters.contains(typeCode) ? outboundTypeCounters[typeCode] : 0) + 1;
        outboundTypeCounters[typeCode] = seq;
        string outboundCode = "OUT-" + typeCode + "-" + formatNumber(seq, 6);
        updateLookupNextCodeNo("outbound_type", "code", typeCode, seq + 1);

        if (numShipments < 1) numShipments = 1;

        // Rule 4.1: For ADJUSTMENT_OUT, customer_id MUST be NULL; otherwise valid customer
        Value customerVal;
        if (normType == "ADJUSTMENT_OUT")
        {
            customerVal = Value{}; // NULL
        }
        else if (normType == "WARRANTY_RETURN")
        {
            customerVal = 4; // Supplier / Business
        }
        else
        {
            customerVal = 1; // Individual customer
        }

        // 1. Create OutboundTransaction record
        int txnId = nextOutboundTxnId++;
        Record outTxn;
        outTxn.set("outbound_trainsaction_id", txnId);
        outTxn.set("outbound_code", outboundCode);
        outTxn.set("source_code", sourceCodeVal);
        outTxn.set("customer_id", customerVal);
        outTxn.set("order_at", date);
        outTxn.set("type", normType);
        outTxn.set("status", string("COMPLETED"));
        outTxn.set("next_ship_no", numShipments + 1);
        outTxn.set("note", note.empty() ? ("Outbound " + normType + " fulfilled successfully") : note);

        db.insert("outbound_transaction", outTxn);

        // 2. Create OutboundDetail records
        HashMap<int, int> outDetIdByPid;
        for (size_t i = 0; i < mergedItems.size(); ++i)
        {
            int pid = mergedItems[i].first;
            int qty = mergedItems[i].second;
            int detId = nextOutboundDetailId++;
            outDetIdByPid[pid] = detId;

            Record outDet;
            outDet.set("outbound_detail_id", detId);
            outDet.set("outbound_transaction_id", txnId);
            outDet.set("product_id", pid);
            outDet.set("ordered_quantity", qty);
            outDet.set("actual_quantity", 0);
            db.insert("outbound_detail", outDet);
        }

        HashMap<int, size_t> poolCursorByPid;
        HashMap<int, int> outActualByPid;

        // 3. Create Shipments and dispatch Units
        for (int s = 1; s <= numShipments; s++)
        {
            int shipId = nextShipmentId++;
            string shipCode = outboundCode + "-S" + formatNumber(s, 2);
            string shipDate = advanceDate(date, s);

            Record shipRecord;
            shipRecord.set("shipment_id", shipId);
            shipRecord.set("shipment_code", shipCode);
            shipRecord.set("outbound_transaction_id", txnId);
            shipRecord.set("ship_at", shipDate);
            shipRecord.set("expected_at", shipDate);
            shipRecord.set("status", string("COMPLETED"));
            db.insert("shipment", shipRecord);

            for (size_t i = 0; i < mergedItems.size(); ++i)
            {
                int pid = mergedItems[i].first;
                int totalQty = mergedItems[i].second;
                int baseQty = totalQty / numShipments;
                int rem = totalQty % numShipments;
                int shipmentQty = baseQty + (s <= rem ? 1 : 0);

                int shipDetId = 0;
                if (shipDetTbl != nullptr && shipmentQty > 0)
                {
                    shipDetId = nextShipmentDetailId++;
                    Record shipDet;
                    shipDet.set("shipment_detail_id", shipDetId);
                    shipDet.set("shipment_id", shipId);
                    shipDet.set("product_id", pid);
                    shipDet.set("received_quantity", shipmentQty);
                    shipDet.set("actual_quantity", 0);
                    db.insert("shipment_detail", shipDet);
                }

                const auto& pool = eligibleUnitsByPid[pid];
                size_t curIdx = poolCursorByPid.contains(pid) ? poolCursorByPid[pid] : 0;
                for (int k = 0; k < shipmentQty; ++k)
                {
                    int uId = pool[curIdx].first;
                    string currentStatus = pool[curIdx].second;
                    curIdx++;

                    Record uUpd;
                    uUpd.set("status", string("OUT"));
                    db.update("unit_inventory", uId, uUpd);

                    if (histTbl != nullptr)
                    {
                        Record histRecord;
                        histRecord.set("history_id", nextStatusHistoryId++);
                        histRecord.set("unit_id", uId);
                        histRecord.set("old_status", currentStatus);
                        histRecord.set("curr_status", string("OUT"));
                        histRecord.set("transaction", shipId);
                        histRecord.set("type", string("OUTBOUND"));
                        db.insert("status_history", histRecord);
                    }
                }
                poolCursorByPid[pid] = curIdx;

                if (shipmentQty > 0)
                {
                    if (shipDetTbl != nullptr && shipDetId > 0)
                    {
                        Record sdUpd;
                        sdUpd.set("actual_quantity", shipmentQty);
                        db.update("shipment_detail", shipDetId, sdUpd);
                    }
                    outActualByPid[pid] = (outActualByPid.contains(pid) ? outActualByPid[pid] : 0) + shipmentQty;
                }
            }
        }

        for (size_t i = 0; i < mergedItems.size(); ++i)
        {
            int pid = mergedItems[i].first;
            int detId = outDetIdByPid[pid];
            int actQty = outActualByPid.contains(pid) ? outActualByPid[pid] : 0;
            Record odUpd;
            odUpd.set("actual_quantity", actQty);
            db.update("outbound_detail", detId, odUpd);
        }

        // 4. Update Product stock quantities
        if (prodTbl != nullptr)
        {
            syncProductQuantities();
        }

        cout << "[+] Generated Outbound Transaction: " << outboundCode << " (ID: " << txnId << ")\n";
        cout << "    - Shipments created: " << numShipments << "\n";
        return txnId;
    }

    int DataGenerator::generateAdjustmentMovement(const string& reason,
                                                  const Vector<AdjustItem>& items,
                                                  const string& adjustDate,
                                                  const string& sourceCode,
                                                  const string& note)
    {
        if (items.empty())
        {
            cerr << "[Generator Error] Adjustment movement requires at least one item.\n";
            return -1;
        }

        const Table* adjTbl = db.getTable("adjustment_movement");
        const Table* adjDetTbl = db.getTable("adjust_movement_detail");
        const Table* unitTbl = db.getTable("unit_inventory");
        const Table* histTbl = db.getTable("status_history");
        const Table* prodTbl = db.getTable("product");

        if (!adjTbl || !adjDetTbl || !unitTbl)
        {
            cerr << "[Generator Error] adjustment_movement, adjust_movement_detail, or unit_inventory missing.\n";
            return -1;
        }

        string fullReason;
        string reasonCode;
        if (reason == "STOCKTAKE_DIFF" || reason == "STD") { fullReason = "STOCKTAKE_DIFF"; reasonCode = "STD"; }
        else if (reason == "DAMAGED" || reason == "DMG") { fullReason = "DAMAGED"; reasonCode = "DMG"; }
        else if (reason == "OTHER" || reason == "OTH") { fullReason = "OTHER"; reasonCode = "OTH"; }
        else
        {
            cerr << "[Generator Error] Invalid adjustment movement reason: " << reason << "\n";
            return -1;
        }

        // Merge duplicate (productId, fromStatus, toStatus) lines
        Vector<AdjustItem> effectiveItems;
        for (size_t i = 0; i < items.size(); ++i)
        {
            const auto& item = items[i];
            if (item.quantity <= 0)
            {
                cerr << "[Generator Error] Adjustment movement quantity must be > 0.\n";
                return -1;
            }
            if (!isValidProduct(item.productId, false))
            {
                cerr << "[Generator Error] Product ID " << item.productId << " does not exist.\n";
                return -1;
            }
            if (isProductFrozen(item.productId))
            {
                cerr << "[Generator Error] Product ID " << item.productId << " is frozen in an active inventory check.\n";
                return -1;
            }
            if (item.fromStatus != "AVAILABLE" && item.fromStatus != "WARRANTY" && item.fromStatus != "SCRAPPED")
            {
                cerr << "[Generator Error] Invalid from_status '" << item.fromStatus << "'.\n";
                return -1;
            }
            if (item.toStatus != "AVAILABLE" && item.toStatus != "WARRANTY" && item.toStatus != "SCRAPPED")
            {
                cerr << "[Generator Error] Invalid to_status '" << item.toStatus << "' (must be internal warehouse status).\n";
                return -1;
            }
            if (item.fromStatus == item.toStatus)
            {
                cerr << "[Generator Error] from_status and to_status must differ.\n";
                return -1;
            }
            if (fullReason == "DAMAGED" && item.toStatus != "SCRAPPED")
            {
                cerr << "[Generator Error] DAMAGED adjustment movement requires to_status = SCRAPPED.\n";
                return -1;
            }

            bool merged = false;
            for (size_t j = 0; j < effectiveItems.size(); ++j)
            {
                if (effectiveItems[j].productId == item.productId &&
                    effectiveItems[j].fromStatus == item.fromStatus &&
                    effectiveItems[j].toStatus == item.toStatus)
                {
                    effectiveItems[j].quantity += item.quantity;
                    merged = true;
                    break;
                }
            }
            if (!merged)
            {
                effectiveItems.push_back(item);
            }
        }

        // Verify cumulative unit availability per (productId, fromStatus) and pre-pick units
        HashMap<string, int> requestedByKey;
        HashMap<string, Vector<int>> poolByKey;
        Vector<int> pids;
        HashMap<int, bool> seenPids;
        for (size_t i = 0; i < effectiveItems.size(); ++i)
        {
            const auto& item = effectiveItems[i];
            if (!seenPids.contains(item.productId))
            {
                seenPids[item.productId] = true;
                pids.push_back(item.productId);
            }

            string key = to_string(item.productId) + "|" + item.fromStatus;
            requestedByKey[key] = (requestedByKey.contains(key) ? requestedByKey[key] : 0) + item.quantity;
            if (!poolByKey.contains(key))
            {
                Vector<int> pool;
                for (const auto& uRec : unitTbl->view("product_id", Value(item.productId)))
                {
                    if (uRec.has("status") && holds_alternative<string>(uRec.get("status")) &&
                        get<string>(uRec.get("status")) == item.fromStatus)
                    {
                        pool.push_back(get<int>(uRec.get("unit_id")));
                    }
                }
                poolByKey[key] = pool;
            }
            if (static_cast<int>(poolByKey[key].size()) < requestedByKey[key])
            {
                cerr << "[Generator Error] Insufficient units in '" << item.fromStatus << "' for product ID "
                     << item.productId << " (need " << requestedByKey[key]
                     << ", have " << poolByKey[key].size() << ").\n";
                return -1;
            }
        }

        Vector<Vector<int>> pickedUnitIdsByLine;
        HashMap<string, size_t> poolCursorByKey;
        HashMap<int, string> shiftedUnitTargets;
        for (size_t i = 0; i < effectiveItems.size(); ++i)
        {
            const auto& item = effectiveItems[i];
            string key = to_string(item.productId) + "|" + item.fromStatus;
            const auto& pool = poolByKey[key];
            size_t cur = poolCursorByKey.contains(key) ? poolCursorByKey[key] : 0;
            Vector<int> lineUnits;
            for (int k = 0; k < item.quantity; ++k)
            {
                int uId = pool[cur++];
                lineUnits.push_back(uId);
                shiftedUnitTargets[uId] = item.toStatus;
            }
            poolCursorByKey[key] = cur;
            pickedUnitIdsByLine.push_back(lineUnits);
        }

        string date = adjustDate.empty() ? getCurrentDate() : adjustDate;

        // Validate / prepare source_code per Rule 5.3 & 5.4
        Value sourceCodeVal;
        if (!sourceCode.empty())
        {
            if (!validateCompletedCheckScope(sourceCode, pids, "adjustment_movement"))
            {
                return -1;
            }
            sourceCodeVal = sourceCode;
        }
        else if (fullReason == "STOCKTAKE_DIFF")
        {
            HashMap<int, int> emptySurplus;
            HashMap<int, bool> emptyRedisc;
            HashMap<int, bool> emptyMissing;
            string genCode = generateAuditCheckWithDiscrepancies(
                date, pids, emptySurplus, emptyRedisc, emptyMissing, shiftedUnitTargets
            );
            if (genCode.empty()) return -1;
            sourceCodeVal = genCode;
        }
        else
        {
            sourceCodeVal = Value{}; // NULL for DAMAGED / OTHER without check
        }

        int adjId = nextAdjustmentMovementId++;
        int seq = nextAdjustSeq++;
        string adjCode = "ADJ-" + reasonCode + "-" + formatNumber(seq, 6);
        updateLookupNextCodeNo("reason", "code", reasonCode, seq + 1);

        Record adjRec;
        adjRec.set("adjust_id", adjId);
        adjRec.set("adjust_code", adjCode);
        adjRec.set("source_code", sourceCodeVal);
        adjRec.set("reason", fullReason);
        adjRec.set("adjusted_at", date);
        adjRec.set("status", string("COMPLETED"));
        adjRec.set("note", note.empty() ? string("Internal adjustment movement " + adjCode) : note);
        db.insert("adjustment_movement", adjRec);

        for (size_t i = 0; i < effectiveItems.size(); ++i)
        {
            const auto& item = effectiveItems[i];
            const Vector<int>& lineUnits = pickedUnitIdsByLine[i];
            int detId = nextAdjustDetailId++;

            for (size_t k = 0; k < lineUnits.size(); ++k)
            {
                int uId = lineUnits[k];
                Record uUpd;
                uUpd.set("status", item.toStatus);
                db.update("unit_inventory", uId, uUpd);

                if (histTbl)
                {
                    Record hRec;
                    hRec.set("history_id", nextStatusHistoryId++);
                    hRec.set("unit_id", uId);
                    hRec.set("old_status", item.fromStatus);
                    hRec.set("curr_status", item.toStatus);
                    hRec.set("transaction", adjId);
                    hRec.set("type", string("ADJUSTMENT"));
                    db.insert("status_history", hRec);
                }
            }

            Record dRec;
            dRec.set("adjust_detail_id", detId);
            dRec.set("adjust_id", adjId);
            dRec.set("product_id", item.productId);
            dRec.set("from_status", item.fromStatus);
            dRec.set("to_status", item.toStatus);
            dRec.set("adjust_quantity", item.quantity);
            dRec.set("actual_quantity", static_cast<int>(lineUnits.size()));
            db.insert("adjust_movement_detail", dRec);
        }

        if (prodTbl)
        {
            syncProductQuantities();
        }

        cout << "[+] Generated Adjustment Movement: " << adjCode << " (ID: " << adjId << ")\n";
        return adjId;
    }

    GenerationSummary DataGenerator::generateBatch(int numInbound, int numOutbound, int numChecks, const string& startDate)
    {
        GenerationSummary summary;
        const Table* prodTbl = db.getTable("product");
        if (prodTbl == nullptr || prodTbl->getRowCount() == 0)
        {
            cerr << "[Generator Error] No products available in database for batch generation.\n";
            return summary;
        }

        // Rule 4.2 & 5.1.3: Only active (is_sold == TRUE), unfrozen products participate in generated transactions
        Vector<int> productIds;
        for (const auto& p : prodTbl->view())
        {
            int pid = get<int>(p.get("product_id"));
            if (isValidProduct(pid, true) && !isProductFrozen(pid))
            {
                productIds.push_back(pid);
            }
        }

        if (productIds.empty())
        {
            cerr << "[Generator Error] No active unfrozen products (is_sold == TRUE) found for batch generation.\n";
            return summary;
        }

        string curDate = startDate.empty() ? "2026-09-10" : startDate;

        // Generate Outbound Transactions FIRST or interleaved?
        // Actually, generating Inbounds first with PUR/WRP/ADJ, or choosing OUT products for WCK/CRT dynamically handles both!
        // Let's run Inbound (5 valid types) -> Outbound (4 valid types) -> Checks -> Optional Movement.
        const string inboundTypes[] = {
            "PURCHASING", "ADJUSTMENT_IN", "WARRANTY_CHECKING", "WARRANTY_REPLACEMENT", "CUSTOMER_RETURN"
        };
        for (int i = 0; i < numInbound; i++)
        {
            string inType = inboundTypes[nextRandom() % 5];
            int numItems = 1 + (nextRandom() % 3);
            int numDeliveries = 1 + (nextRandom() % 2);

            Vector<Pair<int, int>> items;
            HashMap<int, bool> usedPids;

            if (inType == "WARRANTY_CHECKING" || inType == "CUSTOMER_RETURN")
            {
                // Pick from products that currently have OUT units
                Vector<int> outEligiblePids;
                for (size_t idx = 0; idx < productIds.size(); ++idx)
                {
                    if (countUnitsByStatus(productIds[idx], "OUT") > 0)
                    {
                        outEligiblePids.push_back(productIds[idx]);
                    }
                }
                if (outEligiblePids.empty())
                {
                    inType = (nextRandom() % 2 == 0) ? "PURCHASING" : "WARRANTY_REPLACEMENT";
                }
                else
                {
                    for (int j = 0; j < numItems; j++)
                    {
                        int pid = outEligiblePids[nextRandom() % outEligiblePids.size()];
                        if (usedPids.contains(pid)) continue;
                        usedPids[pid] = true;
                        int outAvail = countUnitsByStatus(pid, "OUT");
                        int maxTake = outAvail < 5 ? outAvail : 5;
                        int qty = 1 + (nextRandom() % maxTake);
                        items.push_back(Pair<int, int>(pid, qty));
                    }
                }
            }

            if (items.empty())
            {
                for (int j = 0; j < numItems; j++)
                {
                    int pid = productIds[nextRandom() % productIds.size()];
                    if (usedPids.contains(pid)) continue;
                    usedPids[pid] = true;
                    int qty = 5 + (nextRandom() % 15);
                    items.push_back(Pair<int, int>(pid, qty));
                }
            }

            int id = generateInbound(inType, items, numDeliveries, curDate);
            if (id > 0)
            {
                summary.inboundTxnsGenerated++;
                summary.deliveriesGenerated += numDeliveries;
                for (size_t k = 0; k < items.size(); ++k)
                {
                    summary.unitsCreated += items[k].second;
                }
                if (inType == "ADJUSTMENT_IN")
                {
                    summary.inventoryChecksGenerated++;
                }
            }
            curDate = advanceDate(curDate, 1);
        }

        // Generate Outbound Transactions (all 4 valid types, no RTS)
        const string outboundTypes[] = {
            "SELLING", "ADJUSTMENT_OUT", "WARRANTY_RETURN", "WARRANTY_REPLACEMENT_TO_CUSTOMER"
        };
        for (int i = 0; i < numOutbound; i++)
        {
            string outType = outboundTypes[nextRandom() % 4];
            int numItems = 1 + (nextRandom() % 2);
            int numShipments = 1;

            Vector<int> eligiblePids;
            for (size_t idx = 0; idx < productIds.size(); ++idx)
            {
                if (countEligibleOutboundUnits(productIds[idx], outType) > 0)
                {
                    eligiblePids.push_back(productIds[idx]);
                }
            }
            if (eligiblePids.empty())
            {
                outType = "SELLING";
                for (size_t idx = 0; idx < productIds.size(); ++idx)
                {
                    if (countEligibleOutboundUnits(productIds[idx], outType) > 0)
                    {
                        eligiblePids.push_back(productIds[idx]);
                    }
                }
            }

            Vector<Pair<int, int>> items;
            if (!eligiblePids.empty())
            {
                HashMap<int, bool> usedPids;
                for (int j = 0; j < numItems; j++)
                {
                    int pid = eligiblePids[nextRandom() % eligiblePids.size()];
                    if (usedPids.contains(pid)) continue;
                    int elig = countEligibleOutboundUnits(pid, outType);
                    if (elig >= 1)
                    {
                        int maxQty = elig / 2;
                        if (maxQty > 4) maxQty = 4;
                        if (maxQty < 1) maxQty = 1;
                        int qty = 1 + (nextRandom() % maxQty);
                        items.push_back(Pair<int, int>(pid, qty));
                        usedPids[pid] = true;
                    }
                }
            }

            if (!items.empty())
            {
                int id = generateOutbound(outType, items, numShipments, curDate);
                if (id > 0)
                {
                    summary.outboundTxnsGenerated++;
                    summary.shipmentsGenerated += numShipments;
                    for (size_t k = 0; k < items.size(); ++k)
                    {
                        summary.unitsShipped += items[k].second;
                    }
                    if (outType == "ADJUSTMENT_OUT")
                    {
                        summary.inventoryChecksGenerated++;
                    }
                }
            }
            curDate = advanceDate(curDate, 1);
        }

        // Generate Standalone Inventory Checks (always UNIT)
        for (int k = 0; k < numChecks; k++)
        {
            int numAuditProducts = 2 + (nextRandom() % 4);
            if (numAuditProducts > static_cast<int>(productIds.size()))
            {
                numAuditProducts = static_cast<int>(productIds.size());
            }

            Vector<int> auditPids;
            HashMap<int, bool> usedAudit;
            for (int p = 0; p < numAuditProducts; p++)
            {
                int pid = productIds[nextRandom() % productIds.size()];
                if (usedAudit.contains(pid)) continue;
                usedAudit[pid] = true;
                auditPids.push_back(pid);
            }

            string code = generateInventoryCheck("UNIT", curDate, auditPids);
            if (!code.empty())
            {
                summary.inventoryChecksGenerated++;
            }
            curDate = advanceDate(curDate, 1);
        }

        // Optionally generate an internal DAMAGED adjustment movement (source_code = NULL per Rule 5.3)
        if (numChecks > 0 && !productIds.empty())
        {
            int chosenPid = -1;
            for (size_t idx = 0; idx < productIds.size(); ++idx)
            {
                int candidate = productIds[(nextRandom() + idx) % productIds.size()];
                if (countUnitsByStatus(candidate, "AVAILABLE") >= 1)
                {
                    chosenPid = candidate;
                    break;
                }
            }
            if (chosenPid > 0)
            {
                Vector<AdjustItem> adjItems;
                adjItems.push_back({chosenPid, "AVAILABLE", "SCRAPPED", 1});
                int movId = generateAdjustmentMovement("DAMAGED", adjItems, curDate, "");
                if (movId > 0)
                {
                    summary.adjustmentMovementsGenerated++;
                }
            }
        }

        return summary;
    }

    bool DataGenerator::validateIntegrity() const
    {
        const Table* prodTbl = db.getTable("product");
        const Table* unitTbl = db.getTable("unit_inventory");
        const Table* inTxnTbl = db.getTable("inbound_transaction");
        const Table* delTbl = db.getTable("delivery");
        const Table* outTxnTbl = db.getTable("outbound_transaction");
        const Table* shipTbl = db.getTable("shipment");
        const Table* histTbl = db.getTable("status_history");
        const Table* chkTbl = db.getTable("inventory_check");

        if (!prodTbl || !unitTbl || !inTxnTbl || !delTbl || !outTxnTbl || !shipTbl)
        {
            cerr << "[-] Missing core tables for integrity check.\n";
            return false;
        }

        // 1. Verify all units reference valid products and have unique serials
        HashMap<int, bool> validProductIds;
        for (const auto& p : prodTbl->view())
        {
            validProductIds[get<int>(p.get("product_id"))] = true;
        }

        HashMap<string, bool> seenSerials;
        for (const auto& u : unitTbl->view())
        {
            int pid = get<int>(u.get("product_id"));
            if (!validProductIds.contains(pid))
            {
                cerr << "[-] Unit references non-existent product_id: " << pid << "\n";
                return false;
            }

            string serial = get<string>(u.get("serial"));
            if (seenSerials.contains(serial))
            {
                cerr << "[-] Duplicate serial detected in unit_inventory: " << serial << "\n";
                return false;
            }
            seenSerials[serial] = true;
        }

        // 2. Build map of inventory check codes, statuses, and audited product scopes
        HashMap<string, bool> validCheckCodes;
        HashMap<string, string> checkStatusByCode;
        HashMap<string, int> checkIdByCode;
        HashMap<int, HashMap<int, bool>> checkScopeById;
        if (chkTbl != nullptr)
        {
            for (const auto& c : chkTbl->view())
            {
                if (holds_alternative<string>(c.get("check_code")))
                {
                    string code = get<string>(c.get("check_code"));
                    validCheckCodes[code] = true;
                    checkIdByCode[code] = get<int>(c.get("check_id"));
                    checkStatusByCode[code] = holds_alternative<string>(c.get("status"))
                                            ? get<string>(c.get("status")) : "";
                }
            }
        }
        const Table* cqTbl = db.getTable("inventory_check_quantity");
        if (cqTbl != nullptr)
        {
            for (const auto& cq : cqTbl->view())
            {
                int cId = get<int>(cq.get("check_id"));
                int pid = get<int>(cq.get("product_id"));
                checkScopeById[cId][pid] = true;
            }
        }

        // Pre-index detail product IDs by transaction ID
        const Table* inDetTbl = db.getTable("inbound_detail");
        HashMap<int, Vector<int>> inDetailPidsByTxn;
        if (inDetTbl != nullptr)
        {
            for (const auto& row : inDetTbl->view())
            {
                inDetailPidsByTxn[get<int>(row.get("inbound_transaction_id"))].push_back(get<int>(row.get("product_id")));
            }
        }
        const Table* outDetTbl = db.getTable("outbound_detail");
        HashMap<int, Vector<int>> outDetailPidsByTxn;
        if (outDetTbl != nullptr)
        {
            for (const auto& row : outDetTbl->view())
            {
                int oTxnId = row.has("outbound_transaction_id") ? get<int>(row.get("outbound_transaction_id")) : get<int>(row.get("outbound_trainsaction_id"));
                outDetailPidsByTxn[oTxnId].push_back(get<int>(row.get("product_id")));
            }
        }

        // 3. Verify Inbound Transactions & ADJUSTMENT_IN rules
        HashMap<int, bool> validInboundTxns;
        HashMap<string, bool> seenInboundSourceCodes;
        for (const auto& t : inTxnTbl->view())
        {
            int txnId = get<int>(t.get("inbound_transaction_id"));
            validInboundTxns[txnId] = true;

            string src = holds_alternative<string>(t.get("source_code")) ? get<string>(t.get("source_code")) : "";
            if (!src.empty())
            {
                if (seenInboundSourceCodes.contains(src))
                {
                    cerr << "[-] Duplicate source_code in inbound_transaction: " << src << "\n";
                    return false;
                }
                seenInboundSourceCodes[src] = true;
            }

            string type = get<string>(t.get("type"));
            if (type == "ADJUSTMENT_IN")
            {
                // Must NOT have customer
                if (!holds_alternative<monostate>(t.get("customer_id")))
                {
                    cerr << "[-] Inbound Transaction " << txnId << " (ADJUSTMENT_IN) must NOT have customer_id.\n";
                    return false;
                }
                // source_code MUST reference a valid COMPLETED check_code
                if (src.empty() || !validCheckCodes.contains(src))
                {
                    cerr << "[-] Inbound Transaction " << txnId << " (ADJUSTMENT_IN) source_code (" << src
                         << ") does not reference a valid inventory_check code.\n";
                    return false;
                }
                if (checkStatusByCode[src] != "COMPLETED")
                {
                    cerr << "[-] Inbound Transaction " << txnId << " (ADJUSTMENT_IN) references non-COMPLETED check '" << src << "'.\n";
                    return false;
                }
                int chkId = checkIdByCode[src];
                if (inDetailPidsByTxn.contains(txnId))
                {
                    const auto& pids = inDetailPidsByTxn[txnId];
                    for (size_t i = 0; i < pids.size(); ++i)
                    {
                        if (!checkScopeById[chkId].contains(pids[i]))
                        {
                            cerr << "[-] Inbound Transaction " << txnId << " product ID " << pids[i]
                                 << " is outside scope of check '" << src << "'.\n";
                            return false;
                        }
                    }
                }
            }
            else
            {
                if (holds_alternative<monostate>(t.get("customer_id")))
                {
                    cerr << "[-] Commercial Inbound Transaction " << txnId << " (" << type << ") must have a customer_id.\n";
                    return false;
                }
            }
        }

        // Deliveries link to valid Inbound Transactions
        for (const auto& d : delTbl->view())
        {
            int txnId = get<int>(d.get("inbound_transaction_id"));
            if (!validInboundTxns.contains(txnId))
            {
                cerr << "[-] Delivery references invalid inbound_transaction_id: " << txnId << "\n";
                return false;
            }
        }

        // 4. Verify Outbound Transactions & ADJUSTMENT_OUT rules
        HashMap<int, bool> validOutboundTxns;
        HashMap<string, bool> seenOutboundSourceCodes;
        for (const auto& t : outTxnTbl->view())
        {
            int txnId = get<int>(t.get("outbound_trainsaction_id"));
            validOutboundTxns[txnId] = true;

            string src = holds_alternative<string>(t.get("source_code")) ? get<string>(t.get("source_code")) : "";
            if (!src.empty())
            {
                if (seenOutboundSourceCodes.contains(src))
                {
                    cerr << "[-] Duplicate source_code in outbound_transaction: " << src << "\n";
                    return false;
                }
                seenOutboundSourceCodes[src] = true;
            }

            string type = get<string>(t.get("type"));
            if (type == "RETURN_TO_SUPPLIER" || type == "RTS")
            {
                cerr << "[-] Outbound Transaction " << txnId << " uses removed type '" << type << "'.\n";
                return false;
            }
            if (type == "ADJUSTMENT_OUT")
            {
                // Must NOT have customer
                if (!holds_alternative<monostate>(t.get("customer_id")))
                {
                    cerr << "[-] Outbound Transaction " << txnId << " (ADJUSTMENT_OUT) must NOT have customer_id.\n";
                    return false;
                }
                // source_code MUST reference a valid COMPLETED check_code
                if (src.empty() || !validCheckCodes.contains(src))
                {
                    cerr << "[-] Outbound Transaction " << txnId << " (ADJUSTMENT_OUT) source_code (" << src
                         << ") does not reference a valid inventory_check code.\n";
                    return false;
                }
                if (checkStatusByCode[src] != "COMPLETED")
                {
                    cerr << "[-] Outbound Transaction " << txnId << " (ADJUSTMENT_OUT) references non-COMPLETED check '" << src << "'.\n";
                    return false;
                }
                int chkId = checkIdByCode[src];
                if (outDetailPidsByTxn.contains(txnId))
                {
                    const auto& pids = outDetailPidsByTxn[txnId];
                    for (size_t i = 0; i < pids.size(); ++i)
                    {
                        if (!checkScopeById[chkId].contains(pids[i]))
                        {
                            cerr << "[-] Outbound Transaction " << txnId << " product ID " << pids[i]
                                 << " is outside scope of check '" << src << "'.\n";
                            return false;
                        }
                    }
                }
            }
            else
            {
                if (holds_alternative<monostate>(t.get("customer_id")))
                {
                    cerr << "[-] Commercial Outbound Transaction " << txnId << " (" << type << ") must have a customer_id.\n";
                    return false;
                }
            }
        }

        // Shipments link to valid Outbound Transactions
        for (const auto& s : shipTbl->view())
        {
            int txnId = s.has("outbound_transaction_id") ? get<int>(s.get("outbound_transaction_id")) : 0;
            if (!validOutboundTxns.contains(txnId))
            {
                cerr << "[-] Shipment references invalid outbound_transaction_id: " << txnId << "\n";
                return false;
            }
        }

        // 5. Verify actual_quantity constraints on detail tables
        if (inDetTbl != nullptr)
        {
            for (const auto& row : inDetTbl->view())
            {
                int ord = get<int>(row.get("ordered_quantity"));
                int act = get<int>(row.get("actual_quantity"));
                if (act > ord)
                {
                    cerr << "[-] InboundDetail actual_quantity (" << act << ") exceeds ordered_quantity (" << ord << ")\n";
                    return false;
                }
            }
        }

        const Table* delDetTbl = db.getTable("delivery_detail");
        if (delDetTbl != nullptr)
        {
            for (const auto& row : delDetTbl->view())
            {
                int rec = get<int>(row.get("received_quantity"));
                int act = get<int>(row.get("actual_quantity"));
                if (act > rec)
                {
                    cerr << "[-] DeliveryDetail actual_quantity (" << act << ") exceeds received_quantity (" << rec << ")\n";
                    return false;
                }
            }
        }

        if (outDetTbl != nullptr)
        {
            for (const auto& row : outDetTbl->view())
            {
                int ord = get<int>(row.get("ordered_quantity"));
                int act = get<int>(row.get("actual_quantity"));
                if (act > ord)
                {
                    cerr << "[-] OutboundDetail actual_quantity (" << act << ") exceeds ordered_quantity (" << ord << ")\n";
                    return false;
                }
            }
        }

        const Table* shipDetTbl = db.getTable("shipment_detail");
        if (shipDetTbl != nullptr)
        {
            for (const auto& row : shipDetTbl->view())
            {
                int rec = get<int>(row.get("received_quantity"));
                int act = get<int>(row.get("actual_quantity"));
                if (act > rec)
                {
                    cerr << "[-] ShipmentDetail actual_quantity (" << act << ") exceeds received_quantity (" << rec << ")\n";
                    return false;
                }
            }
        }

        // 6. Verify Inbound status history rule: old_status must ONLY be NULL (monostate / "") or "OUT"
        if (histTbl != nullptr)
        {
            for (const auto& row : histTbl->view())
            {
                string transType = "";
                if (holds_alternative<string>(row.get("type")))
                {
                    transType = get<string>(row.get("type"));
                }
                if (transType == "INBOUND")
                {
                    string oldSt = "";
                    if (!isNull(row.get("old_status")) && holds_alternative<string>(row.get("old_status")))
                    {
                        oldSt = get<string>(row.get("old_status"));
                    }
                    if (!oldSt.empty() && oldSt != "OUT")
                    {
                        cerr << "[-] Status history violation: INBOUND operation had old_status = '" << oldSt
                             << "' (only NULL or OUT permitted).\n";
                        return false;
                    }
                }
            }
        }

        // 7. Verify Product is_sold rule: if is_sold == false, total quantity must be 0
        for (const auto& p : prodTbl->view())
        {
            bool isSold = true;
            if (holds_alternative<bool>(p.get("is_sold")))
            {
                isSold = get<bool>(p.get("is_sold"));
            }
            if (!isSold)
            {
                int avail = get<int>(p.get("available_quantity"));
                int warr = get<int>(p.get("warranty_quantity"));
                int scrap = get<int>(p.get("scrapped_quantity"));
                if (avail + warr + scrap != 0)
                {
                    cerr << "[-] Product " << get<int>(p.get("product_id"))
                         << " has is_sold == FALSE but non-zero stock (" << (avail + warr + scrap) << ")\n";
                    return false;
                }
            }
        }

        // 8. Verify Adjustment Movements & single-check uniqueness (Rule 5.3 & 5.4)
        const Table* adjTbl = db.getTable("adjustment_movement");
        const Table* adjDetTbl = db.getTable("adjust_movement_detail");
        HashMap<int, string> adjReasonById;
        HashMap<int, string> adjSourceById;
        if (adjTbl != nullptr)
        {
            HashMap<string, bool> seenAdjustCodes;
            HashMap<string, bool> seenSourceCodes;
            for (const auto& row : adjTbl->view())
            {
                int aId = get<int>(row.get("adjust_id"));
                string reason = holds_alternative<string>(row.get("reason")) ? get<string>(row.get("reason")) : "";
                string src = holds_alternative<string>(row.get("source_code")) ? get<string>(row.get("source_code")) : "";
                adjReasonById[aId] = reason;
                adjSourceById[aId] = src;

                if (holds_alternative<string>(row.get("adjust_code")))
                {
                    string code = get<string>(row.get("adjust_code"));
                    if (seenAdjustCodes.contains(code))
                    {
                        cerr << "[-] Duplicate adjust_code in adjustment_movement: " << code << "\n";
                        return false;
                    }
                    seenAdjustCodes[code] = true;
                }

                if (reason == "STOCKTAKE_DIFF" && src.empty())
                {
                    cerr << "[-] AdjustmentMovement " << aId << " (STOCKTAKE_DIFF) missing required source_code.\n";
                    return false;
                }

                if (!src.empty())
                {
                    if (seenSourceCodes.contains(src))
                    {
                        cerr << "[-] Duplicate source_code in adjustment_movement: " << src << "\n";
                        return false;
                    }
                    seenSourceCodes[src] = true;

                    if (!validCheckCodes.contains(src))
                    {
                        cerr << "[-] AdjustmentMovement references invalid check_code: " << src << "\n";
                        return false;
                    }
                    if (checkStatusByCode[src] != "COMPLETED")
                    {
                        cerr << "[-] AdjustmentMovement " << aId << " references non-COMPLETED check '" << src << "'.\n";
                        return false;
                    }
                }
            }
        }

        if (adjDetTbl != nullptr && adjTbl != nullptr)
        {
            HashMap<int, bool> validAdjustIds;
            for (const auto& a : adjTbl->view())
            {
                validAdjustIds[get<int>(a.get("adjust_id"))] = true;
            }

            for (const auto& row : adjDetTbl->view())
            {
                int aId = get<int>(row.get("adjust_id"));
                if (!validAdjustIds.contains(aId))
                {
                    cerr << "[-] AdjustMovementDetail references invalid adjust_id: " << aId << "\n";
                    return false;
                }
                int pid = get<int>(row.get("product_id"));
                if (!validProductIds.contains(pid))
                {
                    cerr << "[-] AdjustMovementDetail references invalid product_id: " << pid << "\n";
                    return false;
                }
                int adjQty = get<int>(row.get("adjust_quantity"));
                int actQty = get<int>(row.get("actual_quantity"));
                if (actQty > adjQty)
                {
                    cerr << "[-] AdjustMovementDetail actual_quantity exceeds adjust_quantity\n";
                    return false;
                }
                if (holds_alternative<string>(row.get("to_status")))
                {
                    string toSt = get<string>(row.get("to_status"));
                    if (toSt == "OUT")
                    {
                        cerr << "[-] AdjustMovementDetail to_status must not be OUT (Rule 5.3)\n";
                        return false;
                    }
                    if (adjReasonById.contains(aId) && adjReasonById[aId] == "DAMAGED" && toSt != "SCRAPPED")
                    {
                        cerr << "[-] AdjustMovementDetail for DAMAGED must have to_status == SCRAPPED (Rule 5.3)\n";
                        return false;
                    }
                }
                if (adjSourceById.contains(aId) && !adjSourceById[aId].empty())
                {
                    int chkId = checkIdByCode[adjSourceById[aId]];
                    if (!checkScopeById[chkId].contains(pid))
                    {
                        cerr << "[-] AdjustMovementDetail product ID " << pid
                             << " is outside scope of check '" << adjSourceById[aId] << "'.\n";
                        return false;
                    }
                }
            }
        }

        return true;
    }

    void DataGenerator::printInventorySummary() const
    {
        const Table* prodTbl = db.getTable("product");
        const Table* unitTbl = db.getTable("unit_inventory");
        const Table* inTxnTbl = db.getTable("inbound_transaction");
        const Table* delTbl = db.getTable("delivery");
        const Table* outTxnTbl = db.getTable("outbound_transaction");
        const Table* shipTbl = db.getTable("shipment");
        const Table* histTbl = db.getTable("status_history");
        const Table* chkTbl = db.getTable("inventory_check");

        int totalProducts = prodTbl ? prodTbl->getRowCount() : 0;
        int activeProducts = 0;
        int inactiveProducts = 0;
        if (prodTbl != nullptr)
        {
            for (const auto& p : prodTbl->view())
            {
                bool isSold = true;
                if (holds_alternative<bool>(p.get("is_sold")))
                {
                    isSold = get<bool>(p.get("is_sold"));
                }
                if (isSold) activeProducts++;
                else inactiveProducts++;
            }
        }

        int totalUnits = unitTbl ? unitTbl->getRowCount() : 0;
        int availableUnits = 0;
        int warrantyUnits = 0;
        int outUnits = 0;
        int scrappedUnits = 0;

        if (unitTbl != nullptr)
        {
            for (const auto& u : unitTbl->view())
            {
                string st = get<string>(u.get("status"));
                if (st == "AVAILABLE") availableUnits++;
                else if (st == "WARRANTY") warrantyUnits++;
                else if (st == "OUT") outUnits++;
                else if (st == "SCRAPPED") scrappedUnits++;
            }
        }

        cout << "\n======================================================\n";
        cout << "             WAREHOUSE INVENTORY SUMMARY              \n";
        cout << "======================================================\n";
        cout << "  - Total Master Products:        " << totalProducts
             << " (" << activeProducts << " active, " << inactiveProducts << " inactive)\n";
        cout << "  - Total Physical Units:         " << totalUnits << "\n";
        cout << "    * AVAILABLE in stock:         " << availableUnits << "\n";
        cout << "    * WARRANTY in inspection:     " << warrantyUnits << "\n";
        cout << "    * OUT (Shipped):              " << outUnits << "\n";
        cout << "    * SCRAPPED (Disposed):        " << scrappedUnits << "\n";
        cout << "------------------------------------------------------\n";
        cout << "  - Inbound Transactions:         " << (inTxnTbl ? inTxnTbl->getRowCount() : 0) << "\n";
        cout << "  - Completed Deliveries:         " << (delTbl ? delTbl->getRowCount() : 0) << "\n";
        cout << "  - Outbound Transactions:        " << (outTxnTbl ? outTxnTbl->getRowCount() : 0) << "\n";
        cout << "  - Completed Shipments:          " << (shipTbl ? shipTbl->getRowCount() : 0) << "\n";
        cout << "  - Inventory Checks:             " << (chkTbl ? chkTbl->getRowCount() : 0) << "\n";
        if (histTbl != nullptr)
        {
            cout << "  - Status History Transitions:   " << histTbl->getRowCount() << "\n";
        }
        cout << "======================================================\n";
    }

    void DataGenerator::printProductList() const
    {
        const Table* prodTbl = db.getTable("product");
        if (prodTbl == nullptr || prodTbl->getRowCount() == 0)
        {
            cout << "[!] No products found in database.\n";
            return;
        }

        cout << "\n" << setfill('=') << setw(105) << "" << setfill(' ') << "\n";
        cout << setw(5) << "ID"
             << setw(20) << "Code"
             << setw(32) << "Product Name"
             << setw(13) << "Category"
             << setw(9) << "Avail"
             << setw(9) << "Warr"
             << setw(9) << "Scrap"
             << setw(8) << "Active" << "\n";
        cout << setfill('-') << setw(105) << "" << setfill(' ') << "\n";

        for (const auto& p : prodTbl->view())
        {
            int pid = get<int>(p.get("product_id"));
            string code = get<string>(p.get("product_code"));
            string name = get<string>(p.get("product_name"));
            if (name.length() > 30) name = name.substr(0, 27) + "...";
            string cat = holds_alternative<string>(p.get("category")) ? get<string>(p.get("category")) : "";
            int avail = get<int>(p.get("available_quantity"));
            int warr = get<int>(p.get("warranty_quantity"));
            int scrap = get<int>(p.get("scrapped_quantity"));
            bool isSold = true;
            if (holds_alternative<bool>(p.get("is_sold")))
            {
                isSold = get<bool>(p.get("is_sold"));
            }

            cout << setw(5) << pid
                 << setw(20) << code
                 << setw(32) << name
                 << setw(13) << cat
                 << setw(9) << avail
                 << setw(9) << warr
                 << setw(9) << scrap
                 << setw(8) << (isSold ? "YES" : "NO") << "\n";
        }
        cout << setfill('=') << setw(105) << "" << setfill(' ') << "\n\n";
    }

    void DataGenerator::runConsoleInterface()
    {
        bool running = true;
        while (running)
        {
            cout << "\n======================================================\n";
            cout << "     WAREHOUSE TRANSACTION GENERATOR (PBL2)           \n";
            cout << "======================================================\n";
            cout << "  1. Generate Inbound Transaction (Receiving Flow)    \n";
            cout << "  2. Generate Outbound Transaction (Dispatch Flow)    \n";
            cout << "  3. Perform Inventory Check (Unit Audit: CHK-UNIT)   \n";
            cout << "  4. Generate Adjustment Movement (Internal Shift)    \n";
            cout << "  5. Toggle Product is_sold Status                    \n";
            cout << "  6. Display Products & Stock Levels                  \n";
            cout << "  7. View Inventory & Transaction Summary             \n";
            cout << "  8. Auto Generate Random Batch                       \n";
            cout << "  9. Validate Database Referential Integrity          \n";
            cout << " 10. Save Database Changes to CSV                     \n";
            cout << "  0. Exit                                             \n";
            cout << "======================================================\n";
            cout << "Enter your choice: ";

            int choice = -1;
            if (!(cin >> choice))
            {
                cin.clear();
                string dummy;
                getline(cin, dummy);
                continue;
            }

            if (choice == 0)
            {
                cout << "[+] Exiting generator. Goodbye!\n";
                running = false;
            }
            else if (choice == 1)
            {
                // Inbound Generation Workflow
                cout << "\n--- SELECT INBOUND TYPE ---\n";
                cout << "  1. PURCHASING (Default supplier order)\n";
                cout << "  2. ADJUSTMENT_IN (Stock correction increase)\n";
                cout << "  3. WARRANTY_CHECKING (Customer return for RMA)\n";
                cout << "  4. WARRANTY_REPLACEMENT (Replacement from vendor)\n";
                cout << "  5. CUSTOMER_RETURN (Sale return)\n";
                cout << "Select type (1-5): ";
                int typeChoice = 1;
                cin >> typeChoice;

                string inType = "PURCHASING";
                if (typeChoice == 2) inType = "ADJUSTMENT_IN";
                else if (typeChoice == 3) inType = "WARRANTY_CHECKING";
                else if (typeChoice == 4) inType = "WARRANTY_REPLACEMENT";
                else if (typeChoice == 5) inType = "CUSTOMER_RETURN";

                string sourceCode = "";
                if (inType == "ADJUSTMENT_IN")
                {
                    cout << "[!] ADJUSTMENT_IN Notice: Customer is set to NULL per business rules.\n";
                    cout << "Enter inventory check code (leave empty to auto-generate): ";
                    string dummy;
                    getline(cin, dummy); // Consume newline
                    getline(cin, sourceCode);
                }

                printProductList();

                cout << "How many distinct products to receive? ";
                int numProducts = 1;
                cin >> numProducts;
                if (numProducts < 1) numProducts = 1;

                Vector<Pair<int, int>> items;
                for (int i = 0; i < numProducts; i++)
                {
                    cout << "  Product #" << (i + 1) << " - Enter Product ID: ";
                    int pid;
                    cin >> pid;
                    cout << "  Product #" << (i + 1) << " - Enter Quantity: ";
                    int qty;
                    cin >> qty;
                    if (qty > 0)
                    {
                        items.push_back(Pair<int, int>(pid, qty));
                    }
                }

                cout << "Number of required Delivery records (e.g. 1, 2): ";
                int numDeliveries = 1;
                cin >> numDeliveries;

                cout << "[+] Processing inbound transaction...\n";
                int id = generateInbound(inType, items, numDeliveries, "", "", sourceCode);
                if (id > 0)
                {
                    cout << "[SUCCESS] Inbound transaction created with ID: " << id << "\n";
                }
            }
            else if (choice == 2)
            {
                // Outbound Generation Workflow
                cout << "\n--- SELECT OUTBOUND TYPE ---\n";
                cout << "  1. SELLING (Retail / Order dispatch)\n";
                cout << "  2. ADJUSTMENT_OUT (Stock correction decrease)\n";
                cout << "  3. WARRANTY_RETURN (Return defective part to vendor)\n";
                cout << "  4. WARRANTY_REPLACEMENT_TO_CUSTOMER (Replacement to customer)\n";
                cout << "Select type (1-4): ";
                int typeChoice = 1;
                cin >> typeChoice;

                string outType = "SELLING";
                if (typeChoice == 2) outType = "ADJUSTMENT_OUT";
                else if (typeChoice == 3) outType = "WARRANTY_RETURN";
                else if (typeChoice == 4) outType = "WARRANTY_REPLACEMENT_TO_CUSTOMER";

                string sourceCode = "";
                if (outType == "ADJUSTMENT_OUT")
                {
                    cout << "[!] ADJUSTMENT_OUT Notice: Customer is set to NULL per business rules.\n";
                    cout << "Enter inventory check code (leave empty to auto-generate): ";
                    string dummy;
                    getline(cin, dummy); // Consume newline
                    getline(cin, sourceCode);
                }

                printProductList();

                cout << "How many distinct products to dispatch? ";
                int numProducts = 1;
                cin >> numProducts;
                if (numProducts < 1) numProducts = 1;

                Vector<Pair<int, int>> items;
                for (int i = 0; i < numProducts; i++)
                {
                    cout << "  Product #" << (i + 1) << " - Enter Product ID: ";
                    int pid;
                    cin >> pid;
                    int stock = getAvailableStock(pid);
                    cout << "    [Stock available: " << stock << "]\n";
                    cout << "  Product #" << (i + 1) << " - Enter Quantity: ";
                    int qty;
                    cin >> qty;
                    if (qty > 0)
                    {
                        items.push_back(Pair<int, int>(pid, qty));
                    }
                }

                cout << "Number of required Shipment records (e.g. 1, 2): ";
                int numShipments = 1;
                cin >> numShipments;

                cout << "[+] Processing outbound transaction...\n";
                int id = generateOutbound(outType, items, numShipments, "", "", sourceCode);
                if (id > 0)
                {
                    cout << "[SUCCESS] Outbound transaction created with ID: " << id << "\n";
                }
            }
            else if (choice == 3)
            {
                // Perform Inventory Check (Rule 4.3: UNIT only)
                cout << "\n--- INVENTORY CHECK (UNIT AUDIT) ---\n";
                string checkType = "UNIT";
                string code = generateInventoryCheck(checkType);
                cout << "[SUCCESS] Created Inventory Check: " << code << "\n";
            }
            else if (choice == 4)
            {
                // Generate Adjustment Movement
                cout << "\n--- GENERATE ADJUSTMENT MOVEMENT ---\n";
                cout << "Select Reason:\n";
                cout << "  1. STOCKTAKE_DIFF (Discrepancy from stock count)\n";
                cout << "  2. DAMAGED (Damaged goods reclassified)\n";
                cout << "  3. OTHER (Other warehouse reclassification)\n";
                cout << "Enter choice (1-3): ";
                int rChoice = 1;
                cin >> rChoice;
                string rsn = "STOCKTAKE_DIFF";
                if (rChoice == 2) rsn = "DAMAGED";
                else if (rChoice == 3) rsn = "OTHER";

                printProductList();
                cout << "Enter Product ID: ";
                int pid = 0;
                cin >> pid;

                cout << "From Status (1=AVAILABLE, 2=WARRANTY, 3=SCRAPPED): ";
                int fromStChoice = 1;
                cin >> fromStChoice;
                string fromSt = "AVAILABLE";
                if (fromStChoice == 2) fromSt = "WARRANTY";
                else if (fromStChoice == 3) fromSt = "SCRAPPED";

                cout << "To Status (1=AVAILABLE, 2=WARRANTY, 3=SCRAPPED): ";
                int toStChoice = 3;
                cin >> toStChoice;
                string toSt = "SCRAPPED";
                if (toStChoice == 1) toSt = "AVAILABLE";
                else if (toStChoice == 2) toSt = "WARRANTY";

                cout << "Quantity to adjust: ";
                int qty = 1;
                cin >> qty;

                cout << "Enter Source Check Code (required for STOCKTAKE_DIFF if existing, leave empty to auto-generate for STOCKTAKE_DIFF or NULL for DAMAGED/OTHER): ";
                string dummy, srcCode;
                getline(cin, dummy);
                getline(cin, srcCode);

                Vector<AdjustItem> adjItems;
                adjItems.push_back({pid, fromSt, toSt, qty});
                int id = generateAdjustmentMovement(rsn, adjItems, "", srcCode);
                if (id > 0)
                {
                    cout << "[SUCCESS] Adjustment Movement created with ID: " << id << "\n";
                }
            }
            else if (choice == 5)
            {
                // Toggle Product is_sold
                printProductList();
                cout << "Enter Product ID to toggle is_sold: ";
                int pid = 0;
                cin >> pid;
                cout << "Set is_sold to (1 = TRUE / Active, 0 = FALSE / Inactive): ";
                int flag = 1;
                cin >> flag;

                cout << "Enter reason for status change: ";
                string dummy, reason;
                getline(cin, dummy);
                getline(cin, reason);

                updateProductIsSold(pid, flag == 1, 1, reason);
            }
            else if (choice == 6)
            {
                printProductList();
            }
            else if (choice == 7)
            {
                printInventorySummary();
            }
            else if (choice == 8)
            {
                cout << "Enter number of Inbounds to generate: ";
                int nIn = 5;
                cin >> nIn;
                cout << "Enter number of Outbounds to generate: ";
                int nOut = 5;
                cin >> nOut;
                cout << "Enter number of Inventory Checks to generate: ";
                int nChk = 2;
                cin >> nChk;

                GenerationSummary summary = generateBatch(nIn, nOut, nChk);
                cout << "\n======================================================\n";
                cout << "               BATCH GENERATION REPORT                \n";
                cout << "======================================================\n";
                cout << "  - Inbound Transactions:         " << summary.inboundTxnsGenerated << "\n";
                cout << "  - Deliveries Completed:         " << summary.deliveriesGenerated << "\n";
                cout << "  - Units Added to Inventory:     " << summary.unitsCreated << "\n";
                cout << "  - Outbound Transactions:        " << summary.outboundTxnsGenerated << "\n";
                cout << "  - Shipments Completed:          " << summary.shipmentsGenerated << "\n";
                cout << "  - Units Dispatched (OUT):       " << summary.unitsShipped << "\n";
                cout << "  - Inventory Checks Triggered:   " << summary.inventoryChecksGenerated << "\n";
                cout << "  - Adjustment Movements Created: " << summary.adjustmentMovementsGenerated << "\n";
                cout << "======================================================\n";
            }
            else if (choice == 9)
            {
                bool valid = validateIntegrity();
                cout << "\n[+] Integrity validation: " << (valid ? "[PASSED] 100% VALID" : "[FAILED]") << "\n";
            }
            else if (choice == 10)
            {
                cout << "[+] Saving database to 'data' folder...\n";
                bool saved = Schema::saveDatabase(db, "data", false);
                cout << "[+] Save result: " << (saved ? "SUCCESS" : "FAILED") << "\n";
            }
            else
            {
                cout << "[!] Invalid option. Please choose between 0 and 10.\n";
            }
        }
    }
}
