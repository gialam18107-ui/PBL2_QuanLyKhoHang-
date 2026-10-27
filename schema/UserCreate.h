#ifndef USER_CREATE_H
#define USER_CREATE_H

#include <string>
#include "datastructure/vector/Vector.h"
#include "datastructure/hashmap/Pair.h"
#include "database/Database.h"
#include "database/Record.h"
#include "database/OperationResult.h"

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
    // CUSTOMER CREATION (Rule 4.1)
    // ========================================================

    /// Create an Individual Customer (creates in customer + individual_info atomically)
    OperationResult createIndividualCustomer(
        Database* db,
        const std::string& name,
        const std::string& phone,
        const std::string& email
    );

    /// Create a Business Customer (creates in customer + business_info atomically)
    OperationResult createBusinessCustomer(
        Database* db,
        const std::string& companyName,
        const std::string& taxCode,
        const std::string& phone,
        const std::string& email
    );

    // ========================================================
    // BRAND CREATION (Rule 4.1)
    // ========================================================

    /// Create a new brand/manufacturer
    OperationResult createBrand(
        Database* db,
        const std::string& brandName,
        const std::string& brandCode
    );

    // ========================================================
    // TRANSACTION COUPLED CREATION (Rule 4.3 & 4.4)
    // ========================================================

    /**
     * @brief Create an Inbound Transaction coupled with its primary Delivery.
     * Enforces active product check (is_sold == TRUE).
     * @param inboundType PURCHASING, ADJUSTMENT_IN, WARRANTY_CHECKING, WARRANTY_REPLACEMENT, CUSTOMER_RETURN
     * @param customerId ID of supplier or customer
     * @param items List of Pairs (productId, orderedQuantity)
     * @param orderDate Order date (YYYY-MM-DD)
     * @param expectedDeliveryDate Expected delivery date (YYYY-MM-DD >= orderDate)
     * @param note Order notes
     */
    OperationResult createInboundOrder(
        Database* db,
        const std::string& inboundType,
        int customerId,
        const Vector<Pair<int, int>>& items,
        const std::string& orderDate,
        const std::string& expectedDeliveryDate,
        const std::string& note = ""
    );

    /**
     * @brief Create an Outbound Transaction coupled with its primary Shipment.
     * Enforces active product check and Stock Availability Precondition.
     * @param outboundType SELLING, ADJUSTMENT_OUT, RETURN_TO_SUPPLIER, WARRANTY_RETURN, WARRANTY_REPLACEMENT_TO_CUSTOMER
     * @param customerId ID of customer or partner
     * @param items List of Pairs (productId, orderedQuantity)
     * @param orderDate Order date (YYYY-MM-DD)
     * @param expectedShipDate Expected shipment date (YYYY-MM-DD >= orderDate)
     * @param note Order notes
     */
    OperationResult createOutboundOrder(
        Database* db,
        const std::string& outboundType,
        int customerId,
        const Vector<Pair<int, int>>& items,
        const std::string& orderDate,
        const std::string& expectedShipDate,
        const std::string& note = ""
    );

    // ========================================================
    // AUTO-GENERATION AND VALIDATION HELPERS
    // ========================================================

    /// Calculate next available record ID by scanning max primary key + 1
    int getNextTableId(Database* db, const std::string& tableName);

    /// Calculate next available product_id by scanning max product_id + 1
    int getNextProductId(Database* db);

    /// Generate product code in format: <producer_code>-<cat_code>-XXXX-YYYY (Rule 4.2)
    std::string generateNextProductCode(
        Database* db,
        const std::string& brandName,
        const std::string& categoryName
    );

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
