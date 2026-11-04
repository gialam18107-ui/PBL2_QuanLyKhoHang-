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
     * 2. product counters (initialized with available=0, warranty=0, scrapped=0 directly on product)
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
        const std::string& address,
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
     * @param customerCode Code of supplier or customer (e.g. "BS-0000001", "IN-0000001"). Defaults to "BS-0000000" if empty for ADJUSTMENT_IN.
     * @param items List of Pairs (productId, orderedQuantity)
     * @param orderDate Order date (YYYY-MM-DD)
     * @param expectedDeliveryDate Expected delivery date (YYYY-MM-DD >= orderDate)
     * @param note Order notes
     */
    OperationResult createInboundOrder(
        Database* db,
        const std::string& inboundType,
        const std::string& customerCode,
        const Vector<Pair<int, int>>& items,
        const std::string& orderDate,
        const std::string& expectedDeliveryDate,
        const std::string& note = "",
        const std::string& sourceCode = ""
    );

    /**
     * @brief Create an Outbound Transaction coupled with its primary Shipment.
     * Enforces active product check. Ordered quantity is permitted to exceed current stock (Rule 4.3).
     * @param outboundType SELLING, ADJUSTMENT_OUT, RETURN_TO_SUPPLIER, WARRANTY_RETURN, WARRANTY_REPLACEMENT_TO_CUSTOMER
     * @param customerCode Code of customer or partner (e.g. "IN-0000001", "BS-0000001"). Defaults to "BS-0000000" if empty for ADJUSTMENT_OUT.
     * @param items List of Pairs (productId, orderedQuantity)
     * @param orderDate Order date (YYYY-MM-DD)
     * @param expectedShipDate Expected shipment date (YYYY-MM-DD >= orderDate)
     * @param note Order notes
     */
    OperationResult createOutboundOrder(
        Database* db,
        const std::string& outboundType,
        const std::string& customerCode,
        const Vector<Pair<int, int>>& items,
        const std::string& orderDate,
        const std::string& expectedShipDate,
        const std::string& note = "",
        const std::string& sourceCode = ""
    );

    // ========================================================
    // LOGISTICS BATCH ALLOCATION & MULTI-BATCH ORDERS (Rule 4.3)
    // ========================================================

    /**
     * @brief Logistics batch input structure for multi-batch allocation.
     */
    struct LogisticsBatchInput {
        std::string expectedDate;
        Vector<Pair<int, int>> items; // Pair<productId, received_quantity> allocated for this batch
    };

    /**
     * @brief Create an Inbound Transaction with explicitly allocated delivery batches.
     * Enforces active product check and strict quantity equality:
     * sum(delivery.received_quantity) == ordered_quantity for every product (Rule 4.3).
     */
    OperationResult createInboundOrderWithDeliveries(
        Database* db,
        const std::string& inboundType,
        const std::string& customerCode,
        const Vector<Pair<int, int>>& totalOrderedItems,
        const Vector<LogisticsBatchInput>& deliveryBatches,
        const std::string& orderDate,
        const std::string& note = "",
        const std::string& sourceCode = ""
    );

    /**
     * @brief Create an Outbound Transaction with explicitly allocated shipment batches.
     * Enforces active product check, stock availability, and strict quantity equality:
     * sum(shipment.received_quantity) == ordered_quantity for every product (Rule 4.3).
     */
    OperationResult createOutboundOrderWithShipments(
        Database* db,
        const std::string& outboundType,
        const std::string& customerCode,
        const Vector<Pair<int, int>>& totalOrderedItems,
        const Vector<LogisticsBatchInput>& shipmentBatches,
        const std::string& orderDate,
        const std::string& note = "",
        const std::string& sourceCode = ""
    );

    /**
     * @brief Attempt to create an additional delivery batch for an existing Inbound Transaction (Rule 4.3).
     * NOTE: Strictly prohibited after confirming transaction; always returns false OperationResult.
     */
    OperationResult createDeliveryBatch(
        Database* db,
        int inboundTransactionId,
        const std::string& expectedDate,
        const Vector<Pair<int, int>>& items
    );

    /**
     * @brief Attempt to create an additional shipment batch for an existing Outbound Transaction (Rule 4.3).
     * NOTE: Strictly prohibited after confirming transaction; always returns false OperationResult.
     */
    OperationResult createShipmentBatch(
        Database* db,
        int outboundTransactionId,
        const std::string& expectedDate,
        const Vector<Pair<int, int>>& items
    );

    // ========================================================
    // SERIAL GENERATION & UNIT RECEIVING (Rule 4.4, 5.5)
    // ========================================================

    /// Generate unique Serial in format SN- + 7 uppercase alphanumeric characters
    std::string generateUniqueSerial(Database* db);

    /// Check if unit serial already exists in unit_inventory
    bool isSerialUnique(Database* db, const std::string& serial);

    /// Validate serial format ^SN-[A-Za-z0-9]{7}$
    bool isValidSerialFormat(const std::string& serial);

    /**
     * @brief Receive physical units into warehouse for a Delivery batch (Rule 4.4.2).
     * For PUR/WRP: creates new physical units in unit_inventory.
     * For WCK/CRT: updates existing OUT units back to target status (AVAILABLE, WARRANTY, SCRAPPED).
     * For ADJ: allows transition from any status to any internal status (AVAILABLE, WARRANTY, SCRAPPED), target cannot be OUT.
     * Automatically increments actual_quantity in delivery_detail and inbound_detail.
     * Updates product_quantity buckets and logs immutable records into status_history.
     */
    OperationResult receiveUnitsForDelivery(
        Database* db,
        int deliveryId,
        int productId,
        const std::string& targetStatus,
        int quantity,
        const Vector<std::string>& customSerials = {}
    );

    /// Convenience overload for a single unit receiving
    OperationResult receiveUnitForDelivery(
        Database* db,
        int deliveryId,
        int productId,
        const std::string& targetStatus,
        const std::string& customSerial = ""
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

    // ========================================================
    // INVENTORY CHECK & ADJUSTMENT MOVEMENT CREATION
    // ========================================================

    struct AdjustMovementItem {
        int productId = 0;
        std::string fromStatus;
        std::string toStatus;
        int quantity = 0;
    };

    /**
     * @brief Create an Inventory Check (Rule: product scope must be declared upfront).
     * @param checkType "QUAN" (Quantity Check) or "UNIT" (Detailed Unit Check)
     * @param checkDate YYYY-MM-DD
     * @param productIds Pre-declared list of products to inspect (locked scope)
     * @param note Notes
     */
    OperationResult createInventoryCheck(
        Database* db,
        const std::string& checkType,
        const std::string& checkDate,
        const Vector<int>& productIds,
        const std::string& note = ""
    );

    /**
     * @brief Create an Adjustment Movement (Rule: details must be declared upfront).
     * @param sourceCheckCode Code of inventory check (CHK-...) or empty
     * @param reason Must exist in reason table
     * @param adjustedAt YYYY-MM-DD
     * @param items Pre-declared list of adjustment items (locked scope)
     * @param note Notes
     */
    OperationResult createAdjustmentMovement(
        Database* db,
        const std::string& sourceCheckCode,
        const std::string& reason,
        const std::string& adjustedAt,
        const Vector<AdjustMovementItem>& items,
        const std::string& note = ""
    );

    /**
     * @brief Automatically generate adjustments from an inventory check (if user confirms).
     * For QUAN check: creates ADJUSTMENT_IN or ADJUSTMENT_OUT transactions (customer_id = NULL, source_code = check_code).
     * For UNIT check: creates ADJUSTMENT_IN, ADJUSTMENT_OUT, and/or ADJUSTMENT_MOVEMENT.
     */
    OperationResult createAdjustmentsFromCheck(
        Database* db,
        int checkId,
        const std::string& actionDate
    );

    /// Generate unique check_code format: CHK-{TYPE}-{SEQ:6}
    std::string generateUniqueCheckCode(Database* db, const std::string& checkType);

    /// Generate unique adjust_code format: ADJ-{REASON_CODE}-{SEQ:6}
    std::string generateUniqueAdjustCode(Database* db, const std::string& reason = "STOCKTAKE_DIFF");
}

#endif // USER_CREATE_H
