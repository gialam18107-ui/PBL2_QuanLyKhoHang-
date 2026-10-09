#include "UserQuery.h"
#include "database/Condition.h"
#include "database/Join.h"
#include "database/Query.h"

void user_query::product_query(Database *db, Vector<JoinedRecord> &results, bool is_sold){
    Query q(*db);
    q.from("product")
        .select({"product.product_code", "product.product_name", "product.sku", "product.barcode"})
        .where(Condition("product.is_sold", CompareOperator::EQUAL, is_sold));
    results = q.execute();
}

void user_query::product_quantity_query(Database *db, Vector<JoinedRecord> &results){
    Query q(*db);
    q.from("product_quantity")
        .join(Join("product_quantity", "product_id", "product", "product_id", JoinType::INNER))
        .select({
            "product.product_code",
            "product.product_name",
            "product_quantity.available_quan",
            "product_quantity.warranty_quan",
            "product_quantity.scrapped_quan"
        })
        .where(Condition("product.is_sold", CompareOperator::EQUAL, true));
    results = q.execute();
}

void user_query::unit_inventory_query(Database *db, Vector<JoinedRecord> &results){
    Query q(*db);
    q.from("unit_inventory")
        .join(Join("unit_inventory", "product_id", "product", "product_id", JoinType::INNER))
        .select({
            "product.product_name",
            "unit_inventory.serial",
            "unit_inventory.status"
        });
    results = q.execute();
}

void user_query::status_history_query(Database *db, Vector<JoinedRecord> &results){
    Query q(*db);
    q.from("status_history")
        .join(Join("status_history", "unit_id", "unit_inventory", "unit_id", JoinType::INNER))
        .select({
            "unit_inventory.serial",
            "status_history.old_status",
            "status_history.curr_status",
            "status_history.transaction",
            "status_history.type"
        });
    results = q.execute();
}

void user_query::shipment_detail_query(Database *db, Vector<JoinedRecord> &results){
    Query q(*db);
    q.from("shipment_detail")
        .join(Join("shipment_detail", "shipment_id", "shipment", "shipment_id", JoinType::INNER))
        .join(Join("shipment_detail", "product_id", "product", "product_id", JoinType::INNER))
        .select({
            "shipment.shipment_code",
            "product.product_code",
            "product.product_name",
            "shipment_detail.received_quantity"
        });
    results = q.execute();
}

void user_query::outbound_transaction_query(Database *db, Vector<JoinedRecord> &results){
    Query q(*db);
    q.from("outbound_transaction")
        .join(Join("outbound_transaction", "customer_id", "customer", "customer_id", JoinType::INNER))
        .select({
            "outbound_transaction.outbound_code",
            "customer.customer_code",
            "outbound_transaction.order_at",
            "outbound_transaction.type",
            "outbound_transaction.status",
            "outbound_transaction.note"
        });
    results = q.execute();
}

void user_query::outbound_detail_query(Database *db, Vector<JoinedRecord> &results){
    Query q(*db);
    q.from("outbound_detail")
        .join(Join("outbound_detail", "outbound_transaction_id", "outbound_transaction", "outbound_trainsaction_id", JoinType::INNER))
        .join(Join("outbound_detail", "product_id", "product", "product_id", JoinType::INNER))
        .select({
            "outbound_transaction.outbound_code",
            "product.product_code",
            "outbound_detail.ordered_quantity"
        });
    results = q.execute();
}

void user_query::delivery_detail_query(Database *db, Vector<JoinedRecord> &results){
    Query q(*db);
    q.from("delivery_detail")
        .join(Join("delivery_detail", "delivery_id", "delivery", "delivery_id", JoinType::INNER))
        .join(Join("delivery_detail", "product_id", "product", "product_id", JoinType::INNER))
        .select({
            "delivery.delivery_code",
            "product.product_code",
            "product.product_name",
            "delivery_detail.received_quantity"
        });
    results = q.execute();
}

void user_query::inbound_transaction_query(Database *db, Vector<JoinedRecord> &results){
    Query q(*db);
    q.from("inbound_transaction")
        .join(Join("inbound_transaction", "customer_id", "customer", "customer_id", JoinType::INNER))
        .select({
            "inbound_transaction.inbound_code",
            "customer.customer_code",
            "inbound_transaction.order_at",
            "inbound_transaction.type",
            "inbound_transaction.status",
            "inbound_transaction.note"
        });
    results = q.execute();
}

void user_query::inbound_detail_query(Database *db, Vector<JoinedRecord> &results){
    Query q(*db);
    q.from("inbound_detail")
        .join(Join("inbound_detail", "inbound_transaction_id", "inbound_transaction", "inbound_transaction_id", JoinType::INNER))
        .join(Join("inbound_detail", "product_id", "product", "product_id", JoinType::INNER))
        .select({
            "inbound_transaction.inbound_code",
            "product.product_code",
            "inbound_detail.ordered_quantity"
        });
    results = q.execute();
}