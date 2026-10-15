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
    };

    /**
     * @brief Dedicated transaction & inventory-flow data generator for warehouse management.
     * 
     * Complies strictly with project_rules.md (Rule 7) and QuanLyKhoHang_schema.json:
     * - Generates transaction-related test data ONLY (no master/reference data).
     * - Does NOT generate products, brands, or categories (these come from crawler/master).
     * - Does NOT modify or delete schema, database structure, or join tables.
     * - Inbound Pipeline: InboundTransaction -> InboundDetail -> Delivery -> DeliveryUnitDetail -> UnitInventory -> StatusHistory
     * - Outbound Pipeline: OutboundTransaction -> OutboundDetail -> Shipment -> ShipmentUnitDetail -> Unit status update -> StatusHistory
     * - Status history is chronologically and logically consistent with valid state transitions.
     * - Provides an interactive console interface per Rule 7.3 and CLI batch mode.
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
        int nextDeliveryUnitDetailId;
        int nextOutboundTxnId;
        int nextOutboundDetailId;
        int nextShipmentId;
        int nextShipmentUnitDetailId;
        int nextStatusHistoryId;

        // Sequence number counters for code generation
        HashMap<string, int> inboundTypeCounters;
        HashMap<string, int> outboundTypeCounters;

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
         * @brief Generate an inbound transaction pipeline sequentially and consistently.
         * InboundTransaction -> InboundDetail -> Delivery -> DeliveryUnitDetail -> UnitInventory -> (StatusHistory).
         * @param inboundType PURCHASING, ADJUSTMENT_IN, WARRANTY_CHECKING, WARRANTY_REPLACEMENT, CUSTOMER_RETURN.
         * @param items Vector of Pairs (productId, orderedQuantity).
         * @param numDeliveries Number of deliveries required (>= 1).
         * @param orderDate Date in YYYY-MM-DD format (or generated if empty).
         * @param note Description note.
         * @return Inbound transaction ID, or -1 on error.
         */
        int generateInbound(const string& inboundType,
                            const Vector<Pair<int, int>>& items,
                            int numDeliveries = 1,
                            const string& orderDate = "",
                            const string& note = "");

        /**
         * @brief Generate an outbound transaction pipeline sequentially and consistently.
         * OutboundTransaction -> OutboundDetail -> Shipment -> ShipmentUnitDetail -> Unit status update -> (StatusHistory).
         * @param outboundType SELLING, ADJUSTMENT_OUT, RETURN_TO_SUPPLIER, WARRANTY_RETURN, WARRANTY_REPLACEMENT_TO_CUSTOMER.
         * @param items Vector of Pairs (productId, orderedQuantity).
         * @param numShipments Number of shipments required (>= 1).
         * @param orderDate Date in YYYY-MM-DD format (or generated if empty).
         * @param note Description note.
         * @return Outbound transaction ID, or -1 on error.
         */
        int generateOutbound(const string& outboundType,
                             const Vector<Pair<int, int>>& items,
                             int numShipments = 1,
                             const string& orderDate = "",
                             const string& note = "");

        /**
         * @brief Generate a batch of simulated transactions with random valid products and quantities.
         * @param numInbound Number of inbound transactions to generate.
         * @param numOutbound Number of outbound transactions to generate.
         * @param startDate Starting date in YYYY-MM-DD.
         * @return Summary of generated entities.
         */
        GenerationSummary generateBatch(int numInbound,
                                        int numOutbound,
                                        const string& startDate = "2026-09-10");

        /**
         * @brief Get count of available units for a product.
         */
        int getAvailableStock(int productId) const;

        /**
         * @brief Synchronize product.quantity with actual count of AVAILABLE units in unit_inventory.
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
