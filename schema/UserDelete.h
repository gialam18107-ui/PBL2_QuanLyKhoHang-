#ifndef USER_DELETE_H
#define USER_DELETE_H

#include <string>
#include "database/Database.h"
#include "schema/OperationResult.h"

namespace user_delete
{
    /**
     * @brief Generic record deletion adhering to Rule 6.3 & 6.3.1.
     * Deletes a record identified by recordId from tableName.
     *
     * Validates 3 tiers:
     * 1. Schema & Existence (table exists, record exists).
     * 2. Transaction Immutability (transactions, logistics, and unit inventory cannot be deleted).
     * 3. Referential Protection (blocks deletion if child records reference this record).
     */
    OperationResult deleteRecord(Database* db, const std::string& tableName, int recordId);

    /**
     * @brief High-level atomic deletion of a product (Rule 4.1 & 6.3.1).
     * Verifies that the product has 0 stock and no transaction history.
     * Atomically removes the hardware spec record, product_quantity, and product record.
     */
    OperationResult deleteProduct(Database* db, int productId);
}

#endif // USER_DELETE_H

