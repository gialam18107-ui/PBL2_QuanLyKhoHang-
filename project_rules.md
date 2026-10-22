# System Rules

## 1. Language and Core Implementation

- Use **C++17** only (Python is permitted exclusively for the data crawler).
- Do **not** use any external database engine.
- Do **not** use C++ built-in container data structures (`std::vector`, `std::map`, `std::unordered_map`, `std::list`, etc.) or smart pointers (`std::shared_ptr`, `std::unique_ptr`) for core database/storage implementation (`std::string`, `std::variant`, and basic I/O streams are permitted).
- UI classes in `ui/` may freely use Qt containers and presentation classes, strictly isolated from database core logic.
- Exception for algorithms: `std::sort` is permitted to perform in-place sorting on the cached query result `Vector<Record>&` during presentation.

---

## 2. Data Sources and File Organization

### 2.1 General Data & Schema
- The database schema is defined by `QuanLyKhoHang_schema.json` and must strictly follow all notes, constraints, and relationships specified in that file.
- CSV data is stored in the `data/` folder.
- Queries used for reporting are stored in the `report/` folder.
- Virtual query views/schemas (such as `INBOUND_TRANSACTION_QUERY`, `CPU_QUERY`, `STATUS_HISTORY_QUERY`, etc., defined in `QuanLyKhoHang_schema.json`) are currently not stored as physical CSV tables, but their schema definitions must remain available as testing contracts for `UserQuery`.
- File organization must not alter table names, column names, keys, relationships, or schema definitions.

### 2.2 Domain-Based Folder Structure for CSV Data
CSV files inside `data/` must be organized by the business domain of the corresponding tables:

- **`data/transactions/`**: Contains all inbound and outbound transaction-related tables:
  - `inbound_transaction`, `outbound_transaction`
  - `inbound_detail`, `outbound_detail`
  - `delivery`, `shipment`
  - `delivery_detail`, `shipment_detail`
- **`data/unit/`**: Contains unit-level inventory data:
  - `status_history`
  - `unit_inventory`
- **`data/products/`**: Contains product and product-reference tables:
  - `product_quantity`, `product`, `brand`, `category`
  - Category tables: `cpu`, `mainboard`, `gpu`, `ram`, `ssd`, `psu`, `cooling`, `case`
- **`data/<category>_specs/`**: Each category must have its own specification folder (e.g., `data/ssd_specs/`):
  - Contains specification sub-tables belonging exclusively to that category (e.g., `type`, `gen`, `capacity`).
  - Each sub-table contains exactly two columns: `id` (key) and `spec` (value).
- **`data/customers/`**: Contains customer-related tables:
  - `customer`, `individual_infor`, `business_info`
- **`data/status_type/`**: Contains all master status and type tables:
  - Status tables (`inbound_status`, `outbound_status`, `delivery_status`, `shipment_status`, `unit_status`).
  - Type tables (`inbound_type`, `outbound_type`).

---

## 3. Database Protection, Referential Integrity, and Deletion Scope

- Tables, columns, CSV files, and report files must **never be deleted**.
- **Referential Integrity**: A record must **never be deleted** if it is referenced by, or has an active relationship/dependency with, another table.
- **Strict Permanent Retention**: Confirmed transactions, deliveries, shipments, units, and status histories must **never be deleted**.
- **Permitted Deletions (`UserDelete`)**: Users are only permitted to delete standalone records (such as an unreferenced `Product`, `Brand`, or `Customer`) that have **never** been associated with any transaction, delivery, shipment, or unit inventory records.
- Every transaction and inventory record must satisfy all schema constraints and form a logically valid business chain from the moment it is created or imported.

---

## 4. User Permissions and CRUD Rules

The main program uses a **single normal user role** (no `ADMIN` role or account management required).

### 4.1 Customer Precondition & Master Reference
- **Customer Precondition**: A customer record must exist before creating an Outbound Transaction (or any transaction requiring a partner). If the customer does not exist in the database, the user must add the customer first.
- **Customer Code Auto-Generation**: Customer code is automatically generated based on customer type and sequential number (`next_code_no`, 7 digits) as specified in `QuanLyKhoHang_schema.json`:
  - Individual: `IN-xxxxxxx` (e.g., `IN-0002340`)
  - Business: `BS-xxxxxxx` (e.g., `BS-0002314`)
- **Direct Creation Allowed**:
  - `Customer` (along with `individual_infor` or `business_info`).
  - `Brand`.
  - Allowed specification values in `<category>_specs/` sub-tables.

### 4.2 Product Creation and Category Binding
- Adding a new product is an integrated operation: it creates a record in `product` AND a corresponding record in the appropriate category table (`CPU`, `GPU`, `Mainboard`, `RAM`, `SSD`, `PSU`, `Cooling`, `Case`) with specifications selected from the valid master spec tables.
- All quantity fields in `product_quantity` are initialized to `0`.
- When any unit belonging to a product changes its status, `product_quantity` must be immediately updated.
- **Product Code Auto-Generation**: `product_code` must be automatically generated following the exact schema rule from `QuanLyKhoHang_schema.json`:
  - Format: `<producer_code>-<category_code>-<next_code_no(producer)>-<next_code_no(category)>`
  - Examples: `INTEL-CPU-0001-0001`, `NVI-GPU-0032-0034`, `SAM-SSD-0003-0031`, `AMD-GPU-0323-2323`.
- **Product Selling Status (`product.is_sold`)**:
  - `TRUE`: Sản phẩm đang được kinh doanh / đang được bán (actively on sale).
  - `FALSE`: Sản phẩm ngừng kinh doanh / không còn được bán (discontinued / not on sale).

### 4.3 Coupled Creation Only (Transactions, Delivery, Shipment)
Users may manually input transactions and logistics records, strictly under atomic pairwise constraints:
- **Inbound Pair**: Creating an Inbound Transaction requires simultaneously creating its paired `Delivery` records. Both must initialize their containing products and quantities. Total product quantity across all deliveries must not exceed the quantity in the `Inbound Transaction`.
- **Outbound Pair (Stock Availability Required)**:
  - Creating an Outbound Transaction requires simultaneously creating its paired `Shipment` records.
  - **Stock Precondition**: A product must exist in the warehouse with units currently in the required status for the outbound transaction type (e.g., `AVAILABLE` for `SEL`, `SCRAPPED` or `WARRANTY` for `WRT`, `AVAILABLE` or `WARRANTY` for `WRC`). If eligible units are not present in stock, units cannot be selected for export!
  - Total product quantity across all shipments must not exceed the quantity in the `Outbound Transaction`.
- Standalone creation of an Inbound/Outbound order without its paired `Delivery`/`Shipment` (or vice versa), or standalone creation of `Unit` records via normal CRUD, is strictly prohibited.

### 4.4 Unit Status Lifecycle and Transition Rules

Unit status transitions must adhere strictly to the business rules of each transaction type:

#### 4.4.1 Outbound Transactions (Đơn xuất)
- **General Rule**: The new/target status of **all units** in an outbound transaction **must be `OUT`**.
- **Specific Outbound Type Rules**:
  - **`SEL` (Selling - Bán hàng)**:
    - Precondition: Selected units must have current status `AVAILABLE`.
    - Transition: `AVAILABLE` -> `OUT`.
  - **`WRT` (Warranty Return - Xuất trả hàng bảo hành/lỗi cho hãng hoặc nhà cung cấp)**:
    - Precondition: Selected units must have current status `SCRAPPED` or `WARRANTY`.
    - Transition: `SCRAPPED / WARRANTY` -> `OUT`.
  - **`WRC` (Warranty Replacement to Customer - Xuất trả hàng đổi bảo hành cho khách hàng)**:
    - Precondition: Selected units must have current status `AVAILABLE` or `WARRANTY`.
    - Transition: `AVAILABLE / WARRANTY` -> `OUT`.

#### 4.4.2 Inbound Transactions (Đơn nhập)
- **General Rule**: The new/target status of **all units** in an inbound transaction **must never be `OUT`** (must transition into an internal warehouse status: `AVAILABLE`, `WARRANTY`, or `SCRAPPED`).
- **Specific Inbound Type Rules**:
  - **`PUR` (Purchasing - Mua hàng / Nhập kho mới)**:
    - Precondition: Unit is newly acquired and does not exist in the database (old status is `NULL` / `std::monostate`).
    - Transition: `NULL` -> `AVAILABLE` (normal inventory) or `SCRAPPED` (defective upon receiving/inspection).
    - Automatically generates a unique Serial (`SN-` + 7 alphanumeric characters) and inserts the unit record into `unit_inventory`.
  - **`WRP` (Warranty Replacement - Nhập hàng đổi bảo hành từ hãng/nhà cung cấp)**:
    - Precondition: Unit is a replacement item from the supplier (old status is `NULL` / `std::monostate`).
    - Transition: `NULL` -> `WARRANTY`.
    - Automatically generates a unique Serial and inserts the unit record into `unit_inventory` with status `WARRANTY`.
  - **`WCK` (Warranty Checking - Nhận hàng bảo hành từ khách hàng để kiểm tra)**:
    - Precondition: Unit was previously sold/dispatched and currently has status `OUT`.
    - Transition: `OUT` -> `WARRANTY`.
  - **`CRT` (Customer Return - Khách hàng trả lại hàng)**:
    - Precondition: Unit was previously sold/dispatched and currently has status `OUT`.
    - Transition: `OUT` -> `AVAILABLE`, `WARRANTY`, or `SCRAPPED` (depending on returned unit condition; new status must never remain `OUT`).

#### 4.4.3 Quantity Constraints, Auto-Update Actual Quantity, and History Logging
- **Auto-Update `actual_quantity`**:
  - When adding/attaching a `Unit` to a `Delivery` (Inbound):
    - `delivery_detail.actual_quantity` for the corresponding product in that delivery must automatically increment (+1 for each unit recorded in this delivery).
    - `inbound_detail.actual_quantity` for the corresponding product in that inbound transaction must also automatically increment accordingly (reflecting the cumulative total units actually received for that transaction).
  - When dispatching/attaching a `Unit` to a `Shipment` (Outbound):
    - `shipment_detail.actual_quantity` for the corresponding product in that shipment must automatically increment (+1 for each unit dispatched in this shipment).
    - `outbound_detail.actual_quantity` for the corresponding product in that outbound transaction must also automatically increment accordingly (reflecting the cumulative total units actually dispatched for that transaction).
- **Quantity Constraints**:
  - The number of unit status transitions (`actual_quantity`) in each delivery/shipment batch must not exceed the batch expected quantity (`delivery_detail.received_quantity` for Inbound or `shipment_detail.received_quantity` for Outbound).
  - Total `actual_quantity` across all deliveries/shipments must not exceed the original order quantity of the transaction (`inbound_detail.ordered_quantity` or `outbound_detail.ordered_quantity`).
- **History Logging**: Every unit transition automatically creates an immutable record in `status_history` logging `serial`, `old_status`, `curr_status` (new status), `transaction` (delivery/shipment code), `type` (`INBOUND` / `OUTBOUND`), and `date`.

### 4.5 Update and Delete Restrictions (Immutability & Permitted Updates)
Once a Transaction + `Delivery`/`Shipment` pair (along with its attached `Unit` transitions) is recorded/confirmed:
- **No Deletion**: Confirmed transactions, deliveries, shipments, and unit mappings must **never be deleted**.
- **Permitted Updates (`UserUpdate`)**:
  - **`status`**: Users may modify the `status` field of Transactions (`inbound_transaction`, `outbound_transaction`), `Delivery`, and `Shipment` during order lifecycle processing.
  - **`delivery_at` & `ship_at`**: Users are permitted to edit/update the actual delivery date (`delivery.delivery_at`) and actual shipment date (`shipment.ship_at`) when goods are actually delivered/dispatched or when adjustments to actual logistics dates are required.
- **Frozen Business Data**: Planned order quantities (`ordered_quantity`, `received_quantity`), product references, transaction types, customer/partner details, and attached serial numbers are locked and cannot be edited arbitrarily.

### 4.6 Read-Only Master Data & Direct Unit Restrictions
Users must **never** directly edit:
- Any `Status` or `Type` master table (`INBOUND_TYPE`, `INBOUND_STATUS`, `OUTBOUND_TYPE`, `OUTBOUND_STATUS`, `DELIVERY_STATUS`, `SHIPMENT_STATUS`, `UNIT_STATUS`).
- `Unit.status` directly through raw editing (a unit's status may only change via `Delivery` or `Shipment` workflows).

---

## 5. Code Formats and System Conventions

All auto-generated codes and user inputs must strictly conform to the specifications and notes documented in `QuanLyKhoHang_schema.json`:

1. **Transaction Codes (User Input with Format Validation)**:
   - `inbound_code`: Typed manually by user. Must follow format `IN-<type_code>-<next_code_no>` (6 digits, e.g., `IN-PUR-000032`, `IN-ADJ-003223`, `IN-WCK-002313`), must match the selected inbound type, and must be verified unique in the database.
   - `outbound_code`: Typed manually by user. Must follow format `OUT-<type_code>-<next_code_no>` (6 digits, e.g., `OUT-SEL-000999`, `OUT-RTS-999999`, `OUT-WRC-000001`), must match the selected outbound type, and must be verified unique in the database.
2. **Logistics Codes (Auto-Generated)**:
   - `delivery_code`: Automatically generated from `inbound_code`: `<inbound_code>-D<XX>` (2 digits, e.g., `IN-PUR-000032-D01`, `IN-WCK-002313-D02`).
   - `shipment_code`: Automatically generated from `outbound_code`: `<outbound_code>-S<XX>` (2 digits, e.g., `OUT-SEL-000999-S01`).
3. **Customer Codes (Auto-Generated)**:
   - Individual: `IN-xxxxxxx` (7 digits, e.g., `IN-0002340`).
   - Business: `BS-xxxxxxx` (7 digits, e.g., `BS-0002314`).
4. **Product Code (Auto-Generated)**:
   - `<producer_code>-<category_code>-<next_code_no(producer)>-<next_code_no(category)>` (e.g., `INTEL-CPU-0001-0001`).
5. **Unit Serial (Auto-Generated / Scanned)**:
   - `SN-` + 7 random characters (e.g., `SN-7K3PE33`), unique in warehouse inventory.
6. **NULL Representation in Database**:
   - `NULL` is represented by `std::monostate` in the `Value` variant (`variant<monostate, int, double, string, bool>`).
   - In code, check for `NULL` using `isNull(val)` and assign `NULL` using `Value{}` or `monostate{}`. The string representation of a null value is `"NULL"`.

---

## 6. Indexing, Query, and CRUD Architecture

### 6.1 Index Naming and Selection
- Index naming convention: `index_type + index_column` (where `index_type` is `hash`, `hash_tree`, or `avl_tree`).
  - `hash`: For columns with unique values (primary keys, unique codes).
  - `hash_tree`: For columns with a fixed set of values and many duplicates (e.g., category specs, `product.is_sold`, status, type).
  - `avl_tree`: For date columns (supports range lookup).

### 6.2 Query Execution & Caching Flag (`user_query`)
- Query logic resides in the `query/` library and is implemented in `schema/UserQuery.*`.
- Table data is treated as read-only during queries: extract required data from the table (using indexes where applicable) into a `Vector<Record>`.
- The extracted `Vector<Record>` must contain the table's `*_id` primary key column (hidden from UI view, reserved strictly for CRUD identification).
- Implements queries across all domains: Warehouse, Category Hardware, Auxiliary Specs, and Customers (`individual_customer_query`, `business_customer_query`).
- **UI Caching Flag Mechanism**:
  - The UI maintains a cache validity flag for query results.
  - When the user changes search criteria (selects a new filter column or enters a new filter value), the flag is raised (invalidated/reset), triggering a fresh database query via `user_query`.
  - The resulting `Vector<Record>` is cached in the UI.
  - Subsequent column sorting operates directly in-place on the cached `Vector<Record>&` (using `std::sort` via `sort_query_results`) without re-querying the database.
  - When an insert, update, or delete operation succeeds, the cache flag is set to dirty/invalidated so that subsequent displays re-fetch fresh data from the database.

#### 6.2.1 Pre-built Aggregate & Sort Functions
- Provide 3 pre-built operations on `Vector<Record>` for both single columns and column pairs: `sum`, `count`, and `sort`.
- **`sort`**: Sort directly in-place on the cached `Vector<Record>&`. Permitted to use C++ built-in `std::sort`.
- **`sum` & `count`**: Must be implemented using custom `HashTable`.

### 6.3 Create, Update, and Delete Architecture
- To simplify record modification without SQL parsing overhead:
  - Records in tables are modified directly through their `id` in the table (e.g., `product_id`, `delivery_id`, `customer_id`), without needing SQL `Condition` clauses at the user interface level.
  - When a user interacts with the UI to update or delete a row, the UI retrieves the hidden `*_id` from the selected row in `Vector<Record>`.
  - The UI calls the database/schema methods passing the target table name and record `id`.
  - The parameters of update functions include: `tableName`, `record_id`, `Vector<string>& modifiedColumns`, `Vector<Value>& modifyingValues`.
  - The parameters of insert functions include: `tableName`, `Record& r`.
  - The parameters of delete functions include: `tableName`, `record_id`.
  - Column names, constraints, datatypes, and referential integrity must be validated before modifying tables.
  - Implementation functions must be written in the designated files: `schema/UserCreate.*`, `schema/UserUpdate.*`, `schema/UserDelete.*`.

---

## 7. UI Rules (Qt Widgets)

### 7.1 Architecture & Separation of Concerns
- Use **Qt Widgets** with C++17; store all UI code in `ui/`.
- UI classes may freely use Qt containers and presentation widgets, but must remain strictly separated from database storage, custom data structures, and core inventory business logic.
- Do not modify auto-generated Qt files (e.g., `ui_*.h`) directly.
- Preserve existing UI behavior when modifying interfaces unless explicitly instructed otherwise.
- The `Vector<Record> results` contain `*_id` (e.g., `product_id`, `cpu_id`, `inbound_id`), but **do not display this ID on the screen**. It is used exclusively as internal metadata for CRUD operations.
- The UI must manage the cache validity flag for query results as defined in Section 6.2.

### 7.2 Dedicated Windows and Dialogs for CRUD
- **Insert Windows (Dedicated Dialogs)**:
  - **Product Insertion**: A dedicated dialog providing dropdowns to select `Category` and `Brand`, and dynamic specification selectors populated from the respective `<category>_specs` sub-tables. `product_code` is automatically generated.
  - **Transaction & Logistics Insertion (Inbound / Outbound)**:
    - Precondition check: prompts or allows quick creation of `Customer` if not present (with auto-generated customer code).
    - Pairs `Inbound Transaction` with `Delivery`, or `Outbound Transaction` with `Shipment`.
    - Includes a sub-form to add containing products and their quantities.
    - Outbound check: prevents selecting products or quantities exceeding available stock.
  - **Unit Entry Window for Delivery & Shipment**:
    - Clicking the designated action button on a Delivery or Shipment record opens a dedicated dialog to input/attach units (serial numbers and target statuses) for each product in that delivery or shipment.
    - Adding units automatically increments `actual_quantity` in `delivery_detail`/`shipment_detail` and in `inbound_detail`/`outbound_detail`.
  - **Order Detail Inspection Window**:
    - Both Transaction and Delivery/Shipment interfaces must have an action button to open a sub-window displaying the list of products in that specific order.
    - This window executes the detail query logic (`inbound_detail_query`, `delivery_detail_query`, `outbound_detail_query`, or `shipment_detail_query`) filtered specifically by that order's code (showing products, quantities, prices, etc., without listing individual units).

### 7.3 Navigation & Menus
- **Warehouse vs. Reference Separation**: All category tables (`CPU`, `GPU`, `Mainboard`, etc.), specification sub-tables, `Category`, and `Brand` must be separated from the main warehouse inventory view.
- Add a menu **`Danh mục sản phẩm`** containing all product category tables.
- Add a menu **`Danh mục phụ`** providing access to query specification tables, `Category`, and `Brand`.
- **Customer Domain Separation**:
  - All customer tables (`customer`, `individual_info`, `business_info`) must be separated from the warehouse table into their own domain and sidebar list.
  - Add a dedicated menu **`Khách hàng`** containing separate choices for:
    - **Khách hàng cá nhân (Individual)**
    - **Khách hàng doanh nghiệp (Business)**
  - Customer queries:
    - `customer_id` must be retained in the query `Record` internally for CRUD/identification, but must remain hidden from UI table columns.
    - **Individual Customer** (`individual_customer_query`): Mã khách hàng (`customer.customer_code`), Tên khách hàng (`customer.name`), Số điện thoại (`individual_info.phone_number`), Email (`individual_info.email`).
    - **Business Customer** (`business_customer_query`): Mã đối tác / DN (`customer.customer_code`), Tên doanh nghiệp (`customer.name`), Mã số thuế (`business_info.tax_code`), Số điện thoại (`business_info.phone_number`), Email (`business_info.email`).

### 7.4 Column Filtering & Input Controls
- Call `user_query` functions to filter by the user-selected column:
  - **Primary Key / Unique / Code**: Text input for exact hash lookup.
  - **Date**: Date-range picker (`from date` -> `to date`) using AVL tree range lookup.
  - **Foreign Keys / Multi-duplicate Attributes** (e.g., category specs, `product.is_sold` (`TRUE`: đang bán, `FALSE`: ngừng bán), `type`, `status`): Allow selection from a dropdown/list (backed by `HashTree`) in addition to or instead of raw typing.
  - **Customer Views**: Allow filtering by customer code (`customer_code`, Hash lookup), customer/business name (`name`), phone number (`phone_number`), email (`email`), and tax code (`tax_code`).
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
- **Consistency**: Automatically generates valid unit transitions (selecting existing `AVAILABLE` units randomly for `SEL`, and initializing `NULL -> AVAILABLE/SCRAPPED` for `PUR`) and chronological `status_history` entries that strictly satisfy `QuanLyKhoHang_schema.json` from creation without post-hoc repair.

### 8.2 Data Crawler
- May be implemented in **Python** to scrape product data into category-organized CSV files.
- Must collect category-specific specifications (not a generic spec set) and strictly adhere to all constraints and relationships in `QuanLyKhoHang_schema.json` so outputs can be cleanly imported without modifying existing schema or records.