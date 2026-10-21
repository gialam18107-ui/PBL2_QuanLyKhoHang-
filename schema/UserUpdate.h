#ifndef USERUPDATE_H
#define USERUPDATE_H

#include "database/Database.h"
#include "database/Record.h"
#include "datastructure/vector/Vector.h"
#include <string>

using namespace std;

namespace user_update {

    // ========================================================
    // CƠ CHẾ UPDATE TỔNG QUÁT (project_rules.md 6.3)
    // Tham số: table name, record id, Vector<string> &modifiedColumn, Vector<Value> &modifyingValue
    // ========================================================
    bool updateRecord(
        Database* db,
        const string& tableName,
        const Value& recordId,
        const Vector<string>& modifiedColumns,
        const Vector<Value>& modifyingValues
    );

    // ========================================================
    // CẬP NHẬT TRẠNG THÁI GIAO DỊCH / LOGISTICS (project_rules.md 4.3)
    // Người dùng chỉ được sửa trường status của Transaction, Delivery, Shipment
    // ========================================================
    bool updateInboundTransactionStatus(Database* db, int txnId, const string& newStatus);
    bool updateDeliveryStatus(Database* db, int deliveryId, const string& newStatus);
    bool updateOutboundTransactionStatus(Database* db, int txnId, const string& newStatus);
    bool updateShipmentStatus(Database* db, int shipmentId, const string& newStatus);

    // ========================================================
    // CẬP NHẬT TRẠNG THÁI SẢN PHẨM (project_rules.md 4.1)
    // ========================================================
    bool updateProductIsSold(Database* db, int productId, bool isSold);

} // namespace user_update

#endif // USERUPDATE_H

