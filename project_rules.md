# System Rules

## 1. Language and Core Implementation

- Use **C++17** only (Python is permitted exclusively for the data crawler).
- Do **not** use any external database engine.
- Do **not** use C++ built-in data structures for core database/storage implementation (exception: `std::sort` is allowed on pass-by-value `Vector<Record>` in query processing).

---

## 2. Data Sources and File Organization

### 2.1 General Data & Schema
- The database schema is defined by `QuanLyKhoHang_schema.json` and must strictly follow all notes, constraints, and relationships specified in that file.
- CSV data is stored in the `data/` folder.
- Queries used for reporting are stored in the `report/` folder.
- Query tables are currently out of scope, but they must remain available for testing.
- File organization must not alter table names, column names, keys, relationships, or schema definitions.

### 2.2 Domain-Based Folder Structure
Project files must be organized by the business domain of the corresponding tables:

- **`transactions/`**: Contains all inbound and outbound transaction-related tables:
  - All inbound transactions
  - All outbound transactions
  - `Delivery`
  - `Shipment`
- **`unit/`**: Contains unit-level inventory data:
  - `status_history`
  - `unit_inventory`
- **`products/`**: Contains product and product-reference tables:
  - `product_quantity`, `product`, `Brand`, `Category`
  - Category tables: `CPU`, `Mainboard`, `GPU`, `RAM`, `SSD`, `PSU`, `Cooling`, `Case`
- **`<category>_specs/`**: Each category must have its own specification folder (e.g., `ssd_specs/`):
  - Contains specification sub-tables belonging exclusively to that category (e.g., `type`, `gen`, `capacity`).
  - Each sub-table contains exactly two columns: `id` (key) and `spec` (value).
- **`customers/`**: Contains customer-related tables:
  - `customer`, `individual_infor`, `business_info`
- **`status_type/`**: Contains all master status and type tables:
  - Status tables, Type tables, and `UnitStatus` master data (if represented as a separate table).

---

## 3. Database Protection and Referential Integrity

- Tables, columns, CSV files, and report files must **never be deleted**.
- A record must **never be deleted** if it is referenced by, or has an active relationship/dependency with, another table.
- Every transaction and inventory record must satisfy all schema constraints and form a logically valid business chain from the moment it is created or imported.

---

## 4. User Permissions and CRUD Rules

The main program uses a **single normal user role** (no `ADMIN` role or account management required).

### 4.1 Direct Creation Allowed (Products & Brands)
Users may add new records directly to:
- `Products` (must select an existing `Category`, `Brand`, and valid predefined specification values)
- `CPU`, `GPU`, `Mainboard`, `RAM`, `SSD`, `PSU`, `Cooling`, `Case`
- `Brand`
- Allowed values for specification fields (where permitted by the schema and business rules).

### 4.2 Coupled Creation Only (Transactions, Delivery, Shipment, Unit Transitions)
Users are allowed to manually input transactions and logistics records, but **strictly under atomic pairwise constraints**:
- **Inbound Pair**: Creating an Inbound Transaction requires simultaneously creating its fixed initial set of `Delivery` records.
- **Outbound Pair**: Creating an Outbound Transaction requires simultaneously creating its fixed initial set of `Shipment` records.
- **Unit Transition Mapping**: Within each `Delivery` or `Shipment` being created, the user may attach specific `Unit`s and select the target status for each unit to transition into.
- Standalone creation of an Inbound/Outbound order without its paired `Delivery`/`Shipment` (or vice versa), or standalone creation of `Unit` records via normal CRUD, is strictly prohibited.

### 4.3 Strict Immutability After Confirmation (Update & Delete Restrictions)
Once a Transaction + `Delivery`/`Shipment` pair (along with its attached `Unit` transitions) is confirmed:
- **No Deletion**: Confirmed transactions, deliveries, shipments, and unit mappings must **never be deleted**.
- **Status-Only Updates**: Users may modify **only the `status` field** of Transactions, `Delivery`, and `Shipment` during lifecycle processing.
- **Frozen Business Data**: All other fields (quantities, product references, transaction types, partner details, delivery/shipment contents, and attached unit transition lists) are permanently locked and cannot be edited.

### 4.4 Read-Only Master Data & Direct Unit Restrictions
Users must **never** directly edit:
- Any `Status` or `Type` master table.
- `UnitStatus` master values.
- `Unit.status` directly through the `Unit` table (a unit's status may only change as a controlled side effect of processing/confirming a `Delivery` or `Shipment`).

---

## 5. Transaction, Delivery, Shipment, and Unit Lifecycle

### 5.1 Sources of Transactions
- Transactions, `Delivery`, and `Shipment` records may originate either from:
  1. Manual pairwise user input via the main program (following Section 4.2).
  2. External sources or the auxiliary generator program.
- Regardless of origin, once recorded/confirmed, all non-status business data is immutable. If an external or confirmed record contains an error, corrections must be handled via new compensating/replacement transactions according to business flow, never by mutating historical fields.

### 5.2 Unit Creation and Status History
- Units may only be created or transitioned as a consequence of valid transaction, `Delivery`, or `Shipment` workflows.
- Every unit status change triggered by a `Delivery` or `Shipment` must have a valid previous state and target state, and must automatically record a chronologically consistent entry in `status_history`.

---

## 6. Indexing and Query Architecture

### 6.1 Index Naming and Selection
- Index naming convention: `index_type + index_column` (where `index_type` is `hash`, `hash_tree`, or `avl_tree`).
  - `hash`: For columns with unique values (often handled implicitly by primary/unique constraints).
  - `hash_tree`: For columns with a fixed set of values and many duplicates (e.g., category specs like CPU socket, `product.is_sold`, status, type).
  - `avl_tree`: For date columns.

### 6.2 Query Execution (`namespace Query`)
- Query logic resides in the `query/` library and is implemented in `schema/UserQuery.*`.
- Table data is treated as read-only during queries: extract required data from the table (using indexes where applicable) into a `Vector<Record>` before processing.
- Datatype validation is not required inside `UserQuery`; simply check if the `Vector<Record>` contains the requested rows (return an empty `Vector` if not).
- Users filter by one column at a time. Store the resulting `Vector<Record>` to support subsequent column sorting and avoid redundant query execution.
- **Cache Invalidation**: Any `update` or `delete` operation must trigger a dirty flag, requiring active queries to rebuild.

### 6.3 Pre-built Aggregate & Sort Functions
- Provide 3 pre-built operations on `Vector<Record>` for both single columns and column pairs: `sum`, `count`, and `sort`.
- **`sort`**: Sort directly on a pass-by-value `Vector<Record>`. Permitted to use C++ built-in `std::sort`.
- **`sum` & `count`**: Must be implemented using custom `HashTable`.

---

## 7. UI Rules (Qt Widgets)

### 7.1 Architecture & Separation of Concerns
- Use **Qt Widgets** with C++17; store all UI code in `ui/`.
- UI classes may freely use Qt containers/classes for presentation purposes, but must remain strictly separated from database storage, custom data structures, and core inventory business logic.
- Do not modify auto-generated Qt files (e.g., `ui_*.h`) directly.
- Preserve existing UI behavior when modifying interfaces unless explicitly instructed otherwise.

### 7.2 Navigation & Menus
- **Warehouse vs. Reference Separation**: All category tables (`CPU`, `GPU`, `Mainboard`, etc.), specification sub-tables, `Category`, and `Brand` must be separated from the main warehouse inventory view.
- Add a menu **`Danh mục sản phẩm`** containing all product category tables.
- Add a menu **`Danh mục phụ`** providing access to query specification tables, `Category`, and `Brand`.

### 7.3 Column Filtering & Input Controls
- Call `Query::filterRecords` to filter by the user-selected column:
  - **Primary Key / Unique**: Text input for exact hash lookup.
  - **Date**: Date-range picker (`from date` -> `to date`).
  - **Foreign Keys / Multi-duplicate Attributes** (e.g., category specs, `product.is_sold`, `type`, `status`): Allow selection from a dropdown/list (backed by `HashTree` where applicable) in addition to or instead of raw typing.
- Enforce valid predefined specification values at the UI level when adding products.

---

## 8. Auxiliary Tools

### 8.1 Transaction Data Generator
- **Scope**: Generates transaction-flow test data only (Transactions, `Delivery`, `Shipment`, `Unit`, and `status_history`). Must not generate master data or alter schema/table implementations.
- **Independence**: Generator logic must remain decoupled from internal database/table/query classes.
- **Console Workflow**: Provides a simple console UI allowing the user to select:
  - Transaction type (Inbound / Outbound)
  - Product(s) and quantities
  - Number of `Delivery` or `Shipment` records
- **Consistency**: Automatically generates valid unit transitions (selecting existing units randomly when needed) and chronological `status_history` entries that strictly satisfy `QuanLyKhoHang_schema.json` from creation without post-hoc repair.

### 8.2 Data Crawler
- May be implemented in **Python** to scrape product data into category-organized CSV files.
- Must collect category-specific specifications (not a generic spec set) and strictly adhere to all constraints and relationships in `QuanLyKhoHang_schema.json` so outputs can be cleanly imported without modifying existing schema or records.