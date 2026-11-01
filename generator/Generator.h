#ifndef GENERATOR_H
#define GENERATOR_H

#include <string>
#include "datastructure/vector/Vector.h"
#include "datastructure/hashmap/HashMap.h"
#include "datastructure/hashmap/Pair.h"
#include "database/Database.h"
#include "database/Table.h"
#include "database/Record.h"

using namespace std;

namespace Generator
{
    /**
     * @brief Result summary of batch data generation.
     */
    struct GenerationSummary
    {
        int inboundTxnsGenerated = 0;
        int deliveriesGenerated = 0;
        int unitsCreated = 0;
        int outboundTxnsGenerated = 0;
        int shipmentsGenerated = 0;
        int unitsShipped = 0;
        int inventoryChecksGenerated = 0;
    };

    /**
     * @brief Dedicated transaction & inventory-flow data generator for warehouse management.
     * 
     * Complies strictly with project_rules.md and QuanLyKhoHang_schema.json:
     * - Generates transaction-related test data ONLY (no master/reference data).
     * - Inbound Pipeline: InboundTransaction -> InboundDetail -> Delivery -> DeliveryDetail -> UnitInventory -> StatusHistory
     * - Outbound Pipeline: OutboundTransaction -> OutboundDetail -> Shipment -> ShipmentDetail -> Unit status update -> StatusHistory
     * - Inbound operations ONLY process units whose old status is OUT or NULL.
     * - ADJUSTMENT_IN and ADJUSTMENT_OUT transactions have NO customer (customer_id = NULL) and source_code MUST reference check_code.
     * - Batch generation selects only active products (product.is_sold == TRUE).
     * - Synchronizes product table inventory counters directly.
     * - Strictly no STL containers or smart pointers (uses custom Vector, HashMap, Pair).
     */
    class DataGenerator
    {
    private:
        Database& db;

        // Auto-increment primary key trackers
        int nextUnitId;
        int nextInboundTxnId;
        int nextInboundDetailId;
        int nextDeliveryId;
        int nextDeliveryDetailId;
        int nextOutboundTxnId;
        int nextOutboundDetailId;
        int nextShipmentId;
        int nextShipmentDetailId;
        int nextStatusHistoryId;
        int nextInventoryCheckId;
        int nextCheckQuantityId;
        int nextCheckUnitId;
        int nextAdjustmentMovementId;
        int nextAdjustDetailId;
        int nextIsSoldChangingId;

        // Sequence number counters for code generation
        HashMap<string, int> inboundTypeCounters;
        HashMap<string, int> outboundTypeCounters;
        int nextCheckSeq;
        int nextAdjustSeq;

        // Uniqueness tracking sets
        HashMap<string, bool> existingSerials;

        // Random state
        unsigned int rngSeed;

        // Internal helper methods
        void scanExistingDatabase();
        string generateUniqueSerial();
        string formatNumber(int num, int digits) const;
        string advanceDate(const string& dateStr, int days) const;
        string getCurrentDate() const;
        unsigned int nextRandom();

    public:
        /**
         * @brief Construct generator bound to an active Database instance.
         * @param database Reference to target database.
         * @param seed Random seed for reproducible generation (0 for time-based).
         */
        explicit DataGenerator(Database& database, unsigned int seed = 0);

        /**
         * @brief Initialize generator by scanning existing database records to avoid key collisions.
         */
        void initialize();

        /**
         * @brief Generate an inventory check record (CHK-QUAN or CHK-UNIT) with product snapshots.
         * @param checkType "QUAN" or "UNIT".
         * @param checkDate Date string (or current date if empty).
         * @param productIds List of product IDs included in the audit.
         * @return Generated check_code (e.g., CHK-QUAN-000001).
         */
        string generateInventoryCheck(const string& checkType = "QUAN",
                                      const string& checkDate = "",
                                      const Vector<int>& productIds = Vector<int>());

        /**
         * @brief Generate an inbound transaction pipeline sequentially and consistently.
         * InboundTransaction -> InboundDetail -> Delivery -> DeliveryDetail -> UnitInventory -> (StatusHistory).
         * @param inboundType PURCHASING, ADJUSTMENT_IN, WARRANTY_CHECKING, WARRANTY_REPLACEMENT, CUSTOMER_RETURN.
         * @param items Vector of Pairs (productId, orderedQuantity).
         * @param numDeliveries Number of deliveries required (>= 1).
         * @param orderDate Date in YYYY-MM-DD format (or generated if empty).
         * @param note Description note.
         * @param sourceCode Check code reference (mandatory for ADJUSTMENT_IN; auto-generated if empty).
         * @return Inbound transaction ID, or -1 on error.
         */
        int generateInbound(const string& inboundType,
                            const Vector<Pair<int, int>>& items,
                            int numDeliveries = 1,
                            const string& orderDate = "",
                            const string& note = "",
                            const string& sourceCode = "");

        /**
         * @brief Generate an outbound transaction pipeline sequentially and consistently.
         * OutboundTransaction -> OutboundDetail -> Shipment -> ShipmentDetail -> Unit status update -> (StatusHistory).
         * @param outboundType SELLING, ADJUSTMENT_OUT, RETURN_TO_SUPPLIER, WARRANTY_RETURN, WARRANTY_REPLACEMENT_TO_CUSTOMER.
         * @param items Vector of Pairs (productId, orderedQuantity).
         * @param numShipments Number of shipments required (>= 1).
         * @param orderDate Date in YYYY-MM-DD format (or generated if empty).
         * @param note Description note.
         * @param sourceCode Check code reference (mandatory for ADJUSTMENT_OUT; auto-generated if empty).
         * @return Outbound transaction ID, or -1 on error.
         */
        int generateOutbound(const string& outboundType,
                             const Vector<Pair<int, int>>& items,
                             int numShipments = 1,
                             const string& orderDate = "",
                             const string& note = "",
                             const string& sourceCode = "");

        /**
         * @brief Generate a batch of simulated transactions with random valid products and quantities.
         * @param numInbound Number of inbound transactions to generate.
         * @param numOutbound Number of outbound transactions to generate.
         * @param numChecks Number of standalone inventory checks to generate.
         * @param startDate Starting date in YYYY-MM-DD.
         * @return Summary of generated entities.
         */
        GenerationSummary generateBatch(int numInbound,
                                        int numOutbound,
                                        int numChecks = 0,
                                        const string& startDate = "2026-09-10");

        /**
         * @brief Get count of available units for a product.
         */
        int getAvailableStock(int productId) const;

        /**
         * @brief Change is_sold status for a product. Only allowed to set false if total stock is 0.
         * Logs change into is_sold_changing table.
         * @return True if status successfully changed, false otherwise.
         */
        bool updateProductIsSold(int productId, bool newIsSold, int changedBy = 1, const string& reason = "");

        /**
         * @brief Synchronize product table inventory counters with actual counts in unit_inventory.
         */
        void syncProductQuantities();

        /**
         * @brief Validate that all foreign keys, unique constraints, and inventory rules are met.
         * @return True if database is 100% consistent and valid.
         */
        bool validateIntegrity() const;

        /**
         * @brief Print inventory summary to standard output.
         */
        void printInventorySummary() const;

        /**
         * @brief Display list of all products with current stock information.
         */
        void printProductList() const;

        /**
         * @brief Interactive console interface for transaction generation per Rule 7.3.
         */
        void runConsoleInterface();
    };
}

#endif // GENERATOR_H
