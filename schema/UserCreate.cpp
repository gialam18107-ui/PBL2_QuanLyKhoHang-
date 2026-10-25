#include "UserCreate.h"
#include <sstream>
#include <iomanip>
#include <cctype>
#include "datastructure/vector/Vector.h"

namespace user_create
{
    int getNextProductId(Database* db)
    {
        if (!db) return 1;
        Table* productTable = db->getTable("product");
        if (!productTable) return 1;

        int maxId = 0;
        const auto& rows = productTable->getRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (productTable->isRowDeleted(i)) continue;
            const Record& r = rows[i];
            if (r.has("product_id") && holds_alternative<int>(r.get("product_id"))) {
                int id = get<int>(r.get("product_id"));
                if (id > maxId) maxId = id;
            }
        }
        return maxId + 1;
    }

    std::string generateNextProductCode(
        Database* db,
        const std::string& brandName,
        const std::string& categoryName
    ) {
        if (!db) return "";

        std::string brandCode = brandName;
        Table* brandTable = db->getTable("brand");
        if (brandTable) {
            const auto& rows = brandTable->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (brandTable->isRowDeleted(i)) continue;
                const Record& r = rows[i];
                if (r.has("brand") && holds_alternative<std::string>(r.get("brand"))) {
                    if (get<std::string>(r.get("brand")) == brandName) {
                        if (r.has("code") && holds_alternative<std::string>(r.get("code"))) {
                            brandCode = get<std::string>(r.get("code"));
                        }
                        break;
                    }
                }
            }
        }

        std::string catCode = categoryName;
        Table* catTable = db->getTable("category");
        if (catTable) {
            const auto& rows = catTable->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (catTable->isRowDeleted(i)) continue;
                const Record& r = rows[i];
                if (r.has("category_name") && holds_alternative<std::string>(r.get("category_name"))) {
                    if (get<std::string>(r.get("category_name")) == categoryName) {
                        if (r.has("code") && holds_alternative<std::string>(r.get("code"))) {
                            catCode = get<std::string>(r.get("code"));
                        }
                        break;
                    }
                }
            }
        }

        int brandSeq = 0;
        int catSeq = 0;
        Table* prodTable = db->getTable("product");
        if (prodTable) {
            const auto& rows = prodTable->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (prodTable->isRowDeleted(i)) continue;
                const Record& r = rows[i];
                if (r.has("brand") && holds_alternative<std::string>(r.get("brand"))) {
                    if (get<std::string>(r.get("brand")) == brandName) {
                        brandSeq++;
                    }
                }
                if (r.has("category") && holds_alternative<std::string>(r.get("category"))) {
                    if (get<std::string>(r.get("category")) == categoryName) {
                        catSeq++;
                    }
                }
            }
        }
        brandSeq += 1;
        catSeq += 1;

        std::ostringstream oss;
        oss << brandCode << "-" << catCode << "-"
            << std::setfill('0') << std::setw(4) << brandSeq << "-"
            << std::setfill('0') << std::setw(4) << catSeq;
        return oss.str();
    }

    std::string generateNextSku(
        Database* db,
        const std::string& brandName,
        const std::string& categoryName,
        int productId
    ) {
        std::string brandCode = brandName;
        if (db) {
            Table* brandTable = db->getTable("brand");
            if (brandTable) {
                const auto& rows = brandTable->getRows();
                for (size_t i = 0; i < rows.size(); ++i) {
                    if (brandTable->isRowDeleted(i)) continue;
                    const Record& r = rows[i];
                    if (r.has("brand") && holds_alternative<std::string>(r.get("brand"))) {
                        if (get<std::string>(r.get("brand")) == brandName) {
                            if (r.has("code") && holds_alternative<std::string>(r.get("code"))) {
                                brandCode = get<std::string>(r.get("code"));
                            }
                            break;
                        }
                    }
                }
            }
        }

        std::string catCode = categoryName;
        if (db) {
            Table* catTable = db->getTable("category");
            if (catTable) {
                const auto& rows = catTable->getRows();
                for (size_t i = 0; i < rows.size(); ++i) {
                    if (catTable->isRowDeleted(i)) continue;
                    const Record& r = rows[i];
                    if (r.has("category_name") && holds_alternative<std::string>(r.get("category_name"))) {
                        if (get<std::string>(r.get("category_name")) == categoryName) {
                            if (r.has("code") && holds_alternative<std::string>(r.get("code"))) {
                                catCode = get<std::string>(r.get("code"));
                            }
                            break;
                        }
                    }
                }
            }
        }

        std::ostringstream oss;
        oss << catCode << "-" << brandCode << "-"
            << std::setfill('0') << std::setw(4) << productId;
        return oss.str();
    }

    std::string generateEan13Barcode(int productId)
    {
        std::ostringstream rawOss;
        rawOss << "893" << std::setfill('0') << std::setw(9) << productId;
        std::string raw12 = rawOss.str().substr(0, 12);

        int evenSum = 0;
        int oddSum = 0;
        for (int i = 0; i < 12; ++i) {
            int digit = raw12[i] - '0';
            if (i % 2 == 1) {
                evenSum += digit;
            } else {
                oddSum += digit;
            }
        }
        int total = oddSum + evenSum * 3;
        int checkDigit = (10 - (total % 10)) % 10;

        return raw12 + std::to_string(checkDigit);
    }

    bool isValidProductCodeFormat(const std::string& code)
    {
        // Expected format: <PRODUCER>-<CAT>-<4DIGITS>-<4DIGITS>
        if (code.length() < 13) return false;

        // Split by '-'
        Vector<std::string> parts;
        std::stringstream ss(code);
        std::string token;
        while (std::getline(ss, token, '-')) {
            parts.push_back(token);
        }

        if (parts.size() != 4) return false;
        if (parts[0].empty() || parts[1].empty()) return false;

        // parts[2] and parts[3] must be 4 digits
        if (parts[2].length() != 4 || parts[3].length() != 4) return false;
        for (char c : parts[2]) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        }
        for (char c : parts[3]) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        }

        return true;
    }

    bool isProductCodeUnique(Database* db, const std::string& code)
    {
        if (!db) return false;
        Table* prodTable = db->getTable("product");
        if (!prodTable) return true;

        const auto& rows = prodTable->getRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (prodTable->isRowDeleted(i)) continue;
            const Record& r = rows[i];
            if (r.has("product_code") && holds_alternative<std::string>(r.get("product_code"))) {
                if (get<std::string>(r.get("product_code")) == code) {
                    return false;
                }
            }
        }
        return true;
    }

    bool isBarcodeUnique(Database* db, const std::string& barcode)
    {
        if (!db) return false;
        Table* prodTable = db->getTable("product");
        if (!prodTable) return true;

        const auto& rows = prodTable->getRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (prodTable->isRowDeleted(i)) continue;
            const Record& r = rows[i];
            if (r.has("barcode") && holds_alternative<std::string>(r.get("barcode"))) {
                if (get<std::string>(r.get("barcode")) == barcode) {
                    return false;
                }
            }
        }
        return true;
    }

    bool isSkuUnique(Database* db, const std::string& sku)
    {
        if (!db) return false;
        Table* prodTable = db->getTable("product");
        if (!prodTable) return true;

        const auto& rows = prodTable->getRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (prodTable->isRowDeleted(i)) continue;
            const Record& r = rows[i];
            if (r.has("sku") && holds_alternative<std::string>(r.get("sku"))) {
                if (get<std::string>(r.get("sku")) == sku) {
                    return false;
                }
            }
        }
        return true;
    }

    OperationResult insertRecord(Database* db, const std::string& tableName, const Record& r)
    {
        if (!db) {
            return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        }
        Table* table = db->getTable(tableName);
        if (!table) {
            return OperationResult{false, "Bảng '" + tableName + "' không tồn tại trong cơ sở dữ liệu.", 0};
        }

        bool ok = db->insert(tableName, r);
        if (!ok) {
            return OperationResult{false, "Vi phạm ràng buộc dữ liệu khi thêm vào bảng '" + tableName + "'.", 0};
        }
        return OperationResult{true, "Thêm bản ghi thành công.", 1};
    }

    OperationResult createProduct(
        Database* db,
        const Record& productRecord,
        const std::string& categoryName,
        const Record& specRecord
    ) {
        if (!db) {
            return OperationResult{false, "Cơ sở dữ liệu không hợp lệ.", 0};
        }

        // 1. Schema & Existence Validation
        if (!db->hasTable("product")) {
            return OperationResult{false, "Bảng 'product' không tồn tại.", 0};
        }
        if (!db->hasTable("product_quantity")) {
            return OperationResult{false, "Bảng 'product_quantity' không tồn tại.", 0};
        }

        // Determine category table name
        std::string catUpper = categoryName;
        for (auto& c : catUpper) c = std::toupper(static_cast<unsigned char>(c));

        std::string specTable;
        if (catUpper == "CPU") specTable = "cpu";
        else if (catUpper == "GPU") specTable = "gpu";
        else if (catUpper == "MAINBOARD") specTable = "mainboard";
        else if (catUpper == "RAM") specTable = "ram";
        else if (catUpper == "SSD") specTable = "ssd";
        else if (catUpper == "PSU") specTable = "psu";
        else if (catUpper == "COOLING") specTable = "cooling";
        else if (catUpper == "CASE") specTable = "case";
        else {
            return OperationResult{false, "Danh mục không hợp lệ: '" + categoryName + "'.", 0};
        }

        if (!db->hasTable(specTable)) {
            return OperationResult{false, "Bảng thông số '" + specTable + "' không tồn tại.", 0};
        }

        // 2. Validate product input fields
        if (!productRecord.has("product_name") || isNull(productRecord.get("product_name"))) {
            return OperationResult{false, "Tên sản phẩm không được để trống.", 0};
        }
        std::string prodName = get<std::string>(productRecord.get("product_name"));
        if (prodName.empty()) {
            return OperationResult{false, "Tên sản phẩm không được để trống.", 0};
        }

        if (!productRecord.has("brand") || isNull(productRecord.get("brand"))) {
            return OperationResult{false, "Hãng sản xuất (Brand) không được để trống.", 0};
        }
        std::string brandName = get<std::string>(productRecord.get("brand"));

        // Validate Brand exists in brand table
        Table* brandTable = db->getTable("brand");
        bool brandFound = false;
        if (brandTable) {
            const auto& rows = brandTable->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (brandTable->isRowDeleted(i)) continue;
                const Record& r = rows[i];
                if (r.has("brand") && holds_alternative<std::string>(r.get("brand"))) {
                    if (get<std::string>(r.get("brand")) == brandName) {
                        brandFound = true;
                        break;
                    }
                }
            }
        }
        if (!brandFound) {
            return OperationResult{false, "Hãng sản xuất '" + brandName + "' không tồn tại trong CSDL.", 0};
        }

        // Validate Category exists in category table
        Table* catTable = db->getTable("category");
        bool catFound = false;
        if (catTable) {
            const auto& rows = catTable->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (catTable->isRowDeleted(i)) continue;
                const Record& r = rows[i];
                if (r.has("category_name") && holds_alternative<std::string>(r.get("category_name"))) {
                    if (get<std::string>(r.get("category_name")) == catUpper) {
                        catFound = true;
                        break;
                    }
                }
            }
        }
        if (!catFound) {
            return OperationResult{false, "Danh mục '" + catUpper + "' không tồn tại trong CSDL.", 0};
        }

        // Validate Product Code
        if (!productRecord.has("product_code") || isNull(productRecord.get("product_code"))) {
            return OperationResult{false, "Mã sản phẩm không được để trống.", 0};
        }
        std::string prodCode = get<std::string>(productRecord.get("product_code"));
        if (!isValidProductCodeFormat(prodCode)) {
            return OperationResult{false, "Định dạng mã sản phẩm không hợp lệ (yêu cầu dạng: BRAND-CAT-XXXX-YYYY).", 0};
        }
        if (!isProductCodeUnique(db, prodCode)) {
            return OperationResult{false, "Mã sản phẩm '" + prodCode + "' đã tồn tại trong CSDL.", 0};
        }

        // Validate Barcode
        if (!productRecord.has("barcode") || isNull(productRecord.get("barcode"))) {
            return OperationResult{false, "Mã vạch (Barcode) không được để trống.", 0};
        }
        std::string barcode = get<std::string>(productRecord.get("barcode"));
        if (barcode.length() != 13) {
            return OperationResult{false, "Mã vạch (Barcode) phải đúng 13 ký số (chuẩn EAN-13).", 0};
        }
        if (!isBarcodeUnique(db, barcode)) {
            return OperationResult{false, "Mã vạch '" + barcode + "' đã tồn tại trong CSDL.", 0};
        }

        // Validate SKU
        if (!productRecord.has("sku") || isNull(productRecord.get("sku"))) {
            return OperationResult{false, "Mã SKU không được để trống.", 0};
        }
        std::string sku = get<std::string>(productRecord.get("sku"));
        if (!isSkuUnique(db, sku)) {
            return OperationResult{false, "Mã SKU '" + sku + "' đã tồn tại trong CSDL.", 0};
        }

        // 3. Assign next product_id
        int productId = getNextProductId(db);
        if (productRecord.has("product_id") && holds_alternative<int>(productRecord.get("product_id"))) {
            int reqId = get<int>(productRecord.get("product_id"));
            if (reqId > 0) productId = reqId;
        }

        // Build product record
        Record finalProductRecord = productRecord;
        finalProductRecord.set("product_id", productId);
        finalProductRecord.set("category", catUpper);
        if (!finalProductRecord.has("is_sold")) {
            finalProductRecord.set("is_sold", true);
        }

        // Build product_quantity record (initialized to 0)
        Record finalQuantityRecord;
        finalQuantityRecord.set("product_id", productId);
        finalQuantityRecord.set("available_quan", 0);
        finalQuantityRecord.set("warranty_quan", 0);
        finalQuantityRecord.set("scrapped_quan", 0);

        // Build spec record
        Record finalSpecRecord = specRecord;
        finalSpecRecord.set("product_id", productId);
        if (specTable == "case" && !finalSpecRecord.has("product_name")) {
            finalSpecRecord.set("product_name", prodName);
        }

        // 4. Atomic Multi-Table Transaction Execution
        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu Transaction CSDL.", 0};
        }

        if (!db->insert("product", finalProductRecord)) {
            db->rollback();
            return OperationResult{false, "Lỗi khi lưu dữ liệu vào bảng 'product'.", 0};
        }

        if (!db->insert("product_quantity", finalQuantityRecord)) {
            db->rollback();
            return OperationResult{false, "Lỗi khi khởi tạo số lượng tồn kho trong 'product_quantity'.", 0};
        }

        if (!db->insert(specTable, finalSpecRecord)) {
            db->rollback();
            return OperationResult{false, "Lỗi khi lưu thông số kỹ thuật vào bảng '" + specTable + "'.", 0};
        }

        db->commit();
        return OperationResult{true, "Thêm sản phẩm và đồng bộ thông số kỹ thuật thành công!", 3};
    }
}

