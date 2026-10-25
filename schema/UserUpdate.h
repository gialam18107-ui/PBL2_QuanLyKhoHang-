#ifndef USER_UPDATE_H
#define USER_UPDATE_H

#include <string>
#include "database/Database.h"
#include "database/Record.h"
#include "datastructure/vector/Vector.h"
#include "schema/OperationResult.h"

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
     *    - Frozen/Immutable fields on confirmed transactions and logistics.
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
        bool isSold
    );
}

#endif // USER_UPDATE_H
