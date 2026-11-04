#include "Generator.h"
#include "data_io/DatabaseIO.h"

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
        // 64-bit Linear Congruential Generator (LCG)
        rngSeed = rngSeed * 6364136223846793005ULL + 1ULL;
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
            s += formatNumber(nextUnitId, 5);
        } while (existingSerials.contains(s));
        existingSerials[s] = true;
        return s;
    }

    void DataGenerator::scanExistingDatabase()
    {
        // 1. Scan Unit Inventory
        Table* unitTbl = db.getTable("unit_inventory");
        if (unitTbl != nullptr)
        {
            const auto& rows = unitTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("unit_id"));
                if (id >= nextUnitId) nextUnitId = id + 1;
                if (holds_alternative<string>(rows[i].get("serial")))
                {
                    existingSerials[get<string>(rows[i].get("serial"))] = true;
                }
            }
        }

        // 2. Scan Inbound Transaction
        Table* inTxnTbl = db.getTable("inbound_transaction");
        if (inTxnTbl != nullptr)
        {
            const auto& rows = inTxnTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("inbound_transaction_id"));
                if (id >= nextInboundTxnId) nextInboundTxnId = id + 1;

                if (holds_alternative<string>(rows[i].get("inbound_code")))
                {
                    string code = get<string>(rows[i].get("inbound_code"));
                    // Format: IN-{TYPE}-{SEQ} e.g. IN-PUR-000001
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
        Table* inDetTbl = db.getTable("inbound_detail");
        if (inDetTbl != nullptr)
        {
            const auto& rows = inDetTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("inbound_detail_id"));
                if (id >= nextInboundDetailId) nextInboundDetailId = id + 1;
            }
        }

        // 4. Scan Delivery
        Table* delTbl = db.getTable("delivery");
        if (delTbl != nullptr)
        {
            const auto& rows = delTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("delivery_id"));
                if (id >= nextDeliveryId) nextDeliveryId = id + 1;
            }
        }

        // 4b. Scan Delivery Detail
        Table* delDetTbl = db.getTable("delivery_detail");
        if (delDetTbl != nullptr)
        {
            const auto& rows = delDetTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("delivery_detail_id"));
                if (id >= nextDeliveryDetailId) nextDeliveryDetailId = id + 1;
            }
        }

        // 5. Scan Outbound Transaction
        Table* outTxnTbl = db.getTable("outbound_transaction");
        if (outTxnTbl != nullptr)
        {
            const auto& rows = outTxnTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("outbound_trainsaction_id"));
                if (id >= nextOutboundTxnId) nextOutboundTxnId = id + 1;

                if (holds_alternative<string>(rows[i].get("outbound_code")))
                {
                    string code = get<string>(rows[i].get("outbound_code"));
                    // Format: OUT-{TYPE}-{SEQ} e.g. OUT-SEL-000001
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
        Table* outDetTbl = db.getTable("outbound_detail");
        if (outDetTbl != nullptr)
        {
            const auto& rows = outDetTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("outbound_detail_id"));
                if (id >= nextOutboundDetailId) nextOutboundDetailId = id + 1;
            }
        }

        // 7. Scan Shipment
        Table* shipTbl = db.getTable("shipment");
        if (shipTbl != nullptr)
        {
            const auto& rows = shipTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("shipment_id"));
                if (id >= nextShipmentId) nextShipmentId = id + 1;
            }
        }

        // 7b. Scan Shipment Detail
        Table* shipDetTbl = db.getTable("shipment_detail");
        if (shipDetTbl != nullptr)
        {
            const auto& rows = shipDetTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("shipment_detail_id"));
                if (id >= nextShipmentDetailId) nextShipmentDetailId = id + 1;
            }
        }

        // 8. Scan Status History
        Table* histTbl = db.getTable("status_history");
        if (histTbl != nullptr)
        {
            const auto& rows = histTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("history_id"));
                if (id >= nextStatusHistoryId) nextStatusHistoryId = id + 1;
            }
        }

        // 9. Scan Inventory Check
        Table* chkTbl = db.getTable("inventory_check");
        if (chkTbl != nullptr)
        {
            const auto& rows = chkTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("check_id"));
                if (id >= nextInventoryCheckId) nextInventoryCheckId = id + 1;

                if (holds_alternative<string>(rows[i].get("check_code")))
                {
                    string code = get<string>(rows[i].get("check_code"));
                    // Format: CHK-{TYPE}-{SEQ} e.g. CHK-QUAN-000001
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
        Table* chkQuanTbl = db.getTable("inventory_check_quantity");
        if (chkQuanTbl != nullptr)
        {
            const auto& rows = chkQuanTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("check_quantity_id"));
                if (id >= nextCheckQuantityId) nextCheckQuantityId = id + 1;
            }
        }

        // 11. Scan Inventory Check Unit
        Table* chkUnitTbl = db.getTable("inventory_check_unit");
        if (chkUnitTbl != nullptr)
        {
            const auto& rows = chkUnitTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("check_unit_id"));
                if (id >= nextCheckUnitId) nextCheckUnitId = id + 1;
            }
        }

        // 12. Scan Adjustment Movement
        Table* adjTbl = db.getTable("adjustment_movement");
        if (adjTbl != nullptr)
        {
            const auto& rows = adjTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("adjust_id"));
                if (id >= nextAdjustmentMovementId) nextAdjustmentMovementId = id + 1;

                if (holds_alternative<string>(rows[i].get("adjust_code")))
                {
                    string code = get<string>(rows[i].get("adjust_code"));
                    // Format: ADJ-{REASON}-{SEQ} e.g. ADJ-STD-000001
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
        Table* adjDetTbl = db.getTable("adjust_movement_detail");
        if (adjDetTbl != nullptr)
        {
            const auto& rows = adjDetTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("adjust_detail_id"));
                if (id >= nextAdjustDetailId) nextAdjustDetailId = id + 1;
            }
        }

        // 14. Scan is_sold_changing
        Table* isSoldTbl = db.getTable("is_sold_changing");
        if (isSoldTbl != nullptr)
        {
            const auto& rows = isSoldTbl->getRows();
            for (size_t i = 0; i < rows.size(); i++)
            {
                int id = get<int>(rows[i].get("history_id"));
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
        Table* unitTbl = db.getTable("unit_inventory");
        if (unitTbl == nullptr) return 0;

        int count = 0;
        for (const auto& r : unitTbl->getRows())
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
        Table* prodTbl = db.getTable("product");
        Table* unitTbl = db.getTable("unit_inventory");
        if (prodTbl == nullptr || unitTbl == nullptr) return;

        HashMap<int, int> availableCounts;
        HashMap<int, int> warrantyCounts;
        HashMap<int, int> scrappedCounts;

        for (const auto& u : unitTbl->getRows())
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

        auto& prodRows = prodTbl->getRows();
        for (size_t i = 0; i < prodRows.size(); i++)
        {
            int pid = get<int>(prodRows[i].get("product_id"));
            int avail = availableCounts.contains(pid) ? availableCounts[pid] : 0;
            int warr = warrantyCounts.contains(pid) ? warrantyCounts[pid] : 0;
            int scrap = scrappedCounts.contains(pid) ? scrappedCounts[pid] : 0;

            prodRows[i].set("available_quantity", avail);
            prodRows[i].set("warranty_quantity", warr);
            prodRows[i].set("scrapped_quantity", scrap);
        }
    }

    bool DataGenerator::updateProductIsSold(int productId, bool newIsSold, int changedBy, const string& reason)
    {
        (void)changedBy;
        (void)reason;
        Table* prodTbl = db.getTable("product");
        Table* histTbl = db.getTable("is_sold_changing");
        if (!prodTbl) return false;

        auto& rows = prodTbl->getRows();
        for (size_t i = 0; i < rows.size(); i++)
        {
            if (get<int>(rows[i].get("product_id")) == productId)
            {
                bool curIsSold = true;
                if (holds_alternative<bool>(rows[i].get("is_sold")))
                {
                    curIsSold = get<bool>(rows[i].get("is_sold"));
                }

                if (curIsSold == newIsSold) return true; // No change needed

                // Rule: is_sold can ONLY be set to false if total quantity of the product is 0.
                if (!newIsSold)
                {
                    int avail = get<int>(rows[i].get("available_quantity"));
                    int warr = get<int>(rows[i].get("warranty_quantity"));
                    int scrap = get<int>(rows[i].get("scrapped_quantity"));
                    int totalStock = avail + warr + scrap;

                    if (totalStock > 0)
                    {
                        cerr << "[Generator Error] Cannot set is_sold = FALSE for product " << productId
                             << " because stock remains in warehouse (" << totalStock << " units).\n";
                        return false;
                    }
                }

                rows[i].set("is_sold", newIsSold);

                if (histTbl != nullptr)
                {
                    Record hist;
                    hist.set("history_id", nextIsSoldChangingId++);
                    hist.set("product_id", productId);
                    hist.set("date", getCurrentDate());
                    hist.set("old_status", curIsSold);
                    hist.set("new_status", newIsSold);
                    histTbl->insert(hist);
                }

                cout << "[+] Product ID " << productId << " is_sold changed to "
                     << (newIsSold ? "TRUE" : "FALSE") << "\n";
                return true;
            }
        }

        cerr << "[Generator Error] Product ID " << productId << " not found.\n";
        return false;
    }

    string DataGenerator::generateInventoryCheck(const string& checkType,
                                                const string& checkDate,
                                                const Vector<int>& productIds)
    {
        Table* chkTbl = db.getTable("inventory_check");
        if (!chkTbl) return "";

        string actualType = "UNIT";
        string date = checkDate.empty() ? getCurrentDate() : checkDate;

        int chkId = nextInventoryCheckId++;
        string code = "CHK-" + actualType + "-" + formatNumber(nextCheckSeq++, 6);

        Record chkRec;
        chkRec.set("check_id", chkId);
        chkRec.set("check_code", code);
        chkRec.set("check_type", actualType);
        chkRec.set("check_at", date);
        chkRec.set("status", string("COMPLETED"));
        chkRec.set("note", string("Inventory check audit (" + actualType + ") completed"));
        chkTbl->insert(chkRec);

        Table* chkQuanTbl = db.getTable("inventory_check_quantity");
        Table* unitTbl = db.getTable("unit_inventory");
        Table* prodTbl = db.getTable("product");

        Vector<int> pids = productIds;
        if (pids.empty() && prodTbl)
        {
            for (const auto& r : prodTbl->getRows())
            {
                pids.push_back(get<int>(r.get("product_id")));
            }
        }

        HashMap<int, int> pidToCqId;

        if (chkQuanTbl)
        {
            for (size_t i = 0; i < pids.size(); i++)
            {
                int pid = pids[i];
                int avail = 0, warr = 0, scrap = 0;
                if (unitTbl)
                {
                    for (const auto& u : unitTbl->getRows())
                    {
                        if (get<int>(u.get("product_id")) == pid && holds_alternative<string>(u.get("status")))
                        {
                            string st = get<string>(u.get("status"));
                            if (st == "AVAILABLE") avail++;
                            else if (st == "WARRANTY") warr++;
                            else if (st == "SCRAPPED") scrap++;
                        }
                    }
                }

                int cqId = nextCheckQuantityId++;
                pidToCqId[pid] = cqId;

                Record qRec;
                qRec.set("check_quantity_id", cqId);
                qRec.set("check_id", chkId);
                qRec.set("product_id", pid);
                qRec.set("system_available_quan", avail);
                qRec.set("system_warranty_quan", warr);
                qRec.set("system_scrapped_quan", scrap);
                qRec.set("actual_available_quan", avail);
                qRec.set("actual_warranty_quan", warr);
                qRec.set("actual_scrapped_quan", scrap);
                qRec.set("note", string(""));
                chkQuanTbl->insert(qRec);
            }
        }

        if (actualType == "UNIT")
        {
            Table* chkUnitTbl = db.getTable("inventory_check_unit");
            if (chkUnitTbl && unitTbl)
            {
                HashMap<int, bool> filterPids;
                for (size_t i = 0; i < pids.size(); i++) filterPids[pids[i]] = true;

                for (const auto& u : unitTbl->getRows())
                {
                    int pid = get<int>(u.get("product_id"));
                    if (!filterPids.contains(pid)) continue;

                    string st = get<string>(u.get("status"));
                    if (st == "OUT") continue; // Chỉ kiểm kê các đơn vị hiện vật đang thực tế tồn tại trong kho

                    string sn = get<string>(u.get("serial"));
                    int cqId = pidToCqId.contains(pid) ? pidToCqId[pid] : 1;

                    Record uRec;
                    uRec.set("check_unit_id", nextCheckUnitId++);
                    uRec.set("check_id", chkId);
                    uRec.set("check_quantity_id", cqId);
                    uRec.set("serial", sn);
                    uRec.set("system_status", st);
                    uRec.set("scanned_status", st);
                    chkUnitTbl->insert(uRec);
                }
            }
        }

        cout << "[+] Generated Inventory Check: " << code << " (ID: " << chkId << ")\n";
        return code;
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

        Table* inTxnTbl = db.getTable("inbound_transaction");
        Table* inDetTbl = db.getTable("inbound_detail");
        Table* delTbl = db.getTable("delivery");
        Table* delDetTbl = db.getTable("delivery_detail");
        Table* unitTbl = db.getTable("unit_inventory");
        Table* histTbl = db.getTable("status_history");
        Table* prodTbl = db.getTable("product");

        if (!inTxnTbl || !inDetTbl || !delTbl || !delDetTbl || !unitTbl)
        {
            cerr << "[Generator Error] Missing required database tables for inbound generation.\n";
            return -1;
        }

        // Determine Type Code
        string typeCode = "PUR";
        if (inboundType == "ADJUSTMENT_IN") typeCode = "ADJ";
        else if (inboundType == "WARRANTY_CHECKING") typeCode = "WCK";
        else if (inboundType == "WARRANTY_REPLACEMENT") typeCode = "WRP";
        else if (inboundType == "CUSTOMER_RETURN") typeCode = "CRT";

        int seq = (inboundTypeCounters.contains(typeCode) ? inboundTypeCounters[typeCode] : 0) + 1;
        inboundTypeCounters[typeCode] = seq;
        string inboundCode = "IN-" + typeCode + "-" + formatNumber(seq, 6);

        if (numDeliveries < 1) numDeliveries = 1;
        string date = orderDate.empty() ? getCurrentDate() : orderDate;

        // Rule: For ADJUSTMENT_IN, customer_id MUST be NULL; otherwise valid customer
        Value customerVal;
        if (inboundType == "ADJUSTMENT_IN")
        {
            customerVal = Value{}; // NULL
        }
        else if (inboundType == "WARRANTY_CHECKING" || inboundType == "CUSTOMER_RETURN")
        {
            customerVal = 1; // Individual customer
        }
        else
        {
            customerVal = 4; // Supplier / Business
        }

        // Rule: For ADJUSTMENT_IN, source_code MUST reference a check_code from inventory_check
        Value sourceCodeVal;
        if (inboundType == "ADJUSTMENT_IN")
        {
            string finalSource = sourceCode;
            if (finalSource.empty())
            {
                // Auto-generate quantity audit check for the items involved
                Vector<int> pids;
                for (const auto& item : items) pids.push_back(item.first);
                finalSource = generateInventoryCheck("UNIT", date, pids);
            }
            sourceCodeVal = finalSource;
        }
        else
        {
            if (!sourceCode.empty())
            {
                sourceCodeVal = sourceCode;
            }
            else
            {
                sourceCodeVal = Value{}; // NULL
            }
        }

        // 1. Create InboundTransaction record
        int txnId = nextInboundTxnId++;
        Record inTxn;
        inTxn.set("inbound_transaction_id", txnId);
        inTxn.set("inbound_code", inboundCode);
        inTxn.set("source_code", sourceCodeVal);
        inTxn.set("customer_id", customerVal);
        inTxn.set("order_at", date);
        inTxn.set("type", inboundType);
        inTxn.set("status", string("CONFIRMED"));
        inTxn.set("next_delivery_no", numDeliveries + 1);
        inTxn.set("note", note.empty() ? ("Inbound " + inboundType + " received successfully") : note);

        inTxnTbl->insert(inTxn);

        // 2. Create InboundDetail records
        for (const auto& item : items)
        {
            int pid = item.first;
            int qty = item.second;
            if (qty <= 0) continue;

            Record inDet;
            inDet.set("inbound_detail_id", nextInboundDetailId++);
            inDet.set("inbound_transaction_id", txnId);
            inDet.set("product_id", pid);
            inDet.set("ordered_quantity", qty);
            inDet.set("actual_quantity", 0);
            inDetTbl->insert(inDet);
        }

        // 3. Create Delivery and DeliveryUnitDetail records
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
            delTbl->insert(delRecord);

            for (const auto& item : items)
            {
                int pid = item.first;
                int totalQty = item.second;
                int baseQty = totalQty / numDeliveries;
                int rem = totalQty % numDeliveries;
                int deliveryQty = baseQty + (d <= rem ? 1 : 0);

                if (delDetTbl != nullptr && deliveryQty > 0)
                {
                    Record delDet;
                    delDet.set("delivery_detail_id", nextDeliveryDetailId++);
                    delDet.set("delivery_id", delId);
                    delDet.set("product_id", pid);
                    delDet.set("received_quantity", deliveryQty);
                    delDet.set("actual_quantity", 0);
                    delDetTbl->insert(delDet);
                }

                // Inbound Unit Processing
                // RULE: Inbound operations ONLY permit transitioning units from OUT or NULL!
                // Never modify units currently in AVAILABLE, WARRANTY, or SCRAPPED!
                for (int u = 0; u < deliveryQty; u++)
                {
                    int uId = 0;
                    string serial;
                    string oldStatus = "";
                    string targetStatus = "AVAILABLE";

                    if (inboundType == "PURCHASING")
                    {
                        // PUR: NULL -> AVAILABLE (or small chance of SCRAPPED on inspection)
                        oldStatus = "";
                        targetStatus = (nextRandom() % 25 == 0) ? "SCRAPPED" : "AVAILABLE";
                        uId = nextUnitId++;
                        serial = generateUniqueSerial();

                        Record unitRecord;
                        unitRecord.set("unit_id", uId);
                        unitRecord.set("product_id", pid);
                        unitRecord.set("serial", serial);
                        unitRecord.set("status", targetStatus);
                        unitTbl->insert(unitRecord);
                    }
                    else if (inboundType == "WARRANTY_REPLACEMENT")
                    {
                        // WRP: NULL -> WARRANTY (new replacement unit from vendor)
                        oldStatus = "";
                        targetStatus = "WARRANTY";
                        uId = nextUnitId++;
                        serial = generateUniqueSerial();

                        Record unitRecord;
                        unitRecord.set("unit_id", uId);
                        unitRecord.set("product_id", pid);
                        unitRecord.set("serial", serial);
                        unitRecord.set("status", targetStatus);
                        unitTbl->insert(unitRecord);
                    }
                    else if (inboundType == "WARRANTY_CHECKING")
                    {
                        // WCK: OUT -> WARRANTY (previously sold unit returned for RMA checking)
                        oldStatus = "OUT";
                        targetStatus = "WARRANTY";
                        bool foundOut = false;
                        auto& uRows = unitTbl->getRows();
                        for (size_t r = 0; r < uRows.size(); r++)
                        {
                            if (get<int>(uRows[r].get("product_id")) == pid &&
                                get<string>(uRows[r].get("status")) == "OUT")
                            {
                                uId = get<int>(uRows[r].get("unit_id"));
                                serial = get<string>(uRows[r].get("serial"));
                                uRows[r].set("status", string("WARRANTY"));
                                foundOut = true;
                                break;
                            }
                        }
                        if (!foundOut)
                        {
                            // If no OUT unit exists, create a new unit from NULL
                            oldStatus = "";
                            uId = nextUnitId++;
                            serial = generateUniqueSerial();
                            Record unitRecord;
                            unitRecord.set("unit_id", uId);
                            unitRecord.set("product_id", pid);
                            unitRecord.set("serial", serial);
                            unitRecord.set("status", string("WARRANTY"));
                            unitTbl->insert(unitRecord);
                        }
                    }
                    else if (inboundType == "CUSTOMER_RETURN")
                    {
                        // CRT: OUT -> AVAILABLE
                        oldStatus = "OUT";
                        targetStatus = "AVAILABLE";
                        bool foundOut = false;
                        auto& uRows = unitTbl->getRows();
                        for (size_t r = 0; r < uRows.size(); r++)
                        {
                            if (get<int>(uRows[r].get("product_id")) == pid &&
                                get<string>(uRows[r].get("status")) == "OUT")
                            {
                                uId = get<int>(uRows[r].get("unit_id"));
                                serial = get<string>(uRows[r].get("serial"));
                                uRows[r].set("status", string("AVAILABLE"));
                                foundOut = true;
                                break;
                            }
                        }
                        if (!foundOut)
                        {
                            oldStatus = "";
                            uId = nextUnitId++;
                            serial = generateUniqueSerial();
                            Record unitRecord;
                            unitRecord.set("unit_id", uId);
                            unitRecord.set("product_id", pid);
                            unitRecord.set("serial", serial);
                            unitRecord.set("status", string("AVAILABLE"));
                            unitTbl->insert(unitRecord);
                        }
                    }
                    else // ADJUSTMENT_IN
                    {
                        // Rule: ADJUSTMENT_IN may transition from OUT or NULL -> AVAILABLE/SCRAPPED/WARRANTY
                        // First see if there is an OUT unit to restore; otherwise create new from NULL
                        bool foundOut = false;
                        auto& uRows = unitTbl->getRows();
                        for (size_t r = 0; r < uRows.size(); r++)
                        {
                            if (get<int>(uRows[r].get("product_id")) == pid &&
                                get<string>(uRows[r].get("status")) == "OUT")
                            {
                                uId = get<int>(uRows[r].get("unit_id"));
                                serial = get<string>(uRows[r].get("serial"));
                                oldStatus = "OUT";
                                targetStatus = "AVAILABLE";
                                uRows[r].set("status", targetStatus);
                                foundOut = true;
                                break;
                            }
                        }
                        if (!foundOut)
                        {
                            oldStatus = "";
                            targetStatus = "AVAILABLE";
                            uId = nextUnitId++;
                            serial = generateUniqueSerial();

                            Record unitRecord;
                            unitRecord.set("unit_id", uId);
                            unitRecord.set("product_id", pid);
                            unitRecord.set("serial", serial);
                            unitRecord.set("status", targetStatus);
                            unitTbl->insert(unitRecord);
                        }
                    }

                    // Insert Status History
                    if (histTbl != nullptr)
                    {
                        Record histRecord;
                        histRecord.set("history_id", nextStatusHistoryId++);
                        histRecord.set("unit_id", uId);
                        histRecord.set("old_status", oldStatus);
                        histRecord.set("curr_status", targetStatus);
                        histRecord.set("transaction", delId);
                        histRecord.set("type", string("INBOUND"));
                        histTbl->insert(histRecord);
                    }

                    // Increment actual_quantity on delivery_detail
                    if (delDetTbl != nullptr)
                    {
                        auto& ddRows = delDetTbl->getRows();
                        for (size_t r = 0; r < ddRows.size(); r++)
                        {
                            if (get<int>(ddRows[r].get("delivery_id")) == delId &&
                                get<int>(ddRows[r].get("product_id")) == pid)
                            {
                                int curActual = get<int>(ddRows[r].get("actual_quantity"));
                                ddRows[r].set("actual_quantity", curActual + 1);
                                break;
                            }
                        }
                    }

                    // Increment actual_quantity on inbound_detail
                    auto& idRows = inDetTbl->getRows();
                    for (size_t r = 0; r < idRows.size(); r++)
                    {
                        if (get<int>(idRows[r].get("inbound_transaction_id")) == txnId &&
                            get<int>(idRows[r].get("product_id")) == pid)
                        {
                            int curActual = get<int>(idRows[r].get("actual_quantity"));
                            idRows[r].set("actual_quantity", curActual + 1);
                            break;
                        }
                    }
                }
            }
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

        Table* outTxnTbl = db.getTable("outbound_transaction");
        Table* outDetTbl = db.getTable("outbound_detail");
        Table* shipTbl = db.getTable("shipment");
        Table* shipDetTbl = db.getTable("shipment_detail");
        Table* unitTbl = db.getTable("unit_inventory");
        Table* histTbl = db.getTable("status_history");
        Table* prodTbl = db.getTable("product");

        if (!outTxnTbl || !outDetTbl || !shipTbl || !shipDetTbl || !unitTbl)
        {
            cerr << "[Generator Error] Missing required database tables for outbound generation.\n";
            return -1;
        }

        // Validate stock availability before altering database
        for (const auto& item : items)
        {
            int pid = item.first;
            int requested = item.second;
            int eligibleCount = 0;

            for (const auto& u : unitTbl->getRows())
            {
                if (get<int>(u.get("product_id")) == pid)
                {
                    string st = get<string>(u.get("status"));
                    if (outboundType == "WARRANTY_RETURN")
                    {
                        if (st == "WARRANTY" || st == "SCRAPPED") eligibleCount++;
                    }
                    else if (outboundType == "WARRANTY_REPLACEMENT_TO_CUSTOMER")
                    {
                        if (st == "AVAILABLE" || st == "WARRANTY") eligibleCount++;
                    }

                    else
                    {
                        if (st == "AVAILABLE") eligibleCount++;
                    }
                }
            }

            if (eligibleCount < requested)
            {
                cerr << "[Generator Error] Insufficient eligible stock for product ID " << pid
                     << ". Requested: " << requested << ", Eligible in stock: " << eligibleCount << ".\n";
                return -1;
            }
        }

        // Determine Type Code
        string typeCode = "SEL";
        if (outboundType == "ADJUSTMENT_OUT") typeCode = "ADJ";
        else if (outboundType == "WARRANTY_RETURN" || outboundType == "WRT") typeCode = "WRT";
        else if (outboundType == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || outboundType == "WRC") typeCode = "WRC";

        int seq = (outboundTypeCounters.contains(typeCode) ? outboundTypeCounters[typeCode] : 0) + 1;
        outboundTypeCounters[typeCode] = seq;
        string outboundCode = "OUT-" + typeCode + "-" + formatNumber(seq, 6);

        if (numShipments < 1) numShipments = 1;
        string date = orderDate.empty() ? getCurrentDate() : orderDate;

        // Rule: For ADJUSTMENT_OUT, customer_id MUST be NULL; otherwise valid customer
        Value customerVal;
        if (outboundType == "ADJUSTMENT_OUT")
        {
            customerVal = Value{}; // NULL
        }
        else if (outboundType == "WARRANTY_RETURN" || outboundType == "WRT")
        {
            customerVal = 4; // Supplier / Business
        }
        else
        {
            customerVal = 1; // Individual customer
        }

        // Rule: For ADJUSTMENT_OUT, source_code MUST reference a check_code from inventory_check
        Value sourceCodeVal;
        if (outboundType == "ADJUSTMENT_OUT")
        {
            string finalSource = sourceCode;
            if (finalSource.empty())
            {
                Vector<int> pids;
                for (const auto& item : items) pids.push_back(item.first);
                finalSource = generateInventoryCheck("UNIT", date, pids);
            }
            sourceCodeVal = finalSource;
        }
        else
        {
            if (!sourceCode.empty())
            {
                sourceCodeVal = sourceCode;
            }
            else
            {
                sourceCodeVal = Value{}; // NULL
            }
        }

        // 1. Create OutboundTransaction record
        int txnId = nextOutboundTxnId++;
        Record outTxn;
        outTxn.set("outbound_trainsaction_id", txnId);
        outTxn.set("outbound_code", outboundCode);
        outTxn.set("source_code", sourceCodeVal);
        outTxn.set("customer_id", customerVal);
        outTxn.set("order_at", date);
        outTxn.set("type", outboundType);
        outTxn.set("status", string("CONFIRMED"));
        outTxn.set("next_ship_no", numShipments + 1);
        outTxn.set("note", note.empty() ? ("Outbound " + outboundType + " fulfilled successfully") : note);

        outTxnTbl->insert(outTxn);

        // 2. Create OutboundDetail records
        for (const auto& item : items)
        {
            int pid = item.first;
            int qty = item.second;
            if (qty <= 0) continue;

            Record outDet;
            outDet.set("outbound_detail_id", nextOutboundDetailId++);
            outDet.set("outbound_transaction_id", txnId);
            outDet.set("product_id", pid);
            outDet.set("ordered_quantity", qty);
            outDet.set("actual_quantity", 0);
            outDetTbl->insert(outDet);
        }

        // 3. Create Shipments and dispatch Units
        auto& unitRows = unitTbl->getRows();

        for (int s = 1; s <= numShipments; s++)
        {
            int shipId = nextShipmentId++;
            string shipCode = outboundCode + "-S" + formatNumber(s, 2);
            string shipDate = advanceDate(date, s);

            Record shipRecord;
            shipRecord.set("shipment_id", shipId);
            shipRecord.set("shipment_code", shipCode);
            shipRecord.set("sell_id", txnId);
            shipRecord.set("ship_at", shipDate);
            shipRecord.set("expected_at", shipDate);
            shipRecord.set("status", string("COMPLETED"));
            shipTbl->insert(shipRecord);

            for (const auto& item : items)
            {
                int pid = item.first;
                int totalQty = item.second;
                int baseQty = totalQty / numShipments;
                int rem = totalQty % numShipments;
                int shipmentQty = baseQty + (s <= rem ? 1 : 0);

                if (shipDetTbl != nullptr && shipmentQty > 0)
                {
                    Record shipDet;
                    shipDet.set("shipment_detail_id", nextShipmentDetailId++);
                    shipDet.set("shipment_id", shipId);
                    shipDet.set("product_id", pid);
                    shipDet.set("received_quantity", shipmentQty);
                    shipDet.set("actual_quantity", 0);
                    shipDetTbl->insert(shipDet);
                }

                int dispatched = 0;
                for (size_t u = 0; u < unitRows.size() && dispatched < shipmentQty; u++)
                {
                    if (get<int>(unitRows[u].get("product_id")) == pid)
                    {
                        string currentStatus = get<string>(unitRows[u].get("status"));
                        bool isEligible = false;
                        if (outboundType == "WARRANTY_RETURN")
                        {
                            isEligible = (currentStatus == "WARRANTY" || currentStatus == "SCRAPPED");
                        }
                        else if (outboundType == "WARRANTY_REPLACEMENT_TO_CUSTOMER")
                        {
                            isEligible = (currentStatus == "AVAILABLE" || currentStatus == "WARRANTY");
                        }

                        else
                        {
                            isEligible = (currentStatus == "AVAILABLE");
                        }

                        if (isEligible)
                        {
                            int uId = get<int>(unitRows[u].get("unit_id"));

                            // Transition unit status to OUT
                            unitRows[u].set("status", string("OUT"));

                            // Create StatusHistory record
                            if (histTbl != nullptr)
                            {
                                Record histRecord;
                                histRecord.set("history_id", nextStatusHistoryId++);
                                histRecord.set("unit_id", uId);
                                histRecord.set("old_status", currentStatus);
                                histRecord.set("curr_status", string("OUT"));
                                histRecord.set("transaction", shipId);
                                histRecord.set("type", string("OUTBOUND"));
                                histTbl->insert(histRecord);
                            }

                            // Increment actual_quantity on shipment_detail
                            if (shipDetTbl != nullptr)
                            {
                                auto& sdRows = shipDetTbl->getRows();
                                for (size_t r = 0; r < sdRows.size(); r++)
                                {
                                    if (get<int>(sdRows[r].get("shipment_id")) == shipId &&
                                        get<int>(sdRows[r].get("product_id")) == pid)
                                    {
                                        int curActual = get<int>(sdRows[r].get("actual_quantity"));
                                        sdRows[r].set("actual_quantity", curActual + 1);
                                        break;
                                    }
                                }
                            }

                            // Increment actual_quantity on outbound_detail
                            auto& odRows = outDetTbl->getRows();
                            for (size_t r = 0; r < odRows.size(); r++)
                            {
                                if (get<int>(odRows[r].get("outbound_transaction_id")) == txnId &&
                                    get<int>(odRows[r].get("product_id")) == pid)
                                {
                                    int curActual = get<int>(odRows[r].get("actual_quantity"));
                                    odRows[r].set("actual_quantity", curActual + 1);
                                    break;
                                }
                            }

                            dispatched++;
                        }
                    }
                }
            }
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
        Table* adjTbl = db.getTable("adjustment_movement");
        Table* adjDetTbl = db.getTable("adjust_movement_detail");
        Table* unitTbl = db.getTable("unit_inventory");
        Table* histTbl = db.getTable("status_history");
        Table* prodTbl = db.getTable("product");

        if (!adjTbl || !adjDetTbl)
        {
            cerr << "[Generator Error] adjustment_movement or adjust_movement_detail table missing.\n";
            return -1;
        }

        string fullReason = "STOCKTAKE_DIFF";
        string reasonCode = "STD";
        if (reason == "DAMAGED" || reason == "DMG") { fullReason = "DAMAGED"; reasonCode = "DMG"; }
        else if (reason == "DATA_ERROR" || reason == "DTE") { fullReason = "DATA_ERROR"; reasonCode = "DTE"; }
        else if (reason == "OTHER" || reason == "OTH") { fullReason = "OTHER"; reasonCode = "OTH"; }
        else { fullReason = "STOCKTAKE_DIFF"; reasonCode = "STD"; }

        string date = adjustDate.empty() ? getCurrentDate() : adjustDate;

        string finalSource = sourceCode;
        if (finalSource.empty())
        {
            Vector<int> pids;
            for (size_t i = 0; i < items.size(); ++i) pids.push_back(items[i].productId);
            finalSource = generateInventoryCheck("UNIT", date, pids);
        }

        int adjId = nextAdjustmentMovementId++;
        string adjCode = "ADJ-" + reasonCode + "-" + formatNumber(nextAdjustSeq++, 6);

        Record adjRec;
        adjRec.set("adjust_id", adjId);
        adjRec.set("adjust_code", adjCode);
        adjRec.set("source_code", finalSource);
        adjRec.set("reason", fullReason);
        adjRec.set("adjusted_at", date);
        adjRec.set("status", string("COMPLETED"));
        adjRec.set("note", note.empty() ? string("Internal adjustment movement " + adjCode) : note);
        adjTbl->insert(adjRec);

        for (size_t i = 0; i < items.size(); ++i)
        {
            const auto& item = items[i];
            int detId = nextAdjustDetailId++;

            int moved = 0;
            if (unitTbl)
            {
                auto& uRows = unitTbl->getRows();
                for (size_t u = 0; u < uRows.size() && moved < item.quantity; ++u)
                {
                    if (get<int>(uRows[u].get("product_id")) == item.productId &&
                        get<string>(uRows[u].get("status")) == item.fromStatus)
                    {
                        uRows[u].set("status", item.toStatus);
                        int uId = get<int>(uRows[u].get("unit_id"));

                        if (histTbl)
                        {
                            Record hRec;
                            hRec.set("history_id", nextStatusHistoryId++);
                            hRec.set("unit_id", uId);
                            hRec.set("old_status", item.fromStatus);
                            hRec.set("curr_status", item.toStatus);
                            hRec.set("transaction", adjId);
                            hRec.set("type", string("ADJUSTMENT"));
                            histTbl->insert(hRec);
                        }
                        moved++;
                    }
                }
            }

            Record dRec;
            dRec.set("adjust_detail_id", detId);
            dRec.set("adjust_id", adjId);
            dRec.set("product_id", item.productId);
            dRec.set("from_status", item.fromStatus);
            dRec.set("to_status", item.toStatus);
            dRec.set("adjust_quantity", item.quantity);
            dRec.set("actual_quantity", moved);
            adjDetTbl->insert(dRec);
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
        Table* prodTbl = db.getTable("product");
        if (prodTbl == nullptr || prodTbl->getRowCount() == 0)
        {
            cerr << "[Generator Error] No products available in database for batch generation.\n";
            return summary;
        }

        // Rule: Only active products (is_sold == TRUE) may participate in generated transactions
        Vector<int> productIds;
        for (const auto& p : prodTbl->getRows())
        {
            bool isSold = true;
            if (holds_alternative<bool>(p.get("is_sold")))
            {
                isSold = get<bool>(p.get("is_sold"));
            }
            if (isSold)
            {
                productIds.push_back(get<int>(p.get("product_id")));
            }
        }

        if (productIds.empty())
        {
            cerr << "[Generator Error] No active products (is_sold == TRUE) found for batch generation.\n";
            return summary;
        }

        string curDate = startDate.empty() ? "2026-09-10" : startDate;

        // Generate Inbound Transactions
        const string inboundTypes[] = {"PURCHASING", "ADJUSTMENT_IN", "WARRANTY_CHECKING", "CUSTOMER_RETURN"};
        for (int i = 0; i < numInbound; i++)
        {
            string inType = inboundTypes[nextRandom() % 4];
            int numItems = 1 + (nextRandom() % 3);
            int numDeliveries = 1 + (nextRandom() % 2);

            Vector<Pair<int, int>> items;
            HashMap<int, bool> usedPids;
            for (int j = 0; j < numItems; j++)
            {
                int pid = productIds[nextRandom() % productIds.size()];
                if (usedPids.contains(pid)) continue;
                usedPids[pid] = true;
                int qty = 5 + (nextRandom() % 15);
                items.push_back(Pair<int, int>(pid, qty));
                summary.unitsCreated += qty;
            }

            if (inType == "ADJUSTMENT_IN")
            {
                summary.inventoryChecksGenerated++;
            }

            int id = generateInbound(inType, items, numDeliveries, curDate);
            if (id > 0)
            {
                summary.inboundTxnsGenerated++;
                summary.deliveriesGenerated += numDeliveries;
            }
            curDate = advanceDate(curDate, 1);
        }

        // Generate Outbound Transactions
        const string outboundTypes[] = {"SELLING", "ADJUSTMENT_OUT", "SELLING", "WARRANTY_RETURN"};
        for (int i = 0; i < numOutbound; i++)
        {
            string outType = outboundTypes[nextRandom() % 4];
            int numItems = 1 + (nextRandom() % 2);
            int numShipments = 1;

            Vector<Pair<int, int>> items;
            HashMap<int, bool> usedPids;
            for (int j = 0; j < numItems; j++)
            {
                int pid = productIds[nextRandom() % productIds.size()];
                if (usedPids.contains(pid)) continue;
                int avail = getAvailableStock(pid);
                if (avail > 2)
                {
                    int maxQty = min(avail / 2, 4);
                    if (maxQty < 1) maxQty = 1;
                    int qty = 1 + (nextRandom() % maxQty);
                    items.push_back(Pair<int, int>(pid, qty));
                    usedPids[pid] = true;
                    summary.unitsShipped += qty;
                }
            }

            if (!items.empty())
            {
                if (outType == "ADJUSTMENT_OUT")
                {
                    summary.inventoryChecksGenerated++;
                }

                int id = generateOutbound(outType, items, numShipments, curDate);
                if (id > 0)
                {
                    summary.outboundTxnsGenerated++;
                    summary.shipmentsGenerated += numShipments;
                }
            }
            curDate = advanceDate(curDate, 1);
        }

        // Generate Standalone Inventory Checks (always UNIT)
        for (int k = 0; k < numChecks; k++)
        {
            string checkType = "UNIT";
            int numAuditProducts = 2 + (nextRandom() % 4);
            if (numAuditProducts > static_cast<int>(productIds.size()))
            {
                numAuditProducts = productIds.size();
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

            string code = generateInventoryCheck(checkType, curDate, auditPids);
            if (!code.empty())
            {
                summary.inventoryChecksGenerated++;
            }
            curDate = advanceDate(curDate, 1);
        }

        // Optionally generate an adjustment movement linked to stock inspection
        if (numChecks > 0 && !productIds.empty())
        {
            int pid = productIds[nextRandom() % productIds.size()];
            Vector<AdjustItem> adjItems;
            adjItems.push_back({pid, "AVAILABLE", "SCRAPPED", 1});
            int movId = generateAdjustmentMovement("DAMAGED", adjItems, curDate);
            if (movId > 0)
            {
                summary.adjustmentMovementsGenerated++;
            }
        }

        return summary;
    }

    bool DataGenerator::validateIntegrity() const
    {
        Table* prodTbl = db.getTable("product");
        Table* unitTbl = db.getTable("unit_inventory");
        Table* inTxnTbl = db.getTable("inbound_transaction");
        Table* delTbl = db.getTable("delivery");
        Table* outTxnTbl = db.getTable("outbound_transaction");
        Table* shipTbl = db.getTable("shipment");
        Table* histTbl = db.getTable("status_history");
        Table* chkTbl = db.getTable("inventory_check");

        if (!prodTbl || !unitTbl || !inTxnTbl || !delTbl || !outTxnTbl || !shipTbl)
        {
            cerr << "[-] Missing core tables for integrity check.\n";
            return false;
        }

        // 1. Verify all units reference valid products and have unique serials
        HashMap<int, bool> validProductIds;
        for (const auto& p : prodTbl->getRows())
        {
            validProductIds[get<int>(p.get("product_id"))] = true;
        }

        HashMap<string, bool> seenSerials;
        for (const auto& u : unitTbl->getRows())
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

        // 2. Build map of inventory check codes
        HashMap<string, bool> validCheckCodes;
        if (chkTbl != nullptr)
        {
            for (const auto& c : chkTbl->getRows())
            {
                if (holds_alternative<string>(c.get("check_code")))
                {
                    validCheckCodes[get<string>(c.get("check_code"))] = true;
                }
            }
        }

        // 3. Verify Inbound Transactions & ADJUSTMENT_IN rules
        HashMap<int, bool> validInboundTxns;
        for (const auto& t : inTxnTbl->getRows())
        {
            int txnId = get<int>(t.get("inbound_transaction_id"));
            validInboundTxns[txnId] = true;

            string type = get<string>(t.get("type"));
            if (type == "ADJUSTMENT_IN")
            {
                // Must NOT have customer
                if (!holds_alternative<monostate>(t.get("customer_id")))
                {
                    cerr << "[-] Inbound Transaction " << txnId << " (ADJUSTMENT_IN) must NOT have customer_id.\n";
                    return false;
                }
                // source_code MUST reference a valid check_code
                if (!holds_alternative<string>(t.get("source_code")))
                {
                    cerr << "[-] Inbound Transaction " << txnId << " (ADJUSTMENT_IN) missing required source_code.\n";
                    return false;
                }
                string src = get<string>(t.get("source_code"));
                if (src.empty() || !validCheckCodes.contains(src))
                {
                    cerr << "[-] Inbound Transaction " << txnId << " (ADJUSTMENT_IN) source_code (" << src
                         << ") does not reference a valid inventory_check code.\n";
                    return false;
                }
            }
        }

        // Deliveries link to valid Inbound Transactions
        for (const auto& d : delTbl->getRows())
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
        for (const auto& t : outTxnTbl->getRows())
        {
            int txnId = get<int>(t.get("outbound_trainsaction_id"));
            validOutboundTxns[txnId] = true;

            string type = get<string>(t.get("type"));
            if (type == "ADJUSTMENT_OUT")
            {
                // Must NOT have customer
                if (!holds_alternative<monostate>(t.get("customer_id")))
                {
                    cerr << "[-] Outbound Transaction " << txnId << " (ADJUSTMENT_OUT) must NOT have customer_id.\n";
                    return false;
                }
                // source_code MUST reference a valid check_code
                if (!holds_alternative<string>(t.get("source_code")))
                {
                    cerr << "[-] Outbound Transaction " << txnId << " (ADJUSTMENT_OUT) missing required source_code.\n";
                    return false;
                }
                string src = get<string>(t.get("source_code"));
                if (src.empty() || !validCheckCodes.contains(src))
                {
                    cerr << "[-] Outbound Transaction " << txnId << " (ADJUSTMENT_OUT) source_code (" << src
                         << ") does not reference a valid inventory_check code.\n";
                    return false;
                }
            }
        }

        // Shipments link to valid Outbound Transactions
        for (const auto& s : shipTbl->getRows())
        {
            int txnId = get<int>(s.get("sell_id"));
            if (!validOutboundTxns.contains(txnId))
            {
                cerr << "[-] Shipment references invalid sell_id (outbound_trainsaction_id): " << txnId << "\n";
                return false;
            }
        }

        // 5. Verify actual_quantity constraints on detail tables
        Table* inDetTbl = db.getTable("inbound_detail");
        if (inDetTbl != nullptr)
        {
            for (const auto& row : inDetTbl->getRows())
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

        Table* delDetTbl = db.getTable("delivery_detail");
        if (delDetTbl != nullptr)
        {
            for (const auto& row : delDetTbl->getRows())
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

        Table* outDetTbl = db.getTable("outbound_detail");
        if (outDetTbl != nullptr)
        {
            for (const auto& row : outDetTbl->getRows())
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

        Table* shipDetTbl = db.getTable("shipment_detail");
        if (shipDetTbl != nullptr)
        {
            for (const auto& row : shipDetTbl->getRows())
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

        // 6. Verify Inbound status history rule: old_status must ONLY be "" (NULL) or "OUT"
        if (histTbl != nullptr)
        {
            for (const auto& row : histTbl->getRows())
            {
                string transType = "";
                if (holds_alternative<string>(row.get("type")))
                {
                    transType = get<string>(row.get("type"));
                }
                if (transType == "INBOUND")
                {
                    string oldSt = "";
                    if (holds_alternative<string>(row.get("old_status")))
                    {
                        oldSt = get<string>(row.get("old_status"));
                    }
                    if (oldSt != "" && oldSt != "OUT")
                    {
                        cerr << "[-] Status history violation: INBOUND operation had old_status = '" << oldSt
                             << "' (only NULL or OUT permitted).\n";
                        return false;
                    }
                }
            }
        }

        // 7. Verify Product is_sold rule: if is_sold == false, total quantity must be 0
        for (const auto& p : prodTbl->getRows())
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

        // 8. Verify Adjustment Movements & single-check uniqueness (Rule 5.4 & 5.5)
        Table* adjTbl = db.getTable("adjustment_movement");
        Table* adjDetTbl = db.getTable("adjust_movement_detail");
        if (adjTbl != nullptr)
        {
            HashMap<string, bool> seenAdjustCodes;
            HashMap<string, bool> seenSourceCodes;
            for (const auto& row : adjTbl->getRows())
            {
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

                if (holds_alternative<string>(row.get("source_code")))
                {
                    string src = get<string>(row.get("source_code"));
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
                    }
                }
            }
        }

        if (adjDetTbl != nullptr && adjTbl != nullptr)
        {
            HashMap<int, bool> validAdjustIds;
            for (const auto& a : adjTbl->getRows())
            {
                validAdjustIds[get<int>(a.get("adjust_id"))] = true;
            }

            for (const auto& row : adjDetTbl->getRows())
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
                        cerr << "[-] AdjustMovementDetail to_status must not be OUT (Rule 5.4)\n";
                        return false;
                    }
                }
            }
        }

        return true;
    }

    void DataGenerator::printInventorySummary() const
    {
        Table* prodTbl = db.getTable("product");
        Table* unitTbl = db.getTable("unit_inventory");
        Table* inTxnTbl = db.getTable("inbound_transaction");
        Table* delTbl = db.getTable("delivery");
        Table* outTxnTbl = db.getTable("outbound_transaction");
        Table* shipTbl = db.getTable("shipment");
        Table* histTbl = db.getTable("status_history");
        Table* chkTbl = db.getTable("inventory_check");

        int totalProducts = prodTbl ? prodTbl->getRowCount() : 0;
        int activeProducts = 0;
        int inactiveProducts = 0;
        if (prodTbl != nullptr)
        {
            for (const auto& p : prodTbl->getRows())
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
            for (const auto& u : unitTbl->getRows())
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
        Table* prodTbl = db.getTable("product");
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

        for (const auto& p : prodTbl->getRows())
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
            cout << "  3. Perform Inventory Check (Audit: QUAN or UNIT)    \n";
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
                cout << "  3. RETURN_TO_SUPPLIER (Vendor return)\n";
                cout << "  4. WARRANTY_RETURN (Return defective part to vendor)\n";
                cout << "  5. WARRANTY_REPLACEMENT_TO_CUSTOMER (Replacement to customer)\n";
                cout << "Select type (1-5): ";
                int typeChoice = 1;
                cin >> typeChoice;

                string outType = "SELLING";
                if (typeChoice == 2) outType = "ADJUSTMENT_OUT";
                else if (typeChoice == 3) outType = "RETURN_TO_SUPPLIER";
                else if (typeChoice == 4) outType = "WARRANTY_RETURN";
                else if (typeChoice == 5) outType = "WARRANTY_REPLACEMENT_TO_CUSTOMER";

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
                // Perform Inventory Check
                cout << "\n--- SELECT CHECK TYPE ---\n";
                cout << "  1. QUAN (Quantity snapshot check)\n";
                cout << "  2. UNIT (Detailed serial-level check)\n";
                cout << "Select check type (1-2): ";
                int ct = 1;
                cin >> ct;
                string checkType = (ct == 2) ? "UNIT" : "QUAN";

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
                cout << "  3. DATA_ERROR (Correction of data input error)\n";
                cout << "  4. OTHER (Other warehouse reclassification)\n";
                cout << "Enter choice (1-4): ";
                int rChoice = 1;
                cin >> rChoice;
                string rsn = "STOCKTAKE_DIFF";
                if (rChoice == 2) rsn = "DAMAGED";
                else if (rChoice == 3) rsn = "DATA_ERROR";
                else if (rChoice == 4) rsn = "OTHER";

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

                cout << "Enter Source Check Code (leave empty to auto-generate): ";
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
                bool saved = DataIO::DatabaseIO::writeAll(db, "data");
                cout << "[+] Save result: " << (saved ? "SUCCESS" : "FAILED") << "\n";
            }
            else
            {
                cout << "[!] Invalid option. Please choose between 0 and 10.\n";
            }
        }
    }
}
