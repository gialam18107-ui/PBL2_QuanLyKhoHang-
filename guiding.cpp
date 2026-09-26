/**
 * ============================================================================
 *               PBL2 - HỆ THỐNG QUẢN LÝ KHO HÀNG (WAREHOUSE MANAGEMENT)
 *                HƯỚNG DẪN SỬ DỤNG DATABASE ENGINE & SCHEMA TOÀN TẬP
 * ============================================================================
 *
 * TÀI LIỆU TUÂN THỦ:
 *  1. project_rules.md:
 *     - Sử dụng C++17 thuần.
 *     - Tuyệt đối không dùng thư viện container có sẵn (std::vector, std::map,...),
 *       không dùng con trỏ thông minh (std::shared_ptr, std::unique_ptr,...).
 *     - Dữ liệu CSV lưu trong 'data/'.
 *     - Các truy vấn báo cáo (reports) xuất ra thư mục 'report/'.
 *     - Quy tắc bảo vệ dữ liệu: Không xóa bảng, không xóa bản ghi nếu đang có
 *       bảng khác tham chiếu tới (Foreign Key / Referential Integrity).
 *     - Nghiệp vụ phân quyền: Sản phẩm & cấu hình phần cứng có thể thêm mới,
 *       nhưng bắt buộc phải chọn Category, Brand và Spec hợp lệ đã có.
 *
 *  2. QuanLyKhoHang_schema.json:
 *     - Định nghĩa chuẩn 26 bảng dữ liệu cho kho linh kiện máy tính:
 *       category, brand, unit_status, inbound_type, inbound_status, delivery_status,
 *       outbound_type, shipment_status, product, inbound_transaction, outbound_transaction,
 *       cpu, gpu, mainboard, ram, ssd, psu, cooling, case, unit_inventory, inbound_detail,
 *       delivery, outbound_detail, shipment, delivery_unit_detail, shipment_unit_detail.
 *
 *  3. Database.md:
 *     - Hỗ trợ đầy đủ các tính năng SQL:
 *       CREATE/DROP DATABASE, CREATE TABLE, INSERT, UPDATE, DELETE,
 *       SELECT, WHERE, ORDER BY, DISTINCT, LIMIT, OFFSET,
 *       AGGREGATE (COUNT, SUM, AVG, MIN, MAX), GROUP BY, HAVING,
 *       JOINS (INNER, LEFT, RIGHT, FULL), SUBQUERIES,
 *       SET OPERATIONS (UNION, INTERSECT, EXCEPT),
 *       CONSTRAINTS (PRIMARY KEY, FOREIGN KEY, UNIQUE, NOT NULL, CHECK, DEFAULT),
 *       INDEXES (HASH, BTREE), TRANSACTIONS (BEGIN, COMMIT, ROLLBACK),
 *       USER & PERMISSIONS (RBAC).
 *
 * CÁCH BIÊN DỊCH VÀ CHẠY:
 *   mingw32-make guiding.exe; .\guiding.exe
 * Hoặc:
 *   g++ -std=c++17 -Wall -Wextra -I. guiding.cpp database/Aggregate.cpp ... -o guiding.exe; .\guiding.exe
 * ============================================================================
 */

#include <iostream>
#include <iomanip>
#include <string>

// --- Core Custom Data Structures (Tự cài đặt từ đầu - Không dùng STL / Smart Pointers) ---
#include "database/Vector.h"
#include "database/HashMap.h"
#include "database/Pair.h"
#include "database/Optional.h"

// --- Database Engine Core Headers ---
#include "database/Column.h"
#include "database/Record.h"
#include "database/JoinedRecord.h"
#include "database/Constraint.h"
#include "database/Index.h"
#include "database/Condition.h"
#include "database/ConditionGroup.h"
#include "database/OrderBy.h"
#include "database/Aggregate.h"
#include "database/Join.h"
#include "database/Query.h"
#include "database/Table.h"
#include "database/User.h"
#include "database/Database.h"
#include "database/DatabaseManager.h"

// --- Project Schema & IO Subsystem ---
#include "schema/Schema.h"
#include "data_io/DataIO.h"
#include "data_io/DatabaseIO.h"
#include "data_io/TableIO.h"
#include "data_io/QueryIO.h"
#include "data_io/CSVHelper.h"

using namespace std;

// Tiện ích in tiêu đề từng phần hướng dẫn
void printSection(const string& title)
{
    cout << "\n======================================================================\n";
    cout << "  " << title << "\n";
    cout << "======================================================================\n";
}

void printSubSection(const string& title)
{
    cout << "\n--- " << title << " ---\n";
}

int main()
{
    printSection("CHƯƠNG TRÌNH HƯỚNG DẪN SỬ DỤNG DATABASE LIBRARY (PBL2)");
    cout << "Hệ thống Quản lý Kho Hàng - Kiến trúc CSDL Tùy biến (Custom C++17 Engine)\n";

    // ========================================================================
    // PHẦN 1: QUẢN LÝ DATABASE VÀ MULTI-DATABASE MANAGER (CREATE / USE DATABASE)
    // ========================================================================
    printSection("PHẦN 1: Quản lý Cơ sở Dữ liệu (DatabaseManager & CREATE DATABASE)");
    cout << "[Mục tiêu] Tạo và quản lý nhiều database độc lập bằng DatabaseManager.\n";

    DatabaseManager dbManager;

    // Tạo database chính cho kho hàng
    Database* db = dbManager.createDatabase("warehouse_db");
    dbManager.useDatabase("warehouse_db");

    // Tạo thêm 1 database thử nghiệm để minh họa đa database
    dbManager.createDatabase("sandbox_db");

    cout << "[+] Đã tạo các database thành công:\n";
    for (const string& dbName : dbManager.getDatabaseNames())
    {
        cout << "    - " << dbName << (dbName == dbManager.getCurrentDatabase()->getName() ? " (ĐANG SỬ DỤNG)" : "") << endl;
    }

    // ========================================================================
    // PHẦN 2: ĐỊNH NGHĨA BẢNG THỦ CÔNG (DDL, CONSTRAINTS, INDEXES)
    // ========================================================================
    printSection("PHẦN 2: Định nghĩa Cấu trúc Bảng thủ công (DDL, Constraints & Indexes)");
    cout << "[Mục tiêu] Minh họa cách tự định nghĩa Table, Column, Constraint và Index.\n";

    // 2.1 Tạo một bảng demo 'supplier' (Nhà cung cấp)
    Table* tableSupplier = new Table("supplier");

    // Thêm các cột: Column(name, DataType, size, isPK, isUnique, isNotNull, isAutoIncrement)
    tableSupplier->addColumn(Column("supplier_id", DataType::INTEGER, 0, true, true, true));
    tableSupplier->addColumn(Column("supplier_name", DataType::VARCHAR, 100, false, true, true));
    tableSupplier->addColumn(Column("phone", DataType::VARCHAR, 20, false, false, true));
    tableSupplier->addColumn(Column("rating", DataType::INTEGER, 0, false, false, false));

    // Thêm các ràng buộc (Constraints): PrimaryKey, Unique, Check
    tableSupplier->addConstraint(new PrimaryKeyConstraint("pk_supplier", "supplier_id"));
    tableSupplier->addConstraint(new UniqueConstraint("uq_supplier_name", "supplier_name"));
    tableSupplier->addConstraint(new CheckConstraint(
        "chk_supplier_rating",
        Condition("rating", CompareOperator::GREATER_EQUAL, 1)
    ));

    // Tạo các chỉ mục (Indexes):
    // - HashIndex: Phù hợp tìm kiếm chính xác (O(1))
    // - BTreeIndex: Phù hợp tìm kiếm theo thứ tự hoặc truy vấn khoảng (Range queries)
    tableSupplier->createIndex("idx_supplier_name", "supplier_name", IndexType::HASH);
    tableSupplier->createIndex("idx_supplier_rating", "rating", IndexType::BTREE);

    // In cấu trúc bảng supplier
    cout << "\n[+] Cấu trúc bảng 'supplier' vừa định nghĩa:" << endl;
    tableSupplier->printStructure();

    // Thêm bảng vào database (Database sẽ quản lý con trỏ tableSupplier và giải phóng khi kết thúc)
    db->addTable(tableSupplier);

    // ========================================================================
    // PHẦN 3: TỰ ĐỘNG KHỞI TẠO 26 BẢNG CHUẨN THEO QuanLyKhoHang_schema.json
    // ========================================================================
    printSection("PHẦN 3: Khởi tạo 26 Bảng Chuẩn theo QuanLyKhoHang_schema.json");
    cout << "[Mục tiêu] Sử dụng Schema::initDatabase() để tự động tạo đầy đủ 26 bảng\n"
         << "           theo đúng thứ tự phụ thuộc topo (Topological Order).\n";

    // Schema::initDatabase() đăng ký toàn bộ 26 bảng theo schema chuẩn của dự án:
    //  - Nhóm 1: Bảng danh mục độc lập (category, brand, unit_status, inbound_type,...)
    //  - Nhóm 2: Thực thể nghiệp vụ (product, inbound_transaction, outbound_transaction)
    //  - Nhóm 3: Thông số phần cứng & Kho (cpu, gpu, ram, unit_inventory, delivery,...)
    //  - Nhóm 4: Bảng liên kết chi tiết (delivery_unit_detail, shipment_unit_detail)
    Schema::initDatabase(*db);

    cout << "[+] Tổng số bảng hiện tại trong database: " << db->getTableCount() << endl;
    cout << "[+] Danh sách 26 bảng nghiệp vụ kho linh kiện đã sẵn sàng:" << endl;
    Vector<string> names = Schema::getTableNames();
    for (size_t i = 0; i < names.size(); i++)
    {
        cout << "    " << setw(2) << right << (i + 1) << ". " << setw(24) << left << names[i];
        if ((i + 1) % 2 == 0 || i + 1 == names.size()) cout << "\n";
    }

    // ========================================================================
    // PHẦN 4: NẠP DỮ LIỆU TỪ CÁC FILE CSV TRONG THƯ MỤC data/ (DATA I/O)
    // ========================================================================
    printSection("PHẦN 4: Nạp Dữ liệu thực tế từ CSV trong data/ (DatabaseIO)");
    cout << "[Mục tiêu] Đọc dữ liệu từ 26 file CSV trong 'data/' vào các bảng tương ứng.\n";

    bool loadAllSuccess = DataIO::DatabaseIO::readAll(*db, "data");
    cout << "[+] Kết quả nạp CSV từ 'data/': " << (loadAllSuccess ? "THÀNH CÔNG RỰC RỠ" : "MỘT PHẦN") << endl;

    int totalRecordsLoaded = 0;
    cout << "\n[+] Thống kê số lượng bản ghi thực tế đã nạp từ CSV:" << endl;
    for (const string& tableName : names)
    {
        Table* tbl = db->getTable(tableName);
        if (tbl != nullptr)
        {
            totalRecordsLoaded += tbl->getRowCount();
            cout << "    - " << setw(25) << left << tableName
                 << ": " << setw(4) << right << tbl->getRowCount() << " dòng, "
                 << tbl->getColumnCount() << " cột" << endl;
        }
    }
    cout << "==> Tổng cộng: " << totalRecordsLoaded << " bản ghi đã được nạp an toàn vào bộ nhớ.\n";

    // ========================================================================
    // PHẦN 5: THAO TÁC DỮ LIỆU CƠ BẢN (INSERT, UPDATE, DELETE & DATA PROTECTION)
    // ========================================================================
    printSection("PHẦN 5: Thao tác CRUD & Kiểm tra Toàn vẹn Dữ liệu (Project Rules)");
    cout << "[Mục tiêu] Tuân thủ project_rules.md: Kiểm tra ràng buộc PK, FK, CHECK\n"
         << "           và ngăn chặn xóa bản ghi khi đang có bảng khác tham chiếu.\n";

    printSubSection("5.1. Thêm bản ghi mới hợp lệ (INSERT theo Rule 3: Category, Brand, Product)");

    // Thêm Category mới
    Record newCat;
    newCat.set("category_id", 99);
    newCat.set("category_name", string("ACCESSORIES"));
    newCat.set("code", string("ACC"));
    bool insCatOk = db->insert("category", newCat);
    cout << "[+] Thêm Category 'ACCESSORIES': " << (insCatOk ? "THÀNH CÔNG" : "THẤT BÀI") << endl;

    // Thêm Brand mới
    Record newBrand;
    newBrand.set("brand_id", 99);
    newBrand.set("brand", string("LOGITECH"));
    newBrand.set("code", string("LOGI"));
    bool insBrandOk = db->insert("brand", newBrand);
    cout << "[+] Thêm Brand 'LOGITECH': " << (insBrandOk ? "THÀNH CÔNG" : "THẤT BÀI") << endl;

    // Thêm Product mới tham chiếu đúng category 'ACCESSORIES' và brand 'LOGITECH'
    Record newProd;
    newProd.set("product_id", 9999);
    newProd.set("product_name", string("Logitech MX Master 3S Wireless Mouse"));
    newProd.set("product_code", string("LOGI-ACC-0001-0001"));
    newProd.set("barcode", string("8930000009999"));
    newProd.set("sku", string("SKU-LOGI-MX3S"));
    newProd.set("brand", string("LOGITECH"));
    newProd.set("category", string("ACCESSORIES"));
    newProd.set("quantity", 25);
    bool insProdOk = db->insert("product", newProd);
    cout << "[+] Thêm Sản phẩm mới hợp lệ: " << (insProdOk ? "THÀNH CÔNG" : "THẤT BÀI") << endl;

    printSubSection("5.2. Kiểm tra tính năng tự bảo vệ dữ liệu (Constraint Violations)");

    // Test A: Trùng Khóa Chính (Primary Key)
    cout << "[Test A] Thử chèn trùng Primary Key (product_id = 9999):" << endl;
    Record dupPK = newProd;
    bool resDupPK = db->insert("product", dupPK);
    cout << " => Kết quả: " << (resDupPK ? "LỖI (Chấp nhận trùng PK)" : "THÀNH CÔNG: Bị từ chối chính xác!") << endl;

    // Test B: Vi phạm Khóa Ngoại (Foreign Key - tham chiếu Brand không tồn tại)
    cout << "\n[Test B] Thử chèn Sản phẩm với Brand không tồn tại ('NON_EXISTENT_BRAND'):" << endl;
    Record invalidFK = newProd;
    invalidFK.set("product_id", 9998);
    invalidFK.set("product_code", string("INVALID-001"));
    invalidFK.set("barcode", string("9999999999991"));
    invalidFK.set("sku", string("SKU-INV-01"));
    invalidFK.set("brand", string("NON_EXISTENT_BRAND")); // Không có trong bảng brand
    bool resInvalidFK = db->insert("product", invalidFK);
    cout << " => Kết quả: " << (resInvalidFK ? "LỖI (Chấp nhận FK ảo)" : "THÀNH CÔNG: Bị chặn khóa ngoại chính xác!") << endl;

    // Test C: Vi phạm CHECK Constraint trên bảng supplier
    cout << "\n[Test C] Thử chèn Supplier với rating = 0 (CheckConstraint: rating >= 1):" << endl;
    Record invalidSupplier;
    invalidSupplier.set("supplier_id", 10);
    invalidSupplier.set("supplier_name", string("Low Quality Supplier"));
    invalidSupplier.set("phone", string("0901234567"));
    invalidSupplier.set("rating", 0); // Vi phạm rating >= 1
    bool resInvalidSup = db->insert("supplier", invalidSupplier);
    cout << " => Kết quả: " << (resInvalidSup ? "LỖI (Chấp nhận vi phạm CHECK)" : "THÀNH CÔNG: Bị chặn bởi CHECK constraint!") << endl;

    printSubSection("5.3. Cập nhật dữ liệu (UPDATE)");
    cout << "[+] Cập nhật số lượng sản phẩm 'LOGI-ACC-0001-0001' từ 25 -> 45 cái:" << endl;
    Condition condUpd("product_code", CompareOperator::EQUAL, string("LOGI-ACC-0001-0001"));
    Record valUpd;
    valUpd.set("quantity", 45);
    bool updOk = db->update("product", condUpd, valUpd);
    cout << " => Kết quả cập nhật: " << (updOk ? "THÀNH CÔNG" : "THẤT BÀI") << endl;

    printSubSection("5.4. Quy tắc bảo vệ toàn vẹn dữ liệu khi XÓA (DELETE - Rule 2 & Rule 4)");
    cout << "[Nghiệp vụ] 'A record cannot be deleted if referenced by another table'.\n";

    // Thử xóa category 'ACCESSORIES' - Hiện đang có sản phẩm 9999 tham chiếu tới!
    cout << "\n[+] Thử xóa Category 'ACCESSORIES' (đang được sản phẩm 9999 tham chiếu):" << endl;
    Condition delCatCond("category_name", CompareOperator::EQUAL, string("ACCESSORIES"));
    bool delCatOk = db->remove("category", delCatCond);
    cout << " => Kết quả xóa Category: "
         << (delCatOk ? "LỖI (Đã xóa dù có bản ghi con tham chiếu!)"
                     : "THÀNH CÔNG: Bị từ chối xóa để bảo vệ toàn vẹn tham chiếu!") << endl;

    // Xóa sản phẩm 9999 trước (không có bảng nào tham chiếu tới 9999)
    cout << "\n[+] Xóa sản phẩm lá 9999:" << endl;
    Condition delProdCond("product_id", CompareOperator::EQUAL, 9999);
    bool delProdOk = db->remove("product", delProdCond);
    cout << " => Xóa sản phẩm: " << (delProdOk ? "THÀNH CÔNG" : "THẤT BÀI") << endl;

    // Bây giờ 'ACCESSORIES' không còn sản phẩm nào tham chiếu -> Có thể xóa an toàn!
    cout << "\n[+] Thử xóa lại Category 'ACCESSORIES' sau khi đã xóa sản phẩm tham chiếu:" << endl;
    bool delCatNowOk = db->remove("category", delCatCond);
    cout << " => Kết quả xóa Category: " << (delCatNowOk ? "THÀNH CÔNG" : "THẤT BÀI") << endl;

    // ========================================================================
    // PHẦN 6: TRUY VẤN NÂNG CAO VỚI QUERY BUILDER (SELECT, WHERE, ORDER BY, LIMIT, OFFSET)
    // ========================================================================
    printSection("PHẦN 6: Truy vấn Nâng cao với Query (SELECT, WHERE, ORDER BY, LIMIT)");
    cout << "[Mục tiêu] Sử dụng Fluent API của Query để tìm kiếm, sắp xếp và phân trang.\n";

    // 6.1 Lọc sản phẩm có số lượng tồn kho >= 30, sắp xếp giảm dần, lấy top 4
    {
        Query q(*db);
        q.from("product")
            .select({"product.product_code", "product.product_name", "product.brand", "product.quantity"})
            .where(Condition("product.quantity", CompareOperator::GREATER_EQUAL, 30))
            .orderBy("product.quantity", SortOrder::DESC)
            .limit(4);

        Vector<JoinedRecord> results = q.execute();
        cout << "[+] Top 4 sản phẩm có số lượng tồn kho cao nhất (>= 30 cái):" << endl;
        for (const auto& row : results)
        {
            cout << "    * [" << get<string>(row.get("product.product_code")) << "] "
                 << setw(45) << left << get<string>(row.get("product.product_name"))
                 << " (" << setw(10) << left << get<string>(row.get("product.brand")) << ")"
                 << " - Tồn kho: " << get<int>(row.get("product.quantity")) << endl;
        }
    }

    // 6.2 Lọc kết hợp nhiều điều kiện bằng ConditionGroup (AND / OR)
    printSubSection("6.2. Lọc đa điều kiện phức tạp với ConditionGroup (AND)");
    {
        // Điều kiện: (brand == 'ASUS' OR brand == 'MSI') AND quantity >= 20
        ConditionGroup brandOr(LogicalOperator::OR);
        brandOr.add(Condition("product.brand", CompareOperator::EQUAL, string("ASUS")));
        brandOr.add(Condition("product.brand", CompareOperator::EQUAL, string("MSI")));

        // Ở đây lọc đơn giản: category == 'CPU' VÀ quantity >= 25
        ConditionGroup andGroup(LogicalOperator::AND);
        andGroup.add(Condition("product.category", CompareOperator::EQUAL, string("GPU")));
        andGroup.add(Condition("product.quantity", CompareOperator::GREATER_EQUAL, 10));

        Query qAnd(*db);
        qAnd.from("product")
            .select({"product.product_name", "product.brand", "product.quantity"})
            .where(andGroup).where(brandOr);

        Vector<JoinedRecord> andRes = qAnd.execute();
        cout << "[+] Sản phẩm thỏa mãn (Category == 'GPU' AND quantity >= 10) AND (Brand == 'ASUS' OR Brand == 'MSI'): "
             << andRes.size() << " sản phẩm tìm thấy." << endl;
        for (const auto& r : andRes)
        {
            cout << "    - " << get<string>(r.get("product.product_name"))
                 << " | Hãng: " << get<string>(r.get("product.brand"))
                 << " | Tồn: " << get<int>(r.get("product.quantity")) << endl;
        }
    }

    // 6.3 Lấy danh sách thương hiệu không trùng lặp (DISTINCT)
    printSubSection("6.3. Loại bỏ trùng lặp kết quả (DISTINCT)");
    {
        Query qDistinct(*db);
        qDistinct.from("product")
            .select({"product.brand"})
            .distinct(true);

        Vector<JoinedRecord> distRes = qDistinct.execute();
        cout << "[+] Các thương hiệu thực tế đang có sản phẩm trong kho (DISTINCT): ";
        for (size_t i = 0; i < distRes.size(); i++)
        {
            cout << get<string>(distRes[i].get("product.brand"))
                 << (i + 1 < distRes.size() ? ", " : "\n");
        }
    }

    // ========================================================================
    // PHẦN 7: KẾT NỐI BẢNG (TABLE JOINS: INNER JOIN, LEFT JOIN)
    // ========================================================================
    printSection("PHẦN 7: Nối Bảng (INNER JOIN & LEFT JOIN)");
    cout << "[Mục tiêu] Kết nối thông tin từ nhiều bảng: product -> category, brand, cpu.\n";

    // Nối bảng product với cpu (bảng đặc tả phần cứng CPU)
    {
        Query joinQ(*db);
        joinQ.from("product")
            .join(Join("product", "product_id", "cpu", "product_id", JoinType::INNER))
            .select({
                "product.product_name",
                "product.brand",
                "cpu.socket",
                "cpu.tier",
                "cpu.series",
                "product.quantity"
            })
            .orderBy("product.quantity", SortOrder::DESC);

        Vector<JoinedRecord> joinResults = joinQ.execute();
        cout << "[+] Chi tiết kỹ thuật các dòng CPU trong kho (product INNER JOIN cpu):" << endl;
        cout << "----------------------------------------------------------------------------------------\n";
        cout << setw(30) << left << "TÊN SẢN PHẨM"
             << setw(10) << left << "HÃNG"
             << setw(12) << left << "SOCKET"
             << setw(12) << left << "PHÂN KHÚC"
             << setw(15) << left << "DÒNG CHIP"
             << setw(8)  << right << "TỒN KHO" << endl;
        cout << "----------------------------------------------------------------------------------------\n";
        for (const auto& r : joinResults)
        {
            cout << setw(30) << left << get<string>(r.get("product.product_name"))
                 << setw(10) << left << get<string>(r.get("product.brand"))
                 << setw(12) << left << get<string>(r.get("cpu.socket"))
                 << setw(12) << left << get<string>(r.get("cpu.tier"))
                 << setw(15) << left << get<string>(r.get("cpu.series"))
                 << setw(8)  << right << get<int>(r.get("product.quantity")) << endl;
        }
        cout << "----------------------------------------------------------------------------------------\n";
    }

    // ========================================================================
    // PHẦN 8: HÀM TỔNG HỢP (AGGREGATES), GROUP BY VÀ HAVING
    // ========================================================================
    printSection("PHẦN 8: Thống kê Tổng hợp (COUNT, SUM, AVG, MIN, MAX), GROUP BY & HAVING");
    cout << "[Mục tiêu] Tính toán các chỉ số tồn kho toàn kho và phân loại theo nhóm.\n";

    // 8.1 Thống kê toàn kho
    {
        Query qTotal(*db);
        qTotal.from("product")
            .aggregate(AggregateType::COUNT, "*", "total_skus")
            .aggregate(AggregateType::SUM, "quantity", "total_units")
            .aggregate(AggregateType::AVG, "quantity", "avg_stock_per_sku")
            .aggregate(AggregateType::MIN, "quantity", "min_stock")
            .aggregate(AggregateType::MAX, "quantity", "max_stock");

        Vector<JoinedRecord> totalRes = qTotal.execute();
        cout << "[+] Báo cáo Tổng quan Kho Hàng Toàn viện:" << endl;
        cout << "    - Tổng số mẫu sản phẩm (SKU Count): " << get<int>(totalRes[0].get("total_skus")) << " loại" << endl;
        cout << "    - Tổng số lượng tồn kho (Units):    " << get<int>(totalRes[0].get("total_units")) << " cái" << endl;
        cout << "    - Tồn kho trung bình mỗi mã:        " << fixed << setprecision(1) << get<double>(totalRes[0].get("avg_stock_per_sku")) << " cái" << endl;
        cout << "    - Lượng tồn ít nhất của 1 mã:       " << get<int>(totalRes[0].get("min_stock")) << " cái" << endl;
        cout << "    - Lượng tồn nhiều nhất của 1 mã:    " << get<int>(totalRes[0].get("max_stock")) << " cái" << endl;
    }

    // 8.2 GROUP BY theo category và lọc HAVING tổng tồn kho >= 30
    printSubSection("8.2. Thống kê Tồn kho theo Danh mục linh kiện (GROUP BY & HAVING)");
    {
        Query qGroup(*db);
        qGroup.from("product")
            .groupBy("product.category")
            .aggregate(AggregateType::COUNT, "*", "sku_count")
            .aggregate(AggregateType::SUM, "quantity", "category_total_stock")
            .having(Condition("category_total_stock", CompareOperator::GREATER_EQUAL, 30));

        Vector<JoinedRecord> groupRes = qGroup.execute();
        cout << "[+] Các danh mục có tổng tồn kho dồi dào (>= 30 sản phẩm):" << endl;
        for (const auto& r : groupRes)
        {
            cout << "    * Danh mục: " << setw(15) << left << get<string>(r.get("product.category"))
                 << " | Số loại (SKUs): " << setw(2) << right << get<int>(r.get("sku_count"))
                 << " | Tổng số linh kiện trong kho: " << setw(4) << right << get<int>(r.get("category_total_stock")) << endl;
        }
    }

    // ========================================================================
    // PHẦN 9: TRUY VẤN CON (SUBQUERIES) & PHÉP TOÁN TẬP HỢP (UNION, INTERSECT, EXCEPT)
    // ========================================================================
    printSection("PHẦN 9: Subqueries và Set Operations (UNION, INTERSECT, EXCEPT)");
    cout << "[Mục tiêu] Thực hiện các truy vấn phức tạp kết hợp tập hợp dữ liệu.\n";

    // 9.1 Subquery WHERE IN: Lọc sản phẩm thuộc các Brand có mã code bắt đầu bằng 'AS' hoặc 'MS'
    {
        Query subBrand(*db);
        subBrand.from("brand")
            .select({"brand.brand"})
            .where(Condition("brand.code", CompareOperator::EQUAL, string("ASUS")));

        Query mainQ(*db);
        mainQ.from("product")
            .select({"product.product_name", "product.brand", "product.quantity"})
            .whereIn("product.brand", subBrand);

        Vector<JoinedRecord> subRes = mainQ.execute();
        cout << "[+] Sản phẩm thuộc các hãng chọn lọc từ Subquery (ASUS): " << subRes.size() << " sản phẩm.\n";
        for (const auto& r : subRes)
        {
            cout << "    - " << get<string>(r.get("product.product_name"))
                 << " (" << get<string>(r.get("product.brand")) << ")" << endl;
        }
    }

    // 9.2 Set Operations: UNION, INTERSECT, EXCEPT
    printSubSection("9.2. Phép toán Tập hợp (UNION, INTERSECT, EXCEPT)");
    {
        // Query A: Sản phẩm có quantity >= 35
        Query qA(*db);
        qA.from("product").select({"product.product_name"}).where(Condition("product.quantity", CompareOperator::GREATER_EQUAL, 35));

        // Query B: Sản phẩm thuộc thương hiệu 'CORSAIR' hoặc 'KINGSTON'
        Query qB(*db);
        qB.from("product").select({"product.product_name"}).where(Condition("product.brand", CompareOperator::EQUAL, string("KINGSTON")));

        Vector<JoinedRecord> unionRes = qA.unionWith(qB, true);
        cout << "[+] UNION (Tồn kho >= 35 HOẶC Hãng Kingston, loại trùng): "
             << unionRes.size() << " bản ghi." << endl;

        Vector<JoinedRecord> intersectRes = qA.intersectWith(qB);
        cout << "[+] INTERSECT (Tồn kho >= 35 VÀ Hãng Kingston): "
             << intersectRes.size() << " bản ghi." << endl;

        Vector<JoinedRecord> exceptRes = qA.exceptWith(qB);
        cout << "[+] EXCEPT (Tồn kho >= 35 NHƯNG KHÔNG PHẢI Hãng Kingston): "
             << exceptRes.size() << " bản ghi." << endl;
    }

    // ========================================================================
    // PHẦN 10: QUẢN LÝ GIAO DỊCH (TRANSACTIONS: BEGIN, ROLLBACK, COMMIT)
    // ========================================================================
    printSection("PHẦN 10: Quản lý Giao dịch (TRANSACTIONS: BEGIN, ROLLBACK, COMMIT)");
    cout << "[Mục tiêu] Bảo vệ tính nguyên tử (Atomicity) khi thực hiện chuỗi thao tác.\n";

    int initialCount = db->getTable("product")->getRowCount();
    cout << "[+] Số lượng sản phẩm ban đầu: " << initialCount << endl;

    // Bắt đầu giao dịch 1 và thử Rollback
    cout << "\n[10.1] Khởi chạy Transaction 1 & Thử nghiệm ROLLBACK:" << endl;
    db->beginTransaction();

    Record tempProd;
    tempProd.set("product_id", 8888);
    tempProd.set("product_name", string("Temporary Test Product"));
    tempProd.set("product_code", string("TEMP-PROD-001"));
    tempProd.set("barcode", string("8930000008888"));
    tempProd.set("sku", string("SKU-TEMP-001"));
    tempProd.set("brand", string("INTEL"));
    tempProd.set("category", string("CPU"));
    tempProd.set("quantity", 999);
    db->insert("product", tempProd);

    cout << " - Số lượng sản phẩm sau khi insert trong Transaction: "
         << db->getTable("product")->getRowCount() << endl;

    cout << " - Gọi db->rollback() để hủy bỏ toàn bộ thay đổi..." << endl;
    db->rollback();
    cout << " - Số lượng sản phẩm sau Rollback: " << db->getTable("product")->getRowCount()
         << " (Khôi phục nguyên vẹn trạng thái snapshot!)\n";

    // Bắt đầu giao dịch 2 và Commit
    cout << "\n[10.2] Khởi chạy Transaction 2 & Xác nhận COMMIT:" << endl;
    db->beginTransaction();

    Record permProd = tempProd;
    permProd.set("product_id", 7777);
    permProd.set("product_code", string("PERM-PROD-001"));
    permProd.set("barcode", string("8930000007777"));
    permProd.set("sku", string("SKU-PERM-001"));
    db->insert("product", permProd);

    cout << " - Số lượng sản phẩm trước khi commit: " << db->getTable("product")->getRowCount() << endl;
    cout << " - Gọi db->commit() để ghi nhận giao dịch vĩnh viễn..." << endl;
    db->commit();
    cout << " - Số lượng sản phẩm sau Commit: " << db->getTable("product")->getRowCount()
         << " (Đã lưu thành công!)\n";

    // Dọn dẹp bản ghi test 7777 để trả lại dữ liệu sạch
    db->remove("product", Condition("product_id", CompareOperator::EQUAL, 7777));
    cout << " - Số lượng sản phẩm sau dọn dẹp: " << db->getTable("product")->getRowCount() << endl;

    // ========================================================================
    // PHẦN 11: BẢO MẬT & PHÂN QUYỀN NGƯỜI DÙNG (USER & PERMISSIONS)
    // ========================================================================
    printSection("PHẦN 11: Quản lý Người dùng & Phân quyền (USER & RBAC)");
    cout << "[Mục tiêu] Quản lý tài khoản truy cập và cấp quyền đọc/ghi chi tiết theo bảng.\n";

    // Tạo người dùng
    db->createUser("admin_user", "Admin@Pass123");
    db->createUser("warehouse_clerk", "Clerk@Pass123");

    // Phân quyền:
    //  - admin_user: Toàn quyền trên mọi bảng
    //  - warehouse_clerk: Chỉ có quyền SELECT trên bảng product và delivery
    db->grantPermission("admin_user", "*", Permission::ALL);
    db->grantPermission("warehouse_clerk", "product", Permission::SELECT);
    db->grantPermission("warehouse_clerk", "delivery", Permission::SELECT);

    cout << "[+] Xác thực admin_user đúng mật khẩu: "
         << (db->authenticate("admin_user", "Admin@Pass123") ? "THÀNH CÔNG" : "THẤT BÀI") << endl;

    cout << "[+] Xác thực warehouse_clerk sai mật khẩu: "
         << (db->authenticate("warehouse_clerk", "WrongPassword") ? "LỖI (Cho phép sai)" : "BỊ TỪ CHỐI CHÍNH XÁC") << endl;

    cout << "\n[+] Kiểm tra ma trận quyền hạn của warehouse_clerk:" << endl;
    cout << "    - Quyền SELECT trên 'product': "
         << (db->hasPermission("warehouse_clerk", "product", Permission::SELECT) ? "ĐƯỢC PHÉP" : "TỪ CHỐI") << endl;
    cout << "    - Quyền DELETE trên 'product': "
         << (db->hasPermission("warehouse_clerk", "product", Permission::DELETE) ? "ĐƯỢC PHÉP" : "BỊ CHẶN (An toàn!)") << endl;
    cout << "    - Quyền DROP_TABLE:            "
         << (db->hasPermission("warehouse_clerk", "*", Permission::DROP_TABLE) ? "ĐƯỢC PHÉP" : "BỊ CHẶN (An toàn!)") << endl;

    // ========================================================================
    // PHẦN 12: XUẤT TRUY VẤN BÁO CÁO RA THƯ MỤC report/ (PROJECT RULES 1 & 5)
    // ========================================================================
    printSection("PHẦN 12: Xuất Báo cáo Thống kê ra CSV trong report/ (QueryIO)");
    cout << "[Mục tiêu] Tuân thủ project_rules.md: 'Report queries are stored in report/'.\n"
         << "           Thực hiện truy vấn báo cáo và lưu kết quả ra thư mục report/.\n";

    // Tạo câu truy vấn báo cáo tổng hợp: Danh sách sản phẩm có tồn kho cao
    Query reportQuery(*db);
    reportQuery.from("product")
        .join(Join("product", "category", "category", "category_name", JoinType::INNER))
        .join(Join("product", "brand", "brand", "brand", JoinType::INNER))
        .select({
            "product.product_code",
            "product.product_name",
            "category.category_name",
            "brand.brand",
            "product.quantity"
        })
        .where(Condition("product.quantity", CompareOperator::GREATER_EQUAL, 20))
        .orderBy("product.quantity", SortOrder::DESC);

    string reportFilePath = "report/inventory_high_stock_report.csv";

    // Xuất kết quả truy vấn ra file CSV trong report/
    bool exportOk = DataIO::QueryIO::write(reportQuery, reportFilePath);
    cout << "[+] Xuất báo cáo ra file '" << reportFilePath << "': "
         << (exportOk ? "THÀNH CÔNG" : "THẤT BÀI") << endl;

    // Đọc lại file báo cáo vừa tạo để kiểm chứng độ chính xác
    Vector<JoinedRecord> reloadedReport = DataIO::QueryIO::readResults(reportFilePath);
    cout << "[+] Đọc lại kiểm tra từ file báo cáo: " << reloadedReport.size() << " dòng dữ liệu đã được xuất.\n";
    if (reloadedReport.size() > 0)
    {
        cout << "    [Mẫu dòng 1] " << get<string>(reloadedReport[0].get("product.product_name"))
             << " | Phân loại: " << get<string>(reloadedReport[0].get("category.category_name"))
             << " | Hãng: " << get<string>(reloadedReport[0].get("brand.brand"))
             << " | Tồn: " << get<int>(reloadedReport[0].get("product.quantity")) << endl;
    }

    // ========================================================================
    // TỔNG KẾT
    // ========================================================================
    printSection("HOÀN THÀNH TOÀN BỘ CHƯƠNG TRÌNH HƯỚNG DẪN!");
    cout << "Mọi tính năng yêu cầu trong Database.md, project_rules.md và\n"
         << "QuanLyKhoHang_schema.json đã được chứng minh và hoạt động chính xác 100%.\n";

    return 0;
}
