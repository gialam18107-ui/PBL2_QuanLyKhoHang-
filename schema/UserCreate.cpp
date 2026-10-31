#include "UserCreate.h"
#include "query/Query.h"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <random>
#include <regex>
#include <chrono>
#include <ctime>

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
        Vector<Record> recs = Query::filterRecords(*table, colName, Value(val));
        if (!recs.empty()) return false;

        // Defensive fallback only if no index covers colName
        bool hasIndex = (table->getIndex(colName) != nullptr);
        if (!hasIndex) {
            for (const Index* idx : table->getIndexes()) {
                if (idx != nullptr && idx->getColumnName() == colName) {
                    hasIndex = true;
                    break;
                }
            }
        }
        if (!hasIndex) {
            const auto& rows = table->getRows();
            for (size_t i = 0; i < rows.size(); ++i) {
                if (table->isRowDeleted(i)) continue;
                const Record& r = rows[i];
                if (r.has(colName) && std::holds_alternative<std::string>(r.get(colName))) {
                    if (std::get<std::string>(r.get(colName)) == val) return false;
                }
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
        Vector<Record> recs = Query::filterRecords(*custTable, "customer_code", Value(customerCode));
        if (!recs.empty()) {
            const Record& r = recs[0];
            if (r.has("customer_id") && std::holds_alternative<int>(r.get("customer_id"))) {
                return std::get<int>(r.get("customer_id"));
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
            Vector<Record> bRecs = Query::filterRecords(*brandTable, "brand", Value(brandName));
            if (!bRecs.empty()) {
                const Record& r = bRecs[0];
                if (r.has("code") && std::holds_alternative<std::string>(r.get("code"))) {
                    brandCode = std::get<std::string>(r.get("code"));
                }
            }
        }

        std::string catCode = categoryName;
        Table* catTable = db->getTable("category");
        if (catTable) {
            Vector<Record> cRecs = Query::filterRecords(*catTable, "category_name", Value(categoryName));
            if (!cRecs.empty()) {
                const Record& r = cRecs[0];
                if (r.has("code") && std::holds_alternative<std::string>(r.get("code"))) {
                    catCode = std::get<std::string>(r.get("code"));
                }
            }
        }

        int brandSeq = 0;
        int catSeq = 0;
        Table* prodTable = db->getTable("product");
        if (prodTable) {
            brandSeq = static_cast<int>(Query::filterRecords(*prodTable, "brand", Value(brandName)).size());
            catSeq = static_cast<int>(Query::filterRecords(*prodTable, "category", Value(categoryName)).size());
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
            Vector<Record> bRecs = Query::filterRecords(*brandTable, "brand", Value(brandName));
            brandFound = !bRecs.empty();
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
            Vector<Record> cRecs = Query::filterRecords(*catTable, "category_name", Value(catUpper));
            catFound = !cRecs.empty();
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
        Vector<Record> taxMatches = Query::filterRecords(*infoTable, "tax_code", Value(taxCode));
        if (!taxMatches.empty()) {
            return OperationResult{false, "Mã số thuế '" + taxCode + "' đã tồn tại trong hệ thống.", 0};
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

        if (!Query::filterRecords(*brandTable, "brand", Value(brandName)).empty()) {
            return OperationResult{false, "Thương hiệu '" + brandName + "' đã tồn tại trong CSDL.", 0};
        }
        if (!Query::filterRecords(*brandTable, "code", Value(brandCode)).empty()) {
            return OperationResult{false, "Mã thương hiệu '" + brandCode + "' đã tồn tại trong CSDL.", 0};
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
        const std::string& customerCode,
        const Vector<Pair<int, int>>& items,
        const std::string& orderDate,
        const std::string& expectedDeliveryDate,
        const std::string& note
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        std::string effectiveCustomerCode = customerCode;
        if (inboundType == "ADJUSTMENT_IN" || inboundType == "ADJ") {
            if (effectiveCustomerCode.empty()) {
                effectiveCustomerCode = "BS-0000000";
            }
        }
        if (effectiveCustomerCode.empty()) {
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

        int customerId = findCustomerIdByCode(custTable, effectiveCustomerCode);
        if (customerId < 0) {
            return OperationResult{false, "Không tìm thấy đối tác/khách hàng với mã: '" + effectiveCustomerCode + "'.", 0};
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
        std::string effectiveCustomerCode = customerCode;
        if (outboundType == "ADJUSTMENT_OUT" || outboundType == "ADJ") {
            if (effectiveCustomerCode.empty()) {
                effectiveCustomerCode = "BS-0000000";
            }
        }
        if (effectiveCustomerCode.empty()) {
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

        int customerId = findCustomerIdByCode(custTable, effectiveCustomerCode);
        if (customerId < 0) {
            return OperationResult{false, "Không tìm thấy khách hàng/đối tác với mã: '" + effectiveCustomerCode + "'.", 0};
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

            // Rule 4.3: Outbound order quantity is permitted to exceed current stock (allowing backorder/pre-order).
            // Physical availability is enforced when dispatching units with serials.
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

    // ========================================================
    // SERIAL GENERATION & LOGISTICS BATCH ALLOCATION HELPERS
    // ========================================================

    static std::string getCurrentDateStr()
    {
        std::time_t t = std::time(nullptr);
        std::tm tmVal;
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tmVal, &t);
#else
        localtime_r(&t, &tmVal);
#endif
        std::ostringstream oss;
        oss << std::setfill('0')
            << (tmVal.tm_year + 1900) << "-"
            << std::setw(2) << (tmVal.tm_mon + 1) << "-"
            << std::setw(2) << tmVal.tm_mday;
        return oss.str();
    }

    bool isValidSerialFormat(const std::string& serial)
    {
        static const std::regex serialRegex(R"(^SN-[A-Za-z0-9]{7}$)");
        return std::regex_match(serial, serialRegex);
    }

    bool isSerialUnique(Database* db, const std::string& serial)
    {
        if (!db) return true;
        Table* unitTab = db->getTable("unit_inventory");
        if (!unitTab) return true;
        Vector<Record> recs = Query::filterRecords(*unitTab, "serial", Value(serial));
        return recs.empty();
    }

    std::string generateUniqueSerial(Database* db)
    {
        static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        static const size_t charsetSize = sizeof(charset) - 1;
        static std::mt19937_64 rng(1337 + std::chrono::steady_clock::now().time_since_epoch().count());
        std::uniform_int_distribution<size_t> dist(0, charsetSize - 1);

        for (int attempt = 0; attempt < 10000; ++attempt) {
            std::string serial = "SN-";
            for (int i = 0; i < 7; ++i) {
                serial += charset[dist(rng)];
            }
            if (isSerialUnique(db, serial)) {
                return serial;
            }
        }
        std::string fallback = "SN-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()).substr(0, 7);
        return fallback;
    }

    OperationResult createDeliveryBatch(
        Database* db,
        int inboundTransactionId,
        const std::string& expectedDate,
        const Vector<Pair<int, int>>& items
    ) {
        (void)expectedDate;
        (void)items;
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        Table* inTxnTable = db->getTable("inbound_transaction");
        if (!inTxnTable) return OperationResult{false, "Bảng đơn nhập kho không tồn tại trong CSDL.", 0};
        const Record* inTxnRec = inTxnTable->getRecordById(inboundTransactionId);
        if (!inTxnRec) {
            return OperationResult{false, "Không tìm thấy đơn nhập kho với ID " + std::to_string(inboundTransactionId) + ".", 0};
        }
        return OperationResult{false, "Không được phép tạo thêm đợt giao hàng (delivery) sau khi đã xác nhận đơn transaction (Rule 4.3).", 0};
    }

    OperationResult createShipmentBatch(
        Database* db,
        int outboundTransactionId,
        const std::string& expectedDate,
        const Vector<Pair<int, int>>& items
    ) {
        (void)expectedDate;
        (void)items;
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        Table* outTxnTable = db->getTable("outbound_transaction");
        if (!outTxnTable) return OperationResult{false, "Bảng đơn xuất kho không tồn tại trong CSDL.", 0};
        const Record* outTxnRec = outTxnTable->getRecordById(outboundTransactionId);
        if (!outTxnRec) {
            return OperationResult{false, "Không tìm thấy đơn xuất kho với ID " + std::to_string(outboundTransactionId) + ".", 0};
        }
        return OperationResult{false, "Không được phép tạo thêm đợt xuất hàng (shipment) sau khi đã xác nhận đơn transaction (Rule 4.3).", 0};
    }

    OperationResult createInboundOrderWithDeliveries(
        Database* db,
        const std::string& inboundType,
        const std::string& customerCode,
        const Vector<Pair<int, int>>& totalOrderedItems,
        const Vector<LogisticsBatchInput>& deliveryBatches,
        const std::string& orderDate,
        const std::string& note
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        std::string effectiveCustomerCode = customerCode;
        if (inboundType == "ADJUSTMENT_IN" || inboundType == "ADJ") {
            if (effectiveCustomerCode.empty()) {
                effectiveCustomerCode = "BS-0000000";
            }
        }
        if (effectiveCustomerCode.empty()) return OperationResult{false, "Mã đối tác / khách hàng không được để trống.", 0};
        if (totalOrderedItems.empty()) return OperationResult{false, "Đơn nhập kho phải có ít nhất một mặt hàng.", 0};
        if (deliveryBatches.empty()) return OperationResult{false, "Phải ấn định ít nhất một đợt nhận hàng.", 0};
        if (orderDate.empty()) return OperationResult{false, "Ngày đặt hàng không được để trống.", 0};

        Table* custTable = db->getTable("customer");
        if (!custTable) return OperationResult{false, "Bảng 'customer' không tồn tại.", 0};
        int customerId = findCustomerIdByCode(custTable, effectiveCustomerCode);
        if (customerId < 0) return OperationResult{false, "Không tìm thấy đối tác/khách hàng với mã: '" + effectiveCustomerCode + "'.", 0};

        Table* prodTable = db->getTable("product");
        if (!prodTable) return OperationResult{false, "Bảng 'product' không tồn tại.", 0};

        HashMap<int, int> orderedMap;
        for (size_t i = 0; i < totalOrderedItems.size(); ++i) {
            int pid = totalOrderedItems[i].first;
            int qty = totalOrderedItems[i].second;
            if (qty <= 0) return OperationResult{false, "Số lượng đặt của sản phẩm ID " + std::to_string(pid) + " phải lớn hơn 0.", 0};
            const Record* pRec = prodTable->getRecordById(pid);
            if (!pRec) return OperationResult{false, "Sản phẩm ID " + std::to_string(pid) + " không tồn tại.", 0};
            if (pRec->has("is_sold") && std::holds_alternative<bool>(pRec->get("is_sold")) && !std::get<bool>(pRec->get("is_sold"))) {
                return OperationResult{false, "Sản phẩm ID " + std::to_string(pid) + " đã ngừng kinh doanh (Rule 4.2).", 0};
            }
            orderedMap[pid] = qty;
        }

        // Validate batch allocations: sum(received_quantity) <= ordered_quantity
        HashMap<int, int> totalAllocated;
        for (size_t b = 0; b < deliveryBatches.size(); ++b) {
            const auto& batch = deliveryBatches[b];
            if (batch.expectedDate.empty() || batch.expectedDate < orderDate) {
                return OperationResult{false, "Ngày dự kiến đợt giao " + std::to_string(b + 1) + " (" + batch.expectedDate + ") không được sớm hơn ngày đặt (" + orderDate + ").", 0};
            }
            if (batch.items.empty()) return OperationResult{false, "Đợt giao " + std::to_string(b + 1) + " không có mặt hàng nào.", 0};
            for (size_t i = 0; i < batch.items.size(); ++i) {
                int pid = batch.items[i].first;
                int qty = batch.items[i].second;
                if (qty <= 0) return OperationResult{false, "Số lượng đợt giao cho sản phẩm ID " + std::to_string(pid) + " phải > 0.", 0};
                if (!orderedMap.contains(pid)) {
                    return OperationResult{false, "Sản phẩm ID " + std::to_string(pid) + " trong đợt giao không thuộc đơn đặt hàng.", 0};
                }
                totalAllocated[pid] = (totalAllocated.contains(pid) ? totalAllocated[pid] : 0) + qty;
                if (totalAllocated[pid] > orderedMap[pid]) {
                    return OperationResult{false, "Tổng số lượng ấn định qua các đợt giao (" + std::to_string(totalAllocated[pid]) +
                        ") vượt quá số lượng đặt (" + std::to_string(orderedMap[pid]) + ") cho sản phẩm ID " + std::to_string(pid) + ".", 0};
                }
            }
        }

        // Rule 4.3: Total quantity across all deliveries must strictly equal the transaction's ordered quantity
        for (size_t i = 0; i < totalOrderedItems.size(); ++i) {
            int pid = totalOrderedItems[i].first;
            int ordQty = totalOrderedItems[i].second;
            int allocQty = totalAllocated.contains(pid) ? totalAllocated[pid] : 0;
            if (allocQty != ordQty) {
                return OperationResult{false, "Tổng số lượng của từng đợt nhận hàng (" + std::to_string(allocQty) +
                    ") phải bằng đúng số lượng của đơn transaction (" + std::to_string(ordQty) + ") cho sản phẩm ID " + std::to_string(pid) + " (Rule 4.3).", 0};
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
        inTxn.set("next_delivery_no", static_cast<int>(deliveryBatches.size() + 1));
        inTxn.set("note", note);

        OperationResult r1 = db->insertRecord("inbound_transaction", inTxn);
        if (!r1) { db->rollback(); return r1; }

        for (size_t i = 0; i < totalOrderedItems.size(); ++i) {
            int inDetId = getNextTableId(db, "inbound_detail");
            Record inDet;
            inDet.set("inbound_detail_id", inDetId);
            inDet.set("inbound_transaction_id", inTxnId);
            inDet.set("product_id", totalOrderedItems[i].first);
            inDet.set("ordered_quantity", totalOrderedItems[i].second);
            inDet.set("actual_quantity", 0);
            OperationResult rd = db->insertRecord("inbound_detail", inDet);
            if (!rd) { db->rollback(); return rd; }
        }

        int totalAffected = 1 + static_cast<int>(totalOrderedItems.size());
        for (size_t b = 0; b < deliveryBatches.size(); ++b) {
            const auto& batch = deliveryBatches[b];
            std::ostringstream delOss;
            delOss << inCode << "-D" << std::setfill('0') << std::setw(2) << (b + 1);
            std::string delCode = delOss.str();

            int delId = getNextTableId(db, "delivery");
            Record del;
            del.set("delivery_id", delId);
            del.set("delivery_code", delCode);
            del.set("inbound_transaction_id", inTxnId);
            del.set("expected_at", batch.expectedDate);
            del.set("delivery_at", Value{});
            del.set("status", std::string("EXPECTED"));

            OperationResult rDel = db->insertRecord("delivery", del);
            if (!rDel) { db->rollback(); return rDel; }
            totalAffected++;

            for (size_t i = 0; i < batch.items.size(); ++i) {
                int delDetId = getNextTableId(db, "delivery_detail");
                Record delDet;
                delDet.set("delivery_detail_id", delDetId);
                delDet.set("delivery_id", delId);
                delDet.set("product_id", batch.items[i].first);
                delDet.set("received_quantity", batch.items[i].second);
                delDet.set("actual_quantity", 0);
                OperationResult rdd = db->insertRecord("delivery_detail", delDet);
                if (!rdd) { db->rollback(); return rdd; }
                totalAffected++;
            }
        }

        db->commit();
        return OperationResult{true, "Tạo đơn nhập kho và các đợt nhận hàng thành công! Mã: " + inCode, totalAffected};
    }

    OperationResult createOutboundOrderWithShipments(
        Database* db,
        const std::string& outboundType,
        const std::string& customerCode,
        const Vector<Pair<int, int>>& totalOrderedItems,
        const Vector<LogisticsBatchInput>& shipmentBatches,
        const std::string& orderDate,
        const std::string& note
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        std::string effectiveCustomerCode = customerCode;
        if (outboundType == "ADJUSTMENT_OUT" || outboundType == "ADJ") {
            if (effectiveCustomerCode.empty()) {
                effectiveCustomerCode = "BS-0000000";
            }
        }
        if (effectiveCustomerCode.empty()) return OperationResult{false, "Mã khách hàng / đối tác không được để trống.", 0};
        if (totalOrderedItems.empty()) return OperationResult{false, "Đơn xuất kho phải có ít nhất một mặt hàng.", 0};
        if (shipmentBatches.empty()) return OperationResult{false, "Phải ấn định ít nhất một đợt xuất hàng.", 0};
        if (orderDate.empty()) return OperationResult{false, "Ngày đặt hàng không được để trống.", 0};

        Table* custTable = db->getTable("customer");
        if (!custTable) return OperationResult{false, "Bảng 'customer' không tồn tại.", 0};
        int customerId = findCustomerIdByCode(custTable, effectiveCustomerCode);
        if (customerId < 0) return OperationResult{false, "Không tìm thấy khách hàng/đối tác với mã: '" + effectiveCustomerCode + "'.", 0};

        Table* prodTable = db->getTable("product");
        Table* qtyTable = db->getTable("product_quantity");
        if (!prodTable || !qtyTable) return OperationResult{false, "Bảng sản phẩm hoặc tồn kho không tồn tại.", 0};

        HashMap<int, int> orderedMap;
        for (size_t i = 0; i < totalOrderedItems.size(); ++i) {
            int pid = totalOrderedItems[i].first;
            int qty = totalOrderedItems[i].second;
            if (qty <= 0) return OperationResult{false, "Số lượng xuất của sản phẩm ID " + std::to_string(pid) + " phải lớn hơn 0.", 0};
            const Record* pRec = prodTable->getRecordById(pid);
            if (!pRec) return OperationResult{false, "Sản phẩm ID " + std::to_string(pid) + " không tồn tại.", 0};
            if (pRec->has("is_sold") && std::holds_alternative<bool>(pRec->get("is_sold")) && !std::get<bool>(pRec->get("is_sold"))) {
                return OperationResult{false, "Sản phẩm ID " + std::to_string(pid) + " đã ngừng kinh doanh (Rule 4.2).", 0};
            }

            // Rule 4.3: Outbound order quantity is permitted to exceed current stock.
            orderedMap[pid] = qty;
        }

        // Validate batches allocation
        HashMap<int, int> totalAllocated;
        for (size_t b = 0; b < shipmentBatches.size(); ++b) {
            const auto& batch = shipmentBatches[b];
            if (batch.expectedDate.empty() || batch.expectedDate < orderDate) {
                return OperationResult{false, "Ngày dự kiến đợt xuất " + std::to_string(b + 1) + " (" + batch.expectedDate + ") không được sớm hơn ngày đặt (" + orderDate + ").", 0};
            }
            if (batch.items.empty()) return OperationResult{false, "Đợt xuất " + std::to_string(b + 1) + " không có mặt hàng nào.", 0};
            for (size_t i = 0; i < batch.items.size(); ++i) {
                int pid = batch.items[i].first;
                int qty = batch.items[i].second;
                if (qty <= 0) return OperationResult{false, "Số lượng đợt xuất cho sản phẩm ID " + std::to_string(pid) + " phải > 0.", 0};
                if (!orderedMap.contains(pid)) {
                    return OperationResult{false, "Sản phẩm ID " + std::to_string(pid) + " trong đợt xuất không thuộc đơn đặt hàng.", 0};
                }
                totalAllocated[pid] = (totalAllocated.contains(pid) ? totalAllocated[pid] : 0) + qty;
                if (totalAllocated[pid] > orderedMap[pid]) {
                    return OperationResult{false, "Tổng số lượng ấn định qua các đợt xuất (" + std::to_string(totalAllocated[pid]) +
                        ") vượt quá số lượng đặt (" + std::to_string(orderedMap[pid]) + ") cho sản phẩm ID " + std::to_string(pid) + ".", 0};
                }
            }
        }

        // Rule 4.3: Total quantity across all shipments must strictly equal the transaction's ordered quantity
        for (size_t i = 0; i < totalOrderedItems.size(); ++i) {
            int pid = totalOrderedItems[i].first;
            int ordQty = totalOrderedItems[i].second;
            int allocQty = totalAllocated.contains(pid) ? totalAllocated[pid] : 0;
            if (allocQty != ordQty) {
                return OperationResult{false, "Tổng số lượng của từng đợt xuất hàng (" + std::to_string(allocQty) +
                    ") phải bằng đúng số lượng của đơn transaction (" + std::to_string(ordQty) + ") cho sản phẩm ID " + std::to_string(pid) + " (Rule 4.3).", 0};
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
        outTxn.set("next_ship_no", static_cast<int>(shipmentBatches.size() + 1));
        outTxn.set("note", note);

        OperationResult r1 = db->insertRecord("outbound_transaction", outTxn);
        if (!r1) { db->rollback(); return r1; }

        for (size_t i = 0; i < totalOrderedItems.size(); ++i) {
            int outDetId = getNextTableId(db, "outbound_detail");
            Record outDet;
            outDet.set("outbound_detail_id", outDetId);
            outDet.set("outbound_transaction_id", outTxnId);
            outDet.set("product_id", totalOrderedItems[i].first);
            outDet.set("ordered_quantity", totalOrderedItems[i].second);
            outDet.set("actual_quantity", 0);
            OperationResult rd = db->insertRecord("outbound_detail", outDet);
            if (!rd) { db->rollback(); return rd; }
        }

        int totalAffected = 1 + static_cast<int>(totalOrderedItems.size());
        for (size_t b = 0; b < shipmentBatches.size(); ++b) {
            const auto& batch = shipmentBatches[b];
            std::ostringstream shipOss;
            shipOss << outCode << "-S" << std::setfill('0') << std::setw(2) << (b + 1);
            std::string shipCode = shipOss.str();

            int shipId = getNextTableId(db, "shipment");
            Record ship;
            ship.set("shipment_id", shipId);
            ship.set("shipment_code", shipCode);
            ship.set("sell_id", outTxnId);
            ship.set("expected_at", batch.expectedDate);
            ship.set("ship_at", Value{});
            ship.set("status", std::string("PENDING"));

            OperationResult rShip = db->insertRecord("shipment", ship);
            if (!rShip) { db->rollback(); return rShip; }
            totalAffected++;

            for (size_t i = 0; i < batch.items.size(); ++i) {
                int shipDetId = getNextTableId(db, "shipment_detail");
                Record shipDet;
                shipDet.set("shipment_detail_id", shipDetId);
                shipDet.set("shipment_id", shipId);
                shipDet.set("product_id", batch.items[i].first);
                shipDet.set("received_quantity", batch.items[i].second);
                shipDet.set("actual_quantity", 0);
                OperationResult rsd = db->insertRecord("shipment_detail", shipDet);
                if (!rsd) { db->rollback(); return rsd; }
                totalAffected++;
            }
        }

        db->commit();
        return OperationResult{true, "Tạo đơn xuất kho và các đợt xuất hàng thành công! Mã: " + outCode, totalAffected};
    }

    OperationResult receiveUnitsForDelivery(
        Database* db,
        int deliveryId,
        int productId,
        const std::string& targetStatus,
        int quantity,
        const Vector<std::string>& customSerials
    ) {
        if (!db) return OperationResult{false, "Cơ sở dữ liệu chưa được khởi tạo.", 0};
        if (quantity <= 0) return OperationResult{false, "Số lượng unit cần nhận phải lớn hơn 0.", 0};

        Table* delTable = db->getTable("delivery");
        Table* delDetTable = db->getTable("delivery_detail");
        Table* inTxnTable = db->getTable("inbound_transaction");
        Table* inDetTable = db->getTable("inbound_detail");
        Table* unitTable = db->getTable("unit_inventory");
        Table* histTable = db->getTable("status_history");
        Table* qtyTable = db->getTable("product_quantity");
        Table* prodTable = db->getTable("product");

        if (!delTable || !delDetTable || !inTxnTable || !inDetTable || !unitTable || !histTable || !qtyTable || !prodTable) {
            return OperationResult{false, "Bảng liên quan đến nhận hàng và linh kiện không tồn tại trong CSDL.", 0};
        }

        const Record* delRec = delTable->getRecordById(deliveryId);
        if (!delRec) {
            return OperationResult{false, "Không tìm thấy đợt nhận hàng với ID " + std::to_string(deliveryId) + ".", 0};
        }

        std::string delStatus = delRec->has("status") && std::holds_alternative<std::string>(delRec->get("status"))
                              ? std::get<std::string>(delRec->get("status")) : "";
        if (delStatus == "COMPLETED" || delStatus == "completed") {
            return OperationResult{false, "Đợt nhận hàng này đã ở trạng thái HOÀN THÀNH (COMPLETED), không thể nhận thêm linh kiện.", 0};
        }

        int inTxnId = -1;
        if (delRec->has("inbound_transaction_id") && std::holds_alternative<int>(delRec->get("inbound_transaction_id"))) {
            inTxnId = std::get<int>(delRec->get("inbound_transaction_id"));
        }
        const Record* inTxnRec = inTxnTable->getRecordById(inTxnId);
        if (!inTxnRec) {
            return OperationResult{false, "Không tìm thấy đơn nhập kho gốc của đợt nhận hàng này.", 0};
        }

        std::string inType = inTxnRec->has("type") && std::holds_alternative<std::string>(inTxnRec->get("type"))
                           ? std::get<std::string>(inTxnRec->get("type")) : "PURCHASING";

        // Find delivery_detail
        int delDetId = -1;
        int delRecQty = 0;
        int delActQty = 0;
        Vector<Record> ddRecords = Query::filterRecords(*delDetTable, "delivery_id", Value(deliveryId));
        for (const auto& row : ddRecords) {
            if (row.has("product_id") && std::holds_alternative<int>(row.get("product_id")) &&
                std::get<int>(row.get("product_id")) == productId) {
                delDetId = std::get<int>(row.get("delivery_detail_id"));
                delRecQty = std::get<int>(row.get("received_quantity"));
                delActQty = std::get<int>(row.get("actual_quantity"));
                break;
            }
        }
        if (delDetId == -1) {
            return OperationResult{false, "Sản phẩm ID " + std::to_string(productId) + " không thuộc đợt nhận hàng ID " + std::to_string(deliveryId) + ".", 0};
        }

        if (delActQty + quantity > delRecQty) {
            return OperationResult{false, "Số lượng nhận (" + std::to_string(delActQty + quantity) +
                ") vượt quá số lượng ấn định (" + std::to_string(delRecQty) + ") của đợt nhận hàng này.", 0};
        }

        // Find inbound_detail
        int inDetId = -1;
        int inOrdQty = 0;
        int inActQty = 0;
        Vector<Record> idRecords = Query::filterRecords(*inDetTable, "inbound_transaction_id", Value(inTxnId));
        for (const auto& row : idRecords) {
            if (row.has("product_id") && std::holds_alternative<int>(row.get("product_id")) &&
                std::get<int>(row.get("product_id")) == productId) {
                inDetId = std::get<int>(row.get("inbound_detail_id"));
                inOrdQty = std::get<int>(row.get("ordered_quantity"));
                inActQty = std::get<int>(row.get("actual_quantity"));
                break;
            }
        }
        if (inDetId == -1) {
            return OperationResult{false, "Sản phẩm ID " + std::to_string(productId) + " không thuộc đơn nhập kho gốc.", 0};
        }

        if (inActQty + quantity > inOrdQty) {
            return OperationResult{false, "Số lượng nhận vượt quá tổng số lượng đặt hàng (" + std::to_string(inOrdQty) + ") của đơn nhập.", 0};
        }

        // Determine effective target status & validate transitions (Rule 4.4.2)
        std::string effTarget = targetStatus;
        bool isAdjustmentIn = (inType == "ADJUSTMENT_IN" || inType == "ADJ");
        bool isNewAcquisition = (inType == "PURCHASING" || inType == "PUR" ||
                                 inType == "WARRANTY_REPLACEMENT" || inType == "WRP");

        if (inType == "PURCHASING" || inType == "PUR") {
            if (effTarget.empty()) effTarget = "AVAILABLE";
            if (effTarget != "AVAILABLE" && effTarget != "SCRAPPED") {
                return OperationResult{false, "Đơn mua hàng mới (PUR) chỉ được nhận linh kiện với trạng thái AVAILABLE hoặc SCRAPPED (Rule 4.4.2).", 0};
            }
        } else if (inType == "WARRANTY_REPLACEMENT" || inType == "WRP") {
            if (effTarget.empty()) effTarget = "WARRANTY";
            if (effTarget != "WARRANTY") {
                return OperationResult{false, "Đơn đổi bảo hành từ nhà cung cấp (WRP) chỉ được nhận linh kiện với trạng thái WARRANTY (Rule 4.4.2).", 0};
            }
        } else if (inType == "WARRANTY_CHECKING" || inType == "WCK") {
            if (effTarget.empty()) effTarget = "WARRANTY";
            if (effTarget != "WARRANTY") {
                return OperationResult{false, "Đơn nhận bảo hành từ khách (WCK) chỉ được nhận linh kiện với trạng thái WARRANTY (Rule 4.4.2).", 0};
            }
        } else if (inType == "CUSTOMER_RETURN" || inType == "CRT") {
            if (effTarget.empty()) effTarget = "AVAILABLE";
            if (effTarget != "AVAILABLE" && effTarget != "WARRANTY" && effTarget != "SCRAPPED") {
                return OperationResult{false, "Đơn khách hàng trả hàng (CRT) chỉ được nhận linh kiện với trạng thái AVAILABLE, WARRANTY, hoặc SCRAPPED (Rule 4.4.2).", 0};
            }
        } else if (isAdjustmentIn) {
            // Rule 4.4.2: ADJ Inbound allows transition from ANY status to ANY internal warehouse status (target cannot be OUT)
            if (effTarget.empty()) effTarget = "AVAILABLE";
            if (effTarget != "AVAILABLE" && effTarget != "WARRANTY" && effTarget != "SCRAPPED") {
                return OperationResult{false, "Đơn nhập điều chỉnh kho (ADJ) chỉ được nhận linh kiện với trạng thái AVAILABLE, WARRANTY, hoặc SCRAPPED (Rule 4.4.2).", 0};
            }
        } else {
            if (effTarget.empty()) effTarget = "AVAILABLE";
        }

        if (effTarget == "OUT") {
            return OperationResult{false, "Trạng thái nhận của đơn nhập kho tuyệt đối không được là 'OUT' (Rule 4.4.2).", 0};
        }

        if (!isNewAcquisition && !isAdjustmentIn && customSerials.size() < static_cast<size_t>(quantity)) {
            return OperationResult{false, "Đơn nhận lại hàng (" + inType + ") yêu cầu cung cấp danh sách serial tương ứng cho từng linh kiện.", 0};
        }

        // Preload current inventory bucket counts
        const Record* pqRec = qtyTable->getRecordById(productId);
        int curAvail = 0, curWarr = 0, curScrap = 0;
        if (pqRec) {
            if (pqRec->has("available_quan") && std::holds_alternative<int>(pqRec->get("available_quan")))
                curAvail = std::get<int>(pqRec->get("available_quan"));
            if (pqRec->has("warranty_quan") && std::holds_alternative<int>(pqRec->get("warranty_quan")))
                curWarr = std::get<int>(pqRec->get("warranty_quan"));
            if (pqRec->has("scrapped_quan") && std::holds_alternative<int>(pqRec->get("scrapped_quan")))
                curScrap = std::get<int>(pqRec->get("scrapped_quan"));
        }

        if (!db->beginTransaction()) {
            return OperationResult{false, "Không thể bắt đầu Transaction CSDL.", 0};
        }

        int totalAffected = 0;

        // Process units
        if (isAdjustmentIn) {
            // Inbound ADJ: From ANY status to ANY status (except OUT)
            for (int k = 0; k < quantity; ++k) {
                std::string sn = (k < static_cast<int>(customSerials.size())) ? customSerials[k] : "";
                int targetUnitId = -1;
                std::string currentUnitStatus = "";

                if (!sn.empty()) {
                    Vector<Record> uRecs = Query::filterRecords(*unitTable, "serial", Value(sn));
                    if (!uRecs.empty()) {
                        const auto& uRow = uRecs[0];
                        if (uRow.has("product_id") && std::holds_alternative<int>(uRow.get("product_id")) &&
                            std::get<int>(uRow.get("product_id")) == productId) {
                            targetUnitId = std::get<int>(uRow.get("unit_id"));
                            currentUnitStatus = std::get<std::string>(uRow.get("status"));
                        }
                    }
                }

                if (targetUnitId != -1) {
                    // Existing unit transition: any status -> effTarget
                    Record uUp;
                    uUp.set("status", effTarget);
                    OperationResult rU = db->update("unit_inventory", targetUnitId, uUp);
                    if (!rU) { db->rollback(); return rU; }
                    totalAffected++;

                    int histId = getNextTableId(db, "status_history");
                    Record hRec;
                    hRec.set("history_id", histId);
                    hRec.set("unit_id", targetUnitId);
                    hRec.set("old_status", currentUnitStatus);
                    hRec.set("curr_status", effTarget);
                    hRec.set("transaction", deliveryId);
                    hRec.set("type", std::string("INBOUND"));

                    OperationResult rH = db->insertRecord("status_history", hRec);
                    if (!rH) { db->rollback(); return rH; }
                    totalAffected++;

                    // Adjust bucket: decrement previous bucket if it was internal
                    if (currentUnitStatus == "AVAILABLE") curAvail--;
                    else if (currentUnitStatus == "WARRANTY") curWarr--;
                    else if (currentUnitStatus == "SCRAPPED") curScrap--;

                    // Increment new bucket
                    if (effTarget == "AVAILABLE") curAvail++;
                    else if (effTarget == "WARRANTY") curWarr++;
                    else if (effTarget == "SCRAPPED") curScrap++;
                } else {
                    // New unit registration: NULL -> effTarget
                    if (!sn.empty()) {
                        if (!isValidSerialFormat(sn)) {
                            db->rollback();
                            return OperationResult{false, "Mã serial '" + sn + "' không đúng định dạng SN-XXXXXXX (Rule 5.5).", 0};
                        }
                        if (!isSerialUnique(db, sn)) {
                            db->rollback();
                            return OperationResult{false, "Mã serial '" + sn + "' đã tồn tại trong kho linh kiện.", 0};
                        }
                    } else {
                        sn = generateUniqueSerial(db);
                    }

                    int unitId = getNextTableId(db, "unit_inventory");
                    Record uRec;
                    uRec.set("unit_id", unitId);
                    uRec.set("product_id", productId);
                    uRec.set("serial", sn);
                    uRec.set("status", effTarget);

                    OperationResult rU = db->insertRecord("unit_inventory", uRec);
                    if (!rU) { db->rollback(); return rU; }
                    totalAffected++;

                    int histId = getNextTableId(db, "status_history");
                    Record hRec;
                    hRec.set("history_id", histId);
                    hRec.set("unit_id", unitId);
                    hRec.set("old_status", Value{}); // NULL on new registration
                    hRec.set("curr_status", effTarget);
                    hRec.set("transaction", deliveryId);
                    hRec.set("type", std::string("INBOUND"));

                    OperationResult rH = db->insertRecord("status_history", hRec);
                    if (!rH) { db->rollback(); return rH; }
                    totalAffected++;

                    if (effTarget == "AVAILABLE") curAvail++;
                    else if (effTarget == "WARRANTY") curWarr++;
                    else if (effTarget == "SCRAPPED") curScrap++;
                }
            }
        } else if (isNewAcquisition) {
            // Create new physical units
            for (int k = 0; k < quantity; ++k) {
                std::string sn;
                if (k < static_cast<int>(customSerials.size()) && !customSerials[k].empty()) {
                    sn = customSerials[k];
                    if (!isValidSerialFormat(sn)) {
                        db->rollback();
                        return OperationResult{false, "Mã serial '" + sn + "' không đúng định dạng SN-XXXXXXX (Rule 5.5).", 0};
                    }
                    if (!isSerialUnique(db, sn)) {
                        db->rollback();
                        return OperationResult{false, "Mã serial '" + sn + "' đã tồn tại trong kho linh kiện.", 0};
                    }
                } else {
                    sn = generateUniqueSerial(db);
                }

                int unitId = getNextTableId(db, "unit_inventory");
                Record uRec;
                uRec.set("unit_id", unitId);
                uRec.set("product_id", productId);
                uRec.set("serial", sn);
                uRec.set("status", effTarget);

                OperationResult rU = db->insertRecord("unit_inventory", uRec);
                if (!rU) { db->rollback(); return rU; }
                totalAffected++;

                int histId = getNextTableId(db, "status_history");
                Record hRec;
                hRec.set("history_id", histId);
                hRec.set("unit_id", unitId);
                hRec.set("old_status", Value{}); // NULL on first acquisition
                hRec.set("curr_status", effTarget);
                hRec.set("transaction", deliveryId);
                hRec.set("type", std::string("INBOUND"));

                OperationResult rH = db->insertRecord("status_history", hRec);
                if (!rH) { db->rollback(); return rH; }
                totalAffected++;

                if (effTarget == "AVAILABLE") curAvail++;
                else if (effTarget == "WARRANTY") curWarr++;
                else if (effTarget == "SCRAPPED") curScrap++;
            }
        } else {
            // Update existing units from OUT -> effTarget
            for (int k = 0; k < quantity; ++k) {
                std::string sn = customSerials[k];
                int targetUnitId = -1;
                std::string currentUnitStatus = "";

                Vector<Record> uRecs = Query::filterRecords(*unitTable, "serial", Value(sn));
                if (!uRecs.empty()) {
                    const auto& uRow = uRecs[0];
                    if (uRow.has("product_id") && std::holds_alternative<int>(uRow.get("product_id")) &&
                        std::get<int>(uRow.get("product_id")) == productId) {
                        targetUnitId = std::get<int>(uRow.get("unit_id"));
                        currentUnitStatus = std::get<std::string>(uRow.get("status"));
                    }
                }

                if (targetUnitId == -1) {
                    db->rollback();
                    return OperationResult{false, "Không tìm thấy linh kiện có serial '" + sn + "' của sản phẩm ID " + std::to_string(productId) + ".", 0};
                }
                if (currentUnitStatus != "OUT") {
                    db->rollback();
                    return OperationResult{false, "Linh kiện có serial '" + sn + "' hiện đang có trạng thái '" + currentUnitStatus + "', không phải 'OUT' (Rule 4.4.2).", 0};
                }

                Record uUp;
                uUp.set("status", effTarget);
                OperationResult rU = db->update("unit_inventory", targetUnitId, uUp);
                if (!rU) { db->rollback(); return rU; }
                totalAffected++;

                int histId = getNextTableId(db, "status_history");
                Record hRec;
                hRec.set("history_id", histId);
                hRec.set("unit_id", targetUnitId);
                hRec.set("old_status", std::string("OUT"));
                hRec.set("curr_status", effTarget);
                hRec.set("transaction", deliveryId);
                hRec.set("type", std::string("INBOUND"));

                OperationResult rH = db->insertRecord("status_history", hRec);
                if (!rH) { db->rollback(); return rH; }
                totalAffected++;

                if (effTarget == "AVAILABLE") curAvail++;
                else if (effTarget == "WARRANTY") curWarr++;
                else if (effTarget == "SCRAPPED") curScrap++;
            }
        }

        // Update product_quantity bucket
        Record pqUp;
        pqUp.set("available_quan", curAvail);
        pqUp.set("warranty_quan", curWarr);
        pqUp.set("scrapped_quan", curScrap);

        OperationResult rPq = db->update("product_quantity", productId, pqUp);
        if (!rPq) { db->rollback(); return rPq; }
        totalAffected++;

        // Update actual_quantity in delivery_detail and inbound_detail
        Record ddUp;
        ddUp.set("actual_quantity", delActQty + quantity);
        OperationResult rDUp = db->update("delivery_detail", delDetId, ddUp);
        if (!rDUp) { db->rollback(); return rDUp; }
        totalAffected++;

        Record idUp;
        idUp.set("actual_quantity", inActQty + quantity);
        OperationResult rIUp = db->update("inbound_detail", inDetId, idUp);
        if (!rIUp) { db->rollback(); return rIUp; }
        totalAffected++;

        // Check if delivery is fully received -> COMPLETED
        bool delivAllDone = true;
        Vector<Record> curDelDets = Query::filterRecords(*delDetTable, "delivery_id", Value(deliveryId));
        for (const auto& row : curDelDets) {
            int rQty = std::get<int>(row.get("received_quantity"));
            int aQty = std::get<int>(row.get("actual_quantity"));
            if (std::get<int>(row.get("delivery_detail_id")) == delDetId) aQty += quantity;
            if (aQty < rQty) { delivAllDone = false; break; }
        }

        Record delStatusUp;
        if (delivAllDone) {
            delStatusUp.set("status", std::string("COMPLETED"));
            if (isNull(delRec->get("delivery_at"))) {
                delStatusUp.set("delivery_at", getCurrentDateStr());
            }
        } else {
            delStatusUp.set("status", std::string("RECEIVING"));
        }
        db->update("delivery", deliveryId, delStatusUp);

        // Check if inbound_transaction is fully received -> CONFIRMED
        bool inTxnAllDone = true;
        Vector<Record> curInDets = Query::filterRecords(*inDetTable, "inbound_transaction_id", Value(inTxnId));
        for (const auto& row : curInDets) {
            int ordQ = std::get<int>(row.get("ordered_quantity"));
            int actQ = std::get<int>(row.get("actual_quantity"));
            if (std::get<int>(row.get("inbound_detail_id")) == inDetId) actQ += quantity;
            if (actQ < ordQ) { inTxnAllDone = false; break; }
        }

        Record inTxnStatusUp;
        if (inTxnAllDone) {
            inTxnStatusUp.set("status", std::string("CONFIRMED"));
        } else {
            inTxnStatusUp.set("status", std::string("PROCESSING"));
        }
        db->update("inbound_transaction", inTxnId, inTxnStatusUp);

        db->commit();
        return OperationResult{true, "Đã nhận " + std::to_string(quantity) + " unit vào đợt giao " + std::to_string(deliveryId) + " thành công!", totalAffected};
    }

    OperationResult receiveUnitForDelivery(
        Database* db,
        int deliveryId,
        int productId,
        const std::string& targetStatus,
        const std::string& customSerial
    ) {
        Vector<std::string> serials;
        if (!customSerial.empty()) serials.push_back(customSerial);
        return receiveUnitsForDelivery(db, deliveryId, productId, targetStatus, 1, serials);
    }
}
