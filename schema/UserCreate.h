#ifndef USER_CREATE_H
#define USER_CREATE_H

#include <string>
#include "database/Database.h"
#include "database/Record.h"
#include "schema/OperationResult.h"

namespace user_create
{
    /**
     * @brief Generic record insertion meeting Rule 6.3 & 6.3.1.
     * Validates schema, NOT NULL, UNIQUE, and foreign keys before inserting.
     */
    OperationResult insertRecord(Database* db, const std::string& tableName, const Record& r);

    /**
     * @brief High-level atomic creation of a new product across 3 tables:
     * 1. product (basic product information, is_sold = true)
     * 2. product_quantity (initialized with available=0, warranty=0, scrapped=0)
     * 3. Hardware specification table (cpu, gpu, mainboard, ram, ssd, psu, cooling, case)
     * Enforces atomic transaction rollback if any step fails.
     */
    OperationResult createProduct(
        Database* db,
        const Record& productRecord,
        const std::string& categoryName,
        const Record& specRecord
    );

    // ========================================================
    // AUTO-GENERATION AND VALIDATION HELPERS
    // ========================================================

    /// Calculate next available product_id by scanning max product_id + 1
    int getNextProductId(Database* db);

    /// Generate product code in format: <producer_code>-<cat_code>-XXXX-YYYY (Rule 4.2)
    std::string generateNextProductCode(
        Database* db,
        const std::string& brandName,
        const std::string& categoryName
    );

    /// Generate SKU code in format: <cat_code>-<brand_code>-XXXX (e.g. CPU-INTEL-0095)
    std::string generateNextSku(
        Database* db,
        const std::string& brandName,
        const std::string& categoryName,
        int productId
    );

    /// Generate valid 13-digit EAN-13 barcode with check digit
    std::string generateEan13Barcode(int productId);

    /// Validate product code format: <PRODUCER>-<CAT>-<4DIGITS>-<4DIGITS>
    bool isValidProductCodeFormat(const std::string& code);

    /// Check if a product_code already exists in the product table
    bool isProductCodeUnique(Database* db, const std::string& code);

    /// Check if a barcode already exists in the product table
    bool isBarcodeUnique(Database* db, const std::string& barcode);

    /// Check if an SKU already exists in the product table
    bool isSkuUnique(Database* db, const std::string& sku);
}

#endif // USER_CREATE_H

