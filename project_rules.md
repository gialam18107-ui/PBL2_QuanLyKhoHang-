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
- The database schema is defined by `QuanLyKhoHang_schema.json` and must strictly follow all notes, constraints, column comments, and relationships specified in that file.
- CSV data is stored in the `data/` folder.
- Queries used for reporting are stored in the `report/` folder.
- Virtual query views/schemas (such as `INBOUND_TRANSACTION_QUERY`, `INBOUND_DETAIL_QUERY`, `DELIVERY_QUERY`, `DELIVERY_DETAIL_QUERY`, `OUTBOUND_TRANSACTION_copy`, `OUTBOUND_DETAIL_QUERY`, `SHIPMENT_QUERY`, `SHIPMENT_DETAIL_QUERY`, `PRODUCT_QUANTITY_QUERY`, `UNIT_INVENTORY_QUERY`, `STATUS_HISTORY_QUERY`) are not stored as physical CSV tables, but their schema definitions must remain available as testing contracts for `UserQuery`.
- File organization must not alter table names, column names, keys, relationships, or schema definitions.

### 2.2 Domain-Based Folder Structure for CSV Data
CSV files inside `data/` must be organized by the business domain of the corresponding tables:

- **`data/transactions/`**: Contains all inbound and outbound transaction-related tables:
  - `inbound_transaction`, `outbound_transaction`
  - `inbound_detail`, `outbound_detail`
  - `delivery`, `shipment`
  - `delivery_detail`, `shipment_detail`
- **`data/unit/`**: Contains unit-level inventory data:
  - `unit_inventory`
  - `status_history`
- **`data/products/`**: Contains product and product-reference tables:
  - `product` (stores core product identity along with inventory counters `available_quantity`, `warranty_quantity`, `scrapped_quantity`, and active status `is_sold`)
  - `is_sold_changing` (audit log recording all status transitions of `product.is_sold`)
  - `brand`, `category`
  - Category hardware tables: `cpu`, `mainboard`, `gpu`, `ram`, `ssd`, `psu`, `cooling`, `case`
- **`data/<category>_specs/`**: Each category must have its own specification folder (e.g., `data/ssd_specs/`, `data/cpu_specs/`, etc.):
  - Contains specification sub-tables belonging exclusively to that category (e.g., `type`, `gen`, `capacity`).
  - Each sub-table contains exactly two columns: `id` (key) and `spec` (value).
- **`data/customers/`**: Contains customer-related tables:
  - `customer`, `individual_info`, `business_info`
- **`data/inventory_check/`** (or `data/internal/`): Contains inventory auditing and internal stock adjustment tables:
  - `inventory_check` (header for warehouse stock counts)
  - `inventory_check_quantity` (product-level quantity snapshot and physical counts)
  - `inventory_check_unit` (unit-level serial verification during detailed checks)
  - `adjustment_movement` (header for internal status transfer movements within warehouse)
  - `adjust_movement_detail` (product status shift details: `from_status` -> `to_status`)
- **`data/status_type/`**: Contains all master status, type, and lookup tables:
  - Status tables: `inbound_status`, `outbound_status`, `delivery_status`, `shipment_status`, `unit_status`, `inventory_check_status`, `adjustment_status`.
  - Type & reason tables: `inbound_type`, `outbound_type`, `check_type`, `reason`.

---

## 3. Database Protection, Referential Integrity, and Deletion Scope

- Tables, columns, CSV files, and report files must **never be deleted**.
- **Referential Integrity**: A record must **never be deleted** if it is referenced by, or has an active relationship/dependency with, another table.
- **Strict Permanent Retention**: Confirmed transactions, deliveries, shipments, units, status histories, inventory checks, adjustment movements, and selling status audit logs must **never be deleted**.
- **Permitted Deletions (`UserDelete`)**: Users are only permitted to delete standalone records (such as an unreferenced `Product`, `Brand`, or `Customer`) that have **never** been associated with any transaction, delivery, shipment, unit inventory, inventory check, or movement records.
- Every transaction, inventory check, and inventory record must satisfy all schema constraints and form a logically valid business chain from the moment it is created or imported.

---

## 4. User Permissions and Master Data Rules

The main program uses a **single normal user role** (no account management or complex role matrix required).

### 4.1 Customer Precondition & Adjustment Independence
- **Commercial Transactions Customer Requirement**:
  - For standard external commercial transactions (`PUR`, `SEL`, `WCK`, `WRP`, `CRT`, `WRT`, `WRC`), a valid customer record (`customer_id`) is **mandatory**.
  - If the customer does not exist in the database, the user must create the customer first before creating the transaction.
- **Strict No-Customer Rule for Adjustment Transactions (`ADJUSTMENT_IN` / `ADJUSTMENT_OUT`)**:
  - Warehouse adjustment transactions (`type == "ADJUSTMENT_IN"` or `type == "ADJUSTMENT_OUT"`, code prefix `ADJ`) represent internal warehouse reconciliations originating from stock audit discrepancies, not external commercial dealings.
  - **No customer is permitted for `ADJ` transactions**: `customer_id` must be `NULL` (represented by `std::monostate`).
  - **Pre-creation Validation**: The system and UI must strictly verify that **no customer is selected or assigned** before creating an Inbound or Outbound adjustment transaction. Any attempt to associate a customer with an `ADJ` transaction must be rejected.
- **Customer Code Auto-Generation**:
  - Customer codes are automatically generated based on customer type and sequential number (`next_code_no`, 7 digits) as documented in `QuanLyKhoHang_schema.json`:
    - Individual: `IN-xxxxxxx` (e.g., `IN-0002340`)
    - Business: `BS-xxxxxxx` (e.g., `BS-0002314`)

### 4.2 Product Selling Status (`product.is_sold`), Stock Discontinuance Rule & `IS_SOLD_CHANGING`
- **Product Status Definition**:
  - `TRUE`: Product is actively on sale.
  - `FALSE`: Product is discontinued / stopped selling.
- **Strict Stock Discontinuance Precondition (Chỉ đổi sang FALSE khi tồn kho bằng 0)**:
  - A product's `is_sold` flag can **ONLY** be changed to `FALSE` if the total warehouse quantity of that product is strictly **zero (0)**:
    $$\text{total\_quantity} = \text{available\_quantity} + \text{warranty\_quantity} + \text{scrapped\_quantity} == 0$$
  - If a product still has any inventory remaining in the warehouse ($\text{total\_quantity} > 0$), any attempt to change `is_sold` to `FALSE` must be **strictly rejected** by both the business layer (`UserUpdate`) and the UI with an explicit error alert (cannot discontinue an item while physical stock is still held).
- **Mandatory Audit Logging into `IS_SOLD_CHANGING`**:
  - Every time `product.is_sold` changes status (whether transitioning from `TRUE` to `FALSE` or from `FALSE` to `TRUE`), the system must atomically record an immutable history record into table **`IS_SOLD_CHANGING`**:
    - `history_id`: auto-increment primary key
    - `product_id`: foreign key referencing `PRODUCT.product_id`
    - `date`: current date of the change
    - `old_status`: previous boolean status
    - `new_status`: new boolean status
- **Transaction Ineligibility Rule (Ẩn và không cho chọn sản phẩm ngừng bán)**:
  - If `product.is_sold == FALSE`, the product is discontinued and must **never be selected** for any new transaction orders (neither Inbound nor Outbound).
  - The UI must filter out / hide discontinued products from dropdowns and product pickers during transaction creation (conforming to Note 6 of `QuanLyKhoHang_schema.json`).

### 4.3 Coupled Creation Only (Transactions, Delivery, Shipment)
Users may manually create transactions and logistics records strictly under atomic pairwise constraints:
- **Inbound Pair & Quantity Equality Invariant**:
  - Creating an Inbound Transaction requires simultaneously creating its paired `Delivery` records (either a single delivery or multiple delivery batches).
  - For every product in the transaction, the total allocated quantity across all delivery records must be **strictly equal** to the ordered quantity in the Inbound Transaction ($\sum \text{delivery\_detail.received\_quantity} == \text{inbound\_detail.ordered\_quantity}$). Under-allocation and over-allocation are strictly prohibited.
- **Outbound Pair & Quantity Equality Invariant**:
  - Creating an Outbound Transaction requires simultaneously creating its paired `Shipment` records (either a single shipment or multiple shipment batches).
  - **Permitted Outbound Quantity Exceeding Stock (Đơn xuất được phép vượt tồn kho)**: Outbound transaction order quantities (`outbound_detail.ordered_quantity` and shipment planned quantities) are **permitted to exceed current warehouse stock** (allowing backorders and order reservation ahead of inventory intake). Physical unit dispatching (`actual_quantity`) during shipment fulfillment remains bound to attaching currently available eligible units in the warehouse.
  - For every product in the transaction, the total allocated quantity across all shipment records must be **strictly equal** to the ordered quantity in the Outbound Transaction ($\sum \text{shipment\_detail.received\_quantity} == \text{outbound\_detail.ordered\_quantity}$).
- **Strict Post-Confirmation Lock (Khóa tạo mới Delivery/Shipment sau khi xác nhận đơn)**:
  - All deliveries/shipments associated with a transaction must be declared, allocated, and created atomically at transaction creation time.
  - Once the transaction order is confirmed and committed to the database, **it is strictly prohibited to create additional `Delivery` or `Shipment` records for that transaction**.
- **Mandatory `source_code` Reference for `ADJ` Transactions**:
  - If the transaction `type` is `ADJUSTMENT_IN` or `ADJUSTMENT_OUT`, `source_code` is **mandatory** and must reference a valid `check_code` from an existing `INVENTORY_CHECK` record (e.g., `CHK-QUAN-000001` or `CHK-UNIT-000001`).
  - Standalone creation of an `ADJ` transaction without a referencing `check_code` is strictly prohibited.
- **Product Scope Constraint for Logistics**:
  - `delivery_detail` may only contain products declared in `inbound_detail`.
  - `shipment_detail` may only contain products declared in `outbound_detail`.
- **Logistics Date Constraints**:
  - `delivery.delivery_at >= inbound_transaction.order_at` and `delivery.expected_at >= inbound_transaction.order_at`.
  - `shipment.ship_at >= outbound_transaction.order_at` and `shipment.expected_at >= outbound_transaction.order_at`.
  - Standalone creation of transactions without logistics (or vice versa), or standalone creation of `Unit` records via raw CRUD, is strictly prohibited.

### 4.4 Unit Status Lifecycle and Transition Rules

Unit status transitions must adhere strictly to the business rules of each transaction type:

#### 4.4.1 Inbound Transactions (Đơn nhập)
- **Universal Inbound Precondition (Mọi thao tác nhập inbound chỉ được phép triển khai từ unit có old status là out hoặc null)**:
  - Goods entering the warehouse must originate outside the active warehouse inventory.
  - Therefore, **all Inbound operations without exception** (`PUR`, `WRP`, `WCK`, `CRT`, `ADJ`) are strictly restricted to units whose `old_status` is either:
    1. **`NULL` / uncreated** (`std::monostate`): Brand new unit entering the system for the first time.
    2. **`OUT`**: Unit previously dispatched/sold that is returning or being re-admitted into the warehouse.
  - Attempting to perform any Inbound operation on a unit that is currently inside the warehouse (`old_status` is `AVAILABLE`, `WARRANTY`, or `SCRAPPED`) is **strictly prohibited** and must be rejected.
- **Target Status Invariant**:
  - The new/target status of **all units** in an Inbound transaction **must never be `OUT`** (must transition into an internal warehouse status: `AVAILABLE`, `WARRANTY`, or `SCRAPPED`).
- **Specific Inbound Type Rules**:
  - **`PUR` (Purchasing - Mua hàng / Nhập kho mới)**:
    - Precondition: Unit is newly acquired (`old_status == NULL`).
    - Transition: `NULL -> AVAILABLE` (normal stock) or `SCRAPPED` (defective upon arrival).
    - Automatically generates a unique Serial (`SN-` + 7 alphanumeric characters) and registers the unit into `unit_inventory`.
  - **`WRP` (Warranty Replacement - Nhập hàng đổi bảo hành từ hãng/NCC)**:
    - Precondition: Unit is a replacement item from supplier (`old_status == NULL`).
    - Transition: `NULL -> WARRANTY`.
    - Automatically generates a unique Serial and registers the unit into `unit_inventory` with status `WARRANTY`.
  - **`WCK` (Warranty Checking - Nhận hàng bảo hành từ khách hàng để kiểm tra)**:
    - Precondition: Unit was previously sold/dispatched (`old_status == OUT`).
    - Transition: `OUT -> WARRANTY`.
  - **`CRT` (Customer Return - Khách hàng trả lại hàng)**:
    - Precondition: Unit was previously sold/dispatched (`old_status == OUT`).
    - Transition: `OUT -> AVAILABLE`, `WARRANTY`, or `SCRAPPED` (depending on returned condition; target must never remain `OUT`).
  - **`ADJ` (Adjustment In - Nhập điều chỉnh kho)**:
    - Precondition: Must strictly obey the universal inbound precondition: unit's `old_status` must be **`NULL`** (surplus unrecorded unit discovered during audit) or **`OUT`** (missing unit previously written off as out, now rediscovered).
    - Transition: Permitted to transition from **`OUT` or `NULL`** to **`AVAILABLE`**, **`SCRAPPED`**, or **`WARRANTY`**.
    - If `old_status == NULL`, automatically generates a unique Serial (`SN-` + 7 alphanumeric characters) and registers the unit into `unit_inventory`.
    - Requirement: Must have `source_code` referencing `check_code`, and `customer_id` must be `NULL`.
    - *Note on internal status adjustments*: Units already in warehouse changing status internally (e.g., `AVAILABLE -> SCRAPPED` due to warehouse damage) do not use Inbound `ADJ`; they are handled by `ADJUSTMENT_MOVEMENT`.

#### 4.4.2 Outbound Transactions (Đơn xuất)
- **Universal Outbound Target Invariant**:
  - The target/new status of **all units** in an Outbound transaction **must strictly be `OUT`**.
- **Specific Outbound Type Rules**:
  - **`SEL` (Selling - Bán hàng)**:
    - Precondition: Selected units must have current status `AVAILABLE`.
    - Transition: `AVAILABLE -> OUT`.
  - **`WRT` (Warranty Return - Xuất trả hàng bảo hành/lỗi cho hãng/NCC)**:
    - Precondition: Selected units must have current status `SCRAPPED` or `WARRANTY`.
    - Transition: `SCRAPPED / WARRANTY -> OUT`.
  - **`WRC` (Warranty Replacement to Customer - Xuất trả hàng đổi bảo hành cho khách)**:
    - Precondition: Selected units must have current status `AVAILABLE` or `WARRANTY`.
    - Transition: `AVAILABLE / WARRANTY -> OUT`.
  - **`RTS` (Return to Supplier - Xuất trả hàng cho nhà cung cấp)**:
    - Precondition: Selected units must currently exist in warehouse (status `AVAILABLE`, `WARRANTY`, or `SCRAPPED`). Strictly prohibits units currently outside the warehouse (`OUT -> OUT` is prohibited).
    - Transition: `AVAILABLE / WARRANTY / SCRAPPED -> OUT`.
  - **`ADJ` (Adjustment Out - Xuất điều chỉnh / kiểm kê giảm kho)**:
    - Precondition: Selected units must currently exist in warehouse (status `AVAILABLE`, `WARRANTY`, or `SCRAPPED`).
    - Transition: `AVAILABLE / WARRANTY / SCRAPPED -> OUT`.
    - Requirement: Must have `source_code` referencing `check_code`, and `customer_id` must be `NULL`.

#### 4.4.3 Quantity Synchronization and History Logging
- **Auto-Update `actual_quantity`**:
  - Attaching a unit to a `Delivery` increments `delivery_detail.actual_quantity` (+1) and `inbound_detail.actual_quantity` (+1).
  - Dispatching a unit to a `Shipment` increments `shipment_detail.actual_quantity` (+1) and `outbound_detail.actual_quantity` (+1).
- **Synchronous Product Quantity Counters**:
  - Every unit status transition atomically updates the breakdown counters in `PRODUCT`: `available_quantity`, `warranty_quantity`, and `scrapped_quantity`.
- **Immutable History Logging**:
  - Every unit transition automatically creates an immutable record in `status_history` logging `unit_id`, `old_status`, `curr_status`, `transaction` (`delivery_id` or `shipment_id`), and `type` (`INBOUND` / `OUTBOUND`).

### 4.5 Update and Delete Restrictions (Immutability & Permitted Updates)
Once a Transaction + `Delivery`/`Shipment` pair (along with its attached `Unit` transitions) is recorded/confirmed:
- **No Deletion**: Confirmed transactions, deliveries, shipments, units, and status histories must **never be deleted**.
- **Permitted Updates (`UserUpdate`)**:
  - **`status`**: Users may modify the `status` field of Transactions, `Delivery`, and `Shipment` during order lifecycle processing (`PENDING` -> `PROCESSING` -> `CONFIRMED` / `COMPLETED`).
  - **`delivery_at` & `ship_at`**: Users are permitted to update actual logistics dates, satisfying `delivery.delivery_at >= inbound_transaction.order_at` and `shipment.ship_at >= outbound_transaction.order_at`.
  - **Flexible Unit Target Status Switching During In-Progress Logistics**:
    - While a `Delivery` or `Shipment` is in progress (status is not yet `COMPLETED`), users are permitted to switch the target status (`new_status` / `curr_status`) of recorded units back and forth within valid domain boundaries:
      - Inbound `PUR`: between `AVAILABLE` and `SCRAPPED`.
      - Inbound `CRT`: among `AVAILABLE`, `WARRANTY`, and `SCRAPPED`.
      - Inbound `ADJ`: among `AVAILABLE`, `WARRANTY`, and `SCRAPPED`.
      - Outbound Transactions (`SEL`, `WRT`, `WRC`, `ADJ`): target status remains strictly `OUT`.
- **Strict Post-Completion Lock (Khóa hoàn toàn khi chuyển COMPLETED)**:
  - Once an order, delivery, or shipment transitions to `COMPLETED`:
    - All unit status transitions, additions, and modifications are **strictly locked** (frozen/immutable).
  - Transitioning to `COMPLETED` requires a mandatory modal confirmation dialog with an explicit acknowledgment checkbox.
- **Detail Line Scope Immutability (Bất biến danh mục chi tiết sau khi tạo phiếu)**:
  - Just as `inbound_detail` and `outbound_detail` cannot have products added or removed after transaction order confirmation:
  - **`inventory_check_quantity`**: The list of products audited in an `INVENTORY_CHECK` must be declared upfront upon check creation and **cannot be added or removed later**.
  - **`adjust_movement_detail`**: The list of adjustment lines in an `ADJUSTMENT_MOVEMENT` must be declared upfront upon movement order creation and **cannot be added or removed later**.

### 4.6 Read-Only Master Data & Direct Unit Restrictions
Users must **never** directly edit:
- Any `Status`, `Type`, or `Reason` master table (`INBOUND_TYPE`, `INBOUND_STATUS`, `OUTBOUND_TYPE`, `OUTBOUND_STATUS`, `DELIVERY_STATUS`, `SHIPMENT_STATUS`, `UNIT_STATUS`, `INVENTORY_CHECK_STATUS`, `ADJUSTMENT_STATUS`, `CHECK_TYPE`, `REASON`).
- `unit_inventory.status` directly through raw editing (a unit's status may only change via `Delivery`, `Shipment`, or `Adjustment Movement` workflows).

---

## 5. Inventory Check and Internal Adjustment Management (Kiểm Kê Kho và Điều Chỉnh Nội Bộ)

### 5.1 Two Inventory Check Types (Hai Loại Kiểm Kho)
The warehouse audit system provides two distinct inventory check types defined in `CHECK_TYPE`:
1. **Kiểm kho số lượng (`CHK-QUAN`, Quantity Inventory Check)**:
   - Evaluates inventory solely at the aggregated product quantity level without tracking individual physical serial units.
   - Purpose: Fast periodic reconciliation of total counts in stock.
   - Generates only `inventory_check_quantity` records (no `inventory_check_unit`).
2. **Kiểm kho chi tiết (`CHK-UNIT`, Detailed / Unit Inventory Check)**:
   - Evaluates inventory at the granular individual unit level, inspecting and verifying every specific unit serial (`unit_inventory.serial`) physically present in the warehouse.
   - Purpose: In-depth audit to verify unit serial identities, detect unrecorded units, locate missing units, and inspect physical conditions (status shifts).
   - Generates simultaneously `inventory_check_quantity` (product-level progress tracker) and `inventory_check_unit` (serial-level inspection).

### 5.1.1 Pre-Declaration of Audited Products & Scope Immutability (Khai báo trước phạm vi sản phẩm và tính bất biến)
- **Mandatory Upfront Product Declaration (Bắt buộc khai báo trước danh sách sản phẩm kiểm kê)**:
  - Similar to transactions, every `INVENTORY_CHECK` **must declare in advance exactly which products (`product_id`) will be audited** at the moment the check record is created.
  - The declared products are atomically registered as rows in `inventory_check_quantity` with initial system stock snapshots (`system_available_quan`, `system_warranty_quan`, `system_scrapped_quan`).
- **Strict Post-Creation Lock (Khóa hoàn toàn sau khi tạo đợt kiểm kê)**:
  - Once an `INVENTORY_CHECK` is created and committed to the database, its product audit scope is **frozen/immutable**.
  - It is **strictly prohibited to add, remove, or modify** products in the audit scope of that inventory check.
  - In detailed unit checks (`CHK-UNIT`), candidate units verified in `inventory_check_unit` must strictly belong to the products declared in `inventory_check_quantity`.

### 5.1.2 Core Separation of Stocktake Snapshot and Ledger Adjustment (Nguyên tắc tách biệt Phiếu Kiểm Kê và Chứng Từ Điều Chỉnh)
- **Non-Mutating Audit Worksheet Principle**:
  - `INVENTORY_CHECK`, `inventory_check_quantity`, and `inventory_check_unit` act strictly as **point-in-time stocktake snapshots and reconciliation worksheets**.
  - Recording physical counts or scanning unit serials during an inventory check (`PENDING` / `IN_PROCESS` / `COMPLETED`) **must never directly mutate** `product` inventory counters (`available_quantity`, `warranty_quantity`, `scrapped_quantity`) or `unit_inventory.status`.
- **Mandatory Document-Driven Reconciliation**:
  - Every stock quantity or unit status correction uncovered by an audit must be executed **exclusively** through official post-check adjustment documents (`ADJUSTMENT_IN`, `ADJUSTMENT_OUT`, or `ADJUSTMENT_MOVEMENT`) linked via `source_code = check_code`, preserving a complete immutable audit trail in `status_history`.

### 5.2 Quantity Inventory Check (`CHK-QUAN`) Workflow & Discrepancy Adjustment Orders
- **Initial Lifecycle State**:
  - Upon creation, every `INVENTORY_CHECK` starts strictly in the **`PENDING`** status.
- **Mandatory Completed Status Gate (Chỉ tạo đơn sau khi đợt kiểm kê đã COMPLETED)**:
  - Adjustment orders (`ADJUSTMENT_IN`, `ADJUSTMENT_OUT`) can **ONLY** be generated after the inventory check has transitioned to **`COMPLETED`**.
  - Creating adjustment transactions while the check is `PENDING` or `IN_PROCESS` is strictly prohibited. The audit process must be finalized and officially marked `COMPLETED` before any ledger adjustments are generated.
- **Data Capture in `inventory_check_quantity`**:
  - When a `CHK-QUAN` check is created, the system takes an atomic snapshot of current system quantities from `PRODUCT`:
    - `system_available_quan`
    - `system_warranty_quan`
    - `system_scrapped_quan`
  - During the audit, the user inputs the counted physical quantities:
    - `actual_available_quan`
    - `actual_warranty_quan`
    - `actual_scrapped_quan`
- **Reconciliation & Order Generation Capabilities**:
  - The system computes discrepancy per product: $\Delta = \text{actual\_total} - \text{system\_total}$.
  - **Surplus Discrepancy ($\Delta > 0$)**: The user may generate an **Inbound Adjustment Transaction (`ADJUSTMENT_IN`, `IN-ADJ-xxxxxx`)** for the surplus quantity.
  - **Deficit Discrepancy ($\Delta < 0$)**: The user may generate an **Outbound Adjustment Transaction (`ADJUSTMENT_OUT`, `OUT-ADJ-xxxxxx`)** for the missing quantity.
  - **Explicit User Confirmation Gate (Chỉ tạo đơn nếu người dùng bấm xác nhận tạo)**:
    - The generation of adjustment orders is **never automatic**. The system only creates the transaction if the user explicitly reviews the calculated discrepancies and clicks the confirmation button ("Xác nhận tạo đơn điều chỉnh").
  - **Permitted Internal Adjustment Movement (`ADJUSTMENT_MOVEMENT`) from `CHK-QUAN`**:
    - `CHK-QUAN` audits are **fully permitted** to generate an internal status adjustment movement (`ADJUSTMENT_MOVEMENT`) when discrepancies across status categories are observed (e.g., `available` decreased while `scrapped` increased).
    - The creation is performed via explicit user configuration (manual form where user declares the status shift lines `product_id`, `from_status`, `to_status`, `adjust_quantity` with `source_code = check_code`).
    - **Single-Check Cardinality Invariant**: Obeying Section 5.5, a single `CHK-QUAN` record can be referenced by at most **one** Inbound Adjustment, at most **one** Outbound Adjustment, and at most **one** Adjustment Movement.

### 5.3 Detailed / Unit Inventory Check (`CHK-UNIT`) Workflow & Unit Adjustment Orders
- **Initial Lifecycle State & Candidate Unit Verification**:
  - Upon creation, the check starts strictly in status **`PENDING`**.
  - All candidate units in warehouse (`AVAILABLE`, `WARRANTY`, `SCRAPPED`) for the declared products are registered into `inventory_check_unit` with `scanned_status = NULL` (unverified).
  - The UI does **not pre-populate** units as verified; only when the user scans or inputs a specific serial number does that unit appear in the verified unit list, subsequently updating product counts and audit progress.
- **Data Capture in `inventory_check_quantity` & `inventory_check_unit`**:
  - Product-level snapshot is recorded in `inventory_check_quantity`.
  - For every candidate unit in the warehouse, a record in `inventory_check_unit` captures:
    - `unit_id`: reference to `unit_inventory`
    - `system_status`: recorded status in system at audit creation
    - `scanned_status`: actual physical condition verified during inspection (initially `NULL`, updated upon user entry/scan)
- **Reconciliation & Order Generation Capabilities**:
  - Detailed unit audits can uncover three types of discrepancies:
    1. **Surplus Units (Hàng thừa / tìm thấy ngoài kho)**: Unrecorded units (`old_status == NULL`) or units previously dispatched (`old_status == OUT`) found physically in the warehouse.
       - Can generate an **Inbound Adjustment Transaction (`ADJUSTMENT_IN`)** transitioning units from `OUT` or `NULL` to `AVAILABLE`, `WARRANTY`, or `SCRAPPED`.
    2. **Missing Units (Hàng thiếu / mất)**: Units recorded in system as inside the warehouse (`AVAILABLE`, `WARRANTY`, `SCRAPPED`) but physically absent.
       - Can generate an **Outbound Adjustment Transaction (`ADJUSTMENT_OUT`)** transitioning units to `OUT`.
    3. **Status Shift Discrepancies (Thay đổi trạng thái vật lý trong kho)**: Units physically located in the warehouse whose condition changed (e.g., recorded as `AVAILABLE` but found damaged -> `SCRAPPED`, or moved to `WARRANTY`).
       - Can generate an **Internal Adjustment Movement (`ADJUSTMENT_MOVEMENT`)** shifting units between internal warehouse statuses (`from_status -> to_status`).
  - **Mandatory Completed Status Gate (Chỉ tạo đơn sau khi đợt kiểm kê đã COMPLETED)**:
    - All three types of orders (`ADJUSTMENT_IN`, `ADJUSTMENT_OUT`, `ADJUSTMENT_MOVEMENT`) can **ONLY** be generated after the inventory check has transitioned to **`COMPLETED`**.
  - **Explicit User Confirmation Gate (Chỉ tạo đơn nếu người dùng bấm xác nhận)**:
    - All three types of orders (`ADJUSTMENT_IN`, `ADJUSTMENT_OUT`, `ADJUSTMENT_MOVEMENT`) are created **only upon explicit user confirmation** ("nếu người dùng bấm xác nhận").

#### 5.3.1 Unit-Level Discrepancy Classification & Scanning Rules (Quy tắc xử lý từng trường hợp Unit khi kiểm kê chi tiết)
- **Out-of-Scope Scan Rejection (Chặn quét sản phẩm ngoài phạm vi kiểm kê)**:
  - If the user scans a serial belonging to a `product_id` that was **not** declared in `inventory_check_quantity` for the active check, the system must immediately reject the scan with an explicit error alert (*"Sản phẩm không thuộc phạm vi đợt kiểm kê này"*).
- **Normal Matched Unit (Unit khớp hoàn toàn)**:
  - Precondition: Candidate unit has `system_status` in `{AVAILABLE, WARRANTY, SCRAPPED}` and is physically verified with `scanned_status == system_status`.
  - Behavior: Marked as **Matched (`Khớp`)** and increments the corresponding `actual_*_quan` counter in `inventory_check_quantity`.
- **Missing / Deficit Unit (Unit bị thiếu / vắng mặt)**:
  - Precondition: Candidate unit has `system_status` in `{AVAILABLE, WARRANTY, SCRAPPED}`, and upon finalizing (`COMPLETED`) its `scanned_status` remains `NULL` (never scanned/found physically).
  - Behavior: Flagged as **Missing (`Thiếu`, `-1`)**. Missing units **must never be deleted** from `unit_inventory`; instead, they are queued for an **`ADJUSTMENT_OUT`** transaction transitioning `system_status -> OUT`.
- **Surplus / Overage Unit (Unit bị dư / thừa)** — strictly distinguishes two operational scenarios:
  1. **Type 1 — Rediscovered `OUT` Unit (Tái phát hiện Unit đã xuất trên sổ `system_status == OUT`)**:
     - When scanning an existing serial in `unit_inventory` whose current status is `OUT` (and whose `product_id` is within check scope), the system prompts a confirmation dialog notifying that the unit is recorded as `OUT` on ledger and asks the user to select its actual physical condition (`AVAILABLE`, `WARRANTY`, or `SCRAPPED`).
     - Upon confirmation, a record is inserted into `inventory_check_unit` with `system_status = "OUT"` and `scanned_status = <selected_status>`, incrementing the corresponding `actual_*_quan` in `inventory_check_quantity`.
  2. **Type 2 — Brand-New Unrecorded Unit (Hàng dư mới tinh chưa từng có mã Serial trong hệ thống `old_status == NULL`)**:
     - Because internal serials (`SN-xxxxxxx`) are only minted upon official warehouse intake, **no draft or premature `unit_inventory` record may be created during `CHK-UNIT` counting** (preventing orphan serials if the check is later cancelled or edited).
     - Unrecorded surplus units found physically are recorded at the product level via a **"+ Ghi nhận hàng thừa chưa định danh"** action on the product's `inventory_check_quantity` row, incrementing `actual_available_quan`, `actual_warranty_quan`, or `actual_scrapped_quan` beyond verified existing serials.
     - Official `SN-xxxxxxx` serial generation and `unit_inventory` registration occur strictly during `Delivery` fulfillment of the subsequent **`ADJUSTMENT_IN`** transaction (`NULL -> AVAILABLE / WARRANTY / SCRAPPED`).
- **Quality / Status Mismatch Unit (Unit có mặt nhưng lệch phẩm chất / trạng thái)**:
  - Precondition: Candidate unit has `system_status` in `{AVAILABLE, WARRANTY, SCRAPPED}` and is physically verified with `scanned_status` in `{AVAILABLE, WARRANTY, SCRAPPED}` where `scanned_status != system_status`.
  - Behavior: Total physical warehouse count remains unchanged ($\Delta_{\text{total}} = 0$), `actual_<scanned_status>_quan` is incremented instead of `system_status`, and the unit is flagged as **Status Shift (`Lệch trạng thái`)**, queued for **`ADJUSTMENT_MOVEMENT`** (`from_status = system_status -> to_status = scanned_status`).

#### 5.3.2 Post-Check Abnormal Unit Summarization Funnel (Quy tắc tổng kết các Unit bất thường sau kiểm kê chi tiết)
Once a `CHK-UNIT` audit transitions to `COMPLETED` (or enters reconciliation review), abnormal units must be summarized using a **3-Tier Reconciliation Funnel**:
1. **Tier 1 — High-Level KPI Summary & Discrepancy Filter**:
   - Displays check-wide totals: **System Stock**, **Actual Counted**, **Matched (`Khớp`)**, **Missing (`Thiếu -`)**, **Surplus (`Thừa +`: Rediscovered `OUT` + Unrecorded New)**, and **Status Shift (`Lệch trạng thái ⇄`)**.
   - Provides a **`"Chỉ hiển thị bất thường"` (Show Discrepancies Only)** toggle (enabled by default upon check completion) to hide fully matched rows.
2. **Tier 2 — Product-Level Reconciliation Matrix & Strict No-Netting Invariant (Quy tắc KHÔNG bù trừ ngang)**:
   - Summarizes discrepancies per `product_id` with explicit separate columns: `Matched`, `Surplus (+)`, `Missing (-)`, and `Status Shift (⇄)`.
   - **Strict No-Netting Rule in `CHK-UNIT` (Cấm cấn trừ ngang Thừa - Thiếu trên cùng sản phẩm)**: If a single product simultaneously has $+m$ surplus units (e.g., rediscovered `SN-999` or unrecorded item) and $-n$ missing units (e.g., absent `SN-001`), the system **must never net them off** into $(m - n)$ (even when $m == n$ yields zero net quantity difference). Both `+m Surplus` (requiring `ADJUSTMENT_IN`) and `-n Missing` (requiring `ADJUSTMENT_OUT`) must remain distinct so that individual serial identities in `unit_inventory` are accurately reconciled.
3. **Tier 3 — Action-Oriented Grouping into 3 Reconciliation Buckets (Gom rổ theo 3 hướng xử lý chứng từ)**:
   - All abnormal units/counts are automatically partitioned into 3 dedicated action tabs/buckets matching the 3 adjustment workflows:
     - **Bucket 1 — Missing Units (`Cần Xuất Điều Chỉnh -> ADJUSTMENT_OUT`)**: Lists every unit with `system_status` in `{AVAILABLE, WARRANTY, SCRAPPED}` and `scanned_status == NULL`, grouped by product to generate `OUT-ADJ-xxxxxx`.
     - **Bucket 2 — Surplus Units (`Cần Nhập Điều Chỉnh -> ADJUSTMENT_IN`)**: Lists all rediscovered `OUT` units (`system_status == "OUT"`, `scanned_status != NULL`) alongside unrecorded surplus quantities per product/condition to generate `IN-ADJ-xxxxxx`.
     - **Bucket 3 — Status Shift Units (`Cần Điều Chỉnh Nội Bộ -> ADJUSTMENT_MOVEMENT`)**: Aggregates status-mismatched units by transition tuple `(product_id, from_status, to_status)` with `adjust_quantity` and expandable serial lists underneath, ready to generate `ADJ-<reason>-xxxxxx`.
   - **Idempotent Action Locking**: Once an order for a bucket has been generated (linked via `source_code = check_code`), that bucket's creation button is permanently disabled and displays the linked document code (e.g., `"Đã lập phiếu: OUT-ADJ-000001"`).

### 5.4 Internal Status Adjustment Movement (`ADJUSTMENT_MOVEMENT`)
- Used exclusively for shifting the status of existing warehouse units internally without moving them in or out of the warehouse.
- **Attributes**:
  - `adjust_id`: primary key
  - `adjust_code`: `ADJ-<reason_code>-<xxxxxx>` (e.g., `ADJ-STD-000001`, `ADJ-DMG-000001`)
  - `source_code`: unique, nullable, references `INVENTORY_CHECK.check_code` (mandatory when `reason == "STOCKTAKE_DIFF"`)
  - `reason`: references `REASON.reason` (`STOCKTAKE_DIFF`, `DAMAGED`, `DATA_ERROR`, `OTHER`)
  - `adjusted_at`: execution date
  - `status`: `PENDING`, `COMPLETED`, `PARTLY_COMPLETED`, `CANCELLED`
- **Detail Table (`ADJUST_MOVEMENT_DETAIL`)**:
  - `adjust_detail_id`: primary key
  - `adjust_id`: foreign key to parent movement
  - `product_id`: foreign key to product
  - `from_status`: source warehouse status (`AVAILABLE`, `WARRANTY`, `SCRAPPED`)
  - `to_status`: destination warehouse status (`AVAILABLE`, `WARRANTY`, `SCRAPPED`; must not be `OUT`)
  - `adjust_quantity`: planned quantity to shift
  - `actual_quantity`: actual number of units shifted
- **Reason-Specific Preconditions & Transition Rules (Quy tắc theo lý do điều chuyển `reason`)**:
  - **`STOCKTAKE_DIFF` (Lệch kiểm kê)**:
    - **Mandatory `source_code`**: Must strictly provide a non-empty `source_code` referencing an existing `INVENTORY_CHECK.check_code`.
    - **Audited Product Scope Enforcement**: All products (`product_id`) declared in `adjust_movement_detail` must belong strictly to the product scope audited in the referenced inventory check (`inventory_check_quantity` of `source_code`).
  - **`DAMAGED` (Hư hỏng trong kho)**:
    - **Destination Status Restriction**: All detail lines in `adjust_movement_detail` are only permitted to transition to `SCRAPPED` (`to_status == "SCRAPPED"`, with `from_status` in `{AVAILABLE, WARRANTY}`).
- **Mandatory Pre-Declaration of Movement Details & Detail Immutability (Khai báo cụ thể trước và khóa chi tiết điều chỉnh nội bộ)**:
  - Similar to transactions and inventory checks, every `ADJUSTMENT_MOVEMENT` **must declare specifically in advance all planned adjustment detail lines** (`product_id`, `from_status`, `to_status`, `adjust_quantity`) in `adjust_movement_detail` at the moment the movement order is created.
  - **Strict Post-Creation Lock**: Once the `ADJUSTMENT_MOVEMENT` record is created, **it is strictly prohibited to add, remove, or delete any records in the detail table `adjust_movement_detail`**.

### 5.5 Referential Linkage, Audited Product Scope & Cardinality Invariant (`source_code` referencing `check_code`)
- **Mandatory Linkage Rule**:
  - Whenever an Inbound Transaction or Outbound Transaction is created with type `ADJ` (`ADJUSTMENT_IN` / `ADJUSTMENT_OUT`), or an `ADJUSTMENT_MOVEMENT` is created with `reason == "STOCKTAKE_DIFF"`, its `source_code` **must strictly reference an existing `INVENTORY_CHECK.check_code`**.
- **Audited Product Scope Constraint (Ràng buộc sản phẩm thuộc phạm vi kiểm kê)**:
  - When an `inbound_transaction` has `type = ADJUSTMENT_IN` (`ADJ`), an `outbound_transaction` has `type = ADJUSTMENT_OUT` (`ADJ`), or an `adjustment_movement` has `reason = STOCKTAKE_DIFF`, every `product_id` in the document's detail lines **must strictly belong to the products declared in that inventory check** (`inventory_check_quantity` linked to `source_code`). Selecting or inserting any product outside the referenced inventory check is prohibited.
- **Cardinality Invariant (Quy tắc một đơn nhập, một đơn xuất, một điều chỉnh cho một kiểm kê)**:
  - In `inbound_transaction`, `outbound_transaction`, and `adjustment_movement`, `source_code` carries a **UNIQUE (`[UQ]`) constraint**.
  - A single inventory check (`check_code`) can be referenced by at most **one** Inbound Transaction (`ADJUSTMENT_IN`), at most **one** Outbound Transaction (`ADJUSTMENT_OUT`), and at most **one** Adjustment Movement (`ADJUSTMENT_MOVEMENT`).
  - Associating multiple transactions of the same direction to the same inventory check (e.g., two outbound orders pointing to the same check) is strictly prohibited by schema and referential integrity.
- **No-Customer Validation**:
  - Both Inbound and Outbound adjustment transactions must have `customer_id == NULL`. The system must validate that no customer is selected before committing the order.

### 5.6 Lifecycle, Status Transition & Cancellation Rules for Adjustment Orders (`ADJUSTMENT_IN`, `ADJUSTMENT_OUT`, `ADJUSTMENT_MOVEMENT`)
- **Initial Lifecycle State**:
  - All adjustment orders (`inbound_transaction` with type `ADJUSTMENT_IN`, `outbound_transaction` with type `ADJUSTMENT_OUT`, and `adjustment_movement`) strictly start in the **`PENDING`** status.
- **Cancellation Rule (`CANCELLED`)**:
  - An adjustment order can **ONLY** be transitioned to `CANCELLED` if:
    1. It is currently in `PENDING` status.
    2. **Zero progress has occurred**:
       - For `ADJUSTMENT_IN`: No delivery has been completed (`status == COMPLETED`), and no unit has been received (`actual_quantity == 0` across all deliveries).
       - For `ADJUSTMENT_OUT`: No shipment has been completed (`status == COMPLETED`), and no unit has been dispatched (`actual_quantity == 0` across all shipments).
       - For `ADJUSTMENT_MOVEMENT`: No unit status has been shifted/executed (`actual_quantity == 0` across all detail lines).
  - Once any unit has been received, dispatched, or shifted (`actual_quantity > 0`), or any delivery/shipment is completed, transition to `CANCELLED` is **strictly prohibited**.
  - **Synchronized Cancellation**: Transitioning an `ADJUSTMENT_IN` or `ADJUSTMENT_OUT` to `CANCELLED` automatically transitions all its associated non-completed `delivery` or `shipment` records to `CANCELLED`.
  - **Terminal State Lock**: A `CANCELLED` adjustment order is permanently closed and immutable (cannot be reopened, cannot receive units, cannot transition back to `PENDING` or any other state).
- **Sequential In-Progress & Completion Transitions (Trình tự chuyển đổi trạng thái khi đang thực hiện)**:
  - **`PENDING` ➔ `PROCESSING`**: Occurs when order processing begins.
  - **`PROCESSING` ⇄ `PARTIALLY_CONFIRMED`**: Semi-automatic bi-directional toggling between `PROCESSING` and `PARTIALLY_CONFIRMED` is permitted to reflect partial processing milestones.
  - **`CONFIRMED` (Inbound/Outbound) / `COMPLETED` (Movement) & Under-Fulfillment Rule**:
    - Under normal circumstances, orders transition to completed status upon 100% fulfillment of planned quantities.
    - **Exception for Under-Fulfillment**: If an order cannot be 100% fulfilled (due to missing stock, discrepancy, supplier shortage, or operational closeout), transition to `CONFIRMED` (or `COMPLETED` for movement) **IS PERMITTED**, provided that a **mandatory warning popup dialog with an explicit confirmation checkbox ("dấu tick")** is confirmed by the user.
    - **Permanent Logistics Lock upon Confirmation**: Once an order transitions to `CONFIRMED` (or `COMPLETED`), the order is permanently closed. **All associated logistics batches (deliveries / shipments) are permanently locked against further execution**: no new batches may be added, no existing delivery/shipment record may be edited, and no further units can be scanned, received, or modified (`changeUnitStatusInLogistics` is locked).
  - **Delivery / Shipment Status Updates**: Users are permitted to manually update the status of individual delivery/shipment records in adjustment orders following standard logistics lifecycles, provided the parent order has not reached terminal status (`CONFIRMED` / `CANCELLED`).

---

## 6. Code Formats and System Conventions

All auto-generated codes and user inputs must strictly conform to `QuanLyKhoHang_schema.json`:

1. **Transaction Codes (User Input with Format Validation)**:
   - `inbound_code`: Format `IN-<type_code>-<next_code_no>` (6 digits, e.g., `IN-PUR-000032`, `IN-ADJ-003223`, `IN-WCK-002313`), matches the selected inbound type, unique in database.
   - `outbound_code`: Format `OUT-<type_code>-<next_code_no>` (6 digits, e.g., `OUT-SEL-000999`, `OUT-ADJ-000001`, `OUT-WRC-000001`), matches the selected outbound type, unique in database.
2. **Logistics Codes (Auto-Generated)**:
   - `delivery_code`: Format `<inbound_code>-D<XX>` (2 digits, e.g., `IN-PUR-000032-D01`, `IN-WCK-002313-D02`), derived from `inbound_transaction.next_delivery_no`.
   - `shipment_code`: Format `<outbound_code>-S<XX>` (2 digits, e.g., `OUT-SEL-000999-S01`), derived from `outbound_transaction.next_ship_no`.
3. **Inventory Check Code (Auto-Generated)**:
   - Format: `CHK-<type>-<xxxxxx>` (6 digits, e.g., `CHK-QUAN-000001`, `CHK-UNIT-000001`), derived from `CHECK_TYPE.next_code_no`.
4. **Adjustment Movement Code (Auto-Generated)**:
   - Format: `ADJ-<reason_code>-<xxxxxx>` (6 digits, e.g., `ADJ-STD-000001`, `ADJ-DMG-000002`, `ADJ-DTE-000003`, `ADJ-OTH-000004`), derived from `REASON.next_code_no`.
5. **Customer Codes (Auto-Generated)**:
   - Individual: `IN-xxxxxxx` (7 digits, e.g., `IN-0002340`).
   - Business: `BS-xxxxxxx` (7 digits, e.g., `BS-0002314`).
   - Note: Adjustment transactions do not have a customer (`customer_id = NULL`).
6. **Product Code (Auto-Generated)**:
   - Format: `<producer_code>-<category_code>-<next_code_no(producer)>-<next_code_no(category)>` (e.g., `INTEL-CPU-0001-0001`, `NVI-GPU-0032-0034`, `SAM-SSD-0003-0031`).
7. **Unit Serial (Auto-Generated / Scanned)**:
   - `SN-` + 7 random alphanumeric characters (e.g., `SN-7K3PE33`), unique across inventory.
8. **NULL Representation in Database**:
   - `NULL` is represented by `std::monostate` in the `Value` variant (`variant<monostate, int, double, string, bool>`).
   - In code, check for `NULL` using `isNull(val)` and assign `NULL` using `Value{}` or `monostate{}`. String representation in CSV is `"NULL"`.

---

## 7. Indexing, Query, and CRUD Architecture

### 7.1 Index Naming and Selection
- Index naming convention: `index_type + index_column` (where `index_type` is `hash`, `hash_tree`, or `avl_tree`).
  - `hash`: For columns with unique values (primary keys, unique codes, serial numbers).
  - `hash_tree`: For columns with a fixed set of values and many duplicates (e.g., category specs, `product.is_sold`, status, type, reason).
  - `avl_tree`: For date columns (supports range lookups: `order_at`, `delivery_at`, `ship_at`, `check_at`, `adjusted_at`).

### 7.2 Query Execution & Caching Flag (`user_query`)
- Query logic resides in the `query/` library and is implemented in `schema/UserQuery.*`.
- **Strict Query Encapsulation in `schema/UserQuery.*`**:
  - All database queries, data lookups, table joins, filtered scans, and aggregations required by the system and UI must be implemented exclusively in `schema/UserQuery.*`.
  - Writing inline database queries, ad-hoc table lookups, or directly traversing table rows (`Table::getRows()`) inside UI files (`ui/*.cpp`) is strictly prohibited. The UI must only invoke standardized `user_query::*` functions.
- Table data is treated as read-only during queries: extract required data from the table (using indexes where applicable) into a `Vector<Record>`.
- The extracted `Vector<Record>` must contain the table's `*_id` primary key column (hidden from UI view, reserved strictly for CRUD identification).
- **UI Caching Flag Mechanism**:
  - The UI maintains a cache validity flag for query results.
  - When the user changes search criteria (selects a new filter column or enters a new filter value), the flag is raised (invalidated/reset), triggering a fresh database query via `user_query`.
  - The resulting `Vector<Record>` is cached in the UI. Subsequent column sorting operates directly in-place on the cached `Vector<Record>&` (using `std::sort` via `sort_query_results`) without re-querying the database.
  - When an insert, update, or delete operation succeeds, the cache flag is set to dirty/invalidated so that subsequent displays re-fetch fresh data from the database.

### 7.3 Create, Update, and Delete Architecture & Validation Layers
- Records in tables are modified directly through their `id` in the table (e.g., `product_id`, `delivery_id`, `check_id`), without needing SQL `Condition` clauses at the user interface level.
- Implementation functions must be written in designated schema files: `schema/UserCreate.*`, `schema/UserUpdate.*`, and `schema/UserDelete.*`.
- All CRUD functions must return an explicit status/result object (`OperationResult { bool success; std::string message; }`).
- **Validation Layers**:
  1. **Schema & Existence Validation**:
     - Verify table existence, record existence by ID, column names, and type compatibility.
  2. **Integrity & Constraint Validation**:
     - **NOT NULL**: Reject setting non-nullable columns to `NULL`.
     - **UNIQUE**: Validate uniqueness for codes (`product_code`, `customer_code`, `inbound_code`, `outbound_code`, `check_code`, `adjust_code`, `serial`, `source_code`).
     - **Foreign Key Lookup**: Verify that foreign keys point to valid, existing parent records.
     - **Referential Protection (`UserDelete`)**: Verify that records are not referenced by any active transactional, logistics, audit, or inventory records before deletion.
  3. **Domain Business Logic Validation**:
     - **`is_sold` Validation**:
       - Reject changing `product.is_sold` to `FALSE` if $\text{total\_quantity} > 0$.
       - Atomically insert an audit record into `IS_SOLD_CHANGING` on every status change.
     - **Adjustment Transaction & Movement Validation (`ADJ` / `ADJUSTMENT_MOVEMENT`)**:
       - If transaction type is `ADJUSTMENT_IN` or `ADJUSTMENT_OUT`:
         - Enforce that `customer_id` is `NULL` (no customer). If a customer is provided, reject!
         - Enforce that `source_code` is NOT NULL and exists in `INVENTORY_CHECK.check_code`.
         - Enforce uniqueness: Ensure this `check_code` does not already have an existing transaction of this direction (`INBOUND` or `OUTBOUND`).
         - Enforce audited product scope: Every `product_id` in the order must exist in `inventory_check_quantity` for the referenced `source_code`.
       - For `ADJUSTMENT_MOVEMENT`:
         - If `reason == "STOCKTAKE_DIFF"`: enforce that `source_code` is NOT NULL, exists in `INVENTORY_CHECK.check_code`, and every `product_id` in `adjust_movement_detail` exists in `inventory_check_quantity` for that `source_code`.
         - If `reason == "DAMAGED"`: enforce that all detail rows have `to_status == "SCRAPPED"`.
     - **Adjustment Order Lifecycle & Status Transition Rules (Rule 5.6)**:
       - Applicable to `ADJUSTMENT_IN`, `ADJUSTMENT_OUT`, and `ADJUSTMENT_MOVEMENT`:
       - **Cancellation (`CANCELLED`)**: An adjustment order can ONLY be cancelled if it is currently in `PENDING` and no progress has occurred (`actual_quantity == 0` across all items/deliveries/shipments/movement lines). Once any units or shipments/deliveries are processed, cancellation is strictly forbidden.
       - **Synchronized Cancellation**: Cancelling an `ADJUSTMENT_IN` or `ADJUSTMENT_OUT` automatically transitions all associated non-completed `delivery` or `shipment` records to `CANCELLED`.
       - **Terminal State Lock**: A `CANCELLED` adjustment order is permanently closed and cannot be reopened or edited.
       - **Sequential In-Progress & Completion Transitions**: Transition from `PENDING` to `PROCESSING` occurs when processing begins. Bi-directional transitions between `PROCESSING` and `PARTIALLY_CONFIRMED` are permitted. Transition to `CONFIRMED` (or `COMPLETED` for movement) allows completion upon 100% fulfillment, or under-fulfilled completion with explicit user confirmation popup & checkbox; upon confirmation, all deliveries/shipments are permanently locked against execution or editing.
     - **Commercial Transaction Validation**:
       - If transaction is non-ADJ: enforce that `customer_id` is provided and exists in `customer`.
     - **Inbound Unit Transition Validation**:
       - Enforce that any unit attached to an Inbound Delivery has `old_status == NULL` or `old_status == OUT`.
       - If unit is currently inside the warehouse (`AVAILABLE`, `WARRANTY`, `SCRAPPED`), reject immediately!
       - If inbound type is `ADJ`: allow `new_status` to be `AVAILABLE`, `WARRANTY`, or `SCRAPPED`.
     - **Outbound Unit Transition Validation**:
       - Enforce that any unit attached to an Outbound Shipment has `new_status == OUT`.
       - Enforce that candidate units currently exist in warehouse matching outbound type preconditions.
     - **Chronological Date Validation**: `delivery_at >= order_at` and `ship_at >= order_at`.
     - **Logistics Allocation Equality**: $\sum \text{received\_quantity} == \text{ordered\_quantity}$.
     - **Post-Confirmation Logistics Lock**: Reject adding deliveries or shipments after transaction confirmation.

---

## 8. UI Rules (Qt Widgets)

### 8.1 Architecture & Separation of Concerns
- Use **Qt Widgets** with C++17; store all UI code in `ui/`.
- UI classes may freely use Qt containers and presentation widgets, but must remain strictly separated from database storage, custom data structures, and core inventory business logic.
- UI code must **never** write custom queries, execute raw table scans (`getTable()->getRows()`), manipulate internal table pointers, or directly mutate table rows.
- The `Vector<Record> results` contain `*_id` columns; **do not display primary key IDs on the screen**. They are used exclusively as internal metadata for CRUD operations.

### 8.2 Dedicated Windows and Dialogs for CRUD & Operations
- **Product Management View**:
  - Conforms to Note 9 of `QuanLyKhoHang_schema.json`: The main table view displays `PRODUCT_QUANTITY_QUERY` (`product_code`, `barcode`, `sku`, `product_name`, `available_quantity`, `warranty_quantity`, `scrapped_quantity`).
  - Expanding or opening the product sub-window shows full detailed specifications (`product_query` + hardware category specs).
  - **Discontinuance Switch (`is_sold`)**:
    - The UI provides a toggle/switch for product selling status.
    - If the user attempts to switch `is_sold` to `FALSE` while total quantity > 0, the UI must block the switch and display a warning dialog (`QMessageBox::warning`: "Không thể ngừng bán sản phẩm khi số lượng tồn kho vẫn còn lớn hơn 0").
    - When successfully toggled, calls `UserUpdate`, which logs the transition to `IS_SOLD_CHANGING`.
- **Transaction Creation Dialog (Inbound / Outbound)**:
  - **Dynamic Customer & Source Code Handling Based on Transaction Type**:
    - When the user selects transaction type `ADJUSTMENT_IN` or `ADJUSTMENT_OUT`:
      - The customer selection field is **completely disabled or hidden** (enforcing no customer).
      - The `source_code` field becomes **mandatory**: presents a combobox/selector of available `INVENTORY_CHECK` records (`check_code`).
    - When the user selects a standard commercial type (`PUR`, `SEL`, `WCK`, `WRP`, `CRT`, `WRT`, `WRC`):
      - The customer selection field is **mandatory** (allowing selection or quick creation of `Customer`).
      - The `source_code` field is optional.
  - **Active Product Enforcement**: Dropdowns and item selectors strictly filter out discontinued products (`product.is_sold == FALSE`).
  - **Logistics Product Scope**: Delivery/Shipment detail rows must only contain products defined in the parent transaction's detail rows.
  - **Date Pickers Validation**: Ensure `delivery_at >= order_at` and `ship_at >= order_at`.
- **Unit Entry and Processing for Delivery & Shipment**:
  - **Unified Serial Scanning / Direct Input**: Users enter/scan unit serials into a single input field without needing to click a product first.
  - **Validation & Status Precondition Enforcement**:
    - For Inbound deliveries: strictly verifies that the unit's `old_status` is `NULL` or `OUT`. If the unit already exists in the warehouse, the UI rejects the scan with an explicit error alert.
    - For Outbound shipments: strictly verifies that the unit currently exists in the warehouse in an eligible status (`AVAILABLE`, `WARRANTY`, `SCRAPPED`).
    - Updates `unit_inventory`, writes to `status_history`, and increments `actual_quantity`.
  - **Unit Selection Helper Dialog**: Simulates barcode scanning by allowing users to browse and pick candidate units satisfying preconditions.
  - **Delivery & Shipment Changed Units Inspection View**: Shows all units whose status was altered by that batch (Product Name, Serial, Old Status, New Status) and features a "Tổng kết trạng thái" (Status Summary) button aggregating unit counts by status using `HashTable`.
  - **Order Completion Confirmation Dialog**: Modal popup with mandatory confirmation checkbox before setting status to `COMPLETED`.
- **Inventory Check Interface (Kiểm Kê Kho)**:
  - **Upfront Product Scope Declaration**: When initiating a new inventory check, the UI presents a product selector dialog where users must choose all participating products upfront. Once confirmed, the product list is permanently locked into `inventory_check_quantity` and cannot be modified.
  - Provides two distinct tabs/workflows:
    1. **Kiểm kho số lượng (`CHK-QUAN`)**:
       - Displays declared products with system quantities snapshot (`system_available_quan`, etc.).
       - Allows inputting actual counted quantities (`actual_available_quan`, etc.).
       - Displays calculated discrepancy.
       - Features an explicit action button: **"Xác nhận tạo đơn điều chỉnh"**. Clicking this button creates the corresponding `ADJUSTMENT_IN` or `ADJUSTMENT_OUT` transaction (with `source_code = check_code`, `customer_id = NULL`).
    2. **Kiểm kho chi tiết (`CHK-UNIT`)**:
       - **Master–Detail Split View Layout**:
         - *Top / Master Table (`inventory_check_quantity`)*: High-level progress tracker per product showing System Snapshot vs. Actual Verified counts, audit progress, and separate `Surplus (+)` / `Missing (-)` / `Status Shift (⇄)` indicators (strictly without netting off surplus against missing on the same SKU).
         - *Bottom / Detail Panel (`inventory_check_unit` + Unified Scan Bar)*: Continuous barcode/serial scan bar alongside unit-by-unit reconciliation rows (`Serial | System Status | Scanned Status | Discrepancy Badge`).
       - **Color-Coded Discrepancy Badges & Scan Prompts**:
         - `Khớp (Matched)`: Green badge (`scanned_status == system_status`).
         - `Thiếu (Missing)`: Red/Orange badge upon completion when `scanned_status == NULL`.
         - `Thừa - Tái phát hiện (Surplus - Rediscovered OUT)`: Blue/Amber badge when scanning an `OUT` serial; opens a confirmation prompt to pick physical status (`AVAILABLE` / `WARRANTY` / `SCRAPPED`).
         - `Thừa - Chưa định danh (Surplus - Unrecorded)`: Recorded per product via **"+ Ghi nhận hàng thừa chưa định danh"** without creating premature `unit_inventory` rows before `ADJUSTMENT_IN` delivery.
         - `Lệch trạng thái (Status Shift)`: Purple/Amber badge when `scanned_status != system_status` within warehouse statuses.
         - Scanning any serial outside the declared product scope triggers an immediate warning popup (*"Sản phẩm không thuộc phạm vi đợt kiểm kê này"*).
       - **Post-Completion 3-Tier Abnormal Summary Dialog / View**:
         - Displays top KPI summary cards (System, Counted, Matched, Missing, Surplus, Status Shift) and auto-enables the **`[✓] Chỉ hiển thị bất thường`** filter toggle.
         - Organizes discrepancies into **3 Action-Oriented Tabs** corresponding to the 3 explicit action buttons: **"Tạo đơn xuất điều chỉnh (ADJ-OUT)"**, **"Tạo đơn nhập điều chỉnh (ADJ-IN)"**, and **"Tạo đơn điều chỉnh nội bộ (Movement)"**. Each order is generated **only upon user confirmation**, after which its button locks and displays the generated document code.
- **Internal Adjustment Movement Interface (Điều Chỉnh Nội Bộ)**:
  - **Upfront Detail Declaration**: When creating an adjustment movement, users must configure all item shift details (`product_id`, `from_status`, `to_status`, `adjust_quantity`) upfront. Once confirmed and saved, the detail rows in `adjust_movement_detail` are permanently locked (no adding, editing, or deleting lines).
  - Displays records from `ADJUSTMENT_MOVEMENT` and `adjust_movement_detail`.
  - Shows `adjust_code`, `source_code` (referencing `check_code`), `reason` (`STOCKTAKE_DIFF`, `DAMAGED`, `DATA_ERROR`, `OTHER`), and status.

### 8.3 Navigation & Menus
- **Warehouse vs. Reference Separation**: All category tables, specification sub-tables, `Category`, and `Brand` are isolated under menu **`Danh mục sản phẩm`** and menu **`Danh mục phụ`**.
- **Customer Domain**: Isolated under menu **`Khách hàng`** (choices for Khách hàng cá nhân and Khách hàng doanh nghiệp).
- **Inventory Audit Domain**: Dedicated top menu **`Kiểm kê kho`** for inventory checks and internal adjustment movements.
- **Report Domain**: Dedicated top menu **`Báo cáo`** for Periodic Inventory Movement Report.

### 8.4 Column Filtering & Input Controls
- Primary Key / Unique / Code: Hash lookup.
- Date: AVL tree range lookup (`from date` -> `to date`).
- Foreign Keys / Categorical Attributes (`product.is_sold`, status, type, reason): Dropdown selection backed by `HashTree`.
- Customer Views: Filter by customer code, name, phone, email, and tax code.

---

## 9. Auxiliary Tools

### 9.1 Transaction Data Generator
- **Scope**: Generates transaction-flow test data only (Transactions, `Delivery`, `Shipment`, `Unit`, `status_history`, `inventory_check`, `adjustment_movement`). Must not alter master data or schema implementations.
- **Strict Business Logic Compliance**:
  - **Pairwise Logistics Association**: Every Inbound/Outbound transaction is paired with delivery/shipment records with total quantity equality ($\sum \text{received\_quantity} == \text{ordered\_quantity}$).
  - **Product Eligibility & Scope**: Chooses only products with `product.is_sold == TRUE`.
  - **Chronological Date Generation**: All generated dates must precede the execution date (`generated_date < generation_trigger_date`), with `delivery_at >= order_at` and `ship_at >= order_at`.
  - **Unit Status Lifecycle Preconditions**:
    - Inbound units: ONLY generated from `old_status == NULL` or `old_status == OUT`.
    - Outbound units: target status strictly `OUT`.
    - Inbound `ADJ`: transitions `OUT / NULL -> AVAILABLE / SCRAPPED / WARRANTY`.
  - **Adjustment Transactions Compliance**:
    - All generated `ADJ` transactions must have `customer_id = NULL` (no customer).
    - All generated `ADJ` transactions and `adjustment_movement` records must have `source_code` linking to a valid `INVENTORY_CHECK.check_code`.
    - Strictly obeys the single-check cardinality invariant (at most 1 Inbound ADJ, 1 Outbound ADJ, 1 Movement per check).
  - **Commercial Transactions Compliance**: Every commercial transaction links to a valid `customer_id`.
  - **Quantity Synchronization**: Atomically updates `available_quantity`, `warranty_quantity`, and `scrapped_quantity` in `product`.
  - **History Logging**: Immutable chronological logging into `status_history`.

### 9.2 Data Crawler
- **Implementation**: Implemented in **Python** to scrape authentic product data into category-organized CSV files.
- **Strict Product & Category Classification**: 8 categories (`CPU`, `GPU`, `MAINBOARD`, `RAM`, `SSD`, `PSU`, `COOLING`, `CASE`).
- **Product Code Auto-Generation**: `<producer_code>-<category_code>-<next_code_no(producer)>-<next_code_no(category)>`.
- **Accurate Category-Specific Specifications Classification**:
  - `CPU`: `socket`, `tier` (`I3`, `I5`, `I7`, `I9`, `R3`, `R5`, `R7`, `R9`, `XE`, `TR`, `EP`, etc.), `series` (e.g., `intel 12th`, `intel 13th`, `intel 14th`, `amd 7000`, `amd 9000`).
  - `GPU`: `series`, `producer` (`NVIDIA`, `AMD`), `vram` (GB integer).
  - `MAINBOARD`: `cpu_support` (`INTEL`, `AMD`), `socket`, `size`, `chipset`, `ram_support`.
  - `RAM`: `generation`, `quantity`, `capacity`, `bus`.
  - `SSD`: `type`, `gen`, `capacity`.
  - `PSU`: `quality`, `capacity`, `size`.
  - `COOLING`: `type`.
  - `CASE`: `size`, `product_name`.
  - Extracted and stored into sub-tables under `data/<category>_specs/` (two columns: `id` and `spec`).

---

## 10. Periodic Inventory Movement Report (Báo Cáo Xuất - Nhập - Tồn Định Kỳ)

### 10.1 Overview & Scope
- **Reporting Period**: Monthly basis (Month `MM`, Year `YYYY`).
- **Product Scope & Dynamic Monthly Filtering**:
  - Historical inclusivity: Products with `product.is_sold == FALSE` are included if they had transactions or non-zero stock during the reporting month.
  - **Exclusion Rule**: Excludes products that simultaneously have zero transactions (`inbound_quantity == 0` and `outbound_quantity == 0`) AND zero inventory (`opening_stock == 0` and `closing_stock == 0`).
  - **Inclusion Criteria**: Included if transactions occurred in the month OR inventory was held (`opening_stock > 0` or `closing_stock > 0`).

### 10.2 Report Columns & Data Structure
1. **`product_code`**: Unique product code.
2. **`product_name`**: Full product name.
3. **`opening_stock`**: Total stock remaining in warehouse up to 23:59:59 of the last day of the preceding month.
4. **`inbound_quantity`**: Total quantity actually received during reporting month (from `delivery_detail.actual_quantity` where `delivery.delivery_at` is in period).
5. **`outbound_quantity`**: Total quantity actually dispatched during reporting month (from `shipment_detail.actual_quantity` where `shipment.ship_at` is in period).
6. **`closing_stock`**: Total stock remaining at month end:
   $$\text{closing\_stock} = \text{opening\_stock} + \text{inbound\_quantity} - \text{outbound\_quantity}$$
7. **`available_quan`**: Available commercial stock at month end.
8. **`warranty_quan`**: Stock under warranty at month end.
9. **`scrapped_quan`**: Defective / scrapped stock at month end.

### 10.3 Summary Row & Export
- Bottom of report contains a **Grand Total row** aggregating all numeric columns across all included products.
- Export to `report/BaoCao_Thang_<tháng>_Nam_<Năm>.csv` in UTF-8 format.
- UI maintains cache validity flag; resets to blank on cache invalidation until fresh generation is triggered.
