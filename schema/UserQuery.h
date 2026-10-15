#ifndef USERQUERY_H
#define USERQUERY_H

#include "database/Database.h"
#include "database/Record.h"
#include "datastructure/vector/Vector.h"

namespace user_query {
    // Query for interface
    void product_query(Database *db, Vector<Record> &results, bool is_sold);
    void product_quantity_query(Database *db, Vector<Record> &results);
    void unit_inventory_query(Database *db, Vector<Record> &results);
    void status_history_query(Database *db, Vector<Record> &results);
    void shipment_detail_query(Database *db, Vector<Record> &results);
    void outbound_transaction_query(Database *db, Vector<Record> &results);
    void outbound_detail_query(Database *db, Vector<Record> &results);
    void delivery_detail_query(Database *db, Vector<Record> &results);
    void inbound_transaction_query(Database *db, Vector<Record> &results);
    void inbound_detail_query(Database *db, Vector<Record> &results);
}

#endif
