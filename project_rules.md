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
  - **Status tables**:
    - `inbound_status`: `PENDING`, `PROCESSING`, `COMPLETED`, `CANCELLED`
    - `outbound_status`: `PENDING`, `PROCESSING`, `COMPLETED`, `CANCELLED`
    - `delivery_status`: `PENDING`, `RECEIVING`, `COMPLETED`, `CANCELLED`
    - `shipment_status`: `PENDING`, `PICKING`, `COMPLETED`, `CANCELLED`
    - `unit_status`: `AVAILABLE`, `WARRANTY`, `OUT`, `SCRAPPED`
    - `inventory_check_status`: `PENDING`, `PROCESSING`, `COMPLETED`, `CANCELLED`
    - `adjustment_status`: `PENDING`, `PROCESSING`, `COMPLETED`, `CANCELLED`
  - **Type & reason tables**:
    - `inbound_type`: `PURCHASING` (`PUR`), `ADJUSTMENT_IN` (`ADJ`), `WARRANTY_CHECKING` (`WCK`), `WARRANTY_REPLACEMENT` (`WRP`), `CUSTOMER_RETURN` (`CRT`)
    - `outbound_type`: `SELLING` (`SEL`), `ADJUSTMENT_OUT` (`ADJ`), `WARRANTY_RETURN` (`WRT`), `WARRANTY_REPLACEMENT_TO_CUSTOMER` (`WRC`) *(deprecated: `RETURN_TO_SUPPLIER` / `RTS` removed)*
    - `check_type`: `UNIT` (detailed serial-level inventory check; `QUAN` check type removed)
    - `reason`: `STOCKTAKE_DIFF` (`STD`), `DAMAGED` (`DMG`), `OTHER` (`OTH`) *(deprecated: `DATA_ERROR` removed)*

---

## 3. Database Protection, Referential Integrity, and Deletion Scope

- Tables, columns, CSV files, and report files must **never be deleted**.
- **Referential Integrity**: A record must **never be deleted** if it is referenced by, or has an active relationship/dependency with, another table.
- **Strict Permanent Retention**: Recorded/Completed transactions, deliveries, shipments, units, status histories, inventory checks, adjustment movements, and selling status audit logs must **never be deleted**.
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
  - Database/CSV stores column `is_sold` as `BOOLEAN` (`1` / `0`), while the UI translates this column header as **`Trạng thái`**:
    - `1` (`true`) ➔ **`Đang kinh doanh`**: Product is actively traded / eligible for transactions.
    - `0` (`false`) ➔ **`Ngừng kinh doanh`**: Product is discontinued / stopped trading.
- **Strict Stock Discontinuance Precondition**:
  - A product's `is_sold` flag can **ONLY** be changed to `FALSE` (`"Ngừng kinh doanh"`) if the total warehouse quantity of that product is strictly **zero (0)**:
    $$\text{total\_quantity} = \text{available\_quantity} + \text{warranty\_quantity} + \text{scrapped\_quantity} == 0$$
  - If a product still has any inventory remaining in the warehouse ($\text{total\_quantity} > 0$), any attempt to change `is_sold` to `FALSE` must be **strictly rejected** by both the business layer (`UserUpdate`) and the UI with an explicit error alert (cannot discontinue an item while physical stock is still held).
- **Mandatory Audit Logging into `IS_SOLD_CHANGING`**:
  - Every time `product.is_sold` changes status (whether transitioning from `TRUE` to `FALSE` or from `FALSE` to `TRUE`), the system must atomically record an immutable history record into table **`IS_SOLD_CHANGING`**:
    - `history_id`: auto-increment primary key
    - `product_id`: foreign key referencing `PRODUCT.product_id`
    - `date`: current date of the change
    - `old_status`: previous boolean status
    - `new_status`: new boolean status
- **Transaction Ineligibility Rule**:
  - If `product.is_sold == FALSE` (`"Ngừng kinh doanh"`), the product is discontinued and must **never be selected** for any new transaction orders (neither Inbound nor Outbound).
  - The UI must filter out / hide discontinued products from dropdowns and product pickers during transaction creation (conforming to Note 6 of `QuanLyKhoHang_schema.json`).

### 4.3 Coupled Creation Only (Transactions, Delivery, Shipment)
Users may manually create transactions and logistics records strictly under atomic pairwise constraints:
- **Inbound Pair & Quantity Equality Invariant**:
  - Creating an Inbound Transaction requires simultaneously creating its paired `Delivery` records (either a single delivery or multiple delivery batches).
  - For every product in the transaction, the total allocated quantity across all delivery records must be **strictly equal** to the ordered quantity in the Inbound Transaction ($\sum \text{delivery\_detail.received\_quantity} == \text{inbound\_detail.ordered\_quantity}$). Under-allocation and over-allocation are strictly prohibited.
- **Outbound Pair & Quantity Equality Invariant**:
  - Creating an Outbound Transaction requires simultaneously creating its paired `Shipment` records (either a single shipment or multiple shipment batches).
  - **Permitted Outbound Quantity Exceeding Stock**: Outbound transaction order quantities (`outbound_detail.ordered_quantity` and shipment planned quantities) are **permitted to exceed current warehouse stock** (allowing backorders and order reservation ahead of inventory intake). Physical unit dispatching (`actual_quantity`) during shipment fulfillment remains bound to attaching currently available eligible units in the warehouse.
  - For every product in the transaction, the total allocated quantity across all shipment records must be **strictly equal** to the ordered quantity in the Outbound Transaction ($\sum \text{shipment\_detail.received\_quantity} == \text{outbound\_detail.ordered\_quantity}$).
- **Strict Post-Creation Lock**:
  - All deliveries/shipments associated with a transaction must be declared, allocated, and created atomically at transaction creation time.
  - Once the transaction order is created and committed to the database, **it is strictly prohibited to create additional `Delivery` or `Shipment` records for that transaction**.
- **Mandatory `source_code` Reference for `ADJ` Transactions**:
  - If the transaction `type` is `ADJUSTMENT_IN` or `ADJUSTMENT_OUT`, `source_code` is **mandatory** and must reference a valid `check_code` from an existing `INVENTORY_CHECK` record (e.g., `CHK-UNIT-000001`).
  - Standalone creation of an `ADJ` transaction without a referencing `check_code` is strictly prohibited.
- **Product Scope Constraint for Logistics**:
  - `delivery_detail` may only contain products declared in `inbound_detail`.
  - `shipment_detail` may only contain products declared in `outbound_detail`.
- **Logistics Date Constraints**:
  - `delivery.delivery_at >= inbound_transaction.order_at` and `delivery.expected_at >= inbound_transaction.order_at`.
  - `shipment.ship_at >= outbound_transaction.order_at` and `shipment.expected_at >= outbound_transaction.order_at`.
  - Standalone creation of transactions without logistics (or vice versa), or standalone creation of `Unit` records via raw CRUD, is strictly prohibited.

### 4.4 Unit Status Lifecycle and Transition Rules

Unit status transitions must adhere strictly to the business rules of each transaction type. Any stock intake, dispatch, or unit status transition affecting a product currently covered by an open `INVENTORY_CHECK` (`PENDING` / `PROCESSING`) is blocked by **Stock Freeze (Rule 5.1.3)**:

#### 4.4.1 Inbound Transactions
- **Universal Inbound Precondition**:
  - Goods entering the warehouse must originate outside the active warehouse inventory.
  - Therefore, **all Inbound operations without exception** (`PUR`, `WRP`, `WCK`, `CRT`, `ADJ`) are strictly restricted to units whose `old_status` is either:
    1. **`NULL` / uncreated** (`std::monostate`): Brand new unit entering the system for the first time.
    2. **`OUT`**: Unit previously dispatched/sold that is returning or being re-admitted into the warehouse.
  - Attempting to perform any Inbound operation on a unit that is currently inside the warehouse (`old_status` is `AVAILABLE`, `WARRANTY`, or `SCRAPPED`) is **strictly prohibited** and must be rejected.
- **Target Status Invariant**:
  - The new/target status of **all units** in an Inbound transaction **must never be `OUT`** (must transition into an internal warehouse status: `AVAILABLE`, `WARRANTY`, or `SCRAPPED`).
- **Specific Inbound Type Rules**:
  - **`PUR` (Purchasing)**:
    - Precondition: Unit is newly acquired (`old_status == NULL`).
    - Transition: `NULL -> AVAILABLE` (normal stock) or `SCRAPPED` (defective upon arrival).
    - Automatically generates a unique Serial (`SN-` + 7 alphanumeric characters) and registers the unit into `unit_inventory`.
  - **`WRP` (Warranty Replacement)**:
    - Precondition: Unit is a replacement item from supplier (`old_status == NULL`).
    - Transition: `NULL -> WARRANTY`.
    - Automatically generates a unique Serial and registers the unit into `unit_inventory` with status `WARRANTY`.
  - **`WCK` (Warranty Checking)**:
    - Precondition: Unit was previously sold/dispatched (`old_status == OUT`).
    - Transition: `OUT -> WARRANTY`.
  - **`CRT` (Customer Return)**:
    - Precondition: Unit was previously sold/dispatched (`old_status == OUT`).
    - Transition: `OUT -> AVAILABLE`, `WARRANTY`, or `SCRAPPED` (depending on returned condition; target must never remain `OUT`).
  - **`ADJ` (Adjustment In)**:
    - Precondition: Must strictly obey the universal inbound precondition: unit's `old_status` must be **`NULL`** (surplus unrecorded unit discovered during audit) or **`OUT`** (missing unit previously written off as out, now rediscovered).
    - Transition: Permitted to transition from **`OUT` or `NULL`** to **`AVAILABLE`**, **`SCRAPPED`**, or **`WARRANTY`**.
    - If `old_status == NULL`, automatically generates a unique Serial (`SN-` + 7 alphanumeric characters) and registers the unit into `unit_inventory`.
    - Requirement: Must have `source_code` referencing `check_code`, and `customer_id` must be `NULL`.
    - *Note on internal status adjustments*: Units already in warehouse changing status internally (e.g., `AVAILABLE -> SCRAPPED` due to warehouse damage) do not use Inbound `ADJ`; they are handled by `ADJUSTMENT_MOVEMENT`.

#### 4.4.2 Outbound Transactions
- **Universal Outbound Target Invariant**:
  - The target/new status of **all units** in an Outbound transaction **must strictly be `OUT`**.
- **Specific Outbound Type Rules**:
  - **`SEL` (Selling)**:
    - Precondition: Selected units must have current status `AVAILABLE`.
    - Transition: `AVAILABLE -> OUT`.
  - **`WRT` (Warranty Return)**:
    - Precondition: Selected units must have current status `SCRAPPED` or `WARRANTY`.
    - Transition: `SCRAPPED / WARRANTY -> OUT`.
  - **`WRC` (Warranty Replacement to Customer)**:
    - Precondition: Selected units must have current status `AVAILABLE` or `WARRANTY`.
    - Transition: `AVAILABLE / WARRANTY -> OUT`.
  - **`RTS` (Return to Supplier — Deprecated / Removed)**:
    - No longer supported for creation in the system or UI (lookup entry is retained in CSV solely to preserve historical referential integrity).
  - **`ADJ` (Adjustment Out)**:
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
Once a Transaction + `Delivery`/`Shipment` pair (along with its attached `Unit` transitions) is recorded:
- **No Deletion**: Recorded transactions, deliveries, shipments, units, and status histories must **never be deleted**.
- **Permitted Updates (`UserUpdate`)**:
  - **`status`**: Users may modify the `status` field of Transactions, `Delivery`, and `Shipment` during order lifecycle processing (`PENDING` -> `PROCESSING` -> `COMPLETED`).
  - **`delivery_at` & `ship_at`**: Users are permitted to update actual logistics dates, satisfying `delivery.delivery_at >= inbound_transaction.order_at` and `shipment.ship_at >= outbound_transaction.order_at`.
  - **Flexible Unit Target Status Switching During In-Progress Logistics**:
    - While a `Delivery` or `Shipment` is in progress (status is not yet `COMPLETED`), users are permitted to switch the target status (`new_status` / `curr_status`) of recorded units back and forth within valid domain boundaries:
      - Inbound `PUR`: between `AVAILABLE` and `SCRAPPED`.
      - Inbound `CRT`: among `AVAILABLE`, `WARRANTY`, and `SCRAPPED`.
      - Inbound `ADJ`: among `AVAILABLE`, `WARRANTY`, and `SCRAPPED`.
      - Outbound Transactions (`SEL`, `WRT`, `WRC`, `ADJ`): target status remains strictly `OUT`.
- **Strict Post-Completion Lock**:
  - Once an order, delivery, or shipment transitions to `COMPLETED`:
    - All unit status transitions, additions, and modifications are **strictly locked** (frozen/immutable).
  - Transitioning to `COMPLETED` requires a mandatory modal confirmation dialog with an explicit acknowledgment checkbox.
- **Detail Line Scope Immutability**:
  - Just as `inbound_detail` and `outbound_detail` cannot have products added or removed after transaction order creation:
  - **`inventory_check_quantity`**: The list of products audited in an `INVENTORY_CHECK` must be declared upfront upon check creation and **cannot be added or removed later**.
  - **`adjust_movement_detail`**: The list of adjustment lines in an `ADJUSTMENT_MOVEMENT` must be declared upfront upon movement order creation and **cannot be added or removed later**.

### 4.6 Read-Only Master Data & Direct Unit Restrictions
Users must **never** directly edit:
- Any `Status`, `Type`, or `Reason` master table (`INBOUND_TYPE`, `INBOUND_STATUS`, `OUTBOUND_TYPE`, `OUTBOUND_STATUS`, `DELIVERY_STATUS`, `SHIPMENT_STATUS`, `UNIT_STATUS`, `INVENTORY_CHECK_STATUS`, `ADJUSTMENT_STATUS`, `CHECK_TYPE`, `REASON`).
- `unit_inventory.status` directly through raw editing (a unit's status may only change via `Delivery`, `Shipment`, or `Adjustment Movement` workflows).

---

## 5. Inventory Check and Internal Adjustment Management

### 5.1 Unit Inventory Check (`CHK-UNIT` — Sole Audit Mode)
The warehouse audit system applies **strictly Unit-Level Detailed Inventory Checking (`CHK-UNIT`, `check_type = "UNIT"`)**; aggregate quantity checking (`QUAN`) has been completely removed:
- Evaluates and verifies physical warehouse stock at the individual unit Serial level (`unit_inventory.serial`).
- Purpose: Reconcile Serial identities, detect unrecorded surplus or rediscovered previously-dispatched items, record missing units, and verify physical quality/status shifts.
- Creates records simultaneously across two tables: `inventory_check_quantity` (product-level aggregated progress and initial system snapshot) and `inventory_check_unit` (individual unit serial verification details).

### 5.1.1 Pre-Declaration of Audited Products & Scope Immutability
- **Mandatory Upfront Product Declaration**:
  - Analogous to transaction orders, every `INVENTORY_CHECK` **must explicitly pre-declare the exact list of audited products (`product_id`)** at check creation time.
  - Declared products are recorded in `inventory_check_quantity` alongside a system quantity snapshot taken at creation (`system_available_quan`, `system_warranty_quan`, `system_scrapped_quan`).
- **Strict Post-Creation Lock**:
  - Once an inventory check is created and persisted in the database, its product scope is **frozen/immutable**.
  - **Adding, removing, or modifying** the product list in an existing inventory check is strictly prohibited.
  - Serials scanned or verified in `inventory_check_unit` must strictly belong to products declared in `inventory_check_quantity`.

### 5.1.2 Core Separation of Stocktake Snapshot and Ledger Adjustment
- **Non-Mutating Audit Worksheet Principle**:
  - `INVENTORY_CHECK`, `inventory_check_quantity`, and `inventory_check_unit` serve strictly as **audit worksheets and point-in-time verification records**.
  - Recording physical counts or scanning serials during an audit (`PENDING` / `PROCESSING` / `COMPLETED`) **must never directly mutate** inventory counters on `product` (`available_quantity`, `warranty_quantity`, `scrapped_quantity`) or `unit_inventory.status`.
- **Mandatory Document-Driven Reconciliation**:
  - All stock quantity adjustments or internal serial status changes resulting from an audit must execute exclusively through formal adjustment documents (`ADJUSTMENT_IN`, `ADJUSTMENT_OUT`, or `ADJUSTMENT_MOVEMENT`) linked via `source_code = check_code`, preserving an immutable trail in `status_history`.

### 5.1.3 Inventory Stock Freeze & Concurrency Control During Auditing
- **Scope & Activation Trigger**:
  - A **per-`product_id` stock freeze** is enforced whenever a product appears in `inventory_check_quantity` of any open `INVENTORY_CHECK` currently in **`PENDING`** or **`PROCESSING`** status.
  - Products outside the scope of any open inventory check continue normal inbound, outbound, and internal adjustment operations (localized audits do not block unrelated products).
- **Blocked Operations While Frozen**:
  - **Inbound Receiving (`Delivery`)**: Blocks `receiveUnitsForDelivery` and `receiveUnitsForDeliveryBatch` if any item belongs to a frozen product.
  - **Outbound Dispatching (`Shipment`)**: Blocks `dispatchUnitsForShipment` and `dispatchUnitsForShipmentBatch` if any item belongs to a frozen product.
  - **Logistics Unit Status Switching**: Blocks `changeUnitStatusInLogistics` on units belonging to a frozen product.
  - **Internal Adjustment Movements**: Blocks `executeAdjustmentMovement` and `executeAdjustmentMovementUnits` on any frozen product.
  - **Overlapping Inventory Checks**: Blocks `createInventoryCheck` if any selected product overlaps with another open check in `PENDING` or `PROCESSING`.
- **Release Trigger**:
  - The stock freeze on all audited products is automatically released immediately when the inventory check transitions to a terminal state: **`COMPLETED`** or **`CANCELLED`**.
- **Live Re-Validation & Atomic 1-Transaction Batch Execution**:
  - When opening the Post-Check Reconciliation Report (`InventoryCheckReconciliationDialog`), every discrepant serial is re-validated live against its current database state via `HashIndex` lookup on `unit_inventory.serial`; any row with a stale/conflicting state (e.g., already received, dispatched, or transitioned by another document after audit completion) is automatically unchecked, disabled, and annotated with a conflict reason.
  - All unit receiving (`Delivery`), dispatching (`Shipment`), and reconciliation document creation (`ADJUSTMENT_IN`, `ADJUSTMENT_OUT`, `ADJUSTMENT_MOVEMENT`) collect edits on the UI and commit in **a single atomic database transaction** (`beginTransaction()` -> create header + detail lines + apply unit transitions -> `commit()`, or full `rollback()` on any failure).

### 5.2 Detailed / Unit Inventory Check (`CHK-UNIT`) Workflow & Unit Adjustment Orders
- **Lifecycle & Status Transition Rules**:
  - Initialization: Every newly created check starts in **`PENDING`**.
  - Active Scanning: Transitions to **`PROCESSING`**; **reverting from `PROCESSING` back to `PENDING` is strictly prohibited**.
  - Completion: Transitions to **`COMPLETED`**; once `COMPLETED`, the inventory check is permanently locked against further edits.
  - Cancellation: Transitions to **`CANCELLED`** **only when** the check is in `PENDING` and **no serials or physical quantities have been recorded yet**; `CANCELLED` is also permanently locked.
- **Stock Freeze & Atomic Execution**:
  - Strictly adheres to **Stock Freeze**, **Live Re-Validation**, and **1-Transaction Atomic Commit** defined in **Section 5.1.3**.
- **Candidate Serial Initialization**:
  - All units currently residing in the warehouse (`AVAILABLE`, `WARRANTY`, `SCRAPPED`) for the declared products are pre-populated into `inventory_check_unit` with `scanned_status = NULL` (unverified).
  - The UI never marks units as verified upfront; a unit is recorded as physically present only after the user explicitly scans/enters its serial or applies verification.
- **Recorded Audit Data (`inventory_check_quantity` & `inventory_check_unit`)**:
  - System snapshots and actual counts per product are stored in `inventory_check_quantity`.
  - Per-unit audit rows are stored in `inventory_check_unit`:
    - `unit_id`: foreign key referencing `unit_inventory`
    - `system_status`: ledger status at check creation time
    - `scanned_status`: physical status observed during scan (initially `NULL`)
- **Standard WMS & Accounting Variance Convention**:
  - The system uses book/ledger inventory as the baseline reference according to the formula:
    $$\text{Variance} = \text{Physical Count} - \text{System Quantity}$$
    *(aligned with standard commercial WMS/ERP software such as KiotViet, Sapo, MISA, and Odoo)*:
  - **Surplus (`Variance > 0`, `+`)**: Physically present in warehouse but unrecorded on system (`NULL` or `OUT`) -> create **`ADJUSTMENT_IN`** to add to system stock.
  - **Missing (`Variance < 0`, `-`)**: Recorded on system but physically absent/lost -> create **`ADJUSTMENT_OUT`** to deduct from system stock.
- **3 Discrepancy Types & Handling**:
  1. **Surplus Units**: Serials never recorded in system (`old_status == NULL`) or previously dispatched serials rediscovered physically in warehouse (`old_status == OUT`).
     - Handled via Inbound **`ADJUSTMENT_IN`** (`OUT` or `NULL` -> `AVAILABLE`, `WARRANTY`, or `SCRAPPED`).
  2. **Missing Units**: Serials expected in warehouse (`AVAILABLE`, `WARRANTY`, `SCRAPPED`) not found during physical count (`scanned_status == NULL` upon completion).
     - Handled via Outbound **`ADJUSTMENT_OUT`** (internal warehouse status -> `OUT`).
  3. **Status Shift Discrepancies**: Serials physically present in warehouse whose observed condition differs from ledger status (e.g., ledger `AVAILABLE` vs. physical `SCRAPPED`).
     - Handled via internal **`ADJUSTMENT_MOVEMENT`** (`from_status -> to_status`).
- **Completion Gate**:
  - All 3 reconciliation document types (`ADJUSTMENT_IN`, `ADJUSTMENT_OUT`, `ADJUSTMENT_MOVEMENT`) **are ONLY permitted to be created** after the referencing `INVENTORY_CHECK` has transitioned to **`COMPLETED`**.
- **Explicit User Confirmation**:
  - Adjustment orders are generated only upon explicit user action and confirmation on the UI.

#### 5.2.1 Unit-Level Discrepancy Classification & Scanning Rules
- **Out-of-Scope Scan Rejection**:
  - Scanning a serial belonging to a product outside the pre-declared check scope must be immediately rejected with an error alert.
- **Normal Matched Unit**:
  - Condition: Candidate unit has `scanned_status == system_status`.
  - Action: Marked **Matched**, incrementing the corresponding `actual_*_quan` counter in `inventory_check_quantity`.
- **Missing / Deficit Unit**:
  - Condition: When the check transitions to `COMPLETED`, candidate unit still has `scanned_status == NULL`.
  - Action: Marked **Missing** (`-1`), preserved in database, queued for **`ADJUSTMENT_OUT`** (`system_status -> OUT`).
- **Surplus / Overage Unit** — two distinct cases:
  1. **Rediscovered Dispatched Unit (`system_status == OUT`)**:
     - Scanning a previously dispatched serial prompts a dialog to confirm its physical condition (`AVAILABLE`, `WARRANTY`, or `SCRAPPED`).
     - Recorded in `inventory_check_unit` with `system_status = "OUT"` and `scanned_status = <selected_status>`, incrementing the corresponding `actual_*_quan`.
  2. **Brand-New Unrecorded Unit (`old_status == NULL`)**:
     - Draft serial numbers are never pre-generated during an audit (preventing orphaned serials if the check is cancelled).
     - Recorded at product level via the **`"+ Ghi nhận hàng thừa chưa định danh"`** action on the product row in `inventory_check_quantity`, incrementing `actual_*_quan`.
     - Actual `SN-xxxxxxx` serials are generated only when creating/receiving the subsequent **`ADJUSTMENT_IN`** order.
- **Quality / Status Mismatch Unit**:
  - Condition: Unit is present in warehouse (`scanned_status` and `system_status` are both internal warehouse statuses) with `scanned_status != system_status`.
  - Action: Total quantity remains unchanged; `actual_<scanned_status>_quan` is incremented instead of `system_status`, flagged as a **Status Shift**, and queued for **`ADJUSTMENT_MOVEMENT`**.

#### 5.2.2 Post-Check Abnormal Unit Summarization Funnel
Once `INVENTORY_CHECK` reaches `COMPLETED`:
1. **Tier 1 — High-Level KPI Summary**:
   - Displays totals across System Book Stock, Physical Count, Matched, Missing, Surplus (Rediscovered `OUT` + Unrecorded Surplus), and Status Shifts.
   - Provides an **"Abnormal Only"** (`"Chỉ hiển thị bất thường"`) filter toggle enabled by default after completion.
2. **Tier 2 — Product-Level Reconciliation Matrix (Strict No-Cross-Netting Rule)**:
   - Displays per-product counts: Matched, Surplus (`+`), Missing (`-`), and Status Shift (`⇄`).
   - **Horizontal netting is strictly prohibited**: if a single product has both surplus and missing units, separate Inbound and Outbound adjustment documents must be issued to preserve unit-level Serial accountability.
3. **Tier 3 — Action-Oriented Grouping into 3 Reconciliation Buckets**:
   - **Bucket 1 — Missing Units**: Groups missing units to create **`ADJUSTMENT_OUT`** (`OUT-ADJ-xxxxxx`).
   - **Bucket 2 — Surplus Units**: Groups rediscovered `OUT` and unrecorded surplus items to create **`ADJUSTMENT_IN`** (`IN-ADJ-xxxxxx`).
   - **Bucket 3 — Status Shifts**: Groups condition-mismatched units by `(product_id, from_status, to_status)` to create **`ADJUSTMENT_MOVEMENT`** (`ADJ-STD-xxxxxx`).
   - **Post-Creation Button Lock**: Once a reconciliation document is created with `source_code = check_code`, its creation button is disabled and displays the generated document code.

### 5.3 Internal Status Adjustment Movement (`ADJUSTMENT_MOVEMENT`)
- Dedicated to internal unit status transfers within the warehouse without altering total physical stock count.
- **Stock Freeze Constraint**: Blocked for any product currently frozen by an open inventory check (see **Section 5.1.3**).
- **Header Attributes**:
  - `adjust_id`: Primary Key
  - `adjust_code`: `ADJ-<reason_code>-<xxxxxx>` (e.g., `ADJ-STD-000001`, `ADJ-DMG-000002`, `ADJ-OTH-000003`)
  - `source_code`: Unique Foreign Key referencing `INVENTORY_CHECK.check_code` (mandatory when `reason == "STOCKTAKE_DIFF"`)
  - `reason`: References `REASON` (`STOCKTAKE_DIFF`, `DAMAGED`, `OTHER`; `DATA_ERROR` removed)
  - `adjusted_at`: Execution date
  - `status`: `PENDING`, `PROCESSING`, `COMPLETED`, `CANCELLED`
- **Detail Lines (`ADJUST_MOVEMENT_DETAIL`)**:
  - `adjust_detail_id`: Primary Key
  - `adjust_id`: Foreign Key referencing parent movement
  - `product_id`: Foreign Key referencing `product`
  - `from_status`: Source status (`AVAILABLE`, `WARRANTY`, `SCRAPPED`)
  - `to_status`: Target status (`AVAILABLE`, `WARRANTY`, `SCRAPPED`; never `OUT`)
  - `adjust_quantity`: Planned transfer quantity
  - `actual_quantity`: Executed transfer quantity
- **Rules by Reason (`reason`)**:
  - **`STOCKTAKE_DIFF`**: Must specify a valid `source_code` referencing `check_code`, and all detail products must lie within that check's audited scope.
  - **`DAMAGED`**: Target status must strictly be `SCRAPPED` (`to_status == "SCRAPPED"` from `AVAILABLE` or `WARRANTY`).
  - **`OTHER`**: Ad-hoc internal status movement unrelated to an inventory check.
- **Upfront Detail Declaration & Immutability**:
  - All detail lines in `adjust_movement_detail` must be declared upfront upon movement creation.
  - After creation, **adding, modifying, or deleting detail rows in `adjust_movement_detail` is strictly prohibited**.

### 5.4 Referential Linkage, Audited Product Scope & Cardinality Invariant (`source_code` referencing `check_code`)
- **Mandatory Referential Linkage**:
  - When creating an `inbound_transaction` (`ADJUSTMENT_IN`), `outbound_transaction` (`ADJUSTMENT_OUT`), or `adjustment_movement` (`reason == "STOCKTAKE_DIFF"`), `source_code` **must reference an existing `INVENTORY_CHECK.check_code`** (e.g., `CHK-UNIT-000001`).
- **Audited Product Scope Constraint**:
  - Every product included in check-linked adjustment documents must exist in `inventory_check_quantity` for that referenced check.
- **Cardinality Invariant**:
  - `source_code` enforces a **UNIQUE (`[UQ]`)** constraint: each `check_code` may be linked to at most **one `ADJUSTMENT_IN` order**, **one `ADJUSTMENT_OUT` order**, and **one `ADJUSTMENT_MOVEMENT` document**.
- **No-Customer Invariant**:
  - Both Inbound and Outbound adjustment orders must have `customer_id == NULL`.

### 5.5 Lifecycle, Status Transition & Cancellation Rules
Applies uniformly across `inbound_transaction`, `outbound_transaction`, and `adjustment_movement` under the unified status set **`PENDING`**, **`PROCESSING`**, **`COMPLETED`**, **`CANCELLED`**:

- **Initialization**: Every new order/movement starts in **`PENDING`**.
- **Cancellation (`CANCELLED`)**:
  - Permitted **only** when the order is in **`PENDING`** and **actual execution progress is zero** (`actual_quantity == 0` across all lines, with zero received/dispatched units).
  - Cancelling a parent Inbound/Outbound transaction automatically cascades `CANCELLED` to all non-completed child `delivery`/`shipment` batches.
  - `CANCELLED` is a terminal state and cannot be reopened.
- **Processing & Completion (`PENDING` -> `PROCESSING` -> `COMPLETED`)**:
  - Transitions to **`PROCESSING`** once unit intake/dispatch/transfer begins.
  - **Completion (`COMPLETED`)**:
    - Normal completion: transitions to `COMPLETED` upon reaching 100% planned quantity.
    - **Under-Fulfillment Rule**: Allowed when actual quantity is below planned quantity only after explicit user confirmation via a modal warning dialog with a confirmation checkbox.
    - **Post-`COMPLETED` Immutability**: Once `COMPLETED`, the order and all associated `delivery`/`shipment` batches are permanently frozen (no new batches, no batch edits, no unit transitions).
- **Child Logistics Batch Statuses**:
  - `delivery_status`: `PENDING` -> `RECEIVING` -> `COMPLETED`, or `CANCELLED`.
  - `shipment_status`: `PENDING` -> `PICKING` -> `COMPLETED`, or `CANCELLED`.
  - Users may update batch statuses while the parent order has not reached a terminal state (`COMPLETED` / `CANCELLED`).

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
   - Format: `CHK-UNIT-<xxxxxx>` (6 digits, e.g., `CHK-UNIT-000001`), derived from `CHECK_TYPE.next_code_no` (only `UNIT` check type is used).
4. **Adjustment Movement Code (Auto-Generated)**:
   - Format: `ADJ-<reason_code>-<xxxxxx>` (6 digits, e.g., `ADJ-STD-000001`, `ADJ-DMG-000002`, `ADJ-OTH-000003`), derived from `REASON.next_code_no`.
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
9. **BOOLEAN Representation in Database & CSV**:
   - All `BOOLEAN` values must be uniformly represented as `"1"` (`true`) and `"0"` (`false`) in `valueToString`, CSV files, default values, crawler exports, and UI boolean filters.

---

## 7. Indexing, Query, and CRUD Architecture

### 7.1 Index Naming and Selection
- Index naming convention: Each column has at most one index, keyed directly by `columnName`.
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
     - **Unified Order Lifecycle & Status Transition Rules (Rule 5.5)**:
       - Applicable to `inbound_transaction`, `outbound_transaction`, and `adjustment_movement`:
       - **Unified Status Framework**: `PENDING`, `PROCESSING`, `COMPLETED`, `CANCELLED`.
       - **Cancellation (`CANCELLED`)**: An order/movement can ONLY be cancelled if it is currently in `PENDING` and no progress has occurred (`actual_quantity == 0` across all items/deliveries/shipments/movement lines). Once any units or shipments/deliveries are processed, cancellation is strictly forbidden.
       - **Synchronized Cancellation**: Cancelling an Inbound or Outbound transaction automatically transitions all associated non-completed `delivery` or `shipment` records to `CANCELLED`.
       - **Terminal State Lock**: A `CANCELLED` order is permanently closed and cannot be reopened or edited.
       - **Sequential In-Progress & Completion Transitions**: Transition from `PENDING` to `PROCESSING` occurs when processing begins. Transition to `COMPLETED` allows completion upon 100% fulfillment, or under-fulfilled completion with explicit user confirmation popup & checkbox; upon transition to `COMPLETED`, all deliveries/shipments are permanently locked against execution or editing.
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
     - **Post-Completion Logistics Lock**: Reject adding deliveries or shipments after transaction completion (`status == COMPLETED`).

---

## 8. UI Rules (Qt Widgets)

### 8.1 Architecture & Separation of Concerns
- Use **Qt Widgets** with C++17; store all UI code in `ui/`.
- UI classes may freely use Qt containers and presentation widgets, but must remain strictly separated from database storage, custom data structures, and core inventory business logic.
- UI code must **never** write custom queries, execute raw table scans (`getTable()->getRows()`), manipulate internal table pointers, or directly mutate table rows.
- The `Vector<Record> results` contain `*_id` columns; **do not display primary key IDs on the screen**. They are used exclusively as internal metadata for CRUD operations.

### 8.1.1 UI Localization Convention (Vietnamese Display Labels)
Database and CSV records retain original English codes (`PENDING`, `PURCHASING`, etc.) and `1`/`0` boolean values to preserve foreign key and schema integrity; the UI layer (`ui/`) renders and filters values using the following standardized Vietnamese labels:
- **Order / Batch / Inventory Check / Movement Status**:
  - `PENDING` -> `Chờ xử lí`
  - `PROCESSING` -> `Đang xử lí`
  - `RECEIVING` -> `Đang nhận hàng`
  - `PICKING` -> `Đang lấy hàng`
  - `COMPLETED` -> `Hoàn thành`
  - `CANCELLED` -> `Đã hủy`
- **Unit Status (`unit_status`)**:
  - `AVAILABLE` -> `Khả dụng`, `WARRANTY` -> `Bảo hành`, `SCRAPPED` -> `Lỗi/hỏng`, `OUT` -> `Đã xuất`
- **Inbound Order Type (`inbound_type`)**:
  - `PURCHASING` (`PUR`) -> `Mua mới`
  - `ADJUSTMENT_IN` (`ADJ`) -> `Nhập điều chỉnh`
  - `WARRANTY_CHECKING` (`WCK`) -> `Nhận bảo hành`
  - `WARRANTY_REPLACEMENT` (`WRP`) -> `Đổi BH hãng`
  - `CUSTOMER_RETURN` (`CRT`) -> `Khách trả hàng`
- **Outbound Order Type (`outbound_type`)**:
  - `SELLING` (`SEL`) -> `Bán hàng`
  - `ADJUSTMENT_OUT` (`ADJ`) -> `Xuất điều chỉnh`
  - `WARRANTY_RETURN` (`WRT`) -> `Gửi BH hãng`
  - `WARRANTY_REPLACEMENT_TO_CUSTOMER` (`WRC`) -> `Đổi BH khách`
  - *(Deprecated: `RETURN_TO_SUPPLIER` / `RTS` removed)*
- **Adjustment Reason (`reason`)**:
  - `STOCKTAKE_DIFF` (`STD`) -> `Lệch kiểm kê`, `DAMAGED` (`DMG`) -> `Hư hỏng`, `OTHER` (`OTH`) -> `Khác`
- **Check Type (`check_type`) & History Type (`status_history.type`)**:
  - `UNIT` -> `Theo Serial`; `INBOUND` -> `Nhập kho`, `OUTBOUND` -> `Xuất kho`, `ADJUSTMENT` -> `Điều chuyển`
- **Product Business Status (`product.is_sold`, `is_sold_changing.old_status` / `new_status`)**:
  - UI column & filter header: `is_sold` -> **`Trạng thái`**
  - UI display & filter values: `true` / `1` -> **`Đang kinh doanh`**, `false` / `0` -> **`Ngừng kinh doanh`** *(database and CSV retain column `is_sold` and `1`/`0` per Rule 6.9)*.
- **Compact UI Table & Highlight Conventions**:
  - Progress column header uses **`Thực nhận`** in Inbound views and **`Thực xuất`** in Outbound views.
  - Status mismatch badge in the inventory check unit table renders compactly as **`Lệch`** to allocate extra horizontal width to the product code column (`Mã SP`).
  - Data tables preserve single-cell selection (`SelectItems`) for exact copy-paste while applying a light-blue full-row highlight via `RowHighlightDelegate`.

### 8.2 Dedicated Windows and Dialogs for CRUD & Operations
- **Product Management View**:
  - Conforms to Note 9 of `QuanLyKhoHang_schema.json`: The main table view displays `PRODUCT_QUANTITY_QUERY` (`product_code`, `barcode`, `sku`, `product_name`, `available_quantity`, `warranty_quantity`, `scrapped_quantity`, `is_sold` displayed as `"Trạng thái"`).
  - Expanding or opening the product sub-window shows full detailed specifications (`product_query` + hardware category specs).
  - **Discontinuance Switch (`is_sold`)**:
    - The UI provides a toggle/switch for product business status (`"Đang kinh doanh"` / `"Ngừng kinh doanh"`).
    - If the user attempts to switch `is_sold` to `FALSE` (`"Ngừng kinh doanh"`) while total quantity > 0, the UI must block the switch and display a warning dialog (`QMessageBox::warning`: `"Không thể ngừng kinh doanh sản phẩm khi số lượng tồn kho vẫn còn lớn hơn 0"`).
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
  - **Delivery & Shipment Changed Units Inspection View**: Shows all units whose status was altered by that batch (Product Name, Serial, Old Status, New Status) and features a `"Tổng kết trạng thái"` (Status Summary) button aggregating unit counts by status using `HashTable`.
  - **Order Completion Confirmation Dialog**: Modal popup with mandatory confirmation checkbox before setting status to `COMPLETED`.
- **Inventory Check Interface (`CHK-UNIT` Sole Mode)**:
  - **Upfront Product Scope Declaration**: When creating a new inventory check, the user selects the product scope upfront; upon confirmation, this scope is frozen into `inventory_check_quantity`.
  - **Unit-Level Split View (`CHK-UNIT`)**:
    - *Top Summary Table (`inventory_check_quantity`)*: Tracks per-product progress (Initial Snapshot vs. Physical Count) with separate `Surplus (+)` / `Missing (-)` / `Status Shift (⇄)` columns (strictly no horizontal netting).
    - *Bottom Detail Panel (`inventory_check_unit` + Barcode/Serial Scanner)*: Supports continuous Serial scanning with per-unit reconciliation status (`Serial | System Status | Scanned Status | Discrepancy Badge`).
    - **Color-Coded Badges & Scan Behavior**:
      - `Matched` (`Khớp`): Green badge (`scanned_status == system_status`).
      - `Missing` (`Thiếu`): Red/orange badge upon completion when `scanned_status == NULL`.
      - `Surplus - Rediscovered OUT` (`Thừa - Tái PH`): Blue/orange badge when scanning a serial with `system_status == OUT`; opens a prompt to select physical condition (`AVAILABLE` / `WARRANTY` / `SCRAPPED`).
      - `Surplus - Unrecorded` (`Thừa mới`): Recorded via `"+ Ghi nhận hàng thừa chưa định danh"` on the product row without pre-generating draft serials before creating `ADJUSTMENT_IN`.
      - `Status Shift` (`Lệch`): Purple badge when `scanned_status != system_status` between internal warehouse statuses.
      - Scanning any serial outside the check scope is immediately rejected with a warning.
    - **Post-Completion Reconciliation Funnel**:
      - Displays KPI summary cards (Book Stock, Physical Count, Matched, Missing, Surplus, Status Shifts) and auto-enables the `"[✓] Chỉ hiển thị bất thường"` filter.
      - Provides 3 action tabs with dedicated creation buttons (`"Tạo đơn xuất điều chỉnh"`, `"Tạo đơn nhập điều chỉnh"`, `"Tạo phiếu điều chuyển nội bộ"`). Documents are created only upon explicit confirmation; afterward, each button locks and displays the generated document code.
- **Internal Adjustment Movement Interface**:
  - **Upfront Detail Declaration**: When creating a movement document, the user declares all detail lines (`product_id`, `from_status`, `to_status`, `adjust_quantity`); once saved, `adjust_movement_detail` is permanently locked.
  - Displays data from `ADJUSTMENT_MOVEMENT` and `adjust_movement_detail`.
  - Shows `adjust_code`, `source_code` (referencing `check_code`), `reason` (`STOCKTAKE_DIFF`, `DAMAGED`, `OTHER`), and `status` (`PENDING`, `PROCESSING`, `COMPLETED`, `CANCELLED`).

### 8.3 Navigation & Menus
- **Warehouse vs. Reference Separation**: All category tables, specification sub-tables, `Category`, and `Brand` are isolated under menus `"Danh mục sản phẩm"` and `"Danh mục phụ"`.
- **Customer Domain**: Isolated under menu `"Khách hàng"` (sub-views `"Khách hàng cá nhân"` and `"Khách hàng doanh nghiệp"`).
- **Inventory Audit Domain**: Dedicated top menu `"Kiểm kê kho"` for inventory checks and internal adjustment movements.
- **Report Domain**: Dedicated top menu `"Báo cáo"` for Periodic Inventory Movement Report.

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

## 10. Periodic Inventory Movement Report

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
- Export to `report/BaoCao_Thang_<MM>_Nam_<YYYY>.csv` in UTF-8 format.
- UI maintains cache validity flag; resets to blank on cache invalidation until fresh generation is triggered.
