#ifndef USER_UPDATE_H
#define USER_UPDATE_H

#include <string>
#include "database/Database.h"
#include "database/Record.h"
#include "datastructure/vector/Vector.h"
#include "database/OperationResult.h"

namespace user_update
{
    /**
     * @brief Helper to dynamically find the primary key column name of any table.
     */
    std::string getPrimaryKeyColumn(const Table* table);

    /**
     * @brief Generic update record function adhering to Rule 6.3 & 6.3.1.
     * Modifies the specified columns of a record identified by recordId in tableName.
     *
     * Validates 3 tiers:
     * 1. Schema & Existence (table exists, record exists, columns exist, type compatibility).
     * 2. Integrity & Constraints (NOT NULL, UNIQUE, Foreign Key lookup).
     * 3. Domain Business Rules:
     *    - Frozen/Immutable fields on confirmed transactions, logistics, customer, and brand.
     *    - Chronological date validation: delivery_at >= order_at, ship_at >= order_at.
     */
    OperationResult updateRecord(
        Database* db,
        const std::string& tableName,
        int recordId,
        const Vector<std::string>& modifiedColumns,
        const Vector<Value>& modifyingValues
    );

    /**
     * @brief Update the status of an inbound or outbound transaction (Rule 5.3).
     */
    OperationResult updateTransactionStatus(
        Database* db,
        const std::string& tableName,
        int transactionId,
        const std::string& newStatus
    );

    /**
     * @brief Update the note of an inbound or outbound transaction (Rule 5.3 & User Request).
     */
    OperationResult updateTransactionNote(
        Database* db,
        const std::string& tableName,
        int transactionId,
        const std::string& newNote
    );

    /**
     * @brief Update the actual delivery or shipment date (Rule 5.3).
     * Validates chronological order against parent order_at.
     */
    OperationResult updateLogisticsActualDate(
        Database* db,
        const std::string& tableName,
        int logisticsId,
        const std::string& actualDateStr
    );

    /**
     * @brief Update selling status of a product (product.is_sold) (Rule 4.2).
     */
    OperationResult updateProductSellingStatus(
        Database* db,
        int productId,
        bool isSold,
        const std::string& dateStr = ""
    );

    // ========================================================
    // CUSTOMER UPDATE
    // ========================================================

    /**
     * @brief Update customer contact details (name/company, phone, email, optional tax code).
     * Customer code, ID, and type are strictly protected against modification.
     */
    OperationResult updateCustomerContact(
        Database* db,
        int customerId,
        const std::string& nameOrCompany,
        const std::string& phone,
        const std::string& email,
        const std::string& taxCode = ""
    );

    // ========================================================
    // BRAND UPDATE
    // ========================================================

    /**
     * @brief Update brand display name (code is immutable if products reference it).
     */
    OperationResult updateBrand(
        Database* db,
        int brandId,
        const std::string& newBrandName
    );

    // ========================================================
    // SHIPMENT UNIT DISPATCHING (Rule 4.4.1)
    // ========================================================

    /**
     * @brief Dispatch physical units out of warehouse for a Shipment batch (Rule 4.4.1).
     * Verifies status preconditions (e.g., AVAILABLE for SEL, SCRAPPED/WARRANTY for WRT, AVAILABLE/WARRANTY for WRC).
     * Updates unit status in unit_inventory to 'OUT'.
     * Automatically increments actual_quantity in shipment_detail and outbound_detail.
     * Decrements corresponding bucket in product_quantity and logs immutable record into status_history.
     */
    OperationResult dispatchUnitsForShipment(
        Database* db,
        int shipmentId,
        int productId,
        const Vector<std::string>& serials
    );

    /// Convenience overload for dispatching a single unit
    OperationResult dispatchUnitForShipment(
        Database* db,
        int shipmentId,
        int productId,
        const std::string& serial
    );

    // ========================================================
    // LOGISTICS UNIT STATUS MODIFICATION (Rule 4.4, 4.5)
    // ========================================================

    /**
     * @brief Thay đổi trạng thái tự do một phần cho linh kiện trong đợt delivery/shipment (Rule 4.5).
     * Nghiêm cấm thay đổi nếu đợt đã hoàn thành (COMPLETED).
     * Kiểm tra trạng thái mới có hợp lệ theo loại giao dịch (ví dụ PUR chỉ được đổi giữa AVAILABLE và SCRAPPED).
     * Cập nhật unit_inventory, product_quantity và ghi nhật ký status_history.
     */
    OperationResult changeUnitStatusInLogistics(
        Database* db,
        const std::string& logisticsCode,
        bool isInbound,
        const std::string& serial,
        const std::string& newStatus
    );

    // ========================================================
    // INVENTORY CHECK & ADJUSTMENT MOVEMENT UPDATES
    // ========================================================

    OperationResult updateInventoryCheckQuantity(
        Database* db,
        int checkQuantityId,
        int actualAvail,
        int actualWarr,
        int actualScrap,
        const std::string& note = ""
    );

    OperationResult updateInventoryCheckUnit(
        Database* db,
        int checkUnitId,
        const std::string& scannedStatus
    );

    /**
     * @brief Ghi nhận linh kiện thừa vào inventory_check_unit (Rule 5.3.1).
     * Áp dụng cho:
     * - Type 1 (Rediscovered OUT): system_status = "OUT"
     * - Type 2 (Brand-New Unrecorded): system_status = "" (lưu NULL trong CSDL)
     * Tự động đồng bộ lại các cột actual_*_quan trên inventory_check_quantity.
     */
    OperationResult recordSurplusCheckUnit(
        Database* db,
        int checkId,
        int checkQuantityId,
        const std::string& serial,
        const std::string& systemStatus,
        const std::string& scannedStatus
    );

    OperationResult completeInventoryCheck(
        Database* db,
        int checkId
    );

    OperationResult executeAdjustmentMovement(
        Database* db,
        int adjustId
    );

    struct AdjustUnitEntry {
        int adjustDetailId;
        std::string serial;
    };

    OperationResult executeAdjustmentMovementUnits(
        Database* db,
        int adjustId,
        const Vector<AdjustUnitEntry>& stagedUnits
    );

    OperationResult updateAdjustmentMovementStatus(
        Database* db,
        int adjustId,
        const std::string& newStatus
    );

    OperationResult cancelAdjustmentMovement(
        Database* db,
        int adjustId
    );
}

#endif // USER_UPDATE_H
