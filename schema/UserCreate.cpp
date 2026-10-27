#include "UserCreate.h"
#include <sstream>
#include <iomanip>
#include <cctype>

namespace user_create
{
    static std::string getSpecTableName(const std::string& categoryName)
    {
        std::string catUpper = categoryName;
        for (auto& c : catUpper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (catUpper == "CPU") return "cpu";
        if (catUpper == "GPU") return "gpu";
        if (catUpper == "MAINBOARD") return "mainboard";
        if (catUpper == "RAM") return "ram";
        if (catUpper == "SSD") return "ssd";
        if (catUpper == "PSU") return "psu";
        if (catUpper == "COOLING") return "cooling";
        if (catUpper == "CASE") return "case";
        return "";
    }

    static bool isColumnValueUnique(Table* table, const std::string& colName, const std::string& val)
    {
        if (!table) return true;
        const auto& rows = table->getRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (table->isRowDeleted(i)) continue;
            const Record& r = rows[i];
            if (r.has(colName) && std::holds_alternative<std::string>(r.get(colName))) {
                if (std::get<std::string>(r.get(colName)) == val) return false;
            }
        }
        return true;
    }

    static int extractMaxSequence(Table* table, const std::string& colName, const std::string& prefix, size_t numOffset)
    {
        if (!table) return 0;
        int maxSeq = 0;
        const auto& rows = table->getRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (table->isRowDeleted(i)) continue;
            const Record& r = rows[i];
            if (r.has(colName) && std::holds_alternative<std::string>(r.get(colName))) {
                std::string code = std::get<std::string>(r.get(colName));
                if (code.rfind(prefix, 0) == 0 && code.length() > numOffset) {
                    try {
                        int seq = std::stoi(code.substr(numOffset));
                        if (seq > maxSeq) maxSeq = seq;
                    } catch (...) {}
                }
            }
        }
        return maxSeq;
    }

    static int findCustomerIdByCode(Table* custTable, const std::string& customerCode)
    {
        if (!custTable || customerCode.empty()) return -1;
        const auto& rows = custTable->getRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (custTable->isRowDeleted(i)) continue;
            const Record& r = rows[i];
            if (r.has("customer_code") && std::holds_alternative<std::string>(r.get("customer_code"))) {
                if (std::get<std::string>(r.get("customer_code")) == customerCode) {
                    if (r.has("customer_id") && std::holds_alternative<int>(r.get("customer_id"))) {
                        return std::get<int>(r.get("customer_id"));
                    }
                }
            }
        }
        return -1;
    }

    int getNextTableId(Database* db, const std::string& tableName)
    {
        if (!db) return 1;
        Table* table = db->getTable(tableName);
        if (!table) return 1;
        std::string pkCol = table->getPrimaryKeyColumn();
        if (pkCol.empty()) return 1;

        int maxId = 0;
        const auto& rows = table->getRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (table->isRowDeleted(i)) continue;
            const Record& r = rows[i];
            if (r.has(pkCol) && std::holds_alternative<int>(r.get(pkCol))) {
                int id = std::get<int>(r.get(pkCol));
                if (id > maxId) maxId = id;
            }
        }
        return maxId + 1;
    }

    int getNextProductId(Database* db)
    {
        return getNextTableId(db, "product");
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
                if (r.has("brand") && std::holds_alternative<std::string>(r.get("brand")) && std::get<std::string>(r.get("brand")) == brandName) {
                    if (r.has("code") && std::holds_alternative<std::string>(r.get("code"))) {
                        brandCode = std::get<std::string>(r.get("code"));
                    }
                    break;
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
                if (r.has("category_name") && std::holds_alternative<std::string>(r.get("category_name")) && std::get<std::string>(r.get("category_name")) == categoryName) {
                    if (r.has("code") && std::holds_alternative<std::string>(r.get("code"))) {
                        catCode = std::get<std::string>(r.get("code"));
                    }
                    break;
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
                if (r.has("brand") && std::holds_alternative<std::string>(r.get("brand")) && std::get<std::string>(r.get("brand")) == brandName) {
                    brandSeq++;
                }
                if (r.has("category") && std::holds_alternative<std::string>(r.get("category")) && std::get<std::string>(r.get("category")) == categoryName) {
                    catSeq++;
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

    bool isValidProductCodeFormat(const std::string& code)
    {
        if (code.length() < 13) return false;

        Vector<std::string> parts;
        std::stringstream ss(code);
        std::string token;
        while (std::getline(ss, token, '-')) {
            parts.push_back(token);
        }

        if (parts.size() != 4 || parts[0].empty() || parts[1].empty()) return false;
        if (parts[2].length() != 4 || parts[3].length() != 4) return false;

        for (char c : parts[2]) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        for (char c : parts[3]) if (!std::isdigit(static_cast<unsigned char>(c))) return false;

        return true;
    }

    bool isProductCodeUnique(Database* db, const std::string& code)
    {
        return !db || isColumnValueUnique(db->getTable("product"), "product_code", code);
    }

    bool isBarcodeUnique(Database* db, const std::string& barcode)
    {
        return !db || isColumnValueUnique(db->getTable("product"), "barcode", barcode);
    }

    bool isSkuUnique(Database* db, const std::string& sku)
    {
        return !db || isColumnValueUnique(db->getTable("product"), "sku", sku);
    }

    OperationResult insertRecord(Database* db, const std::string& tableName, const Record& r)
    {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        return db->insertRecord(tableName, r);
    }

    OperationResult createProduct(
        Database* db,
        const Record& productRecord,
        const std::string& categoryName,
        const Record& specRecord
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu không hợp lệ.", 0};

        if (!db->hasTable("product")) return OperationResult{false, "Bảng 'product' không tồn tại.", 0};
        if (!db->hasTable("product_quantity")) return OperationResult{false, "Bảng 'product_quantity' không tồn tại.", 0};

        std::string specTable = getSpecTableName(categoryName);
        if (specTable.empty()) {
            return OperationResult{false, "Danh mục không hợp lệ: '" + categoryName + "'.", 0};
        }
        if (!db->hasTable(specTable)) {
            return OperationResult{false, "Bảng thông số '" + specTable + "' không tồn tại.", 0};
        }

        // Validate product input fields
        if (!productRecord.has("product_name") || isNull(productRecord.get("product_name"))) {
            return OperationResult{false, "Tên sản phẩm không được để trống.", 0};
        }
        std::string prodName = std::get<std::string>(productRecord.get("product_name"));
        if (prodName.empty()) {
            return OperationResult{false, "Tên sản phẩm không được để trống.", 0};
        }

        if (!productRecord.has("brand") || isNull(productRecord.get("brand"))) {
            return OperationResult{false, "Hãng sản xuất (Brand) không được để trống.", 0};
        }
        std::string brandName = std::get<std::string>(productRecord.get("brand"));

        // Validate Brand exists
        Table* brandTable = db->getTable("brand");
        bool brandFound = false;
        if (brandTable) {
            const auto& rows = brandTable->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (brandTable->isRowDeleted(i)) continue;
                const Record& r = rows[i];
                if (r.has("brand") && std::holds_alternative<std::string>(r.get("brand")) && std::get<std::string>(r.get("brand")) == brandName) {
                    brandFound = true;
                    break;
                }
            }
        }
        if (!brandFound) {
            return OperationResult{false, "Hãng sản xuất '" + brandName + "' không tồn tại trong CSDL.", 0};
        }

        // Validate Category exists
        std::string catUpper = categoryName;
        for (auto& c : catUpper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        Table* catTable = db->getTable("category");
        bool catFound = false;
        if (catTable) {
            const auto& rows = catTable->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (catTable->isRowDeleted(i)) continue;
                const Record& r = rows[i];
                if (r.has("category_name") && std::holds_alternative<std::string>(r.get("category_name")) && std::get<std::string>(r.get("category_name")) == catUpper) {
                    catFound = true;
                    break;
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
        std::string prodCode = std::get<std::string>(productRecord.get("product_code"));
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
        std::string barcode = std::get<std::string>(productRecord.get("barcode"));
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
        std::string sku = std::get<std::string>(productRecord.get("sku"));
        if (!isSkuUnique(db, sku)) {
            return OperationResult{false, "Mã SKU '" + sku + "' đã tồn tại trong CSDL.", 0};
        }

        // Assign product_id
        int productId = getNextProductId(db);
        if (productRecord.has("product_id") && std::holds_alternative<int>(productRecord.get("product_id"))) {
            int reqId = std::get<int>(productRecord.get("product_id"));
            if (reqId > 0) productId = reqId;
        }

        Record finalProductRecord = productRecord;
        finalProductRecord.set("product_id", productId);
        finalProductRecord.set("category", catUpper);
        if (!finalProductRecord.has("is_sold")) {
            finalProductRecord.set("is_sold", true);
        }

        Record finalQuantityRecord;
        finalQuantityRecord.set("product_id", productId);
        finalQuantityRecord.set("available_quan", 0);
        finalQuantityRecord.set("warranty_quan", 0);
        finalQuantityRecord.set("scrapped_quan", 0);

        Record finalSpecRecord = specRecord;
        finalSpecRecord.set("product_id", productId);
        if (specTable == "case" && !finalSpecRecord.has("product_name")) {
            finalSpecRecord.set("product_name", prodName);
        }

        // Atomic Transaction Execution
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

    OperationResult createIndividualCustomer(
        Database* db,
        const std::string& name,
        const std::string& phone,
        const std::string& email
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        if (name.empty()) return OperationResult{false, "Họ tên khách hàng không được để trống.", 0};
        if (phone.empty()) return OperationResult{false, "Số điện thoại không được để trống.", 0};

        Table* custTable = db->getTable("customer");
        Table* infoTable = db->getTable("individual_info");
        if (!custTable || !infoTable) {
            return OperationResult{false, "Bảng khách hàng không tồn tại trong CSDL.", 0};
        }

        int maxSeq = extractMaxSequence(custTable, "customer_code", "IN-", 3);
        int newSeq = maxSeq + 1;
        std::ostringstream oss;
        oss << "IN-" << std::setfill('0') << std::setw(7) << newSeq;
        std::string custCode = oss.str();

        int custId = getNextTableId(db, "customer");

        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu Transaction CSDL.", 0};
        }

        Record rCust;
        rCust.set("customer_id", custId);
        rCust.set("customer_code", custCode);
        rCust.set("name", name);
        rCust.set("type", std::string("individual"));

        OperationResult resCust = db->insertRecord("customer", rCust);
        if (!resCust) {
            db->rollback();
            return OperationResult{false, "Lỗi khi lưu bảng customer: " + resCust.message, 0};
        }

        Record rInfo;
        rInfo.set("customer_id", custId);
        rInfo.set("phone_number", phone);
        rInfo.set("email", email);
        rInfo.set("next_code_no", newSeq + 1);

        OperationResult resInfo = db->insertRecord("individual_info", rInfo);
        if (!resInfo) {
            db->rollback();
            return OperationResult{false, "Lỗi khi lưu bảng individual_info: " + resInfo.message, 0};
        }

        db->commit();
        return OperationResult{true, "Tạo khách hàng cá nhân thành công! Mã: " + custCode, 2};
    }

    OperationResult createBusinessCustomer(
        Database* db,
        const std::string& companyName,
        const std::string& taxCode,
        const std::string& phone,
        const std::string& email
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        if (companyName.empty()) return OperationResult{false, "Tên công ty/doanh nghiệp không được để trống.", 0};
        if (taxCode.empty()) return OperationResult{false, "Mã số thuế không được để trống.", 0};
        if (phone.empty()) return OperationResult{false, "Số điện thoại không được để trống.", 0};

        Table* custTable = db->getTable("customer");
        Table* infoTable = db->getTable("business_info");
        if (!custTable || !infoTable) {
            return OperationResult{false, "Bảng khách hàng không tồn tại trong CSDL.", 0};
        }

        // Unique tax code check
        const auto& bRows = infoTable->getRows();
        for (size_t i = 0; i < bRows.size(); ++i) {
            if (infoTable->isRowDeleted(i)) continue;
            const Record& r = bRows[i];
            if (r.has("tax_code") && std::holds_alternative<std::string>(r.get("tax_code")) && std::get<std::string>(r.get("tax_code")) == taxCode) {
                return OperationResult{false, "Mã số thuế '" + taxCode + "' đã tồn tại trong hệ thống.", 0};
            }
        }

        int maxSeq = extractMaxSequence(custTable, "customer_code", "BS-", 3);
        int newSeq = maxSeq + 1;
        std::ostringstream oss;
        oss << "BS-" << std::setfill('0') << std::setw(7) << newSeq;
        std::string custCode = oss.str();

        int custId = getNextTableId(db, "customer");

        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu Transaction CSDL.", 0};
        }

        Record rCust;
        rCust.set("customer_id", custId);
        rCust.set("customer_code", custCode);
        rCust.set("name", companyName);
        rCust.set("type", std::string("business"));

        OperationResult resCust = db->insertRecord("customer", rCust);
        if (!resCust) {
            db->rollback();
            return OperationResult{false, "Lỗi khi lưu bảng customer: " + resCust.message, 0};
        }

        Record rInfo;
        rInfo.set("customer_id", custId);
        rInfo.set("phone_number", phone);
        rInfo.set("email", email);
        rInfo.set("tax_code", taxCode);
        rInfo.set("next_code_no", newSeq + 1);

        OperationResult resInfo = db->insertRecord("business_info", rInfo);
        if (!resInfo) {
            db->rollback();
            return OperationResult{false, "Lỗi khi lưu bảng business_info: " + resInfo.message, 0};
        }

        db->commit();
        return OperationResult{true, "Tạo khách hàng doanh nghiệp thành công! Mã: " + custCode, 2};
    }

    OperationResult createBrand(
        Database* db,
        const std::string& brandName,
        const std::string& brandCode
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        if (brandName.empty() || brandCode.empty()) {
            return OperationResult{false, "Tên thương hiệu và mã thương hiệu không được để trống.", 0};
        }

        Table* brandTable = db->getTable("brand");
        if (!brandTable) return OperationResult{false, "Bảng 'brand' không tồn tại trong CSDL.", 0};

        const auto& rows = brandTable->getRows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (brandTable->isRowDeleted(i)) continue;
            const Record& r = rows[i];
            if (r.has("brand") && std::holds_alternative<std::string>(r.get("brand")) && std::get<std::string>(r.get("brand")) == brandName) {
                return OperationResult{false, "Thương hiệu '" + brandName + "' đã tồn tại trong CSDL.", 0};
            }
            if (r.has("code") && std::holds_alternative<std::string>(r.get("code")) && std::get<std::string>(r.get("code")) == brandCode) {
                return OperationResult{false, "Mã thương hiệu '" + brandCode + "' đã tồn tại trong CSDL.", 0};
            }
        }

        int bId = getNextTableId(db, "brand");
        Record rBrand;
        rBrand.set("brand_id", bId);
        rBrand.set("brand", brandName);
        rBrand.set("code", brandCode);

        return db->insertRecord("brand", rBrand);
    }

    OperationResult createInboundOrder(
        Database* db,
        const std::string& inboundType,
        const std::string& customer_Code,
        const Vector<Pair<int, int>>& items,
        const std::string& orderDate,
        const std::string& expectedDeliveryDate,
        const std::string& note
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        if (customer_Code.empty()) {
            return OperationResult{false, "Mã đối tác / khách hàng không được để trống.", 0};
        }
        if (items.empty()) return OperationResult{false, "Đơn nhập kho phải có ít nhất một mặt hàng.", 0};
        if (orderDate.empty() || expectedDeliveryDate.empty()) {
            return OperationResult{false, "Ngày đặt hàng và ngày dự kiến giao không được để trống.", 0};
        }
        if (expectedDeliveryDate < orderDate) {
            return OperationResult{false, "Ngày dự kiến nhận hàng (" + expectedDeliveryDate + ") không được sớm hơn ngày đặt (" + orderDate + ").", 0};
        }

        Table* custTable = db->getTable("customer");
        if (!custTable) return OperationResult{false, "Bảng 'customer' không tồn tại.", 0};

        int customerId = findCustomerIdByCode(custTable, customerCode);
        if (customerId < 0) {
            return OperationResult{false, "Không tìm thấy đối tác/khách hàng với mã: '" + customerCode + "'.", 0};
        }

        Table* prodTable = db->getTable("product");
        if (!prodTable) return OperationResult{false, "Bảng 'product' không tồn tại.", 0};

        for (size_t i = 0; i < items.size(); ++i) {
            int pid = items[i].first;
            int qty = items[i].second;
            if (qty <= 0) {
                return OperationResult{false, "Số lượng nhập của sản phẩm ID " + std::to_string(pid) + " phải lớn hơn 0.", 0};
            }
            const Record* pRec = prodTable->getRecordById(pid);
            if (!pRec) {
                return OperationResult{false, "Sản phẩm ID " + std::to_string(pid) + " không tồn tại trong danh mục.", 0};
            }
            if (pRec->has("is_sold") && std::holds_alternative<bool>(pRec->get("is_sold")) && !std::get<bool>(pRec->get("is_sold"))) {
                return OperationResult{false, "Sản phẩm ID " + std::to_string(pid) + " đã ngừng kinh doanh (is_sold = FALSE), không thể nhập kho (Rule 4.2).", 0};
            }
        }

        std::string typeCode = "PUR";
        if (inboundType == "CUSTOMER_RETURN" || inboundType == "CRT") typeCode = "CRT";
        else if (inboundType == "ADJUSTMENT_IN" || inboundType == "ADJ") typeCode = "ADJ";
        else if (inboundType == "WARRANTY_CHECKING" || inboundType == "WCK") typeCode = "WCK";
        else if (inboundType == "WARRANTY_REPLACEMENT" || inboundType == "WRP") typeCode = "WRP";

        Table* inTxnTable = db->getTable("inbound_transaction");
        if (!inTxnTable) return OperationResult{false, "Bảng 'inbound_transaction' không tồn tại.", 0};

        int maxSeq = extractMaxSequence(inTxnTable, "inbound_code", "IN-" + typeCode + "-", 7);
        int newSeq = maxSeq + 1;
        std::ostringstream inOss;
        inOss << "IN-" << typeCode << "-" << std::setfill('0') << std::setw(6) << newSeq;
        std::string inCode = inOss.str();
        std::string delCode = inCode + "-D01";

        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu Transaction CSDL.", 0};
        }

        int inTxnId = getNextTableId(db, "inbound_transaction");
        Record inTxn;
        inTxn.set("inbound_transaction_id", inTxnId);
        inTxn.set("inbound_code", inCode);
        inTxn.set("customer_id", customerId);
        inTxn.set("order_at", orderDate);
        inTxn.set("type", inboundType);
        inTxn.set("status", std::string("PENDING"));
        inTxn.set("next_delivery_no", 2);
        inTxn.set("note", note);

        OperationResult r1 = db->insertRecord("inbound_transaction", inTxn);
        if (!r1) { db->rollback(); return r1; }

        for (size_t i = 0; i < items.size(); ++i) {
            int inDetId = getNextTableId(db, "inbound_detail");
            Record inDet;
            inDet.set("inbound_detail_id", inDetId);
            inDet.set("inbound_transaction_id", inTxnId);
            inDet.set("product_id", items[i].first);
            inDet.set("ordered_quantity", items[i].second);
            inDet.set("actual_quantity", 0);
            OperationResult rd = db->insertRecord("inbound_detail", inDet);
            if (!rd) { db->rollback(); return rd; }
        }

        int delId = getNextTableId(db, "delivery");
        Record del;
        del.set("delivery_id", delId);
        del.set("delivery_code", delCode);
        del.set("inbound_transaction_id", inTxnId);
        del.set("expected_at", expectedDeliveryDate);
        del.set("delivery_at", Value{});
        del.set("status", std::string("EXPECTED"));

        OperationResult r2 = db->insertRecord("delivery", del);
        if (!r2) { db->rollback(); return r2; }

        for (size_t i = 0; i < items.size(); ++i) {
            int delDetId = getNextTableId(db, "delivery_detail");
            Record delDet;
            delDet.set("delivery_detail_id", delDetId);
            delDet.set("delivery_id", delId);
            delDet.set("product_id", items[i].first);
            delDet.set("received_quantity", items[i].second);
            delDet.set("actual_quantity", 0);
            OperationResult rdd = db->insertRecord("delivery_detail", delDet);
            if (!rdd) { db->rollback(); return rdd; }
        }

        db->commit();
        return OperationResult{true, "Tạo đơn nhập kho và đợt nhận hàng thành công! Mã: " + inCode, static_cast<int>(2 + items.size() * 2)};
    }

    OperationResult createOutboundOrder(
        Database* db,
        const std::string& outboundType,
        const std::string& customerCode,
        const Vector<Pair<int, int>>& items,
        const std::string& orderDate,
        const std::string& expectedShipDate,
        const std::string& note
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        if (customerCode.empty()) {
            return OperationResult{false, "Mã khách hàng / đối tác không được để trống.", 0};
        }
        if (items.empty()) return OperationResult{false, "Đơn xuất kho phải có ít nhất một mặt hàng.", 0};
        if (orderDate.empty() || expectedShipDate.empty()) {
            return OperationResult{false, "Ngày đặt hàng và ngày dự kiến xuất không được để trống.", 0};
        }
        if (expectedShipDate < orderDate) {
            return OperationResult{false, "Ngày dự kiến xuất hàng (" + expectedShipDate + ") không được sớm hơn ngày đặt (" + orderDate + ").", 0};
        }

        Table* custTable = db->getTable("customer");
        if (!custTable) return OperationResult{false, "Bảng 'customer' không tồn tại.", 0};

        int customerId = findCustomerIdByCode(custTable, customerCode);
        if (customerId < 0) {
            return OperationResult{false, "Không tìm thấy khách hàng/đối tác với mã: '" + customerCode + "'.", 0};
        }

        Table* prodTable = db->getTable("product");
        Table* qtyTable = db->getTable("product_quantity");
        if (!prodTable || !qtyTable) {
            return OperationResult{false, "Bảng sản phẩm hoặc tồn kho không tồn tại.", 0};
        }

        for (size_t i = 0; i < items.size(); ++i) {
            int pid = items[i].first;
            int qty = items[i].second;
            if (qty <= 0) {
                return OperationResult{false, "Số lượng xuất của sản phẩm ID " + std::to_string(pid) + " phải lớn hơn 0.", 0};
            }
            const Record* pRec = prodTable->getRecordById(pid);
            if (!pRec) {
                return OperationResult{false, "Sản phẩm ID " + std::to_string(pid) + " không tồn tại trong danh mục.", 0};
            }
            if (pRec->has("is_sold") && std::holds_alternative<bool>(pRec->get("is_sold")) && !std::get<bool>(pRec->get("is_sold"))) {
                return OperationResult{false, "Sản phẩm ID " + std::to_string(pid) + " đã ngừng kinh doanh (is_sold = FALSE), không thể xuất kho (Rule 4.2).", 0};
            }

            const Record* qRec = qtyTable->getRecordById(pid);
            int avail = 0, wrt = 0, scr = 0;
            if (qRec) {
                if (qRec->has("available_quan") && std::holds_alternative<int>(qRec->get("available_quan")))
                    avail = std::get<int>(qRec->get("available_quan"));
                if (qRec->has("warranty_quan") && std::holds_alternative<int>(qRec->get("warranty_quan")))
                    wrt = std::get<int>(qRec->get("warranty_quan"));
                if (qRec->has("scrapped_quan") && std::holds_alternative<int>(qRec->get("scrapped_quan")))
                    scr = std::get<int>(qRec->get("scrapped_quan"));
            }

            if (outboundType == "SELLING" || outboundType == "SEL") {
                if (avail < qty) {
                    return OperationResult{false, "Không đủ tồn kho khả dụng cho sản phẩm ID " + std::to_string(pid) + " (Yêu cầu: " + std::to_string(qty) + ", Khả dụng: " + std::to_string(avail) + ").", 0};
                }
            } else if (outboundType == "WARRANTY_RETURN" || outboundType == "WRT") {
                if (wrt + scr < qty) {
                    return OperationResult{false, "Không đủ hàng bảo hành/phế phẩm để xuất trả cho sản phẩm ID " + std::to_string(pid) + " (Yêu cầu: " + std::to_string(qty) + ", Hiện có: " + std::to_string(wrt + scr) + ").", 0};
                }
            }
        }

        std::string typeCode = "SEL";
        if (outboundType == "ADJUSTMENT_OUT" || outboundType == "ADJ") typeCode = "ADJ";
        else if (outboundType == "RETURN_TO_SUPPLIER" || outboundType == "RTS") typeCode = "RTS";
        else if (outboundType == "WARRANTY_RETURN" || outboundType == "WRT") typeCode = "WRT";
        else if (outboundType == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || outboundType == "WRC") typeCode = "WRC";

        Table* outTxnTable = db->getTable("outbound_transaction");
        if (!outTxnTable) return OperationResult{false, "Bảng 'outbound_transaction' không tồn tại.", 0};

        int maxSeq = extractMaxSequence(outTxnTable, "outbound_code", "OUT-" + typeCode + "-", 8);
        int newSeq = maxSeq + 1;
        std::ostringstream outOss;
        outOss << "OUT-" << typeCode << "-" << std::setfill('0') << std::setw(6) << newSeq;
        std::string outCode = outOss.str();
        std::string shipCode = outCode + "-S01";

        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu Transaction CSDL.", 0};
        }

        int outTxnId = getNextTableId(db, "outbound_transaction");
        Record outTxn;
        outTxn.set("outbound_trainsaction_id", outTxnId);
        outTxn.set("outbound_code", outCode);
        outTxn.set("customer_id", customerId);
        outTxn.set("order_at", orderDate);
        outTxn.set("type", outboundType);
        outTxn.set("status", std::string("PENDING"));
        outTxn.set("next_ship_no", 2);
        outTxn.set("note", note);

        OperationResult r1 = db->insertRecord("outbound_transaction", outTxn);
        if (!r1) { db->rollback(); return r1; }

        for (size_t i = 0; i < items.size(); ++i) {
            int outDetId = getNextTableId(db, "outbound_detail");
            Record outDet;
            outDet.set("outbound_detail_id", outDetId);
            outDet.set("outbound_transaction_id", outTxnId);
            outDet.set("product_id", items[i].first);
            outDet.set("ordered_quantity", items[i].second);
            outDet.set("actual_quantity", 0);
            OperationResult rd = db->insertRecord("outbound_detail", outDet);
            if (!rd) { db->rollback(); return rd; }
        }

        int shipId = getNextTableId(db, "shipment");
        Record ship;
        ship.set("shipment_id", shipId);
        ship.set("shipment_code", shipCode);
        ship.set("sell_id", outTxnId);
        ship.set("expected_at", expectedShipDate);
        ship.set("ship_at", Value{});
        ship.set("status", std::string("PENDING"));

        OperationResult r2 = db->insertRecord("shipment", ship);
        if (!r2) { db->rollback(); return r2; }

        for (size_t i = 0; i < items.size(); ++i) {
            int shipDetId = getNextTableId(db, "shipment_detail");
            Record shipDet;
            shipDet.set("shipment_detail_id", shipDetId);
            shipDet.set("shipment_id", shipId);
            shipDet.set("product_id", items[i].first);
            shipDet.set("received_quantity", items[i].second);
            shipDet.set("actual_quantity", 0);
            OperationResult rsd = db->insertRecord("shipment_detail", shipDet);
            if (!rsd) { db->rollback(); return rsd; }
        }

        db->commit();
        return OperationResult{true, "Tạo đơn xuất kho và đợt xuất hàng thành công! Mã: " + outCode, static_cast<int>(2 + items.size() * 2)};
    }
}
