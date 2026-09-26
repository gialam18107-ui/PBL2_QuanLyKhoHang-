# Rules

## 1. Technical
- C++17 only.
- No external database engines.
- No built-in data structures.
- Schema is defined by `QuanLyKhoHang_schema.json` and saved in `schema/`.
- CSV data is stored in `data/`.
- Report queries are stored in `report/`.

## 2. Data Protection
- Tables and columns cannot be deleted.
- CSV and report files cannot be deleted.
- A record cannot be deleted if referenced by another table.

## 3. User Permissions
### Can add
- Products, CPU, GPU, Mainboard, RAM, SSD, PSU, Cooling, Case
- Category, Brand

When adding a product, the user must select an existing category, brand, and valid spec.

### Can edit
- Transaction, Delivery, and Shipment status only.

### Cannot edit
- Any status/type master table.
- UnitStatus values.
- Unit status directly.

### Cannot create directly in main program
- Transactions
- Deliveries
- Shipments

These may only be created through direct external input or auxiliary programs simulating external sales/purchasing systems.

## 4. Inventory Rules
- Units can only be created through Transactions, Deliveries, or Shipments.
- Unit status can only change through Transactions.
- If a record has dependencies, it cannot be deleted.
- Spec fields must accept only predefined valid values.
- Users may manage the allowed values for each spec field through the UI.

## 5. Reports
- Query tables are currently out of scope but must remain available for testing.

## 6. Generator
- Generator only generates data; it must not modify or delete schema, database, tables, queries, or join tables.
- Generator must respect all constraints, notes, and relationships in `QuanLyKhoHang_schema.json`.
- Normal tables may be generated record-by-record.
- Units, inventory status, transactions, deliveries, shipments, inbound transactions, and outbound transactions must be generated sequentially and consistently.
- Related records must be generated together.
  - Example: InboundTransaction → InboundDetail → Delivery → DeliveryDetail → Units → old/new statuses.