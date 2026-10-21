#ifndef USERCREATE_H
#define USERCREATE_H

#include "database/Database.h"
#include "database/Record.h"
#include "datastructure/vector/Vector.h"
#include <string>

using namespace std;

namespace user_create {

    // ========================================================
    // CƠ CHẾ INSERT TỔNG QUÁT (project_rules.md 6.3)
    // Tham số: table name, Record &r
    // ========================================================
    bool insertRecord(Database* db, const string& tableName, const Record& r);

    // ========================================================
    // TẠO DỮ LIỆU ĐƯỢC PHÉP TRỰC TIẾP (project_rules.md 4.1)
    // - Brand, Category, Specs
    // - Products & Category hardware specs (CPU, GPU, ...)
    // ========================================================
    bool createBrand(Database* db, const string& brandName, const string& code = "");
    bool createCategory(Database* db, const string& categoryName, const string& code = "");
    bool createSpec(Database* db, const string& specTableName, const string& specValue);

    bool createProduct(Database* db, const Record& prodRecord, const string& categoryTable = "", const Record& categorySpecRecord = Record());

    bool createCpu(Database* db, const Record& prod, const string& socket, const string& tier, const string& series);
    bool createGpu(Database* db, const Record& prod, const string& series, const string& producer, const string& vram);
    bool createMainboard(Database* db, const Record& prod, const string& cpuSupport, const string& socket, const string& size, const string& chipset, const string& ramSupport);
    bool createRam(Database* db, const Record& prod, const string& generation, const string& capacity, const string& bus);
    bool createSsd(Database* db, const Record& prod, const string& type, const string& gen, const string& capacity);
    bool createPsu(Database* db, const Record& prod, const string& quality, const string& capacity, const string& size);
    bool createCooling(Database* db, const Record& prod, const string& type);
    bool createCase(Database* db, const Record& prod, const string& size);

    // ========================================================
    // TỰ ĐỘNG SINH MÃ HÓA ĐƠN & GIAO VẬN (QuanLyKhoHang_schema.json)
    // - Inbound Code:  IN-<TYPE_CODE>-<000001>
    // - Delivery Code: <INBOUND_CODE>-D<01>
    // - Outbound Code: OUT-<TYPE_CODE>-<000001>
    // - Shipment Code: <OUTBOUND_CODE>-S<01>
    // ========================================================
    string generateInboundCode(Database* db, const string& inboundType);
    string generateDeliveryCode(const string& inboundCode, int deliveryNo = 1);
    string generateOutboundCode(Database* db, const string& outboundType);
    string generateShipmentCode(const string& outboundCode, int shipmentNo = 1);

    // ========================================================
    // TẠO CẶP GIAO DỊCH VÀ ĐIỀU CHUYỂN UNIT (project_rules.md 4.2 & 5)
    // Ràng buộc kép nguyên tử:
    // - Inbound Transaction + Delivery + Unit transitions
    // - Outbound Transaction + Shipment + Unit transitions
    // ========================================================
    struct InboundItemDetail {
        int productId;
        int orderedQuantity;
        int receivedQuantity; // receivedQuantity <= orderedQuantity
        Vector<string> serials; // Danh sách số serial của các unit kèm theo
        string targetUnitStatus; // Trạng thái gán cho unit (mặc định: "AVAILABLE")
    };

    bool createInboundPair(
        Database* db,
        const Record& inboundTxn,
        const Record& delivery,
        const Vector<InboundItemDetail>& items
    );

    struct OutboundItemDetail {
        int productId;
        int orderedQuantity;
        int shippedQuantity; // shippedQuantity <= orderedQuantity
        Vector<string> serials; // Danh sách số serial của unit được xuất đi
        string targetUnitStatus; // Trạng thái mới của unit (ví dụ: "DELIVERED", "SOLD")
    };

    bool createOutboundPair(
        Database* db,
        const Record& outboundTxn,
        const Record& shipment,
        const Vector<OutboundItemDetail>& items
    );

} // namespace user_create

#endif // USERCREATE_H
