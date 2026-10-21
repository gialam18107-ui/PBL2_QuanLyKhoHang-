#ifndef USERDELETE_H
#define USERDELETE_H

#include "database/Database.h"
#include "database/Record.h"
#include "datastructure/vector/Vector.h"
#include <string>

using namespace std;

namespace user_delete {

    // ========================================================
    // CƠ CHẾ XÓA TỔNG QUÁT THEO ID (project_rules.md 6.3)
    // Tham số: table name, record id
    // ========================================================
    bool deleteRecord(Database* db, const string& tableName, const Value& recordId);

    // ========================================================
    // CÁC THAO TÁC XÓA AN TOÀN ĐƯỢC PHÉP (project_rules.md 3 & 4)
    // - Kiểm tra ràng buộc toàn vẹn tham chiếu (FK)
    // - Chặn xóa master status/type tables
    // - Chặn xóa giao dịch đã xác nhận (CONFIRMED)
    // - Chặn xóa trực tiếp Unit khỏi unit_inventory
    // ========================================================
    bool deleteProduct(Database* db, int productId);
    bool deleteBrand(Database* db, const string& brandName);
    bool deleteCategory(Database* db, int categoryId);
    bool deleteSpec(Database* db, const string& specTableName, int specId);
    bool deleteUnconfirmedInbound(Database* db, int txnId);
    bool deleteUnconfirmedOutbound(Database* db, int txnId);

} // namespace user_delete

#endif // USERDELETE_H

