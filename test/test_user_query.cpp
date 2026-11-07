#include <iostream>
#include <iomanip>
#include <cassert>
#include <string>

#include "database/Database.h"
#include "data_io/DatabaseIO.h"
#include "data_io/ExportReport.h"
#include "schema/UserQuery.h"

using namespace std;
using namespace DataIO;

void printSection(const string& title)
{
    cout << "\n==========================================================" << endl;
    cout << "  " << title << endl;
    cout << "==========================================================" << endl;
}

// ============================================================================
// 1. TEST TẤT CẢ CÁC TRUY VẤN VIEW ĐẦY ĐỦ CÁC CỘT VỚI REAL DATA
// ============================================================================
void test_all_user_queries_full_view(Database& db)
{
    printSection("[TEST 1] Testing all user queries returning FULL columns");

    // 1.1 Product quantity query (Core product view)
    Vector<Record> prodQuantities;
    user_query::product_quantity_query(&db, prodQuantities);
    cout << "  [1.1] product_quantity_query: " << prodQuantities.size() << " records." << endl;
    assert(prodQuantities.size() > 0);
    assert(prodQuantities[0].has("product_id"));
    assert(prodQuantities[0].has("product.product_id"));
    assert(prodQuantities[0].has("product.product_code"));
    assert(prodQuantities[0].has("product.barcode"));
    assert(prodQuantities[0].has("product.sku"));
    assert(prodQuantities[0].has("product.product_name"));
    assert(prodQuantities[0].has("product_quantity.available_quan"));
    assert(prodQuantities[0].has("product_quantity.warranty_quan"));
    assert(prodQuantities[0].has("product_quantity.scrapped_quan"));
    assert(prodQuantities[0].has("product.is_sold"));

    // 1.2 Unit inventory query
    Vector<Record> units;
    user_query::unit_inventory_query(&db, units);
    cout << "  [1.2] unit_inventory_query: " << units.size() << " records." << endl;
    assert(units.size() > 0);
    assert(units[0].has("unit_id"));
    assert(units[0].has("unit_inventory.unit_id"));
    assert(units[0].has("product.product_name"));
    assert(units[0].has("unit_inventory.serial"));
    assert(units[0].has("unit_inventory.status"));

    // 1.3 Status history query (Transaction code & Date)
    Vector<Record> histories;
    user_query::status_history_query(&db, histories);
    cout << "  [1.3] status_history_query: " << histories.size() << " records." << endl;
    assert(histories.size() > 0);
    assert(histories[0].has("history_id"));
    assert(histories[0].has("status_history.history_id"));
    assert(histories[0].has("unit_inventory.serial"));
    assert(histories[0].has("status_history.old_status"));
    assert(histories[0].has("status_history.curr_status"));
    assert(histories[0].has("status_history.transaction"));
    assert(histories[0].has("status_history.type"));
    assert(histories[0].has("status_history.date"));

    // Kiểm tra transaction code là chuỗi mã giao dịch (không phải ID số thuần túy)
    string txCode = get<string>(histories[0].get("status_history.transaction"));
    assert(!txCode.empty());
    assert(txCode.rfind("IN-", 0) == 0 || txCode.rfind("OUT-", 0) == 0);
    cout << "        Sample transaction code in status_history: " << txCode << endl;

    // 1.4 Inbound transaction query
    Vector<Record> inbounds;
    user_query::inbound_transaction_query(&db, inbounds);
    cout << "  [1.4] inbound_transaction_query: " << inbounds.size() << " records." << endl;
    assert(inbounds.size() > 0);
    assert(inbounds[0].has("inbound_transaction_id"));
    assert(inbounds[0].has("inbound_transaction.inbound_transaction_id"));
    assert(inbounds[0].has("inbound_transaction.inbound_code"));
    assert(inbounds[0].has("customer.customer_code"));
    assert(inbounds[0].has("inbound_transaction.order_at"));
    assert(inbounds[0].has("inbound_transaction.type"));
    assert(inbounds[0].has("inbound_transaction.status"));
    assert(inbounds[0].has("inbound_transaction.note"));

    // 1.5 Inbound detail query (Có actual_quantity)
    Vector<Record> inboundDetails;
    user_query::inbound_detail_query(&db, inboundDetails);
    cout << "  [1.5] inbound_detail_query: " << inboundDetails.size() << " records." << endl;
    assert(inboundDetails.size() > 0);
    assert(inboundDetails[0].has("inbound_detail_id"));
    assert(inboundDetails[0].has("inbound_detail.inbound_detail_id"));
    assert(inboundDetails[0].has("inbound_transaction.inbound_code"));
    assert(inboundDetails[0].has("product.product_code"));
    assert(inboundDetails[0].has("product.product_name"));
    assert(inboundDetails[0].has("inbound_detail.ordered_quantity"));
    assert(inboundDetails[0].has("inbound_detail.actual_quantity"));

    // 1.6 Delivery query
    Vector<Record> deliveries;
    user_query::delivery_query(&db, deliveries);
    cout << "  [1.6] delivery_query: " << deliveries.size() << " records." << endl;
    assert(deliveries.size() > 0);
    assert(deliveries[0].has("delivery_id"));
    assert(deliveries[0].has("delivery.delivery_id"));
    assert(deliveries[0].has("delivery.delivery_code"));
    assert(deliveries[0].has("inbound_transaction.inbound_code"));
    assert(deliveries[0].has("delivery.delivery_at"));
    assert(deliveries[0].has("delivery.expected_at"));
    assert(deliveries[0].has("delivery.status"));

    // 1.7 Delivery detail query (Có actual_quantity)
    Vector<Record> deliveryDetails;
    user_query::delivery_detail_query(&db, deliveryDetails);
    cout << "  [1.7] delivery_detail_query: " << deliveryDetails.size() << " records." << endl;
    assert(deliveryDetails.size() > 0);
    assert(deliveryDetails[0].has("delivery_detail_id"));
    assert(deliveryDetails[0].has("delivery_detail.delivery_detail_id"));
    assert(holds_alternative<int>(deliveryDetails[0].get("delivery_detail_id")));
    assert(deliveryDetails[0].has("delivery.delivery_code"));
    assert(deliveryDetails[0].has("product.product_code"));
    assert(deliveryDetails[0].has("product.product_name"));
    assert(deliveryDetails[0].has("delivery_detail.received_quantity"));
    assert(deliveryDetails[0].has("delivery_detail.actual_quantity"));

    // 1.8 Outbound transaction query
    Vector<Record> outbounds;
    user_query::outbound_transaction_query(&db, outbounds);
    cout << "  [1.8] outbound_transaction_query: " << outbounds.size() << " records." << endl;
    assert(outbounds.size() > 0);
    assert(outbounds[0].has("outbound_transaction_id"));
    assert(outbounds[0].has("outbound_transaction.outbound_transaction_id"));
    assert(outbounds[0].has("outbound_transaction.outbound_code"));
    assert(outbounds[0].has("customer.customer_code"));
    assert(outbounds[0].has("outbound_transaction.order_at"));
    assert(outbounds[0].has("outbound_transaction.type"));
    assert(outbounds[0].has("outbound_transaction.status"));
    assert(outbounds[0].has("outbound_transaction.note"));

    // 1.9 Outbound detail query (Có actual_quantity)
    Vector<Record> outboundDetails;
    user_query::outbound_detail_query(&db, outboundDetails);
    cout << "  [1.9] outbound_detail_query: " << outboundDetails.size() << " records." << endl;
    assert(outboundDetails.size() > 0);
    assert(outboundDetails[0].has("outbound_detail_id"));
    assert(outboundDetails[0].has("outbound_detail.outbound_detail_id"));
    assert(outboundDetails[0].has("outbound_transaction.outbound_code"));
    assert(outboundDetails[0].has("product.product_code"));
    assert(outboundDetails[0].has("product.product_name"));
    assert(outboundDetails[0].has("outbound_detail.ordered_quantity"));
    assert(outboundDetails[0].has("outbound_detail.actual_quantity"));

    // 1.10 Shipment query
    Vector<Record> shipments;
    user_query::shipment_query(&db, shipments);
    cout << "  [1.10] shipment_query: " << shipments.size() << " records." << endl;
    assert(shipments.size() > 0);
    assert(shipments[0].has("shipment_id"));
    assert(shipments[0].has("shipment.shipment_id"));
    assert(shipments[0].has("shipment.shipment_code"));
    assert(shipments[0].has("outbound_transaction.outbound_code"));
    assert(shipments[0].has("shipment.ship_at"));
    assert(shipments[0].has("shipment.expected_at"));
    assert(shipments[0].has("shipment.status"));

    // 1.11 Shipment detail query (Có actual_quantity)
    Vector<Record> shipmentDetails;
    user_query::shipment_detail_query(&db, shipmentDetails);
    cout << "  [1.11] shipment_detail_query: " << shipmentDetails.size() << " records." << endl;
    assert(shipmentDetails.size() > 0);
    assert(shipmentDetails[0].has("shipment_detail_id"));
    assert(shipmentDetails[0].has("shipment_detail.shipment_detail_id"));
    assert(holds_alternative<int>(shipmentDetails[0].get("shipment_detail_id")));
    assert(shipmentDetails[0].has("shipment.shipment_code"));
    assert(shipmentDetails[0].has("product.product_code"));
    assert(shipmentDetails[0].has("product.product_name"));
    assert(shipmentDetails[0].has("shipment_detail.received_quantity"));
    assert(shipmentDetails[0].has("shipment_detail.actual_quantity"));

    // 1.12 Category Hardware Classification Queries
    Vector<Record> cpus;
    user_query::cpu_query(&db, cpus);
    cout << "  [1.12a] cpu_query: " << cpus.size() << " records." << endl;
    assert(cpus.size() > 0);
    assert(cpus[0].has("cpu_id"));
    assert(cpus[0].has("cpu.cpu_id"));
    assert(cpus[0].has("product.product_code"));
    assert(cpus[0].has("product.product_name"));
    assert(cpus[0].has("cpu.socket"));
    assert(cpus[0].has("cpu.tier"));
    assert(cpus[0].has("cpu.series"));
    assert(cpus[0].has("product.is_sold"));

    Vector<Record> gpus;
    user_query::gpu_query(&db, gpus);
    cout << "  [1.12b] gpu_query: " << gpus.size() << " records." << endl;
    assert(gpus.size() > 0);
    assert(gpus[0].has("gpu_id"));
    assert(gpus[0].has("gpu.gpu_id"));
    assert(gpus[0].has("product.product_code"));
    assert(gpus[0].has("gpu.series"));
    assert(gpus[0].has("gpu.producer"));
    assert(gpus[0].has("gpu.vram"));
    assert(gpus[0].has("product.is_sold"));

    Vector<Record> ssds;
    user_query::ssd_query(&db, ssds);
    cout << "  [1.12c] ssd_query: " << ssds.size() << " records." << endl;
    assert(ssds.size() > 0);
    assert(ssds[0].has("ssd_id"));
    assert(ssds[0].has("ssd.ssd_id"));
    assert(ssds[0].has("product.product_code"));
    assert(ssds[0].has("ssd.type"));
    assert(ssds[0].has("ssd.gen"));
    assert(ssds[0].has("ssd.capacity"));
    assert(ssds[0].has("product.is_sold"));

    // 1.13 Auxiliary Spec & Metadata Query
    Vector<Record> brands;
    user_query::auxiliary_spec_query(&db, "brand", brands);
    cout << "  [1.13a] auxiliary_spec_query (brand): " << brands.size() << " records." << endl;
    assert(brands.size() > 0);
    assert(brands[0].has("brand_id"));
    assert(brands[0].has("brand.brand_id"));
    assert(brands[0].has("brand.brand"));

    Vector<Record> cpuSockets;
    user_query::auxiliary_spec_query(&db, "cpu_socket", cpuSockets);
    cout << "  [1.13b] auxiliary_spec_query (cpu_socket): " << cpuSockets.size() << " records." << endl;
    assert(cpuSockets.size() > 0);
    assert(cpuSockets[0].has("id"));
    assert(cpuSockets[0].has("cpu_socket.id"));
    assert(cpuSockets[0].has("cpu_socket.spec"));

    // 1.14 Customer Queries (Rule 9.4: Tách riêng Individual và Business)
    Vector<Record> indCusts;
    user_query::individual_customer_query(&db, indCusts);
    cout << "  [1.14a] individual_customer_query: " << indCusts.size() << " records." << endl;
    assert(indCusts.size() >= 2);
    assert(indCusts[0].has("customer_id"));
    assert(indCusts[0].has("customer.customer_id"));
    assert(indCusts[0].has("customer.customer_code"));
    assert(indCusts[0].has("customer.name"));
    assert(indCusts[0].has("individual_info.phone_number"));
    assert(indCusts[0].has("individual_info.email"));

    Vector<Record> busCusts;
    user_query::business_customer_query(&db, busCusts);
    cout << "  [1.14b] business_customer_query: " << busCusts.size() << " records." << endl;
    assert(busCusts.size() >= 3);
    assert(busCusts[0].has("customer_id"));
    assert(busCusts[0].has("customer.customer_id"));
    assert(busCusts[0].has("customer.customer_code"));
    assert(busCusts[0].has("customer.name"));
    assert(busCusts[0].has("business_info.tax_code"));
    assert(busCusts[0].has("business_info.phone_number"));
    assert(busCusts[0].has("business_info.email"));

    cout << "\n  [PASS] All user queries returned FULL columns data successfully!" << endl;
}

// ============================================================
// 2. TEST TÌM KIẾM THEO CẤU TRÚC DỮ LIỆU VÀ CÁC CỘT MỚI
// ============================================================
void test_filtering_and_data_structures(Database& db)
{
    printSection("[TEST 2] Testing Filter using Data Structures (Hash, AVL, HashTree)");

    // 2.1 STATUS_HISTORY: Tìm kiếm theo mã giao dịch (tìm bên delivery/shipment bằng Hash index trước)
    Vector<Record> histByTx;
    user_query::status_history_query(&db, histByTx, "transaction", Value(string("IN-PUR-000001-D01")));
    cout << "  [2.1] status_history_query by transaction 'IN-PUR-000001-D01': " 
         << histByTx.size() << " records." << endl;
    assert(histByTx.size() > 0);
    assert(get<string>(histByTx[0].get("status_history.transaction")) == "IN-PUR-000001-D01");

    // 2.2 STATUS_HISTORY: Tìm kiếm theo ngày cụ thể (tìm bên delivery/shipment bằng AVL tree trước)
    Vector<Record> histByDate;
    user_query::status_history_query(&db, histByDate, "date", Value(string("2026-09-02")));
    cout << "  [2.2] status_history_query by date '2026-09-02': " 
         << histByDate.size() << " records." << endl;
    assert(histByDate.size() > 0);
    assert(histByDate[0].has("status_history.date"));

    // 2.3 STATUS_HISTORY: Tìm kiếm theo khoảng ngày (AVL tree range)
    Vector<Record> histByDateRange;
    user_query::status_history_query(&db, histByDateRange, "date", Value(string("2026-09-01")), Value(string("2026-09-15")));
    cout << "  [2.3] status_history_query by date range [2026-09-01 .. 2026-09-15]: " 
         << histByDateRange.size() << " records." << endl;
    assert(histByDateRange.size() > 0);

    // 2.4 CATEGORY QUERIES: Lọc theo 'is_sold' (Cây băm / Hash Tree)
    // is_sold = true (đang kinh doanh): 25 CPUs
    Vector<Record> activeCpus;
    user_query::cpu_query(&db, activeCpus, "is_sold", Value(true));
    cout << "  [2.4a] cpu_query with is_sold = true: " << activeCpus.size() << " records." << endl;
    assert(activeCpus.size() > 0);
    for (size_t i = 0; i < activeCpus.size(); ++i) {
        assert(get<bool>(activeCpus[i].get("product.is_sold")) == true);
    }

    // is_sold = false (ngừng kinh doanh)
    Vector<Record> inactiveCpus;
    user_query::cpu_query(&db, inactiveCpus, "is_sold", Value(false));
    cout << "  [2.4b] cpu_query with is_sold = false: " << inactiveCpus.size() << " records." << endl;
    assert(inactiveCpus.size() == 0 || inactiveCpus.size() == 2);

    // 2.5 CATEGORY QUERIES: Lọc theo thông số danh mục (ví dụ: CPU socket LGA1700)
    Vector<Record> lga1700Cpus;
    user_query::cpu_query(&db, lga1700Cpus, "socket", Value(string("LGA1700")));
    cout << "  [2.5] cpu_query with socket = 'LGA1700': " << lga1700Cpus.size() << " records." << endl;
    assert(lga1700Cpus.size() > 0);
    for (size_t i = 0; i < lga1700Cpus.size(); ++i) {
        assert(get<string>(lga1700Cpus[i].get("cpu.socket")) == "LGA1700");
    }

    // 2.6 PRODUCT_QUANTITY: Lọc theo is_sold = true (HashTree index)
    Vector<Record> activeProds;
    user_query::product_quantity_query(&db, activeProds, "is_sold", Value(true));
    cout << "  [2.6] product_quantity_query with is_sold = true: " << activeProds.size() << " records." << endl;
    assert(activeProds.size() > 0);

    // 2.7 CUSTOMER QUERIES: Lọc theo mã khách hàng và mã số thuế (Rule 9.4)
    Vector<Record> indByCode;
    user_query::individual_customer_query(&db, indByCode, "customer_code", Value(string("IN-0000001")));
    cout << "  [2.7a] individual_customer_query with customer_code = 'IN-0000001': " << indByCode.size() << " records." << endl;
    assert(indByCode.size() == 1);
    assert(get<string>(indByCode[0].get("customer.name")) == "Nguyen Van An");

    Vector<Record> busByTax;
    user_query::business_customer_query(&db, busByTax, "tax_code", Value(string("0101234567")));
    cout << "  [2.7b] business_customer_query with tax_code = '0101234567': " << busByTax.size() << " records." << endl;
    assert(busByTax.size() == 1);
    assert(get<string>(busByTax[0].get("customer.name")) == "FPT Information System");

    // 2.8 INBOUND TRANSACTION & DELIVERY: Lọc theo type (HashTree), status (HashTree) và order_at/delivery_at (AVL)
    Vector<Record> inbPur;
    user_query::inbound_transaction_query(&db, inbPur, "type", Value(string("PURCHASING")));
    cout << "  [2.8a] inbound_transaction_query with type = 'PURCHASING': " << inbPur.size() << " records." << endl;
    assert(inbPur.size() > 0);
    for (size_t i = 0; i < inbPur.size(); ++i) {
        assert(get<string>(inbPur[i].get("inbound_transaction.type")) == "PURCHASING");
    }

    Vector<Record> inbByCust;
    user_query::inbound_transaction_query(&db, inbByCust, "customer_code", Value(string("BS-0000001")));
    cout << "  [2.8b] inbound_transaction_query by customer_code = 'BS-0000001': " << inbByCust.size() << " records." << endl;
    assert(inbByCust.size() > 0);

    Vector<Record> inbByDateRange;
    user_query::inbound_transaction_query(&db, inbByDateRange, "order_at", Value(string("2026-09-01")), Value(string("2026-09-15")));
    cout << "  [2.8c] inbound_transaction_query by order_at range: " << inbByDateRange.size() << " records." << endl;
    assert(inbByDateRange.size() > 0);

    Vector<Record> delivByStatus;
    user_query::delivery_query(&db, delivByStatus, "status", Value(string("COMPLETED")));
    cout << "  [2.8d] delivery_query with status = 'COMPLETED': " << delivByStatus.size() << " records." << endl;
    assert(delivByStatus.size() > 0);

    Vector<Record> delivByCode;
    user_query::delivery_query(&db, delivByCode, "delivery_code", Value(string("IN-PUR-000001-D01")));
    cout << "  [2.8e] delivery_query with delivery_code = 'IN-PUR-000001-D01': " << delivByCode.size() << " records." << endl;
    assert(delivByCode.size() == 1);

    Vector<Record> delivByDateRange;
    user_query::delivery_query(&db, delivByDateRange, "delivery_at", Value(string("2026-09-01")), Value(string("2026-09-15")));
    cout << "  [2.8f] delivery_query by delivery_at range: " << delivByDateRange.size() << " records." << endl;
    assert(delivByDateRange.size() > 0);

    Vector<Record> delivDetailsByCode;
    user_query::delivery_detail_query(&db, delivDetailsByCode, "delivery_code", Value(string("IN-PUR-000001-D01")));
    cout << "  [2.8g] delivery_detail_query with delivery_code = 'IN-PUR-000001-D01': " << delivDetailsByCode.size() << " records." << endl;
    assert(delivDetailsByCode.size() > 0);

    // 2.9 OUTBOUND TRANSACTION & SHIPMENT: Lọc theo type (HashTree), status (HashTree), code (Hash), và ship_at (AVL)
    Vector<Record> outbSel;
    user_query::outbound_transaction_query(&db, outbSel, "type", Value(string("SELLING")));
    cout << "  [2.9a] outbound_transaction_query with type = 'SELLING': " << outbSel.size() << " records." << endl;
    assert(outbSel.size() > 0);
    for (size_t i = 0; i < outbSel.size(); ++i) {
        assert(get<string>(outbSel[i].get("outbound_transaction.type")) == "SELLING");
    }

    Vector<Record> outbByCust;
    user_query::outbound_transaction_query(&db, outbByCust, "customer_code", Value(string("IN-0000001")));
    cout << "  [2.9b] outbound_transaction_query by customer_code = 'IN-0000001': " << outbByCust.size() << " records." << endl;
    assert(outbByCust.size() > 0);

    Vector<Record> outbByDateRange;
    user_query::outbound_transaction_query(&db, outbByDateRange, "order_at", Value(string("2026-09-01")), Value(string("2026-09-15")));
    cout << "  [2.9c] outbound_transaction_query by order_at range: " << outbByDateRange.size() << " records." << endl;
    assert(outbByDateRange.size() > 0);

    Vector<Record> shipByStatus;
    user_query::shipment_query(&db, shipByStatus, "status", Value(string("COMPLETED")));
    cout << "  [2.9d] shipment_query with status = 'COMPLETED': " << shipByStatus.size() << " records." << endl;
    assert(shipByStatus.size() > 0);

    Vector<Record> shipByCode;
    user_query::shipment_query(&db, shipByCode, "shipment_code", Value(string("OUT-SEL-000001-S01")));
    cout << "  [2.9e] shipment_query with shipment_code = 'OUT-SEL-000001-S01': " << shipByCode.size() << " records." << endl;
    assert(shipByCode.size() == 1);

    Vector<Record> shipByDateRange;
    user_query::shipment_query(&db, shipByDateRange, "ship_at", Value(string("2026-09-01")), Value(string("2026-09-15")));
    cout << "  [2.9f] shipment_query by ship_at range: " << shipByDateRange.size() << " records." << endl;
    assert(shipByDateRange.size() > 0);

    Vector<Record> shipDetailsByCode;
    user_query::shipment_detail_query(&db, shipDetailsByCode, "shipment_code", Value(string("OUT-SEL-000001-S01")));
    cout << "  [2.9g] shipment_detail_query with shipment_code = 'OUT-SEL-000001-S01': " << shipDetailsByCode.size() << " records." << endl;
    assert(shipDetailsByCode.size() > 0);

    cout << "\n  [PASS] Data structure filtering on transactions, dates, and category specs verified successfully!" << endl;
}

// ============================================================================
// 3. TEST SẮP XẾP TRÊN VECTOR<RECORD> ĐÃ LƯU TRỮ
// ============================================================================
void test_post_query_sort(Database& db)
{
    printSection("[TEST 3] Testing Sort on stored Vector<Record>");

    Vector<Record> records;
    user_query::product_quantity_query(&db, records);
    assert(records.size() > 0);

    // Sắp xếp tăng dần theo product_code
    user_query::sort_query_results(records, "product.product_code", true);
    for (size_t i = 0; i + 1 < records.size(); ++i)
    {
        string c1 = get<string>(records[i].get("product.product_code"));
        string c2 = get<string>(records[i + 1].get("product.product_code"));
        assert(c1 <= c2);
    }
    cout << "  - Sorted ASC by 'product.product_code': First = " 
         << get<string>(records[0].get("product.product_code")) << endl;

    // Sắp xếp giảm dần theo available_quan
    user_query::sort_query_results(records, "product_quantity.available_quan", false);
    for (size_t i = 0; i + 1 < records.size(); ++i)
    {
        int q1 = get<int>(records[i].get("product_quantity.available_quan"));
        int q2 = get<int>(records[i + 1].get("product_quantity.available_quan"));
        assert(q1 >= q2);
    }
    cout << "  - Sorted DESC by 'available_quan': Highest = " 
         << get<int>(records[0].get("product_quantity.available_quan"))
         << " | Lowest = " << get<int>(records[records.size() - 1].get("product_quantity.available_quan")) << endl;

    cout << "\n  [PASS] Sorting on stored Vector<Record> verified successfully!" << endl;
}

// ============================================================================
// 4. TEST BÁO CÁO XUẤT - NHẬP - TỒN ĐỊNH KỲ (Section 9)
// ============================================================================
void test_periodic_inventory_report(Database& db)
{
    printSection("[TEST 4] Testing Periodic Inventory Report (Section 9)");

    Vector<Record> reportResults;
    Record grandTotal;
    user_query::periodic_inventory_report(&db, 9, 2026, reportResults, grandTotal);
    cout << "  - Report records for 09/2026: " << reportResults.size() << " products." << endl;
    assert(reportResults.size() > 0);
    assert(reportResults[0].has("product_code"));
    assert(reportResults[0].has("product_name"));
    assert(reportResults[0].has("opening_stock"));
    assert(reportResults[0].has("inbound_quantity"));
    assert(reportResults[0].has("outbound_quantity"));
    assert(reportResults[0].has("closing_stock"));
    assert(reportResults[0].has("available_quan"));
    assert(reportResults[0].has("warranty_quan"));
    assert(reportResults[0].has("scrapped_quan"));

    // Kiểm tra tính bất biến: Closing = Opening + Inbound - Outbound
    for (size_t i = 0; i < reportResults.size(); ++i) {
        int openStock = get<int>(reportResults[i].get("opening_stock"));
        int inQty = get<int>(reportResults[i].get("inbound_quantity"));
        int outQty = get<int>(reportResults[i].get("outbound_quantity"));
        int closeStock = get<int>(reportResults[i].get("closing_stock"));
        if (closeStock != openStock + inQty - outQty) {
            cout << "Product: " << get<string>(reportResults[i].get("product_code"))
                 << " Open: " << openStock << " In: " << inQty << " Out: " << outQty
                 << " Close: " << closeStock << endl;
        }
        assert(closeStock == openStock + inQty - outQty);
    }

    assert(grandTotal.has("opening_stock"));
    assert(grandTotal.has("closing_stock"));
    cout << "  - Grand Total: Opening = " << get<int>(grandTotal.get("opening_stock"))
         << " | Inbound = " << get<int>(grandTotal.get("inbound_quantity"))
         << " | Outbound = " << get<int>(grandTotal.get("outbound_quantity"))
         << " | Closing = " << get<int>(grandTotal.get("closing_stock")) << endl;

    bool expOk = DataIO::export_periodic_inventory_report_csv(9, 2026, reportResults, grandTotal);
    assert(expOk);
    cout << "  - Exported report to report/BaoCao_Thang_09_Nam_2026.csv successfully!" << endl;

    cout << "\n  [PASS] Periodic Inventory Report verified successfully!" << endl;
}

// ============================================================================
// MAIN RUNNER
// ============================================================================
int main()
{
    cout << "==========================================================" << endl;
    cout << "      TESTING USERQUERY ACCORDING TO PROJECT RULES §11    " << endl;
    cout << "==========================================================" << endl;

    Database db;
    cout << "[+] Loading database tables from 'data' directory..." << endl;
    bool ok = DatabaseIO::readAll(db, "data");
    DatabaseIO::readAllCategorySpecs(db, "data");
    cout << "[+] DatabaseIO::readAll status: " << (ok ? "SUCCESS" : "PARTIAL") << endl;
    cout << "[+] Loaded table count: " << db.getTableCount() << endl;
    assert(db.getTableCount() >= 32);

    test_all_user_queries_full_view(db);
    test_filtering_and_data_structures(db);
    test_post_query_sort(db);
    test_periodic_inventory_report(db);

    cout << "\n==========================================================" << endl;
    cout << "   >>> ALL USERQUERY TESTS PASSED SUCCESSFULLY (100%)! <<<" << endl;
    cout << "==========================================================" << endl;
    return 0;
}

