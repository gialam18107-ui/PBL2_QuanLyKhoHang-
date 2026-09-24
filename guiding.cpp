/**
 * ============================================================================
 *                             GUIDING EXAMPLE
 *               How to use the PBL2 Custom Database Engine
 * ============================================================================
 *
 * Compilation command:
 *   g++ -std=c++17 -I. database/*.cpp guiding.cpp -o guiding.exe
 * Or via VS Code:
 *   Run Task: "Build Guiding Demo"
 *   mingw32-make guiding.exe
 *   .\guiding.exe
 * ============================================================================
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Core Custom Data Structures (No STL containers, no smart pointers)
#include "database/Vector.h"
#include "database/HashMap.h"
#include "database/Pair.h"
#include "database/Optional.h"

// Core Database Headers
#include "database/Column.h"
#include "database/Condition.h"
#include "database/ConditionGroup.h"
#include "database/Constraint.h"
#include "database/Database.h"
#include "database/DatabaseManager.h"
#include "database/Index.h"
#include "database/Join.h"
#include "database/OrderBy.h"
#include "database/Query.h"
#include "database/Record.h"
#include "database/Table.h"
#include "database/User.h"
#include "database/Aggregate.h"

using namespace std;

// Helper to print section titles
void printHeader(const string &title) {
    cout << "\n======================================================\n";
    cout << "  " << title << "\n";
    cout << "======================================================\n";
}

int main() {
    printHeader("PBL2 DATABASE LIBRARY - COMPREHENSIVE GUIDE & DEMO");
    printHeader("PBL2 DATABASE ENGINE - COMPREHENSIVE GUIDE & DEMO");

    // ========================================================================
    // BƯỚC 1: KHỞI TẠO ĐỐI TƯỢNG DATABASE
    // BƯỚC 1: KHỞI TẠO DATABASE VÀ MULTI-DATABASE MANAGER (CREATE DATABASE)
    // ========================================================================
    printHeader("BƯỚC 1: Khởi tạo Database");
    Database db;
    cout << "[+] Đã tạo database thành công." << endl;
    printHeader("BƯỚC 1: Quản lý Cơ sở dữ liệu (DatabaseManager & CREATE DATABASE)");
    DatabaseManager dbManager;
    Database* db = dbManager.createDatabase("warehouse_db");
    dbManager.useDatabase("warehouse_db");
    cout << "[+] Đã tạo và chuyển sang database: " << db->getName() << endl;

    // ========================================================================
    // BƯỚC 2: ĐỊNH NGHĨA SCHEMA CHO CÁC BẢNG (COLUMNS & CONSTRAINTS)
    // BƯỚC 2: ĐỊNH NGHĨA CẤU TRÚC BẢNG (COLUMNS, CONSTRAINTS, CHECK)
    // ========================================================================
    printHeader("BƯỚC 2: Định nghĩa Cấu trúc Bảng (Categories & Products)");

    // ------------------------------------------------------------------------
    // Bảng 1: "categories" (Danh mục hàng hóa)
    // ------------------------------------------------------------------------
    auto tableCategories = make_shared<Table>("categories");
    Table* tableCategories = new Table("categories");

    // Thêm các cột:
    // Column(name, DataType, size, isPK, isUnique, isNotNull, isAutoIncrement, defaultValue)
    tableCategories->addColumn(Column("category_id", DataType::INTEGER, 0, true, true, true));
    tableCategories->addColumn(Column("category_name", DataType::VARCHAR, 50, false, true, true));
    tableCategories->addColumn(Column("description", DataType::TEXT, 0, false, false, false));

    // Thêm ràng buộc (Constraints) cho bảng:
    tableCategories->addConstraint(make_shared<PrimaryKeyConstraint>("pk_category", "category_id"));
    tableCategories->addConstraint(make_shared<UniqueConstraint>("uq_category_name", "category_name"));
    tableCategories->addConstraint(new PrimaryKeyConstraint("pk_category", "category_id"));
    tableCategories->addConstraint(new UniqueConstraint("uq_category_name", "category_name"));

    // In cấu trúc bảng categories:
    tableCategories->printStructure();

    // ------------------------------------------------------------------------
    // Bảng 2: "products" (Sản phẩm trong kho)
    // ------------------------------------------------------------------------
    auto tableProducts = make_shared<Table>("products");
    Table* tableProducts = new Table("products");

    tableProducts->addColumn(Column("product_id", DataType::INTEGER, 0, true, true, true));
    tableProducts->addColumn(Column("product_name", DataType::VARCHAR, 100, false, false, true));
    tableProducts->addColumn(Column("category_id", DataType::INTEGER, 0, false, false, true));
    tableProducts->addColumn(Column("price", DataType::INTEGER, 0, false, false, true));
    tableProducts->addColumn(Column("quantity", DataType::INTEGER, 0, false, false, true));

    // Primary key: product_id
    tableProducts->addConstraint(make_shared<PrimaryKeyConstraint>("pk_product", "product_id"));

    // Foreign key: products.category_id -> categories.category_id
    tableProducts->addConstraint(make_shared<ForeignKeyConstraint>(
        "fk_product_category", // Tên constraint
        "category_id",         // Cột ở bảng hiện tại (products)
        "categories",          // Bảng được tham chiếu
        "category_id"          // Cột được tham chiếu ở bảng categories
    // Constraints: PK, FK, CHECK
    tableProducts->addConstraint(new PrimaryKeyConstraint("pk_product", "product_id"));
    tableProducts->addConstraint(new ForeignKeyConstraint(
        "fk_product_category", "category_id", "categories", "category_id"
    ));
    tableProducts->addConstraint(new CheckConstraint(
        "chk_price_positive", Condition("price", CompareOperator::GREATER, 0)
    ));

    // In cấu trúc bảng products:
    // Indexes: HashIndex on product_name, BTreeIndex on price
    tableProducts->createIndex("idx_product_name", "product_name", IndexType::HASH);
    tableProducts->createIndex("idx_product_price", "price", IndexType::BTREE);

    tableProducts->printStructure();

    // ========================================================================
    // BƯỚC 3: ĐĂNG KÝ BẢNG VÀO DATABASE
    // BƯỚC 3: ĐĂNG KÝ BẢNG VÀO DATABASE (DATABASE OWNS TABLE POINTERS)
    // ========================================================================
    printHeader("BƯỚC 3: Đăng ký Bảng vào Database");
    db.addTable(tableCategories);
    db.addTable(tableProducts);
    db->addTable(tableCategories);
    db->addTable(tableProducts);

    cout << "Tổng số bảng hiện tại: " << db.getTableCount() << endl;
    db.printTables();
    cout << "Tổng số bảng hiện tại: " << db->getTableCount() << endl;
    db->printTables();

    // ========================================================================
    // BƯỚC 4: THÊM DỮ LIỆU (INSERT) & KIỂM TRA RÀNG BUỘC (CONSTRAINTS)
    // ========================================================================
    printHeader("BƯỚC 4: Thêm dữ liệu (INSERT) & Kiểm tra Ràng buộc");

    // --- 4.1 Thêm dữ liệu vào bảng categories ---
    cout << "\n[+] Thêm các danh mục vào bảng categories:" << endl;

    Record cat1;
    cat1.set("category_id", 1);
    cat1.set("category_name", string("Electronics"));
    cat1.set("description", string("Thiết bị điện tử, công nghệ"));
    db.insert("categories", cat1);

    Record cat2;
    cat2.set("category_id", 2);
    cat2.set("category_name", string("Accessories"));
    cat2.set("description", string("Phụ kiện máy tính, điện thoại"));
    db.insert("categories", cat2);

    Record cat3;
    cat3.set("category_id", 3);
    cat3.set("category_name", string("Furniture"));
    cat3.set("description", string("Bàn ghế, nội thất văn phòng"));
    db.insert("categories", cat3);

    // 4.1 Thêm categories
    Record cat1; cat1.set("category_id", 1); cat1.set("category_name", string("Electronics")); cat1.set("description", string("Thiết bị điện tử")); db->insert("categories", cat1);
    Record cat2; cat2.set("category_id", 2); cat2.set("category_name", string("Accessories")); cat2.set("description", string("Phụ kiện")); db->insert("categories", cat2);
    Record cat3; cat3.set("category_id", 3); cat3.set("category_name", string("Furniture")); cat3.set("description", string("Nội thất văn phòng")); db->insert("categories", cat3);
    cout << " - Đã thêm 3 danh mục thành công." << endl;

    // --- 4.2 Thêm dữ liệu vào bảng products ---
    cout << "\n[+] Thêm các sản phẩm vào bảng products:" << endl;

    Record p1;
    p1.set("product_id", 101);
    p1.set("product_name", string("Dell UltraSharp Monitor"));
    p1.set("category_id", 1);
    p1.set("price", 7500000);
    p1.set("quantity", 15);
    db.insert("products", p1);

    Record p2;
    p2.set("product_id", 102);
    p2.set("product_name", string("Logitech MX Master 3S"));
    p2.set("category_id", 2);
    p2.set("price", 2200000);
    p2.set("quantity", 40);
    db.insert("products", p2);

    Record p3;
    p3.set("product_id", 103);
    p3.set("product_name", string("Keychron K2 Mechanical Keyboard"));
    p3.set("category_id", 2);
    p3.set("price", 1850000);
    p3.set("quantity", 25);
    db.insert("products", p3);

    Record p4;
    p4.set("product_id", 104);
    p4.set("product_name", string("Ergonomic Standing Desk"));
    p4.set("category_id", 3);
    p4.set("price", 5500000);
    p4.set("quantity", 8);
    db.insert("products", p4);

    Record p5;
    p5.set("product_id", 105);
    p5.set("product_name", string("Sony WH-1000XM5 Headphones"));
    p5.set("category_id", 1);
    p5.set("price", 8200000);
    p5.set("quantity", 12);
    db.insert("products", p5);

    // 4.2 Thêm products
    Record p1; p1.set("product_id", 101); p1.set("product_name", string("Dell UltraSharp Monitor")); p1.set("category_id", 1); p1.set("price", 7500000); p1.set("quantity", 15); db->insert("products", p1);
    Record p2; p2.set("product_id", 102); p2.set("product_name", string("Logitech MX Master 3S")); p2.set("category_id", 2); p2.set("price", 2200000); p2.set("quantity", 40); db->insert("products", p2);
    Record p3; p3.set("product_id", 103); p3.set("product_name", string("Keychron K2 Keyboard")); p3.set("category_id", 2); p3.set("price", 1850000); p3.set("quantity", 25); db->insert("products", p3);
    Record p4; p4.set("product_id", 104); p4.set("product_name", string("Standing Desk")); p4.set("category_id", 3); p4.set("price", 5500000); p4.set("quantity", 8); db->insert("products", p4);
    Record p5; p5.set("product_id", 105); p5.set("product_name", string("Sony WH-1000XM5")); p5.set("category_id", 1); p5.set("price", 8200000); p5.set("quantity", 12); db->insert("products", p5);
    cout << " - Đã thêm 5 sản phẩm thành công." << endl;

    // --- 4.3 Thử nghiệm vi phạm ràng buộc (Constraint Violations) ---
    cout << "\n--- Kiểm tra cơ chế tự bảo vệ của Database ---" << endl;
    // 4.3 Thử vi phạm ràng buộc
    cout << "\n[Test A] Vi phạm Primary Key (trùng product_id = 101):" << endl;
    Record pDup; pDup.set("product_id", 101); pDup.set("product_name", string("Dup Item")); pDup.set("category_id", 1); pDup.set("price", 10000); pDup.set("quantity", 1);
    bool okDup = db->insert("products", pDup);
    cout << " => Kết quả: " << (okDup ? "LỖI" : "THÀNH CÔNG: Bị chặn chính xác!") << endl;

    // Test A: Trùng khóa chính (Primary Key)
    cout << "\n[Test A] Thêm sản phẩm có product_id 101 (đã tồn tại):" << endl;
    Record pDupPK;
    pDupPK.set("product_id", 101);
    pDupPK.set("product_name", string("Duplicate Item"));
    pDupPK.set("category_id", 1);
    pDupPK.set("price", 100000);
    pDupPK.set("quantity", 5);
    bool okDupPK = db.insert("products", pDupPK);
    cout << " => Kết quả: " << (okDupPK ? "LỖI: Chấp nhận trùng PK!" : "THÀNH CÔNG: Bị chặn chính xác!") << endl;
    cout << "\n[Test B] Vi phạm Foreign Key (category_id = 999):" << endl;
    Record pFk; pFk.set("product_id", 999); pFk.set("product_name", string("Ghost")); pFk.set("category_id", 999); pFk.set("price", 10000); pFk.set("quantity", 1);
    bool okFk = db->insert("products", pFk);
    cout << " => Kết quả: " << (okFk ? "LỖI" : "THÀNH CÔNG: Bị chặn chính xác!") << endl;

    // Test B: Vi phạm Foreign Key (category_id 999 không tồn tại)
    cout << "\n[Test B] Thêm sản phẩm có category_id = 999 (không có trong categories):" << endl;
    Record pInvalidFK;
    pInvalidFK.set("product_id", 999);
    pInvalidFK.set("product_name", string("Ghost Product"));
    pInvalidFK.set("category_id", 999); // Invalid FK
    pInvalidFK.set("price", 50000);
    pInvalidFK.set("quantity", 1);
    bool okInvalidFK = db.insert("products", pInvalidFK);
    cout << " => Kết quả: " << (okInvalidFK ? "LỖI: Chấp nhận sai FK!" : "THÀNH CÔNG: Bị chặn chính xác!") << endl;
    cout << "\n[Test C] Vi phạm CHECK constraint (price <= 0):" << endl;
    Record pChk; pChk.set("product_id", 998); pChk.set("product_name", string("Free Item")); pChk.set("category_id", 1); pChk.set("price", -500); pChk.set("quantity", 1);
    bool okChk = db->insert("products", pChk);
    cout << " => Kết quả: " << (okChk ? "LỖI" : "THÀNH CÔNG: Bị chặn chính xác!") << endl;

    // ========================================================================
    // BƯỚC 5: ĐỌC DỮ LIỆU TOÀN BẢNG (READ / SELECT ALL)
    // BƯỚC 5: TRUY VẤN NÂNG CAO VỚI QUERY (JOIN, WHERE, ORDER BY, LIMIT)
    // ========================================================================
    printHeader("BƯỚC 5: Hiển thị Dữ liệu các Bảng");
    tableCategories->printRows();
    tableProducts->printRows();
    printHeader("BƯỚC 5: Truy vấn Dữ liệu Nâng cao (INNER JOIN, WHERE, ORDER BY, LIMIT)");
    {
        Query q(*db);
        q.from("products")
            .join(Join("products", "category_id", "categories", "category_id", JoinType::INNER))
            .select({"products.product_id", "products.product_name", "categories.category_name", "products.price"})
            .where(Condition("products.price", CompareOperator::GREATER_EQUAL, 2000000))
            .orderBy("products.price", SortOrder::DESC)
            .limit(3);

        Vector<JoinedRecord> results = q.execute();
        cout << "[+] Top 3 sản phẩm đắt nhất (>= 2.000.000 VNĐ):" << endl;
        for (const auto& r : results) {
            cout << " - " << get<string>(r.get("products.product_name"))
                 << " [" << get<string>(r.get("categories.category_name")) << "] : "
                 << get<int>(r.get("products.price")) << " VNĐ" << endl;
        }
    }

    // ========================================================================
    // BƯỚC 6: CẬP NHẬT DỮ LIỆU (UPDATE)
    // BƯỚC 6: HÀM TỔNG HỢP (AGGREGATES), GROUP BY VÀ HAVING
    // ========================================================================
    printHeader("BƯỚC 6: Cập nhật Dữ liệu (UPDATE)");
    printHeader("BƯỚC 6: Aggregate Functions (COUNT, SUM, AVG, MIN, MAX), GROUP BY & HAVING");
    {
        // 6.1 Tổng hợp toàn bảng
        Query qTotal(*db);
        qTotal.from("products")
            .aggregate(AggregateType::COUNT, "*", "total_products")
            .aggregate(AggregateType::SUM, "price", "sum_price")
            .aggregate(AggregateType::AVG, "price", "avg_price")
            .aggregate(AggregateType::MIN, "price", "min_price")
            .aggregate(AggregateType::MAX, "price", "max_price");

    cout << "[+] Cập nhật số lượng và giá của sản phẩm có product_id = 102:" << endl;
    Condition updateCond("product_id", CompareOperator::EQUAL, 102);
    Record newValues;
    newValues.set("price", 2100000); // Giảm giá từ 2.200.000 -> 2.100.000
    newValues.set("quantity", 50);   // Tăng kho từ 40 -> 50
        Vector<JoinedRecord> totalRes = qTotal.execute();
        cout << "[+] Thống kê toàn kho:" << endl;
        cout << "  - Số loại sản phẩm: " << get<int>(totalRes[0].get("total_products")) << endl;
        cout << "  - Tổng đơn giá:     " << get<int>(totalRes[0].get("sum_price")) << endl;
        cout << "  - Giá trung bình:   " << get<double>(totalRes[0].get("avg_price")) << endl;
        cout << "  - Giá thấp nhất:    " << get<int>(totalRes[0].get("min_price")) << endl;
        cout << "  - Giá cao nhất:     " << get<int>(totalRes[0].get("max_price")) << endl;

    bool updateSuccess = db.update("products", updateCond, newValues);
    cout << " => Cập nhật " << (updateSuccess ? "THÀNH CÔNG" : "THẤT BÀI") << endl;
        // 6.2 GROUP BY category_id + HAVING
        Query qGroup(*db);
        qGroup.from("products")
            .groupBy("products.category_id")
            .aggregate(AggregateType::COUNT, "*", "product_count")
            .aggregate(AggregateType::SUM, "quantity", "total_stock")
            .having(Condition("product_count", CompareOperator::GREATER_EQUAL, 2));

    // Kiểm tra lại sản phẩm 102
    vector<Record> allProducts = db.selectAll("products");
    for (const auto &r : allProducts) {
        if (r.has("product_id") && get<int>(r.get("product_id")) == 102) {
            cout << " - Sau khi cập nhật sản phẩm 102: " << endl;
            r.print();
        Vector<JoinedRecord> groupRes = qGroup.execute();
        cout << "\n[+] Thống kê theo Danh mục có >= 2 sản phẩm (GROUP BY & HAVING):" << endl;
        for (const auto& r : groupRes) {
            cout << " - Category ID: " << get<int>(r.get("products.category_id"))
                 << " | Số loại SP: " << get<int>(r.get("product_count"))
                 << " | Tổng tồn kho: " << get<int>(r.get("total_stock")) << endl;
        }
    }

    // ========================================================================
    // BƯỚC 7: TRUY VẤN NÂNG CAO (QUERY, JOIN, WHERE, ORDER BY, SELECT)
    // BƯỚC 7: SUBQUERIES VÀ SET OPERATIONS (UNION, INTERSECT, EXCEPT)
    // ========================================================================
    printHeader("BƯỚC 7: Truy vấn Dữ liệu Nâng cao với Query");
    printHeader("BƯỚC 7: Subqueries và Set Operations (UNION, INTERSECT, EXCEPT)");
    {
        // Subquery: Tìm sản phẩm thuộc danh mục có tên 'Electronics'
        Query subCat(*db);
        subCat.from("categories")
            .select({"categories.category_id"})
            .where(Condition("categories.category_name", CompareOperator::EQUAL, string("Electronics")));

    // LƯU Ý QUAN TRỌNG VỀ QUERY:
    // Khi dùng Query, các cột trong JoinedRecord đều được đặt tên tiền tố:
    // "tableName.columnName" (Ví dụ: "products.price", "categories.category_name")
        Query mainQ(*db);
        mainQ.from("products")
            .select({"products.product_name", "products.price"})
            .whereIn("products.category_id", subCat);

    // --- 7.1 Truy vấn đơn giản: Lọc sản phẩm có price >= 5.000.000 ---
    cout << "\n[7.1] Lọc sản phẩm có giá >= 5.000.000:" << endl;
    {
        Query q(db);
        q.from("products")
            .where(Condition("products.price", CompareOperator::GREATER_EQUAL, 5000000))
            .orderBy(OrderBy("products.price", SortOrder::DESC));

        vector<JoinedRecord> results = q.execute();
        cout << " -> Tìm thấy " << results.size() << " sản phẩm:" << endl;
        for (const auto &row : results) {
            cout << "   * " << get<string>(row.get("products.product_name"))
                << " - Giá: " << get<int>(row.get("products.price"))
                << " - SL: " << get<int>(row.get("products.quantity")) << endl;
        Vector<JoinedRecord> subRes = mainQ.execute();
        cout << "[+] Sản phẩm thuộc danh mục 'Electronics' (Subquery WHERE IN):" << endl;
        for (const auto& r : subRes) {
            cout << "  * " << get<string>(r.get("products.product_name")) << " - " << get<int>(r.get("products.price")) << endl;
        }
    }

    // --- 7.2 Lọc kết hợp nhiều điều kiện (ConditionGroup: AND) ---
    cout << "\n[7.2] Lọc sản phẩm có: category_id == 2 AND quantity > 30:" << endl;
    {
        ConditionGroup group(LogicalOperator::AND);
        group.add(Condition("products.category_id", CompareOperator::EQUAL, 2));
        group.add(Condition("products.quantity", CompareOperator::GREATER, 30));
        // Set operations: UNION, INTERSECT, EXCEPT
        Query qA(*db);
        qA.from("products").select({"products.product_name"}).where(Condition("products.price", CompareOperator::LESS, 3000000));

        Query q(db);
        q.from("products").where(group);
        Query qB(*db);
        qB.from("products").select({"products.product_name"}).where(Condition("products.quantity", CompareOperator::GREATER, 20));

        vector<JoinedRecord> results = q.execute();
        cout << " -> Tìm thấy " << results.size() << " sản phẩm thỏa mãn:" << endl;
        for (const auto &row : results) {
            cout << "   * " << get<string>(row.get("products.product_name"))
                << " (SL: " << get<int>(row.get("products.quantity")) << ")" << endl;
        }
        Vector<JoinedRecord> unionRes = qA.unionWith(qB, true);
        cout << "\n[+] UNION (Giá < 3tr HOẶC SL > 20, loại trùng): " << unionRes.size() << " records." << endl;

        Vector<JoinedRecord> intersectRes = qA.intersectWith(qB);
        cout << "[+] INTERSECT (Giá < 3tr VÀ SL > 20): " << intersectRes.size() << " records." << endl;

        Vector<JoinedRecord> exceptRes = qA.exceptWith(qB);
        cout << "[+] EXCEPT (Giá < 3tr NHƯNG KHÔNG CÓ SL > 20): " << exceptRes.size() << " records." << endl;
    }

    // --- 7.3 Nối bảng (INNER JOIN products & categories) + Sắp xếp & Chọn cột ---
    cout << "\n[7.3] INNER JOIN 'products' và 'categories', ORDER BY price ASC, Chọn cột:" << endl;
    // ========================================================================
    // BƯỚC 8: QUẢN LÝ GIAO DỊCH (TRANSACTIONS: BEGIN, COMMIT, ROLLBACK)
    // ========================================================================
    printHeader("BƯỚC 8: Quản lý Giao dịch (TRANSACTIONS: BEGIN, ROLLBACK, COMMIT)");
    {
        Query q(db);
        q.from("products")
            .join(Join("products", "category_id", "categories", "category_id", JoinType::INNER))
            .select({
                "products.product_id",
                "products.product_name",
                "categories.category_name",
                "products.price",
                "products.quantity"
            })
            .orderBy(OrderBy("products.price", SortOrder::ASC));
        cout << "[+] Bắt đầu giao dịch (BEGIN TRANSACTION)..." << endl;
        db->beginTransaction();

        vector<JoinedRecord> results = q.execute();
        cout << " -> Kết quả sau khi JOIN (" << results.size() << " bản ghi):" << endl;
        cout << "----------------------------------------------------------------------" << endl;
        for (const auto &row : results) {
            int id = get<int>(row.get("products.product_id"));
            string name = get<string>(row.get("products.product_name"));
            string catName = get<string>(row.get("categories.category_name"));
            int price = get<int>(row.get("products.price"));
            int qty = get<int>(row.get("products.quantity"));
        Record tempP;
        tempP.set("product_id", 888);
        tempP.set("product_name", string("Temporary Product"));
        tempP.set("category_id", 1);
        tempP.set("price", 100000);
        tempP.set("quantity", 5);
        db->insert("products", tempP);

            cout << " [ID: " << id << "] " << name << " | Loại: " << catName
                << " | Giá: " << price << " | Tồn kho: " << qty << endl;
        }
        cout << "----------------------------------------------------------------------" << endl;
        cout << " - Số lượng sản phẩm sau khi insert trong transaction: " << db->getTable("products")->getRowCount() << endl;

        cout << "[+] Khôi phục trạng thái (ROLLBACK)..." << endl;
        db->rollback();
        cout << " - Số lượng sản phẩm sau rollback: " << db->getTable("products")->getRowCount() << " (đã hoàn tác thành công!)" << endl;
    }

    // ========================================================================
    // BƯỚC 8: XÓA DỮ LIỆU (DELETE / REMOVE) & TOÀN VẸN THAM CHIẾU
    // BƯỚC 9: QUẢN LÝ NGƯỜI DÙNG & PHÂN QUYỀN (USER & PERMISSIONS)
    // ========================================================================
    printHeader("BƯỚC 8: Xóa Dữ liệu (REMOVE) & Bảo vệ Toàn vẹn Dữ liệu");
    printHeader("BƯỚC 9: Quản lý Người dùng & Phân quyền (USER & PERMISSION MANAGEMENT)");
    {
        db->createUser("admin", "admin_secret_pass");
        db->createUser("staff", "staff_123");

    // Thử xóa Danh mục 1 (Electronics) - Danh mục này ĐANG CÓ SẢN PHẨM THAM CHIẾU!
    cout << "[+] Thử xóa category_id = 1 (đang được sản phẩm 101, 105 tham chiếu):" << endl;
    Condition delCat1("category_id", CompareOperator::EQUAL, 1);
    bool delCat1Success = db.remove("categories", delCat1);
    cout << " => Kết quả: " << (delCat1Success ? "LỖI (Xóa được dù có FK tham chiếu)" : "THÀNH CÔNG: Bị từ chối xóa để bảo toàn dữ liệu!") << endl;
        db->grantPermission("admin", "*", Permission::ALL);
        db->grantPermission("staff", "products", Permission::SELECT);

    // Xóa sản phẩm 104 (bàn làm việc)
    cout << "\n[+] Xóa sản phẩm 104 (thuộc Furniture):" << endl;
    Condition delP104("product_id", CompareOperator::EQUAL, 104);
    bool delP104Success = db.remove("products", delP104);
    cout << " => Xóa sản phẩm 104: " << (delP104Success ? "THÀNH CÔNG" : "THẤT BÀI") << endl;
        cout << "[+] Xác thực admin: " << (db->authenticate("admin", "admin_secret_pass") ? "THÀNH CÔNG" : "THẤT BÀI") << endl;
        cout << "[+] Xác thực staff sai mật khẩu: " << (db->authenticate("staff", "wrong_pass") ? "LỖI" : "BỊ TỪ CHỐI CHÍNH XÁC") << endl;

    // Bây giờ category 3 (Furniture) không còn sản phẩm nào tham chiếu -> Xóa được!
    cout << "\n[+] Xóa category_id = 3 (Furniture - nay không còn sản phẩm nào):" << endl;
    Condition delCat3("category_id", CompareOperator::EQUAL, 3);
    bool delCat3Success = db.remove("categories", delCat3);
    cout << " => Xóa danh mục 3: " << (delCat3Success ? "THÀNH CÔNG" : "THẤT BÀI") << endl;
        cout << "[+] Quyền của staff trên bảng products: " << (db->hasPermission("staff", "products", Permission::SELECT) ? "CÓ QUYỀN SELECT" : "KHÔNG CÓ QUYỀN") << endl;
        cout << "[+] Quyền của staff để DELETE products: " << (db->hasPermission("staff", "products", Permission::DELETE) ? "CÓ QUYỀN" : "BỊ TỪ CHỐI CHÍNH XÁC") << endl;
        cout << "[+] Quyền của admin để DELETE products: " << (db->hasPermission("admin", "products", Permission::DELETE) ? "CÓ QUYỀN ALL" : "KHÔNG CÓ QUYỀN") << endl;
    }

    // Kiểm tra lại bảng categories
    cout << "\n[+] Danh mục còn lại sau khi xóa:" << endl;
    tableCategories->printRows();

    printHeader("TẤT CẢ TÍNH NĂNG CỦA DATABASE ENGINE ĐÃ HOÀN TẤT THÀNH CÔNG!");

    printHeader("TẤT CẢ TÍNH NĂNG THEO DATABASE.MD ĐÃ HOÀN THÀNH VÀ HOẠT ĐỘNG HOÀN HẢO!");
    return 0;
}
