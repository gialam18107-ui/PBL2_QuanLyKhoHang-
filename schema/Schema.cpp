#include "Schema.h"
#include "database/Column.h"
#include "database/Constraint.h"

namespace Schema
{
    // ========================================================
    // 1. CATEGORY
    // ========================================================
    Table* createCategoryTable()
    {
        auto table = new Table("category");
        table->addColumn(Column("category_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("category_name", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("code", DataType::VARCHAR, 20, false, true, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_category", "category_id"));
        table->addConstraint(new UniqueConstraint("uq_category_name", "category_name"));
        table->addConstraint(new UniqueConstraint("uq_category_code", "code"));
        return table;
    }

    // ========================================================
    // 2. BRAND
    // ========================================================
    Table* createBrandTable()
    {
        auto table = new Table("brand");
        table->addColumn(Column("brand_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("brand", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("code", DataType::VARCHAR, 20, false, true, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_brand", "brand_id"));
        table->addConstraint(new UniqueConstraint("uq_brand_name", "brand"));
        table->addConstraint(new UniqueConstraint("uq_brand_code", "code"));
        return table;
    }

    // ========================================================
    // 3. UNIT_STATUS
    // ========================================================
    Table* createUnitStatusTable()
    {
        auto table = new Table("unit_status");
        table->addColumn(Column("status_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, true, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_unit_status", "status_id"));
        table->addConstraint(new UniqueConstraint("uq_unit_status", "status"));
        return table;
    }

    // ========================================================
    // 4. INBOUND_TYPE
    // ========================================================
    Table* createInboundTypeTable()
    {
        auto table = new Table("inbound_type");
        table->addColumn(Column("id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("type", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("code", DataType::VARCHAR, 20, false, true, true));
        table->addColumn(Column("next_code_no", DataType::INTEGER, 0, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_inbound_type", "id"));
        table->addConstraint(new UniqueConstraint("uq_inbound_type", "type"));
        table->addConstraint(new UniqueConstraint("uq_inbound_type_code", "code"));
        return table;
    }

    // ========================================================
    // 5. INBOUND_STATUS
    // ========================================================
    Table* createInboundStatusTable()
    {
        auto table = new Table("inbound_status");
        table->addColumn(Column("status_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, true, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_inbound_status", "status_id"));
        table->addConstraint(new UniqueConstraint("uq_inbound_status", "status"));
        return table;
    }

    // ========================================================
    // 6. DELIVERY_STATUS
    // ========================================================
    Table* createDeliveryStatusTable()
    {
        auto table = new Table("delivery_status");
        table->addColumn(Column("status_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, true, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_delivery_status", "status_id"));
        table->addConstraint(new UniqueConstraint("uq_delivery_status", "status"));
        return table;
    }

    // ========================================================
    // 7. OUTBOUND_TYPE
    // ========================================================
    Table* createOutboundTypeTable()
    {
        auto table = new Table("outbound_type");
        table->addColumn(Column("id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("type", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("code", DataType::VARCHAR, 20, false, true, true));
        table->addColumn(Column("next_code_no", DataType::INTEGER, 0, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_outbound_type", "id"));
        table->addConstraint(new UniqueConstraint("uq_outbound_type", "type"));
        table->addConstraint(new UniqueConstraint("uq_outbound_type_code", "code"));
        return table;
    }

    // ========================================================
    // 8. OUTBOUND_STATUS
    // ========================================================
    Table* createOutboundStatusTable()
    {
        auto table = new Table("outbound_status");
        table->addColumn(Column("status_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, true, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_outbound_status", "status_id"));
        table->addConstraint(new UniqueConstraint("uq_outbound_status", "status"));
        return table;
    }

    // ========================================================
    // 9. SHIPMENT_STATUS
    // ========================================================
    Table* createShipmentStatusTable()
    {
        auto table = new Table("shipment_status");
        table->addColumn(Column("status_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, true, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_shipment_status", "status_id"));
        table->addConstraint(new UniqueConstraint("uq_shipment_status", "status"));
        return table;
    }

    // ========================================================
    // 10. CUSTOMER
    // ========================================================
    Table* createCustomerTable()
    {
        auto table = new Table("customer");
        table->addColumn(Column("customer_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("customer_code", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("name", DataType::VARCHAR, 100, false, false, true));
        table->addColumn(Column("type", DataType::VARCHAR, 50, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_customer", "customer_id"));
        table->addConstraint(new UniqueConstraint("uq_customer_code", "customer_code"));
        table->createIndex("hash_tree_type", "type", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 11. INDIVIDUAL_INFO
    // ========================================================
    Table* createIndividualInfoTable()
    {
        auto table = new Table("individual_info");
        table->addColumn(Column("customer_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("phone_number", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("email", DataType::VARCHAR, 100, false, false, true));
        table->addColumn(Column("next_code_no", DataType::INTEGER, 0, false, false, true, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_individual_info", "customer_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_individual_customer", "customer_id", "customer", "customer_id"));
        return table;
    }

    // ========================================================
    // 12. BUSINESS_INFO
    // ========================================================
    Table* createBusinessInfoTable()
    {
        auto table = new Table("business_info");
        table->addColumn(Column("customer_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("phone_number", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("email", DataType::VARCHAR, 100, false, false, true));
        table->addColumn(Column("tax_code", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("next_code_no", DataType::INTEGER, 0, false, false, true, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_business_info", "customer_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_business_customer", "customer_id", "customer", "customer_id"));
        return table;
    }

    // ========================================================
    // 13. PRODUCT
    // ========================================================
    Table* createProductTable()
    {
        auto table = new Table("product");
        table->addColumn(Column("product_id", DataType::INTEGER, 0, true, true, true));
        table->addColumn(Column("product_name", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("product_code", DataType::VARCHAR, 20, false, true, true));
        table->addColumn(Column("barcode", DataType::VARCHAR, 13, false, true, true));
        table->addColumn(Column("sku", DataType::VARCHAR, 20, false, true, true));
        table->addColumn(Column("brand", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("category", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("is_sold", DataType::BOOLEAN, 0, false, false, true, false, "0"));

        table->addConstraint(new PrimaryKeyConstraint("pk_product", "product_id"));
        table->addConstraint(new UniqueConstraint("uq_product_code", "product_code"));
        table->addConstraint(new UniqueConstraint("uq_product_barcode", "barcode"));
        table->addConstraint(new UniqueConstraint("uq_product_sku", "sku"));
        table->addConstraint(new ForeignKeyConstraint("fk_product_brand", "brand", "brand", "brand"));
        table->addConstraint(new ForeignKeyConstraint("fk_product_category", "category", "category", "category_name"));
        table->createIndex("hash_tree_is_sold", "is_sold", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 14. PRODUCT_QUANTITY
    // ========================================================
    Table* createProductQuantityTable()
    {
        auto table = new Table("product_quantity");
        table->addColumn(Column("product_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("available_quan", DataType::INTEGER, 0, false, false, true, false, "0"));
        table->addColumn(Column("warranty_quan", DataType::INTEGER, 0, false, false, true, false, "0"));
        table->addColumn(Column("scrapped_quan", DataType::INTEGER, 0, false, false, true, false, "0"));

        table->addConstraint(new PrimaryKeyConstraint("pk_product_quantity", "product_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_product_quantity_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 15. CPU
    // ========================================================
    Table* createCpuTable()
    {
        auto table = new Table("cpu");
        table->addColumn(Column("cpu_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("socket", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("tier", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("series", DataType::VARCHAR, 100, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_cpu", "cpu_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_cpu_product", "product_id", "product", "product_id"));
        table->createIndex("hash_tree_socket", "socket", IndexType::HASH_TREE);
        table->createIndex("hash_tree_tier", "tier", IndexType::HASH_TREE);
        table->createIndex("hash_tree_series", "series", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 16. GPU
    // ========================================================
    Table* createGpuTable()
    {
        auto table = new Table("gpu");
        table->addColumn(Column("gpu_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("series", DataType::VARCHAR, 100, false, false, true));
        table->addColumn(Column("producer", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("vram", DataType::INTEGER, 0, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_gpu", "gpu_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_gpu_product", "product_id", "product", "product_id"));
        table->createIndex("hash_tree_series", "series", IndexType::HASH_TREE);
        table->createIndex("hash_tree_producer", "producer", IndexType::HASH_TREE);
        table->createIndex("hash_tree_vram", "vram", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 17. MAINBOARD
    // ========================================================
    Table* createMainboardTable()
    {
        auto table = new Table("mainboard");
        table->addColumn(Column("mainboard_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("cpu_support", DataType::VARCHAR, 100, false, false, true));
        table->addColumn(Column("socket", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("size", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("chipset", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("ram_support", DataType::VARCHAR, 50, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_mainboard", "mainboard_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_mainboard_product", "product_id", "product", "product_id"));
        table->createIndex("hash_tree_cpu_support", "cpu_support", IndexType::HASH_TREE);
        table->createIndex("hash_tree_socket", "socket", IndexType::HASH_TREE);
        table->createIndex("hash_tree_size", "size", IndexType::HASH_TREE);
        table->createIndex("hash_tree_chipset", "chipset", IndexType::HASH_TREE);
        table->createIndex("hash_tree_ram_support", "ram_support", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 18. RAM
    // ========================================================
    Table* createRamTable()
    {
        auto table = new Table("ram");
        table->addColumn(Column("ram_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("generation", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("quantity", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("capacity", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("bus", DataType::VARCHAR, 50, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_ram", "ram_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_ram_product", "product_id", "product", "product_id"));
        table->createIndex("hash_tree_generation", "generation", IndexType::HASH_TREE);
        table->createIndex("hash_tree_capacity", "capacity", IndexType::HASH_TREE);
        table->createIndex("hash_tree_bus", "bus", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 19. SSD
    // ========================================================
    Table* createSsdTable()
    {
        auto table = new Table("ssd");
        table->addColumn(Column("ssd_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("type", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("gen", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("capacity", DataType::VARCHAR, 50, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_ssd", "ssd_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_ssd_product", "product_id", "product", "product_id"));
        table->createIndex("hash_tree_type", "type", IndexType::HASH_TREE);
        table->createIndex("hash_tree_gen", "gen", IndexType::HASH_TREE);
        table->createIndex("hash_tree_capacity", "capacity", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 20. PSU
    // ========================================================
    Table* createPsuTable()
    {
        auto table = new Table("psu");
        table->addColumn(Column("psu_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("quality", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("capacity", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("size", DataType::VARCHAR, 50, false, false, false));

        table->addConstraint(new PrimaryKeyConstraint("pk_psu", "psu_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_psu_product", "product_id", "product", "product_id"));
        table->createIndex("hash_tree_quality", "quality", IndexType::HASH_TREE);
        table->createIndex("hash_tree_capacity", "capacity", IndexType::HASH_TREE);
        table->createIndex("hash_tree_size", "size", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 21. COOLING
    // ========================================================
    Table* createCoolingTable()
    {
        auto table = new Table("cooling");
        table->addColumn(Column("cooling_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("type", DataType::VARCHAR, 100, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_cooling", "cooling_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_cooling_product", "product_id", "product", "product_id"));
        table->createIndex("hash_tree_type", "type", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 22. CASE
    // ========================================================
    Table* createCaseTable()
    {
        auto table = new Table("case");
        table->addColumn(Column("case_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("product_name", DataType::VARCHAR, 150, false, false, true));
        table->addColumn(Column("size", DataType::VARCHAR, 50, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_case", "case_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_case_product", "product_id", "product", "product_id"));
        table->createIndex("hash_tree_size", "size", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 23. UNIT_INVENTORY
    // ========================================================
    Table* createUnitInventoryTable()
    {
        auto table = new Table("unit_inventory");
        table->addColumn(Column("unit_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("serial", DataType::VARCHAR, 100, false, true, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_unit_inventory", "unit_id"));
        table->addConstraint(new UniqueConstraint("uq_unit_inventory_serial", "serial"));
        table->addConstraint(new ForeignKeyConstraint("fk_unit_inventory_product", "product_id", "product", "product_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_unit_inventory_status", "status", "unit_status", "status"));
        table->createIndex("hash_tree_status", "status", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 24. INBOUND_TRANSACTION
    // ========================================================
    Table* createInboundTransactionTable()
    {
        auto table = new Table("inbound_transaction");
        table->addColumn(Column("inbound_transaction_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("inbound_code", DataType::CHAR, 12, false, true, true));
        table->addColumn(Column("customer_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("order_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("type", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("status", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("next_delivery_no", DataType::INTEGER, 0, false, false, true, false, "0"));
        table->addColumn(Column("note", DataType::TEXT, 65535, false, false, false));

        table->addConstraint(new PrimaryKeyConstraint("pk_inbound_transaction", "inbound_transaction_id"));
        table->addConstraint(new UniqueConstraint("uq_inbound_transaction_code", "inbound_code"));
        table->addConstraint(new ForeignKeyConstraint("fk_inbound_transaction_customer", "customer_id", "customer", "customer_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_inbound_transaction_type", "type", "inbound_type", "type"));
        table->addConstraint(new ForeignKeyConstraint("fk_inbound_transaction_status", "status", "inbound_status", "status"));
        table->createIndex("avl_tree_order_at", "order_at", IndexType::AVL);
        table->createIndex("hash_tree_type", "type", IndexType::HASH_TREE);
        table->createIndex("hash_tree_status", "status", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 25. INBOUND_DETAIL
    // ========================================================
    Table* createInboundDetailTable()
    {
        auto table = new Table("inbound_detail");
        table->addColumn(Column("inbound_detail_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("inbound_transaction_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("ordered_quantity", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("actual_quantity", DataType::INTEGER, 0, false, false, true, false, "0"));

        table->addConstraint(new PrimaryKeyConstraint("pk_inbound_detail", "inbound_detail_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_inbound_detail_txn", "inbound_transaction_id", "inbound_transaction", "inbound_transaction_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_inbound_detail_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 26. DELIVERY
    // ========================================================
    Table* createDeliveryTable()
    {
        auto table = new Table("delivery");
        table->addColumn(Column("delivery_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("delivery_code", DataType::CHAR, 16, false, true, true));
        table->addColumn(Column("inbound_transaction_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("expected_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("delivery_at", DataType::DATE, 0, false, false, false));
        table->addColumn(Column("status", DataType::VARCHAR, 10, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_delivery", "delivery_id"));
        table->addConstraint(new UniqueConstraint("uq_delivery_code", "delivery_code"));
        table->addConstraint(new ForeignKeyConstraint("fk_delivery_inbound_txn", "inbound_transaction_id", "inbound_transaction", "inbound_transaction_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_delivery_status", "status", "delivery_status", "status"));
        table->createIndex("avl_tree_delivery_at", "delivery_at", IndexType::AVL);
        table->createIndex("avl_tree_expected_at", "expected_at", IndexType::AVL);
        table->createIndex("hash_tree_status", "status", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 27. DELIVERY_DETAIL (From Schema JSON)
    // ========================================================
    Table* createDeliveryDetailTable()
    {
        auto table = new Table("delivery_detail");
        table->addColumn(Column("id", DataType::VARCHAR, 255, true, true, true));
        table->addColumn(Column("delivery_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("received_quantity", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("actual_quantity", DataType::INTEGER, 0, false, false, true, false, "0"));

        table->addConstraint(new PrimaryKeyConstraint("pk_delivery_detail", "id"));
        table->addConstraint(new ForeignKeyConstraint("fk_delivery_detail_delivery", "delivery_id", "delivery", "delivery_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_delivery_detail_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 28. OUTBOUND_TRANSACTION
    // ========================================================
    Table* createOutboundTransactionTable()
    {
        auto table = new Table("outbound_transaction");
        // Column header in CSV/JSON is outbound_trainsaction_id
        table->addColumn(Column("outbound_trainsaction_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("outbound_code", DataType::CHAR, 13, false, true, true));
        table->addColumn(Column("customer_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("order_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("type", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("status", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("next_ship_no", DataType::INTEGER, 0, false, false, true, false, "0"));
        table->addColumn(Column("note", DataType::TEXT, 65535, false, false, false));

        table->addConstraint(new PrimaryKeyConstraint("pk_outbound_transaction", "outbound_trainsaction_id"));
        table->addConstraint(new UniqueConstraint("uq_outbound_transaction_code", "outbound_code"));
        table->addConstraint(new ForeignKeyConstraint("fk_outbound_transaction_customer", "customer_id", "customer", "customer_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_outbound_transaction_type", "type", "outbound_type", "type"));
        table->addConstraint(new ForeignKeyConstraint("fk_outbound_transaction_status", "status", "outbound_status", "status"));
        table->createIndex("avl_tree_order_at", "order_at", IndexType::AVL);
        table->createIndex("hash_tree_type", "type", IndexType::HASH_TREE);
        table->createIndex("hash_tree_status", "status", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 29. OUTBOUND_DETAIL
    // ========================================================
    Table* createOutboundDetailTable()
    {
        auto table = new Table("outbound_detail");
        table->addColumn(Column("outbound_detail_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("outbound_transaction_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("ordered_quantity", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("actual_quantity", DataType::INTEGER, 0, false, false, true, false, "0"));

        table->addConstraint(new PrimaryKeyConstraint("pk_outbound_detail", "outbound_detail_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_outbound_detail_txn", "outbound_transaction_id", "outbound_transaction", "outbound_trainsaction_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_outbound_detail_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 30. SHIPMENT
    // ========================================================
    Table* createShipmentTable()
    {
        auto table = new Table("shipment");
        table->addColumn(Column("shipment_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("shipment_code", DataType::CHAR, 17, false, true, true));
        table->addColumn(Column("sell_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("expected_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("ship_at", DataType::DATE, 0, false, false, false));
        table->addColumn(Column("status", DataType::VARCHAR, 255, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_shipment", "shipment_id"));
        table->addConstraint(new UniqueConstraint("uq_shipment_code", "shipment_code"));
        table->addConstraint(new ForeignKeyConstraint("fk_shipment_outbound_txn", "sell_id", "outbound_transaction", "outbound_trainsaction_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_shipment_status", "status", "shipment_status", "status"));
        table->createIndex("avl_tree_ship_at", "ship_at", IndexType::AVL);
        table->createIndex("avl_tree_expected_at", "expected_at", IndexType::AVL);
        table->createIndex("hash_tree_status", "status", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 31. SHIPMENT_DETAIL (From Schema JSON)
    // ========================================================
    Table* createShipmentDetailTable()
    {
        auto table = new Table("shipment_detail");
        table->addColumn(Column("id", DataType::VARCHAR, 17, true, true, true));
        table->addColumn(Column("shipment_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("received_quantity", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("actual_quantity", DataType::INTEGER, 0, false, false, true, false, "0"));

        table->addConstraint(new PrimaryKeyConstraint("pk_shipment_detail", "id"));
        table->addConstraint(new ForeignKeyConstraint("fk_shipment_detail_shipment", "shipment_id", "shipment", "shipment_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_shipment_detail_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 32. STATUS_HISTORY
    // ========================================================
    Table* createStatusHistoryTable()
    {
        auto table = new Table("status_history");
        table->addColumn(Column("history_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("unit_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("old_status", DataType::VARCHAR, 255, false, false, false)); // Nullable on first appearance
        table->addColumn(Column("curr_status", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("transaction", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("type", DataType::VARCHAR, 255, false, false, true)); // INBOUND / OUTBOUND

        table->addConstraint(new PrimaryKeyConstraint("pk_status_history", "history_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_status_history_unit", "unit_id", "unit_inventory", "unit_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_status_history_curr_status", "curr_status", "unit_status", "status"));
        table->createIndex("hash_tree_type", "type", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // SPECIFICATION SUB-TABLE FACTORY (Rule 6 & Rule 10.4)
    // ========================================================
    Table* createSpecTable(const string& specName)
    {
        auto table = new Table(specName);
        table->addColumn(Column("id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("spec", DataType::VARCHAR, 255, false, false, true));
        table->addConstraint(new PrimaryKeyConstraint("pk_" + specName, "id"));
        return table;
    }

    // ========================================================
    // SCHEMA MANAGEMENT & HELPERS
    // ========================================================

    Table* createTable(const string& tableName)
    {
        // Level 1: Independent
        if (tableName == "category") return createCategoryTable();
        if (tableName == "brand") return createBrandTable();
        if (tableName == "unit_status") return createUnitStatusTable();
        if (tableName == "inbound_type") return createInboundTypeTable();
        if (tableName == "inbound_status") return createInboundStatusTable();
        if (tableName == "delivery_status") return createDeliveryStatusTable();
        if (tableName == "outbound_type") return createOutboundTypeTable();
        if (tableName == "outbound_status") return createOutboundStatusTable();
        if (tableName == "shipment_status") return createShipmentStatusTable();
        if (tableName == "customer") return createCustomerTable();

        // Level 2
        if (tableName == "product") return createProductTable();
        if (tableName == "individual_info") return createIndividualInfoTable();
        if (tableName == "business_info") return createBusinessInfoTable();
        if (tableName == "inbound_transaction") return createInboundTransactionTable();
        if (tableName == "outbound_transaction") return createOutboundTransactionTable();

        // Level 3
        if (tableName == "product_quantity") return createProductQuantityTable();
        if (tableName == "cpu") return createCpuTable();
        if (tableName == "gpu") return createGpuTable();
        if (tableName == "mainboard") return createMainboardTable();
        if (tableName == "ram") return createRamTable();
        if (tableName == "ssd") return createSsdTable();
        if (tableName == "psu") return createPsuTable();
        if (tableName == "cooling") return createCoolingTable();
        if (tableName == "case") return createCaseTable();
        if (tableName == "unit_inventory") return createUnitInventoryTable();
        if (tableName == "inbound_detail") return createInboundDetailTable();
        if (tableName == "delivery") return createDeliveryTable();
        if (tableName == "outbound_detail") return createOutboundDetailTable();
        if (tableName == "shipment") return createShipmentTable();

        // Level 4
        if (tableName == "delivery_detail") return createDeliveryDetailTable();
        if (tableName == "shipment_detail") return createShipmentDetailTable();
        if (tableName == "status_history") return createStatusHistoryTable();

        return nullptr;
    }

    Vector<string> getTableNames()
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
            "outbound_status",
            "shipment_status",
            "customer",

            // Level 2: Depends on Level 1
            "product",
            "individual_info",
            "business_info",
            "inbound_transaction",
            "outbound_transaction",

            // Level 3: Depends on Level 2
            "product_quantity",
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
            "delivery_detail",
            "shipment_detail",
            "status_history"
        };
    }

    Vector<Table*> createAllTables()
    {
        Vector<Table*> allTables;
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
