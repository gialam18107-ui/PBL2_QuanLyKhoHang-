#ifndef USERQUERY_H
#define USERQUERY_H

#include "database/Database.h"
#include "database/JoinedRecord.h"
#include "database/Query.h"
#include "database/Vector.h"

namespace user_query{
    void product_query(Database *db, Vector<JoinedRecord> &results, bool is_sold);
    void product_quantity_query(Database *db, Vector<JoinedRecord> &results);
    void unit_inventory_query(Database *db, Vector<JoinedRecord> &results);
    void status_history_query(Database *db, Vector<JoinedRecord> &results);
    void shipment_detail_query(Database *db, Vector<JoinedRecord> &results);
    void outbound_transaction_query(Database *db, Vector<JoinedRecord> &results);
    void outbound_detail_query(Database *db, Vector<JoinedRecord> &results);
    void delivery_detail_query(Database *db, Vector<JoinedRecord> &results);
    void inbound_transaction_query(Database *db, Vector<JoinedRecord> &results);
    void inbound_detail_query(Database *db, Vector<JoinedRecord> &results);
}

#endif