#include "Schema.h"
#include "database/Column.h"
#include "database/Constraint.h"

namespace Schema
{
    // ========================================================
    // 1. CATEGORY
    // ========================================================
    shared_ptr<Table> createCategoryTable()
    {
        auto table = make_shared<Table>("category");
        table->addColumn(Column("category_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("category_name", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("code", DataType::VARCHAR, 20, false, true, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_category", "category_id"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_category_name", "category_name"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_category_code", "code"));
        return table;
    }

    // ========================================================
    // 2. BRAND
    // ========================================================
    shared_ptr<Table> createBrandTable()
    {
        auto table = make_shared<Table>("brand");
        table->addColumn(Column("brand_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("brand", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("code", DataType::VARCHAR, 20, false, true, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_brand", "brand_id"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_brand_name", "brand"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_brand_code", "code"));
        return table;
    }

    // ========================================================
    // 3. UNIT_STATUS
    // ========================================================
    shared_ptr<Table> createUnitStatusTable()
    {
        auto table = make_shared<Table>("unit_status");
        table->addColumn(Column("status_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, true, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_unit_status", "status_id"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_unit_status", "status"));
        return table;
    }

    // ========================================================
    // 4. INBOUND_TYPE
    // ========================================================
    shared_ptr<Table> createInboundTypeTable()
    {
        auto table = make_shared<Table>("inbound_type");
        table->addColumn(Column("id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("type", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("code", DataType::VARCHAR, 20, false, true, true));
        table->addColumn(Column("next_code_no", DataType::INTEGER, 0, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_inbound_type", "id"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_inbound_type", "type"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_inbound_type_code", "code"));
        return table;
    }

    // ========================================================
    // 5. INBOUND_STATUS
    // ========================================================
    shared_ptr<Table> createInboundStatusTable()
    {
        auto table = make_shared<Table>("inbound_status");
        table->addColumn(Column("status_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, true, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_inbound_status", "status_id"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_inbound_status", "status"));
        return table;
    }

    // ========================================================
    // 6. DELIVERY_STATUS
    // ========================================================
    shared_ptr<Table> createDeliveryStatusTable()
    {
        auto table = make_shared<Table>("delivery_status");
        table->addColumn(Column("status_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, true, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_delivery_status", "status_id"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_delivery_status", "status"));
        return table;
    }

    // ========================================================
    // 7. OUTBOUND_TYPE
    // ========================================================
    shared_ptr<Table> createOutboundTypeTable()
    {
        auto table = make_shared<Table>("outbound_type");
        table->addColumn(Column("id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("type", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("code", DataType::VARCHAR, 20, false, true, true));
        table->addColumn(Column("next_code_no", DataType::INTEGER, 0, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_outbound_type", "id"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_outbound_type", "type"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_outbound_type_code", "code"));
        return table;
    }

    // ========================================================
    // 8. SHIPMENT_STATUS
    // ========================================================
    shared_ptr<Table> createShipmentStatusTable()
    {
        auto table = make_shared<Table>("shipment_status");
        table->addColumn(Column("status_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, true, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_shipment_status", "status_id"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_shipment_status", "status"));
        return table;
    }

    // ========================================================
    // 9. PRODUCT
    // ========================================================
    shared_ptr<Table> createProductTable()
    {
        auto table = make_shared<Table>("product");
        table->addColumn(Column("product_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("product_name", DataType::VARCHAR, 150, false, false, true));
        table->addColumn(Column("product_code", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("barcode", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("sku", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("brand", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("category", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("quantity", DataType::INTEGER, 0, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_product", "product_id"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_product_code", "product_code"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_product_barcode", "barcode"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_product_sku", "sku"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_product_brand", "brand", "brand", "brand"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_product_category", "category", "category", "category_name"));
        return table;
    }

    // ========================================================
    // 10. CPU
    // ========================================================
    shared_ptr<Table> createCpuTable()
    {
        auto table = make_shared<Table>("cpu");
        table->addColumn(Column("cpu_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("socket", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("tier", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("series", DataType::VARCHAR, 100, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_cpu", "cpu_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_cpu_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 11. GPU
    // ========================================================
    shared_ptr<Table> createGpuTable()
    {
        auto table = make_shared<Table>("gpu");
        table->addColumn(Column("gpu_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("series", DataType::VARCHAR, 100, false, false, true));
        table->addColumn(Column("producer", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("vram", DataType::INTEGER, 0, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_gpu", "gpu_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_gpu_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 12. MAINBOARD
    // ========================================================
    shared_ptr<Table> createMainboardTable()
    {
        auto table = make_shared<Table>("mainboard");
        table->addColumn(Column("mainboard_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("cpu_support", DataType::VARCHAR, 100, false, false, true));
        table->addColumn(Column("socket", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("size", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("chipset", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("ram_support", DataType::VARCHAR, 50, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_mainboard", "mainboard_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_mainboard_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 13. RAM
    // ========================================================
    shared_ptr<Table> createRamTable()
    {
        auto table = make_shared<Table>("ram");
        table->addColumn(Column("ram_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("generation", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("quantity", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("capacity", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("bus", DataType::INTEGER, 0, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_ram", "ram_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_ram_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 14. SSD
    // ========================================================
    shared_ptr<Table> createSsdTable()
    {
        auto table = make_shared<Table>("ssd");
        table->addColumn(Column("ssd_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("type", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("gen", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("capacity", DataType::VARCHAR, 50, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_ssd", "ssd_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_ssd_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 15. PSU
    // ========================================================
    shared_ptr<Table> createPsuTable()
    {
        auto table = make_shared<Table>("psu");
        table->addColumn(Column("psu_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("quality", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("capacity", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("size", DataType::VARCHAR, 50, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_psu", "psu_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_psu_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 16. COOLING
    // ========================================================
    shared_ptr<Table> createCoolingTable()
    {
        auto table = make_shared<Table>("cooling");
        table->addColumn(Column("cooling_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("type", DataType::VARCHAR, 100, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_cooling", "cooling_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_cooling_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 17. CASE
    // ========================================================
    shared_ptr<Table> createCaseTable()
    {
        auto table = make_shared<Table>("case");
        table->addColumn(Column("case_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("product_name", DataType::VARCHAR, 150, false, false, true));
        table->addColumn(Column("size", DataType::VARCHAR, 50, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_case", "case_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_case_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 18. UNIT_INVENTORY
    // ========================================================
    shared_ptr<Table> createUnitInventoryTable()
    {
        auto table = make_shared<Table>("unit_inventory");
        table->addColumn(Column("unit_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("serial", DataType::VARCHAR, 100, false, true, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_unit_inventory", "unit_id"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_unit_inventory_serial", "serial"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_unit_inventory_product", "product_id", "product", "product_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_unit_inventory_status", "status", "unit_status", "status"));
        return table;
    }

    // ========================================================
    // 19. INBOUND_TRANSACTION
    // ========================================================
    shared_ptr<Table> createInboundTransactionTable()
    {
        auto table = make_shared<Table>("inbound_transaction");
        table->addColumn(Column("inbound_transaction_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("inbound_code", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("order_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("type", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("next_delivery_no", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("note", DataType::TEXT, 0, false, false, false));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_inbound_transaction", "inbound_transaction_id"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_inbound_transaction_code", "inbound_code"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_inbound_transaction_type", "type", "inbound_type", "type"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_inbound_transaction_status", "status", "inbound_status", "status"));
        return table;
    }

    // ========================================================
    // 20. INBOUND_DETAIL
    // ========================================================
    shared_ptr<Table> createInboundDetailTable()
    {
        auto table = make_shared<Table>("inbound_detail");
        table->addColumn(Column("inbound_detail_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("inbound_transaction_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("ordered_quantity", DataType::INTEGER, 0, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_inbound_detail", "inbound_detail_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_inbound_detail_txn", "inbound_transaction_id", "inbound_transaction", "inbound_transaction_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_inbound_detail_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 21. DELIVERY
    // ========================================================
    shared_ptr<Table> createDeliveryTable()
    {
        auto table = make_shared<Table>("delivery");
        table->addColumn(Column("delivery_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("delivery_code", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("inbound_transaction_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("delivery_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("expected_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_delivery", "delivery_id"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_delivery_code", "delivery_code"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_delivery_inbound_txn", "inbound_transaction_id", "inbound_transaction", "inbound_transaction_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_delivery_status", "status", "delivery_status", "status"));
        return table;
    }

    // ========================================================
    // 22. DELIVERY_UNIT_DETAIL
    // ========================================================
    shared_ptr<Table> createDeliveryUnitDetailTable()
    {
        auto table = make_shared<Table>("delivery_unit_detail");
        table->addColumn(Column("delivery_unit_detail_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("delivery_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("serial", DataType::VARCHAR, 100, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_delivery_unit_detail", "delivery_unit_detail_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_delivery_unit_delivery", "delivery_id", "delivery", "delivery_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_delivery_unit_product", "product_id", "product", "product_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_delivery_unit_serial", "serial", "unit_inventory", "serial"));
        return table;
    }

    // ========================================================
    // 23. OUTBOUND_TYPE
    // ========================================================
    // (createOutboundTypeTable is already defined above)

    // ========================================================
    // 24. OUTBOUND_TRANSACTION
    // ========================================================
    shared_ptr<Table> createOutboundTransactionTable()
    {
        auto table = make_shared<Table>("outbound_transaction");
        // Column header in CSV is outbound_trainsaction_id
        table->addColumn(Column("outbound_trainsaction_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("outbound_code", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("order_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("type", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("next_ship_no", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("note", DataType::TEXT, 0, false, false, false));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_outbound_transaction", "outbound_trainsaction_id"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_outbound_transaction_code", "outbound_code"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_outbound_transaction_type", "type", "outbound_type", "type"));
        return table;
    }

    // ========================================================
    // 25. OUTBOUND_DETAIL
    // ========================================================
    shared_ptr<Table> createOutboundDetailTable()
    {
        auto table = make_shared<Table>("outbound_detail");
        table->addColumn(Column("outbound_detail_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("outbound_transaction_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("ordered_quantity", DataType::INTEGER, 0, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_outbound_detail", "outbound_detail_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_outbound_detail_txn", "outbound_transaction_id", "outbound_transaction", "outbound_trainsaction_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_outbound_detail_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 26. SHIPMENT
    // ========================================================
    shared_ptr<Table> createShipmentTable()
    {
        auto table = make_shared<Table>("shipment");
        table->addColumn(Column("shipment_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("shipment_code", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("sell_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("ship_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("expected_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_shipment", "shipment_id"));
        table->addConstraint(make_shared<UniqueConstraint>("uq_shipment_code", "shipment_code"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_shipment_outbound_txn", "sell_id", "outbound_transaction", "outbound_trainsaction_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_shipment_status", "status", "shipment_status", "status"));
        return table;
    }

    // ========================================================
    // 27. SHIPMENT_UNIT_DETAIL
    // ========================================================
    shared_ptr<Table> createShipmentUnitDetailTable()
    {
        auto table = make_shared<Table>("shipment_unit_detail");
        table->addColumn(Column("shipment_unit_detail_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("shipment_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("serial", DataType::VARCHAR, 100, false, false, true));

        table->addConstraint(make_shared<PrimaryKeyConstraint>("pk_shipment_unit_detail", "shipment_unit_detail_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_shipment_unit_shipment", "shipment_id", "shipment", "shipment_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_shipment_unit_product", "product_id", "product", "product_id"));
        table->addConstraint(make_shared<ForeignKeyConstraint>("fk_shipment_unit_serial", "serial", "unit_inventory", "serial"));
        return table;
    }

    // ========================================================
    // SCHEMA MANAGEMENT & HELPERS
    // ========================================================

    shared_ptr<Table> createTable(const string& tableName)
    {
        if (tableName == "category") return createCategoryTable();
        if (tableName == "brand") return createBrandTable();
        if (tableName == "unit_status") return createUnitStatusTable();
        if (tableName == "inbound_type") return createInboundTypeTable();
        if (tableName == "inbound_status") return createInboundStatusTable();
        if (tableName == "delivery_status") return createDeliveryStatusTable();
        if (tableName == "outbound_type") return createOutboundTypeTable();
        if (tableName == "shipment_status") return createShipmentStatusTable();

        if (tableName == "product") return createProductTable();

        if (tableName == "cpu") return createCpuTable();
        if (tableName == "gpu") return createGpuTable();
        if (tableName == "mainboard") return createMainboardTable();
        if (tableName == "ram") return createRamTable();
        if (tableName == "ssd") return createSsdTable();
        if (tableName == "psu") return createPsuTable();
        if (tableName == "cooling") return createCoolingTable();
        if (tableName == "case") return createCaseTable();

        if (tableName == "unit_inventory") return createUnitInventoryTable();

        if (tableName == "inbound_transaction") return createInboundTransactionTable();
        if (tableName == "inbound_detail") return createInboundDetailTable();
        if (tableName == "delivery") return createDeliveryTable();
        if (tableName == "delivery_unit_detail") return createDeliveryUnitDetailTable();

        if (tableName == "outbound_transaction") return createOutboundTransactionTable();
        if (tableName == "outbound_detail") return createOutboundDetailTable();
        if (tableName == "shipment") return createShipmentTable();
        if (tableName == "shipment_unit_detail") return createShipmentUnitDetailTable();

        return nullptr;
    }

    vector<string> getTableNames()
    {
        return {
            // Level 1: Independent
            "category",
            "brand",
            "unit_status",
            "inbound_type",
            "inbound_status",
            "delivery_status",
            "outbound_type",
            "shipment_status",

            // Level 2: Depends on Level 1
            "product",
            "inbound_transaction",
            "outbound_transaction",

            // Level 3: Depends on Level 2
            "cpu",
            "gpu",
            "mainboard",
            "ram",
            "ssd",
            "psu",
            "cooling",
            "case",
            "unit_inventory",
            "inbound_detail",
            "delivery",
            "outbound_detail",
            "shipment",

            // Level 4: Depends on Level 3
            "delivery_unit_detail",
            "shipment_unit_detail"
        };
    }

    vector<shared_ptr<Table>> createAllTables()
    {
        vector<shared_ptr<Table>> allTables;
        for (const string& name : getTableNames())
        {
            auto tbl = createTable(name);
            if (tbl != nullptr)
            {
                allTables.push_back(tbl);
            }
        }
        return allTables;
    }

    void initDatabase(Database& db)
    {
        for (const auto& tbl : createAllTables())
        {
            db.addTable(tbl);
        }
    }
}
