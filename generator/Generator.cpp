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
                existingSerials[get<string>(rows[i].get("serial"))] = true;
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

                string code = get<string>(rows[i].get("inbound_code"));
                // Format: IN-{TYPE}-{SEQ} e.g. IN-PUR-000001
                if (code.length() >= 13)
                {
                    string typeCode = code.substr(3, 3);
                    int seq = stoi(code.substr(7));
                    int cur = inboundTypeCounters.contains(typeCode) ? inboundTypeCounters[typeCode] : 0;
                    if (seq > cur) inboundTypeCounters[typeCode] = seq;
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

                string code = get<string>(rows[i].get("outbound_code"));
                // Format: OUT-{TYPE}-{SEQ} e.g. OUT-SEL-000001
                if (code.length() >= 14)
                {
                    string typeCode = code.substr(4, 3);
                    int seq = stoi(code.substr(8));
                    int cur = outboundTypeCounters.contains(typeCode) ? outboundTypeCounters[typeCode] : 0;
                    if (seq > cur) outboundTypeCounters[typeCode] = seq;
                }
            }
        }

        // 7. Scan Outbound Detail
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

        // 8. Scan Shipment
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

        // 8b. Scan Shipment Detail
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
            int actual = availableCounts.contains(pid) ? availableCounts[pid] : 0;
            prodRows[i].set("quantity", actual);
        }

        Table* prodQuanTbl = db.getTable("product_quantity");
        if (prodQuanTbl != nullptr)
        {
            auto& pqRows = prodQuanTbl->getRows();
            for (size_t i = 0; i < pqRows.size(); i++)
            {
                int pid = get<int>(pqRows[i].get("product_id"));
                pqRows[i].set("available_quan", availableCounts.contains(pid) ? availableCounts[pid] : 0);
                pqRows[i].set("warranty_quan", warrantyCounts.contains(pid) ? warrantyCounts[pid] : 0);
                pqRows[i].set("scrapped_quan", scrappedCounts.contains(pid) ? scrappedCounts[pid] : 0);
            }
        }
    }

    int DataGenerator::generateInbound(const string& inboundType,
                                        const Vector<Pair<int, int>>& items,
                                        int numDeliveries,
                                        const string& orderDate,
                                        const string& note)
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

        // Choose appropriate customer: Business (4) for supplier purchasing/warranty replacement, Individual (1) for customer returns
        int customerId = (inboundType == "WARRANTY_CHECKING" || inboundType == "CUSTOMER_RETURN") ? 1 : 4;

        // 1. Create InboundTransaction record
        int txnId = nextInboundTxnId++;
        Record inTxn;
        inTxn.set("inbound_transaction_id", txnId);
        inTxn.set("inbound_code", inboundCode);
        inTxn.set("customer_id", customerId);
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
        // Split product quantities evenly across deliveries
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
                // Distribute remainder onto first delivery
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

                for (int u = 0; u < deliveryQty; u++)
                {
                    int uId = 0;
                    string serial;
                    string oldStatus = "";
                    string targetStatus = "AVAILABLE";

                    if (inboundType == "PURCHASING")
                    {
                        // PUR: NULL -> AVAILABLE (or small chance of SCRAPPED upon inspection)
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
                        // WCK: OUT -> WARRANTY (previously sold unit returned for checking)
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
                        // CRT: OUT -> AVAILABLE (or WARRANTY/SCRAPPED)
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
                    else
                    {
                        // ADJUSTMENT_IN: NULL -> AVAILABLE
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

                    // Auto-increment actual_quantity on delivery_detail
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

                    // Auto-increment actual_quantity on inbound_detail
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

        // 4. Update Product stock quantity
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
                                        const string& note)
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

        // Required unit status depends on outbound type
        // SEL: AVAILABLE -> OUT
        // WRT: SCRAPPED / WARRANTY -> OUT
        // WRC: AVAILABLE / WARRANTY -> OUT
        // RTS: AVAILABLE / WARRANTY / SCRAPPED -> OUT
        string requiredStatus = "AVAILABLE";
        if (outboundType == "WARRANTY_RETURN")
        {
            requiredStatus = "WARRANTY";
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
                    else if (outboundType == "RETURN_TO_SUPPLIER")
                    {
                        if (st == "AVAILABLE" || st == "WARRANTY" || st == "SCRAPPED") eligibleCount++;
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
        else if (outboundType == "RETURN_TO_SUPPLIER") typeCode = "RTS";
        else if (outboundType == "WARRANTY_RETURN") typeCode = "WRT";
        else if (outboundType == "WARRANTY_REPLACEMENT_TO_CUSTOMER") typeCode = "WRC";

        int seq = (outboundTypeCounters.contains(typeCode) ? outboundTypeCounters[typeCode] : 0) + 1;
        outboundTypeCounters[typeCode] = seq;
        string outboundCode = "OUT-" + typeCode + "-" + formatNumber(seq, 6);

        if (numShipments < 1) numShipments = 1;
        string date = orderDate.empty() ? getCurrentDate() : orderDate;

        // Choose appropriate customer: Individual (1) for retail sales/replacements, Business (4) for supplier returns
        int customerId = (outboundType == "WARRANTY_RETURN" || outboundType == "RETURN_TO_SUPPLIER") ? 4 : 1;

        // 1. Create OutboundTransaction record
        int txnId = nextOutboundTxnId++;
        Record outTxn;
        outTxn.set("outbound_trainsaction_id", txnId);
        outTxn.set("outbound_code", outboundCode);
        outTxn.set("customer_id", customerId);
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
                        else if (outboundType == "RETURN_TO_SUPPLIER")
                        {
                            isEligible = (currentStatus == "AVAILABLE" || currentStatus == "WARRANTY" || currentStatus == "SCRAPPED");
                        }
                        else
                        {
                            isEligible = (currentStatus == "AVAILABLE");
                        }

                        if (isEligible)
                        {
                            int uId = get<int>(unitRows[u].get("unit_id"));
                            string serial = get<string>(unitRows[u].get("serial"));

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

                            // Auto-increment actual_quantity on shipment_detail
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

                            // Auto-increment actual_quantity on outbound_detail
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

        // 4. Update Product stock quantity
        if (prodTbl != nullptr)
        {
            syncProductQuantities();
        }

        cout << "[+] Generated Outbound Transaction: " << outboundCode << " (ID: " << txnId << ")\n";
        cout << "    - Shipments created: " << numShipments << "\n";
        return txnId;
    }

    GenerationSummary DataGenerator::generateBatch(int numInbound, int numOutbound, const string& startDate)
    {
        GenerationSummary summary;
        Table* prodTbl = db.getTable("product");
        if (prodTbl == nullptr || prodTbl->getRowCount() == 0)
        {
            cerr << "[Generator Error] No products available in database for batch generation.\n";
            return summary;
        }

        Vector<int> productIds;
        for (const auto& p : prodTbl->getRows())
        {
            productIds.push_back(get<int>(p.get("product_id")));
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

            int id = generateInbound(inType, items, numDeliveries, curDate);
            if (id > 0)
            {
                summary.inboundTxnsGenerated++;
                summary.deliveriesGenerated += numDeliveries;
            }
            curDate = advanceDate(curDate, 1);
        }

        // Generate Outbound Transactions
        const string outboundTypes[] = {"SELLING", "ADJUSTMENT_OUT", "SELLING"};
        for (int i = 0; i < numOutbound; i++)
        {
            string outType = outboundTypes[nextRandom() % 3];
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
                int id = generateOutbound(outType, items, numShipments, curDate);
                if (id > 0)
                {
                    summary.outboundTxnsGenerated++;
                    summary.shipmentsGenerated += numShipments;
                }
            }
            curDate = advanceDate(curDate, 1);
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

        if (!prodTbl || !unitTbl || !inTxnTbl || !delTbl || !outTxnTbl || !shipTbl)
        {
            cerr << "[-] Missing core tables for integrity check.\n";
            return false;
        }

        // 1. Verify all units reference valid products
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

        // 2. Verify Deliveries link to valid Inbound Transactions
        HashMap<int, bool> validInboundTxns;
        for (const auto& t : inTxnTbl->getRows())
        {
            validInboundTxns[get<int>(t.get("inbound_transaction_id"))] = true;
        }
        for (const auto& d : delTbl->getRows())
        {
            int txnId = get<int>(d.get("inbound_transaction_id"));
            if (!validInboundTxns.contains(txnId))
            {
                cerr << "[-] Delivery references invalid inbound_transaction_id: " << txnId << "\n";
                return false;
            }
        }

        // 3. Verify Shipments link to valid Outbound Transactions
        HashMap<int, bool> validOutboundTxns;
        for (const auto& t : outTxnTbl->getRows())
        {
            validOutboundTxns[get<int>(t.get("outbound_trainsaction_id"))] = true;
        }
        for (const auto& s : shipTbl->getRows())
        {
            int txnId = get<int>(s.get("sell_id"));
            if (!validOutboundTxns.contains(txnId))
            {
                cerr << "[-] Shipment references invalid sell_id (outbound_trainsaction_id): " << txnId << "\n";
                return false;
            }
        }

        // 4. Verify actual_quantity constraints on detail tables
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

        int totalProducts = prodTbl ? prodTbl->getRowCount() : 0;
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
        cout << "  - Total Master Products:        " << totalProducts << "\n";
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

        cout << "\n" << setfill('=') << setw(90) << "" << setfill(' ') << "\n";
        cout << setw(5) << "ID"
             << setw(22) << "Code"
             << setw(38) << "Product Name"
             << setw(14) << "Category"
             << setw(11) << "Stock" << "\n";
        cout << setfill('-') << setw(90) << "" << setfill(' ') << "\n";

        for (const auto& p : prodTbl->getRows())
        {
            int pid = get<int>(p.get("product_id"));
            string code = get<string>(p.get("product_code"));
            string name = get<string>(p.get("product_name"));
            if (name.length() > 35) name = name.substr(0, 32) + "...";
            string cat = get<string>(p.get("category"));
            int stock = getAvailableStock(pid);

            cout << setw(5) << pid
                 << setw(22) << code
                 << setw(38) << name
                 << setw(14) << cat
                 << setw(11) << stock << "\n";
        }
        cout << setfill('=') << setw(90) << "" << setfill(' ') << "\n\n";
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
            cout << "  3. Display Products & Stock Levels                  \n";
            cout << "  4. View Inventory & Transaction Summary             \n";
            cout << "  5. Auto Generate Random Batch                       \n";
            cout << "  6. Validate Database Referential Integrity          \n";
            cout << "  7. Save Database Changes to CSV                     \n";
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
                cout << "  2. ADJUSTMENT_IN (Stock correction)\n";
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
                int id = generateInbound(inType, items, numDeliveries);
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
                int id = generateOutbound(outType, items, numShipments);
                if (id > 0)
                {
                    cout << "[SUCCESS] Outbound transaction created with ID: " << id << "\n";
                }
            }
            else if (choice == 3)
            {
                printProductList();
            }
            else if (choice == 4)
            {
                printInventorySummary();
            }
            else if (choice == 5)
            {
                cout << "Enter number of Inbounds to generate: ";
                int nIn = 5;
                cin >> nIn;
                cout << "Enter number of Outbounds to generate: ";
                int nOut = 5;
                cin >> nOut;

                GenerationSummary summary = generateBatch(nIn, nOut);
                cout << "\n======================================================\n";
                cout << "               BATCH GENERATION REPORT                \n";
                cout << "======================================================\n";
                cout << "  - Inbound Transactions:         " << summary.inboundTxnsGenerated << "\n";
                cout << "  - Deliveries Completed:         " << summary.deliveriesGenerated << "\n";
                cout << "  - Units Added to Inventory:     " << summary.unitsCreated << "\n";
                cout << "  - Outbound Transactions:        " << summary.outboundTxnsGenerated << "\n";
                cout << "  - Shipments Completed:          " << summary.shipmentsGenerated << "\n";
                cout << "  - Units Dispatched (OUT):       " << summary.unitsShipped << "\n";
                cout << "======================================================\n";
            }
            else if (choice == 6)
            {
                bool valid = validateIntegrity();
                cout << "\n[+] Integrity validation: " << (valid ? "[PASSED] 100% VALID" : "[FAILED]") << "\n";
            }
            else if (choice == 7)
            {
                cout << "[+] Saving database to 'data' folder...\n";
                bool saved = DataIO::DatabaseIO::writeAll(db, "data");
                cout << "[+] Save result: " << (saved ? "SUCCESS" : "FAILED") << "\n";
            }
            else
            {
                cout << "[!] Invalid option. Please choose between 0 and 7.\n";
            }
        }
    }
}
