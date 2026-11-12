#include "Schema.h"
#include "database/Column.h"
#include "database/Constraint.h"

#include <cctype>
#include <filesystem>

namespace Schema
{
    // ========================================================
    // 1. CATEGORY
    // ========================================================
    Table* createCategoryTable()
    {
        auto table = new Table("category");
        table->addColumn(Column("category_id", DataType::INTEGER, 0, true, true, true, true));
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
    // 11. CHECK_TYPE (From Schema JSON)
    // ========================================================
    Table* createCheckTypeTable()
    {
        auto table = new Table("check_type");
        table->addColumn(Column("type_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("check_type", DataType::VARCHAR, 10, false, true, true));
        table->addColumn(Column("next_code_no", DataType::INTEGER, 0, false, false, true, false, "0"));

        table->addConstraint(new PrimaryKeyConstraint("pk_check_type", "type_id"));
        table->addConstraint(new UniqueConstraint("uq_check_type", "check_type"));
        return table;
    }

    // ========================================================
    // 12. INVENTORY_CHECK_STATUS (From Schema JSON)
    // ========================================================
    Table* createInventoryCheckStatusTable()
    {
        auto table = new Table("inventory_check_status");
        table->addColumn(Column("status_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, true, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_inventory_check_status", "status_id"));
        table->addConstraint(new UniqueConstraint("uq_inventory_check_status", "status"));
        return table;
    }

    // ========================================================
    // 13. REASON (From Schema JSON)
    // ========================================================
    Table* createReasonTable()
    {
        auto table = new Table("reason");
        table->addColumn(Column("reason_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("reason", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("code", DataType::VARCHAR, 10, false, true, true));
        table->addColumn(Column("next_code_no", DataType::INTEGER, 0, false, false, false, false, "0"));

        table->addConstraint(new PrimaryKeyConstraint("pk_reason", "reason_id"));
        table->addConstraint(new UniqueConstraint("uq_reason", "reason"));
        table->addConstraint(new UniqueConstraint("uq_reason_code", "code"));
        return table;
    }

    // ========================================================
    // 14. ADJUSTMENT_STATUS (From Schema JSON)
    // ========================================================
    Table* createAdjustmentStatusTable()
    {
        auto table = new Table("adjustment_status");
        table->addColumn(Column("status_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, true, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_adjustment_status", "status_id"));
        table->addConstraint(new UniqueConstraint("uq_adjustment_status", "status"));
        return table;
    }

    // ========================================================
    // 15. INDIVIDUAL_INFO
    // ========================================================
    Table* createIndividualInfoTable()
    {
        auto table = new Table("individual_info");
        table->addColumn(Column("customer_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("phone_number", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("email", DataType::VARCHAR, 100, false, true, false));
        table->addColumn(Column("next_code_no", DataType::INTEGER, 0, false, false, true, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_individual_info", "customer_id"));
        table->addConstraint(new UniqueConstraint("uq_individual_phone", "phone_number"));
        table->addConstraint(new UniqueConstraint("uq_individual_email", "email"));
        table->addConstraint(new ForeignKeyConstraint("fk_individual_customer", "customer_id", "customer", "customer_id"));
        return table;
    }

    // ========================================================
    // 16. BUSINESS_INFO
    // ========================================================
    Table* createBusinessInfoTable()
    {
        auto table = new Table("business_info");
        table->addColumn(Column("customer_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("phone_number", DataType::VARCHAR, 50, false, true, false));
        table->addColumn(Column("email", DataType::VARCHAR, 100, false, true, false));
        table->addColumn(Column("tax_code", DataType::VARCHAR, 50, false, true, true));
        table->addColumn(Column("address", DataType::TEXT, 0, false, false, true));
        table->addColumn(Column("next_code_no", DataType::INTEGER, 0, false, false, true, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_business_info", "customer_id"));
        table->addConstraint(new UniqueConstraint("uq_business_phone", "phone_number"));
        table->addConstraint(new UniqueConstraint("uq_business_email", "email"));
        table->addConstraint(new UniqueConstraint("uq_business_tax_code", "tax_code"));
        table->addConstraint(new ForeignKeyConstraint("fk_business_customer", "customer_id", "customer", "customer_id"));
        return table;
    }

    // ========================================================
    // 17. PRODUCT (Contains available_quantity, warranty_quantity, scrapped_quantity, is_sold)
    // ========================================================
    Table* createProductTable()
    {
        auto table = new Table("product");
        table->addColumn(Column("product_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("product_name", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("product_code", DataType::VARCHAR, 20, false, true, true));
        table->addColumn(Column("barcode", DataType::VARCHAR, 13, false, true, true));
        table->addColumn(Column("sku", DataType::VARCHAR, 20, false, true, true));
        table->addColumn(Column("brand", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("category", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("available_quantity", DataType::INTEGER, 0, false, false, true, false, "0"));
        table->addColumn(Column("warranty_quantity", DataType::INTEGER, 0, false, false, true, false, "0"));
        table->addColumn(Column("scrapped_quantity", DataType::INTEGER, 0, false, false, true, false, "0"));
        table->addColumn(Column("is_sold", DataType::BOOLEAN, 0, false, false, true, false, "1"));

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
    // 18. IS_SOLD_CHANGING (From Schema JSON)
    // ========================================================
    Table* createIsSoldChangingTable()
    {
        auto table = new Table("is_sold_changing");
        table->addColumn(Column("history_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("date", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("old_status", DataType::BOOLEAN, 0, false, false, true));
        table->addColumn(Column("new_status", DataType::BOOLEAN, 0, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_is_sold_changing", "history_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_is_sold_changing_product", "product_id", "product", "product_id"));
        table->createIndex("avl_tree_date", "date", IndexType::AVL);
        return table;
    }

    // ========================================================
    // 20. CPU
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
    // 21. GPU
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
    // 22. MAINBOARD
    // ========================================================
    Table* createMainboardTable()
    {
        auto table = new Table("mainboard");
        table->addColumn(Column("mainboard_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("cpu_support", DataType::VARCHAR, 50, false, false, true));
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
    // 23. RAM
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
    // 24. SSD
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
    // 25. PSU
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
    // 26. COOLING
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
    // 27. CASE
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
    // 28. UNIT_INVENTORY
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
    // 29. INVENTORY_CHECK (From Schema JSON)
    // ========================================================
    Table* createInventoryCheckTable()
    {
        auto table = new Table("inventory_check");
        table->addColumn(Column("check_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("check_code", DataType::VARCHAR, 20, false, true, true));
        table->addColumn(Column("check_type", DataType::VARCHAR, 10, false, false, true));
        table->addColumn(Column("check_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("status", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("note", DataType::TEXT, 65535, false, false, false));

        table->addConstraint(new PrimaryKeyConstraint("pk_inventory_check", "check_id"));
        table->addConstraint(new UniqueConstraint("uq_inventory_check_code", "check_code"));
        table->addConstraint(new ForeignKeyConstraint("fk_inventory_check_type", "check_type", "check_type", "check_type"));
        table->addConstraint(new ForeignKeyConstraint("fk_inventory_check_status", "status", "inventory_check_status", "status"));
        table->createIndex("avl_tree_check_at", "check_at", IndexType::AVL);
        table->createIndex("hash_tree_status", "status", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 30. INBOUND_TRANSACTION (Includes source_code, nullable customer_id)
    // ========================================================
    Table* createInboundTransactionTable()
    {
        auto table = new Table("inbound_transaction");
        table->addColumn(Column("inbound_transaction_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("inbound_code", DataType::VARCHAR, 20, false, true, true));
        table->addColumn(Column("source_code", DataType::VARCHAR, 50, false, true, false)); // nullable, unique
        table->addColumn(Column("customer_id", DataType::INTEGER, 0, false, false, false)); // nullable for ADJ
        table->addColumn(Column("order_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("type", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("status", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("next_delivery_no", DataType::INTEGER, 0, false, false, true, false, "0"));
        table->addColumn(Column("note", DataType::TEXT, 65535, false, false, false));

        table->addConstraint(new PrimaryKeyConstraint("pk_inbound_transaction", "inbound_transaction_id"));
        table->addConstraint(new UniqueConstraint("uq_inbound_transaction_code", "inbound_code"));
        table->addConstraint(new UniqueConstraint("uq_inbound_source_code", "source_code"));
        table->addConstraint(new ForeignKeyConstraint("fk_inbound_transaction_customer", "customer_id", "customer", "customer_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_inbound_transaction_type", "type", "inbound_type", "type"));
        table->addConstraint(new ForeignKeyConstraint("fk_inbound_transaction_status", "status", "inbound_status", "status"));
        table->createIndex("avl_tree_order_at", "order_at", IndexType::AVL);
        table->createIndex("hash_tree_type", "type", IndexType::HASH_TREE);
        table->createIndex("hash_tree_status", "status", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 31. INBOUND_DETAIL
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
    // 32. DELIVERY
    // ========================================================
    Table* createDeliveryTable()
    {
        auto table = new Table("delivery");
        table->addColumn(Column("delivery_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("delivery_code", DataType::VARCHAR, 20, false, true, true));
        table->addColumn(Column("inbound_transaction_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("expected_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("delivery_at", DataType::DATE, 0, false, false, false));
        table->addColumn(Column("status", DataType::VARCHAR, 20, false, false, true));

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
    // 33. DELIVERY_DETAIL
    // ========================================================
    Table* createDeliveryDetailTable()
    {
        auto table = new Table("delivery_detail");
        table->addColumn(Column("delivery_detail_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("delivery_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("received_quantity", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("actual_quantity", DataType::INTEGER, 0, false, false, true, false, "0"));

        table->addConstraint(new PrimaryKeyConstraint("pk_delivery_detail", "delivery_detail_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_delivery_detail_delivery", "delivery_id", "delivery", "delivery_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_delivery_detail_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 34. OUTBOUND_TRANSACTION (Includes source_code, nullable customer_id)
    // ========================================================
    Table* createOutboundTransactionTable()
    {
        auto table = new Table("outbound_transaction");
        table->addColumn(Column("outbound_trainsaction_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("outbound_code", DataType::VARCHAR, 20, false, true, true));
        table->addColumn(Column("source_code", DataType::VARCHAR, 50, false, true, false)); // nullable, unique
        table->addColumn(Column("customer_id", DataType::INTEGER, 0, false, false, false)); // nullable for ADJ
        table->addColumn(Column("order_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("type", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("status", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("next_ship_no", DataType::INTEGER, 0, false, false, true, false, "0"));
        table->addColumn(Column("note", DataType::TEXT, 65535, false, false, false));

        table->addConstraint(new PrimaryKeyConstraint("pk_outbound_transaction", "outbound_trainsaction_id"));
        table->addConstraint(new UniqueConstraint("uq_outbound_transaction_code", "outbound_code"));
        table->addConstraint(new UniqueConstraint("uq_outbound_source_code", "source_code"));
        table->addConstraint(new ForeignKeyConstraint("fk_outbound_transaction_customer", "customer_id", "customer", "customer_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_outbound_transaction_type", "type", "outbound_type", "type"));
        table->addConstraint(new ForeignKeyConstraint("fk_outbound_transaction_status", "status", "outbound_status", "status"));
        table->createIndex("avl_tree_order_at", "order_at", IndexType::AVL);
        table->createIndex("hash_tree_type", "type", IndexType::HASH_TREE);
        table->createIndex("hash_tree_status", "status", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 35. OUTBOUND_DETAIL
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
    // 36. SHIPMENT
    // ========================================================
    Table* createShipmentTable()
    {
        auto table = new Table("shipment");
        table->addColumn(Column("shipment_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("shipment_code", DataType::VARCHAR, 20, false, true, true));
        table->addColumn(Column("outbound_transaction_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("expected_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("ship_at", DataType::DATE, 0, false, false, false));
        table->addColumn(Column("status", DataType::VARCHAR, 255, false, false, true));

        table->addConstraint(new PrimaryKeyConstraint("pk_shipment", "shipment_id"));
        table->addConstraint(new UniqueConstraint("uq_shipment_code", "shipment_code"));
        table->addConstraint(new ForeignKeyConstraint("fk_shipment_outbound_txn", "outbound_transaction_id", "outbound_transaction", "outbound_trainsaction_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_shipment_status", "status", "shipment_status", "status"));
        table->createIndex("avl_tree_ship_at", "ship_at", IndexType::AVL);
        table->createIndex("avl_tree_expected_at", "expected_at", IndexType::AVL);
        table->createIndex("hash_tree_status", "status", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 37. SHIPMENT_DETAIL
    // ========================================================
    Table* createShipmentDetailTable()
    {
        auto table = new Table("shipment_detail");
        table->addColumn(Column("shipment_detail_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("shipment_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("received_quantity", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("actual_quantity", DataType::INTEGER, 0, false, false, true, false, "0"));

        table->addConstraint(new PrimaryKeyConstraint("pk_shipment_detail", "shipment_detail_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_shipment_detail_shipment", "shipment_id", "shipment", "shipment_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_shipment_detail_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 38. STATUS_HISTORY
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
        table->createIndex("hash_tree_transaction", "transaction", IndexType::HASH_TREE);
        table->createIndex("hash_tree_type", "type", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 39. INVENTORY_CHECK_QUANTITY (From Schema JSON)
    // ========================================================
    Table* createInventoryCheckQuantityTable()
    {
        auto table = new Table("inventory_check_quantity");
        table->addColumn(Column("check_quantity_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("check_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("system_available_quan", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("system_warranty_quan", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("system_scrapped_quan", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("actual_available_quan", DataType::INTEGER, 0, false, false, true, false, "0"));
        table->addColumn(Column("actual_warranty_quan", DataType::INTEGER, 0, false, false, true, false, "0"));
        table->addColumn(Column("actual_scrapped_quan", DataType::INTEGER, 0, false, false, true, false, "0"));
        table->addColumn(Column("note", DataType::TEXT, 65535, false, false, false));

        table->addConstraint(new PrimaryKeyConstraint("pk_inv_check_quantity", "check_quantity_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_inv_check_quan_check", "check_id", "inventory_check", "check_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_inv_check_quan_product", "product_id", "product", "product_id"));
        return table;
    }

    // ========================================================
    // 40. INVENTORY_CHECK_UNIT (From Schema JSON)
    // ========================================================
    Table* createInventoryCheckUnitTable()
    {
        auto table = new Table("inventory_check_unit");
        table->addColumn(Column("check_unit_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("check_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("check_quantity_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("serial", DataType::VARCHAR, 100, false, false, true));
        table->addColumn(Column("system_status", DataType::VARCHAR, 50, false, false, false));
        table->addColumn(Column("scanned_status", DataType::VARCHAR, 50, false, false, false));

        table->addConstraint(new PrimaryKeyConstraint("pk_inv_check_unit", "check_unit_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_inv_check_unit_check", "check_id", "inventory_check", "check_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_inv_check_unit_quan", "check_quantity_id", "inventory_check_quantity", "check_quantity_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_inv_check_unit_sys_st", "system_status", "unit_status", "status"));
        table->addConstraint(new ForeignKeyConstraint("fk_inv_check_unit_scan_st", "scanned_status", "unit_status", "status"));
        table->createIndex("hash_tree_inv_check_unit_serial", "serial", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 41. ADJUSTMENT_MOVEMENT (From Schema JSON)
    // ========================================================
    Table* createAdjustmentMovementTable()
    {
        auto table = new Table("adjustment_movement");
        table->addColumn(Column("adjust_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("adjust_code", DataType::VARCHAR, 20, false, true, true));
        table->addColumn(Column("source_code", DataType::VARCHAR, 50, false, true, false)); // nullable, unique
        table->addColumn(Column("reason", DataType::VARCHAR, 50, false, false, true));
        table->addColumn(Column("adjusted_at", DataType::DATE, 0, false, false, true));
        table->addColumn(Column("status", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("note", DataType::TEXT, 65535, false, false, false));

        table->addConstraint(new PrimaryKeyConstraint("pk_adjustment_movement", "adjust_id"));
        table->addConstraint(new UniqueConstraint("uq_adjustment_code", "adjust_code"));
        table->addConstraint(new UniqueConstraint("uq_adjustment_source_code", "source_code"));
        table->addConstraint(new ForeignKeyConstraint("fk_adj_movement_reason", "reason", "reason", "reason"));
        table->addConstraint(new ForeignKeyConstraint("fk_adj_movement_status", "status", "adjustment_status", "status"));
        table->createIndex("avl_tree_adjusted_at", "adjusted_at", IndexType::AVL);
        table->createIndex("hash_tree_status", "status", IndexType::HASH_TREE);
        return table;
    }

    // ========================================================
    // 42. ADJUST_MOVEMENT_DETAIL (From Schema JSON)
    // ========================================================
    Table* createAdjustMovementDetailTable()
    {
        auto table = new Table("adjust_movement_detail");
        table->addColumn(Column("adjust_detail_id", DataType::INTEGER, 0, true, true, true, true));
        table->addColumn(Column("adjust_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("product_id", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("from_status", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("to_status", DataType::VARCHAR, 255, false, false, true));
        table->addColumn(Column("adjust_quantity", DataType::INTEGER, 0, false, false, true));
        table->addColumn(Column("actual_quantity", DataType::INTEGER, 0, false, false, true, false, "0"));

        table->addConstraint(new PrimaryKeyConstraint("pk_adjust_movement_detail", "adjust_detail_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_adj_mov_det_parent", "adjust_id", "adjustment_movement", "adjust_id"));
        table->addConstraint(new ForeignKeyConstraint("fk_adj_mov_det_product", "product_id", "product", "product_id"));
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
        table->createIndex("hash_tree_spec", "spec", IndexType::HASH_TREE);
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
        if (tableName == "check_type") return createCheckTypeTable();
        if (tableName == "inventory_check_status") return createInventoryCheckStatusTable();
        if (tableName == "reason") return createReasonTable();
        if (tableName == "adjustment_status") return createAdjustmentStatusTable();

        // Level 2: Depends on Level 1
        if (tableName == "product") return createProductTable();
        if (tableName == "individual_info") return createIndividualInfoTable();
        if (tableName == "business_info") return createBusinessInfoTable();
        if (tableName == "inventory_check") return createInventoryCheckTable();
        if (tableName == "inbound_transaction") return createInboundTransactionTable();
        if (tableName == "outbound_transaction") return createOutboundTransactionTable();
        if (tableName == "adjustment_movement") return createAdjustmentMovementTable();

        // Level 3: Depends on Level 2
        if (tableName == "is_sold_changing") return createIsSoldChangingTable();
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
        if (tableName == "inventory_check_quantity") return createInventoryCheckQuantityTable();
        if (tableName == "adjust_movement_detail") return createAdjustMovementDetailTable();

        // Level 4: Depends on Level 3
        if (tableName == "delivery_detail") return createDeliveryDetailTable();
        if (tableName == "shipment_detail") return createShipmentDetailTable();
        if (tableName == "status_history") return createStatusHistoryTable();
        if (tableName == "inventory_check_unit") return createInventoryCheckUnitTable();

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
            "check_type",
            "inventory_check_status",
            "reason",
            "adjustment_status",

            // Level 2: Depends on Level 1
            "product",
            "individual_info",
            "business_info",
            "inventory_check",
            "inbound_transaction",
            "outbound_transaction",
            "adjustment_movement",

            // Level 3: Depends on Level 2
            "is_sold_changing",
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
            "inventory_check_quantity",
            "adjust_movement_detail",

            // Level 4: Depends on Level 3
            "delivery_detail",
            "shipment_detail",
            "status_history",
            "inventory_check_unit"
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

    void initDatabase(Database& db, bool includeSpecs)
    {
        for (const string& name : getTableNames())
        {
            if (!db.hasTable(name))
            {
                auto tbl = createTable(name);
                if (tbl != nullptr)
                {
                    db.addTable(tbl);
                }
            }
        }

        if (includeSpecs)
        {
            for (const string& cat : getCategoriesWithSpecs())
            {
                for (const string& specName : getSpecNamesForCategory(cat))
                {
                    string dbTableName = cat + "_" + specName;
                    if (!db.hasTable(dbTableName))
                    {
                        db.addTable(createSpecTable(dbTableName));
                    }
                }
            }
        }
    }

    // ========================================================
    // DOMAIN & FILE PATH MAPPING (Warehouse Schema Layout)
    // ========================================================

    string getTableDomain(const string& tableName)
    {
        string name = tableName;
        for (char& c : name) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

        if (name == "inbound_transaction" || name == "inbound_detail" ||
            name == "delivery" || name == "delivery_detail" ||
            name == "outbound_transaction" || name == "outbound_detail" ||
            name == "shipment" || name == "shipment_detail")
        {
            return "transactions";
        }

        if (name == "status_history" || name == "unit_inventory")
        {
            return "unit";
        }

        if (name == "product" || name == "is_sold_changing" ||
            name == "cpu" || name == "mainboard" || name == "gpu" ||
            name == "ram" || name == "ssd" || name == "psu" ||
            name == "cooling" || name == "case" || name == "brand" || name == "category")
        {
            return "products";
        }

        if (name == "customer" || name == "individual_info" || name == "individual_infor" || name == "business_info")
        {
            return "customers";
        }

        if (name == "unit_status" || name == "inbound_type" || name == "inbound_status" ||
            name == "delivery_status" || name == "outbound_type" || name == "outbound_status" ||
            name == "shipment_status" || name == "inventory_check_status" ||
            name == "adjustment_status" || name == "check_type" || name == "reason")
        {
            return "status_type";
        }

        if (name == "inventory_check" || name == "inventory_check_quantity" ||
            name == "inventory_check_unit" || name == "adjustment_movement" ||
            name == "adjust_movement_detail")
        {
            return "inventory_check";
        }

        return "";
    }

    static string normalizeBaseFolder(const string& folderPath)
    {
        string base = folderPath;
        if (!base.empty() && base.back() != '/' && base.back() != '\\')
        {
            base += "/";
        }
        return base;
    }

    string getTableFolder(const string& tableName, const string& baseFolder)
    {
        string domain = getTableDomain(tableName);
        string base = normalizeBaseFolder(baseFolder);

        if (!domain.empty())
        {
            if (base.find(domain + "/") != string::npos || base.find(domain + "\\") != string::npos)
            {
                return baseFolder;
            }

            string domainFolder = base + domain;
            if (filesystem::exists(domainFolder + "/" + tableName + ".csv"))
            {
                return domainFolder;
            }

            if (domain == "unit" && filesystem::exists(base + "units/" + tableName + ".csv"))
            {
                return base + "units";
            }

            if (filesystem::exists(base + tableName + ".csv"))
            {
                return baseFolder;
            }

            return domainFolder;
        }

        return baseFolder;
    }

    string getTableFilePath(const string& tableName, const string& baseFolder)
    {
        return normalizeBaseFolder(getTableFolder(tableName, baseFolder)) + tableName + ".csv";
    }

    Vector<string> getDomains()
    {
        return {"transactions", "unit", "products", "customers", "status_type", "inventory_check"};
    }

    Vector<string> getTablesInDomain(const string& domain)
    {
        Vector<string> result;
        string dom = domain;
        for (char& c : dom) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

        for (const string& tableName : getTableNames())
        {
            if (getTableDomain(tableName) == dom)
            {
                result.push_back(tableName);
            }
        }
        return result;
    }

    Vector<string> getCategoriesWithSpecs()
    {
        return {"cpu", "gpu", "mainboard", "ram", "ssd", "psu", "cooling", "case"};
    }

    Vector<string> getSpecNamesForCategory(const string& category)
    {
        string cat = category;
        for (char& c : cat) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

        if (cat == "cpu") return {"socket", "tier", "series"};
        if (cat == "gpu") return {"series", "producer", "vram"};
        if (cat == "mainboard") return {"cpu_support", "socket", "size", "chipset", "ram_support"};
        if (cat == "ram") return {"generation", "quantity", "capacity", "bus"};
        if (cat == "ssd") return {"type", "gen", "capacity"};
        if (cat == "psu") return {"quality", "capacity", "size"};
        if (cat == "cooling") return {"type"};
        if (cat == "case") return {"size", "product_name"};
        return {};
    }

    string getSpecFilePath(const string& category, const string& specName, const string& baseFolder)
    {
        string cat = category;
        for (char& c : cat) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        return normalizeBaseFolder(baseFolder) + cat + "_specs/" + specName + ".csv";
    }

    void configureDatabaseIO(DataIO::DatabaseIO& dbIO, const string& baseFolder, bool includeSpecs)
    {
        for (const string& tableName : getTableNames())
        {
            dbIO.setTablePath(tableName, getTableFilePath(tableName, baseFolder));
        }

        if (includeSpecs)
        {
            for (const string& cat : getCategoriesWithSpecs())
            {
                for (const string& specName : getSpecNamesForCategory(cat))
                {
                    string dbTableName = cat + "_" + specName;
                    dbIO.setTablePath(dbTableName, getSpecFilePath(cat, specName, baseFolder));
                }
            }
        }
    }

    DataIO::DatabaseIO createDatabaseIO(const string& baseFolder, bool includeSpecs)
    {
        DataIO::DatabaseIO dbIO;
        configureDatabaseIO(dbIO, baseFolder, includeSpecs);
        return dbIO;
    }

    bool loadDatabase(Database& db, const string& baseFolder, bool includeSpecs)
    {
        initDatabase(db, includeSpecs);
        DataIO::DatabaseIO dbIO = createDatabaseIO(baseFolder, includeSpecs);
        return dbIO.readAll(db);
    }

    bool saveDatabase(const Database& db, const string& baseFolder, bool includeSpecs)
    {
        DataIO::DatabaseIO dbIO = createDatabaseIO(baseFolder, includeSpecs);
        return dbIO.writeAll(db);
    }
}
