# System Rules

## 1. Language and Implementation

- Use **C++17** only.
- Do **not** use any external database engine.
- Do **not** use C++ built-in data structures.

---

## 2. Data Sources and Project Structure

- The database schema is defined by `QuanLyKhoHang_schema.json` and must follow all notes, constraints, and relationships specified in that file.
- CSV data is stored in the `data` folder.
- Queries used for reporting are stored in the `report` folder.
- Query tables are currently out of scope, but they must remain available for testing.

---

## 3. Database and Schema Protection

- Tables and columns must **never be deleted**.
- CSV files must **never be deleted**.
- Report files must **never be deleted**.
- A record must not be deleted if it is referenced by, or has a relationship with, another table.

---

## 4. User Permissions

The main program uses a **single normal user role**. There is no `ADMIN` role and no account-management feature is required.

### 4.1 Users May Add

Users are allowed to add new records to the following tables:

- `Products`
- `CPU`
- `GPU`
- `Mainboard`
- `RAM`
- `SSD`
- `PSU`
- `Cooling`
- `Case`
- `Category`
- `Brand`

When adding a new `Products` record, the user must select an existing:

- `Category`
- `Brand`
- valid specification values defined by the system

### 4.2 Users May Edit

Users may modify **status fields only** for:

- Transactions
- `Delivery`
- `Shipment`

No other fields of these records may be modified through normal user operations.

### 4.3 Read-Only Master Data

The following data must not be edited through the main program:

- Any `Status` master table
- Any `Type` master table
- `UnitStatus` values
- Any externally generated transaction data except the status fields explicitly allowed above

### 4.4 Direct Creation Restrictions

The main program must **not** allow users to create the following records directly:

- Transactions
- `Delivery`
- `Shipment`
- `Unit`

These records must originate from an external source or an auxiliary program that simulates the corresponding external business system.

---

## 5. Transaction, Delivery, Shipment, and Unit Rules

### 5.1 External Source of Transactions

- Every inbound and outbound transaction must originate from an **external source**.
- `Delivery` and `Shipment` records must also originate from that external source or from an auxiliary program simulating the external system.
- The main program acts as the **inventory receiving and processing system**; it is not the source of business transactions.
- The main program must not provide normal CRUD functionality for creating new:
  - Transactions
  - `Delivery`
  - `Shipment`
  - `Unit`
- Direct external input and dedicated auxiliary programs are allowed to create these records in the database.
- The main program must not provide a mechanism for users to alter the business data received from an external source.
- In particular, transaction quantity, product references, transaction type, source references, delivery/shipment contents, and other source-defined fields are immutable after import.
- If the external source contains an error, the correction must come from the external source through a new corrected/replacement input according to the business process. The warehouse user must not create an internal adjustment to replace the external decision.

### 5.2 Transaction Status Processing

- A newly imported transaction, delivery, or shipment must enter the appropriate initial status defined by the schema/business rules.
- The warehouse user may update the **status only** while processing the received record.
- Confirming a transaction does not grant permission to edit its business data afterward.
- If a user confirms an incorrect record, the record itself must remain unchanged; correction must be provided by the external source.

### 5.3 Unit Creation and Status Changes

- Units may only be created as a consequence of valid transaction, delivery, or shipment input from the external source or an authorized auxiliary simulator.
- Units must not be created directly through the `Unit` table's normal CRUD interface.
- Unit status may only change through transaction processing.
- Users must not directly modify a unit's status value through the `Unit` table or the `UnitStatus` master table.
- `UnitStatus` values are fixed business values and are read-only.

### 5.4 Referential Integrity and Consistency

- Every transaction must have all required related records according to the schema, business rules, and relationships defined in `QuanLyKhoHang_schema.json`.
- A transaction, delivery, shipment, or unit must not be deleted when doing so would violate any existing relationship or dependency.
- All generated or imported inventory records must form a logically valid business chain.

---

## 6. Product Specification Input Rules

- Specification fields must accept only predefined valid values.
- The UI must enforce valid specification values for each product category, including CPU, Mainboard, RAM, GPU, SSD, PSU, Cooling, and Case.
- Users may manage the allowed values for each specification field through the UI where permitted by the schema and business rules.
- When entering a product specification, the user must only be able to select values valid for the corresponding field/category.
- UI-level validation must remain consistent with the constraints, notes, and relationships defined in `QuanLyKhoHang_schema.json`.

---

## 7. Generator Rules

The generator is responsible for generating **transaction-related test data only**. It must not be used as a general-purpose database generator.

### 7.1 Generator Scope

- The generator may generate data only for entities belonging to the transaction/inventory-flow process, together with the status-history records required by those transactions.
- The generator must not generate unrelated master/reference data.
- The generator must not modify or delete existing schema, database structure, query definitions, or join-table definitions.

### 7.2 Generator Independence

- Generator logic must be independent of the database implementation, table implementation, query implementation, and join-table implementation.
- The generator must only create data required for the selected transaction scenario.
- The generator must respect all constraints, notes, and relationships defined in `QuanLyKhoHang_schema.json`.

### 7.3 Transaction Generation Workflow

- The generator must provide a simple **console interface**.
- The user must be able to choose:
  - Transaction type (inbound or outbound, as supported by the schema)
  - Product(s) involved
  - Quantity for each selected product
  - Number of required `Delivery` records
  - Number of required `Shipment` records
- From these selections, the generator must create all required related transaction records and the corresponding unit/status-history data.
- When necessary, the generator may select existing units randomly from the database, but the resulting state transitions must remain logically valid.

### 7.4 Status History Generation

- Status history must be generated together with the transaction flow that causes it.
- Every generated unit status transition must have a valid previous state and new state according to the business rules.
- The generated status history must be chronologically and logically consistent with the related transaction, delivery, shipment, and unit records.
- The generator must not create impossible or contradictory inventory states merely to increase the amount of test data.

### 7.5 Data Validity

- Generated data must satisfy all schema constraints and relationships from the moment it is generated.
- The generator must not generate invalid data first and repair it afterward.
- Generated transactions must be consistent with the selected products, quantities, deliveries/shipments, units, and status history.

---

## 8. Data Crawler Rules

- The data crawler may be implemented in **Python**.
- The crawler may collect product information from websites and write the collected data to CSV files.
- Crawled data must be organized according to the product categories used by the system.
- The crawler must collect specifications appropriate to each category rather than using one generic specification set for all products.
- The crawler must respect all constraints, notes, and relationships defined in `QuanLyKhoHang_schema.json`.
- CSV output must contain data that can be imported and validated by the main system without violating schema or business rules.
- The crawler must not modify or delete the database schema or existing database records unless explicitly required by a separate import process.

## 9. UI Rules
- Use Qt Widgets with C++17.
- Keep UI code separate from database, custom data structures, and business logic.
- UI code can be saved in `ui/`.
- UI may freely use Qt classes and containers for UI-related purposes.
- Do not move database logic, inventory rules, or core application data management into UI classes.
- When modifying an existing UI, preserve existing behavior unless the change explicitly requires otherwise.
- Do not modify generated Qt files such as `ui_*.h` directly when using Qt Designer.
