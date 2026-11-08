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
    - `outbound_type`: `SELLING` (`SEL`), `ADJUSTMENT_OUT` (`ADJ`), `WARRANTY_RETURN` (`WRT`), `WARRANTY_REPLACEMENT_TO_CUSTOMER` (`WRC`)
    - `check_type`: `UNIT` (Kiểm kho chi tiết theo từng serial; loại `QUAN` đã bỏ)
    - `reason`: `STOCKTAKE_DIFF` (`STD`), `DAMAGED` (`DMG`), `OTHER` (`OTH`) (lý do `DATA_ERROR` đã bỏ)

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
- **Strict Post-Creation Lock (Khóa tạo mới Delivery/Shipment sau khi tạo đơn)**:
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
- **Strict Post-Completion Lock (Khóa hoàn toàn khi chuyển COMPLETED)**:
  - Once an order, delivery, or shipment transitions to `COMPLETED`:
    - All unit status transitions, additions, and modifications are **strictly locked** (frozen/immutable).
  - Transitioning to `COMPLETED` requires a mandatory modal confirmation dialog with an explicit acknowledgment checkbox.
- **Detail Line Scope Immutability (Bất biến danh mục chi tiết sau khi tạo phiếu)**:
  - Just as `inbound_detail` and `outbound_detail` cannot have products added or removed after transaction order creation:
  - **`inventory_check_quantity`**: The list of products audited in an `INVENTORY_CHECK` must be declared upfront upon check creation and **cannot be added or removed later**.
  - **`adjust_movement_detail`**: The list of adjustment lines in an `ADJUSTMENT_MOVEMENT` must be declared upfront upon movement order creation and **cannot be added or removed later**.

### 4.6 Read-Only Master Data & Direct Unit Restrictions
Users must **never** directly edit:
- Any `Status`, `Type`, or `Reason` master table (`INBOUND_TYPE`, `INBOUND_STATUS`, `OUTBOUND_TYPE`, `OUTBOUND_STATUS`, `DELIVERY_STATUS`, `SHIPMENT_STATUS`, `UNIT_STATUS`, `INVENTORY_CHECK_STATUS`, `ADJUSTMENT_STATUS`, `CHECK_TYPE`, `REASON`).
- `unit_inventory.status` directly through raw editing (a unit's status may only change via `Delivery`, `Shipment`, or `Adjustment Movement` workflows).

---

## 5. Inventory Check and Internal Adjustment Management (Kiểm Kê Kho và Điều Chỉnh Nội Bộ)

### 5.1 Unit Inventory Check (`CHK-UNIT` - Loại Kiểm Kê Duy Nhất)
Hệ thống kiểm kê kho áp dụng **duy nhất hình thức Kiểm kê chi tiết theo đơn vị cá thể (`CHK-UNIT`, check_type = "UNIT")**; hình thức kiểm kê số lượng tổng (`QUAN`) đã bị loại bỏ hoàn toàn:
- Đánh giá và kiểm tra thực tế tại kho ở mức từng cá thể Serial cụ thể (`unit_inventory.serial`).
- Mục đích: Đối soát danh tính Serial, phát hiện hàng dư chưa định danh hoặc hàng đã xuất tái xuất hiện, ghi nhận hàng thiếu, và kiểm tra tình trạng vật lý (lệch trạng thái phẩm chất).
- Tạo đồng thời 2 bảng: `inventory_check_quantity` (bảng theo dõi tiến độ tổng hợp theo sản phẩm và snapshot ban đầu) và `inventory_check_unit` (chi tiết kiểm tra từng số Serial).

### 5.1.1 Pre-Declaration of Audited Products & Scope Immutability (Khai báo trước phạm vi sản phẩm và tính bất biến)
- **Mandatory Upfront Product Declaration (Bắt buộc khai báo trước danh sách sản phẩm kiểm kê)**:
  - Tương tự như các đơn hàng, mỗi đợt `INVENTORY_CHECK` **bắt buộc phải khai báo trước chính xác các sản phẩm (`product_id`) sẽ tham gia kiểm kê** ngay tại thời điểm tạo đợt kiểm kho.
  - Các sản phẩm được khai báo sẽ được ghi nhận vào `inventory_check_quantity` kèm bản chụp số lượng hệ thống tại thời điểm đó (`system_available_quan`, `system_warranty_quan`, `system_scrapped_quan`).
- **Strict Post-Creation Lock (Khóa hoàn toàn sau khi tạo đợt kiểm kê)**:
  - Sau khi đợt kiểm kê được tạo và lưu vào cơ sở dữ liệu, danh mục sản phẩm kiểm kê bị **khóa bất biến (frozen/immutable)**.
  - **Tuyệt đối cấm thêm, bớt hoặc chỉnh sửa** danh sách sản phẩm trong đợt kiểm kê đó.
  - Các cá thể Serial được quét hoặc xác minh trong `inventory_check_unit` bắt buộc phải thuộc các sản phẩm đã khai báo trong `inventory_check_quantity`.

### 5.1.2 Core Separation of Stocktake Snapshot and Ledger Adjustment (Nguyên tắc tách biệt Phiếu Kiểm Kê và Chứng Từ Điều Chỉnh)
- **Non-Mutating Audit Worksheet Principle**:
  - `INVENTORY_CHECK`, `inventory_check_quantity`, và `inventory_check_unit` đóng vai trò là **bảng tính đối soát và ghi nhận hiện trạng tại thời điểm kiểm kê**.
  - Việc ghi nhận số lượng thực tế hay quét mã serial trong quá trình kiểm kê (`PENDING` / `PROCESSING` / `COMPLETED`) **tuyệt đối không được trực tiếp làm thay đổi** số lượng tồn kho trong `product` (`available_quantity`, `warranty_quantity`, `scrapped_quantity`) hay trạng thái `unit_inventory.status`.
- **Mandatory Document-Driven Reconciliation**:
  - Mọi điều chỉnh tăng/giảm tồn kho hoặc chuyển đổi trạng thái serial sau kiểm kê đều phải thông qua các chứng từ điều chỉnh chính thức (`ADJUSTMENT_IN`, `ADJUSTMENT_OUT`, hoặc `ADJUSTMENT_MOVEMENT`) có liên kết `source_code = check_code`, bảo toàn lịch sử bất biến trong `status_history`.

### 5.2 Detailed / Unit Inventory Check (`CHK-UNIT`) Workflow & Unit Adjustment Orders
- **Trạng thái vòng đời của đợt kiểm kê**:
  - Khởi tạo: Bắt đầu ở trạng thái **`PENDING`**.
  - Đang kiểm kê / quét serial: Chuyển sang **`PROCESSING`**.
  - Hoàn tất kiểm kê: Chuyển sang **`COMPLETED`**.
  - Hủy đợt: Chuyển sang **`CANCELLED`** (chỉ cho phép khi ở `PENDING` và chưa quét bất kỳ serial nào).
- **Khởi tạo danh sách Serial ứng viên**:
  - Toàn bộ các unit hiện có trong kho (`AVAILABLE`, `WARRANTY`, `SCRAPPED`) của các sản phẩm đã khai báo sẽ được nạp vào `inventory_check_unit` với `scanned_status = NULL` (chưa xác minh).
  - Giao diện không đánh dấu sẵn là đã kiểm; chỉ khi người dùng quét/nhập số serial cụ thể thì serial đó mới được ghi nhận trạng thái thực tế.
- **Dữ liệu ghi nhận trong `inventory_check_quantity` & `inventory_check_unit`**:
  - Snapshot số lượng sản phẩm lưu tại `inventory_check_quantity`.
  - Từng serial ứng viên lưu tại `inventory_check_unit`:
    - `unit_id`: tham chiếu đến `unit_inventory`
    - `system_status`: trạng thái trên sổ sách tại thời điểm lập phiếu
    - `scanned_status`: tình trạng thực tế ghi nhận khi quét (ban đầu là `NULL`)
- **Phát hiện và xử lý 3 loại chênh lệch**:
  1. **Surplus Units (Hàng thừa / phát hiện thêm)**: Serial chưa từng có trong hệ thống (`old_status == NULL`) hoặc serial đã từng xuất kho nay tìm thấy lại trong kho (`old_status == OUT`).
     - Lập đơn **Nhập điều chỉnh (`ADJUSTMENT_IN`)** chuyển trạng thái từ `OUT` hoặc `NULL` sang `AVAILABLE`, `WARRANTY`, hoặc `SCRAPPED`.
  2. **Missing Units (Hàng thiếu / thất thoát)**: Serial có trên sổ sách trong kho (`AVAILABLE`, `WARRANTY`, `SCRAPPED`) nhưng không tìm thấy thực tế (`scanned_status == NULL`).
     - Lập đơn **Xuất điều chỉnh (`ADJUSTMENT_OUT`)** chuyển trạng thái từ trong kho sang `OUT`.
  3. **Status Shift Discrepancies (Lệch trạng thái / phẩm chất)**: Serial có mặt thực tế trong kho nhưng tình trạng thực tế khác với sổ sách (ví dụ: sổ là `AVAILABLE` nhưng thực tế hỏng -> `SCRAPPED`).
     - Lập phiếu **Điều chuyển nội bộ (`ADJUSTMENT_MOVEMENT`)** chuyển đổi giữa các trạng thái nội bộ trong kho (`from_status -> to_status`).
- **Cổng kiểm soát hoàn tất (Chỉ tạo đơn sau khi đợt kiểm kê đã COMPLETED)**:
  - Cả 3 loại chứng từ điều chỉnh (`ADJUSTMENT_IN`, `ADJUSTMENT_OUT`, `ADJUSTMENT_MOVEMENT`) **CHỈ ĐƯỢC PHÉP TẠO** sau khi đợt kiểm kê đã chính thức chuyển sang trạng thái **`COMPLETED`**.
- **Xác nhận người dùng**:
  - Chỉ tạo các chứng từ điều chỉnh khi người dùng bấm nút xác nhận cụ thể trên giao diện.

#### 5.2.1 Unit-Level Discrepancy Classification & Scanning Rules (Quy tắc xử lý từng trường hợp Unit)
- **Out-of-Scope Scan Rejection (Chặn quét sản phẩm ngoài phạm vi kiểm kê)**:
  - Nếu quét serial thuộc sản phẩm không nằm trong danh mục đã khai báo của đợt kiểm kê, hệ thống phải lập tức từ chối và cảnh báo lỗi.
- **Normal Matched Unit (Unit khớp hoàn toàn)**:
  - Điều kiện: Unit ứng viên có `scanned_status == system_status`.
  - Xử lý: Đánh dấu **Khớp (`Matched`)**, tăng bộ đếm `actual_*_quan` tương ứng trong `inventory_check_quantity`.
- **Missing / Deficit Unit (Unit bị thiếu / vắng mặt)**:
  - Điều kiện: Khi đợt kiểm kê chuyển `COMPLETED`, unit ứng viên vẫn có `scanned_status == NULL`.
  - Xử lý: Đánh dấu **Thiếu (`Missing`, `-1`)**, không xóa khỏi cơ sở dữ liệu, chờ xử lý qua đơn **`ADJUSTMENT_OUT`** (`system_status -> OUT`).
- **Surplus / Overage Unit (Unit bị dư / thừa)** — phân biệt 2 tình huống:
  1. **Tái phát hiện Unit đã xuất (`system_status == OUT`)**:
     - Quét serial đã xuất trước đây: hiển thị hộp thoại xác nhận tình trạng thực tế (`AVAILABLE`, `WARRANTY`, hoặc `SCRAPPED`).
     - Ghi nhận vào `inventory_check_unit` với `system_status = "OUT"`, `scanned_status = <tình_trạng_chọn>`, tăng `actual_*_quan` tương ứng.
  2. **Hàng mới tinh chưa có Serial trên hệ thống (`old_status == NULL`)**:
     - Không sinh mã serial nháp trong quá trình kiểm kê (tránh serial mồ côi nếu hủy đợt).
     - Ghi nhận ở cấp độ sản phẩm bằng thao tác **"+ Ghi nhận hàng thừa chưa định danh"** trên dòng sản phẩm trong `inventory_check_quantity`, làm tăng `actual_*_quan`.
     - Serial `SN-xxxxxxx` chỉ được sinh ra khi thực hiện tiếp nhận qua `Delivery` của đơn **`ADJUSTMENT_IN`** sau đó.
- **Quality / Status Mismatch Unit (Lệch phẩm chất / trạng thái)**:
  - Điều kiện: Unit có mặt tại kho nhưng `scanned_status != system_status` (đều thuộc các trạng thái trong kho).
  - Xử lý: Tổng số lượng tồn kho không đổi, ghi nhận tăng `actual_<scanned_status>_quan` thay vì `system_status`, gắn nhãn **Lệch trạng thái**, chờ xử lý qua **`ADJUSTMENT_MOVEMENT`**.

#### 5.2.2 Post-Check Abnormal Unit Summarization Funnel (Phễu tổng kết bất thường)
Sau khi `INVENTORY_CHECK` chuyển sang `COMPLETED`:
1. **Tier 1 — High-Level KPI Summary**:
   - Hiển thị tổng số: Tồn sổ sách, Thực tế kiểm đếm, Khớp, Thiếu, Thừa (Tái phát hiện OUT + Hàng mới chưa định danh), Lệch trạng thái.
   - Nút bật/tắt **"Chỉ hiển thị bất thường"** (mặc định bật sau khi hoàn tất kiểm kê).
2. **Tier 2 — Product-Level Reconciliation Matrix (Quy tắc KHÔNG bù trừ ngang)**:
   - Hiển thị theo từng sản phẩm: Khớp, Thừa (+), Thiếu (-), Lệch trạng thái (⇄).
   - **Tuyệt đối không bù trừ ngang** giữa Thừa và Thiếu trên cùng một sản phẩm: vừa có hàng thừa vừa có hàng thiếu thì phải xử lý cả đơn nhập và đơn xuất riêng biệt để đảm bảo tính định danh của từng Serial.
3. **Tier 3 — Action-Oriented Grouping into 3 Reconciliation Buckets (Gom theo 3 hướng xử lý chứng từ)**:
   - **Nhánh 1 — Hàng thiếu**: Gom các unit thiếu để tạo đơn xuất điều chỉnh **`ADJUSTMENT_OUT`** (`OUT-ADJ-xxxxxx`).
   - **Nhánh 2 — Hàng thừa**: Gom các unit tái phát hiện và hàng chưa định danh để tạo đơn nhập điều chỉnh **`ADJUSTMENT_IN`** (`IN-ADJ-xxxxxx`).
   - **Nhánh 3 — Lệch trạng thái**: Gom các unit lệch phẩm chất theo bộ `(product_id, from_status, to_status)` để tạo phiếu điều chuyển nội bộ **`ADJUSTMENT_MOVEMENT`** (`ADJ-STD-xxxxxx`).
   - **Khóa thao tác sau khi lập chứng từ**: Khi chứng từ đã được tạo (gắn `source_code = check_code`), nút tạo tương ứng sẽ bị vô hiệu hóa và hiển thị mã phiếu đã lập.

### 5.3 Internal Status Adjustment Movement (`ADJUSTMENT_MOVEMENT`)
- Dùng riêng cho việc điều chuyển trạng thái linh kiện nội bộ trong kho mà không làm thay đổi tổng tồn kho.
- **Thuộc tính**:
  - `adjust_id`: Khóa chính
  - `adjust_code`: `ADJ-<reason_code>-<xxxxxx>` (ví dụ: `ADJ-STD-000001`, `ADJ-DMG-000002`, `ADJ-OTH-000003`)
  - `source_code`: Khóa ngoại duy nhất trỏ tới `INVENTORY_CHECK.check_code` (bắt buộc khi `reason == "STOCKTAKE_DIFF"`)
  - `reason`: Tham chiếu bảng `REASON` (`STOCKTAKE_DIFF`, `DAMAGED`, `OTHER`) *(lý do `DATA_ERROR` đã bỏ)*
  - `adjusted_at`: Ngày thực hiện
  - `status`: `PENDING`, `PROCESSING`, `COMPLETED`, `CANCELLED`
- **Chi tiết (`ADJUST_MOVEMENT_DETAIL`)**:
  - `adjust_detail_id`: Khóa chính
  - `adjust_id`: Khóa ngoại tham chiếu phiếu chính
  - `product_id`: Khóa ngoại sản phẩm
  - `from_status`: Trạng thái nguồn (`AVAILABLE`, `WARRANTY`, `SCRAPPED`)
  - `to_status`: Trạng thái đích (`AVAILABLE`, `WARRANTY`, `SCRAPPED`; không được là `OUT`)
  - `adjust_quantity`: Số lượng dự kiến điều chuyển
  - `actual_quantity`: Số lượng thực tế đã điều chuyển
- **Quy tắc theo lý do (`reason`)**:
  - **`STOCKTAKE_DIFF` (Lệch kiểm kê)**: Bắt buộc có `source_code` trỏ tới `check_code` hợp lệ, và toàn bộ sản phẩm trong chi tiết phải thuộc phạm vi đợt kiểm kê đó.
  - **`DAMAGED` (Hư hỏng trong kho)**: Trạng thái đích bắt buộc phải là `SCRAPPED` (`to_status == "SCRAPPED"` với `from_status` là `AVAILABLE` hoặc `WARRANTY`).
  - **`OTHER` (Lý do khác)**: Điều chuyển trạng thái thông thường không gắn với đợt kiểm kê.
- **Khai báo chi tiết và khóa bất biến**:
  - Toàn bộ các dòng chi tiết trong `adjust_movement_detail` phải được khai báo ngay khi tạo phiếu.
  - Sau khi phiếu được tạo, **tuyệt đối cấm thêm, sửa, xóa các dòng chi tiết** trong `adjust_movement_detail`.

### 5.4 Referential Linkage, Audited Product Scope & Cardinality Invariant (`source_code` referencing `check_code`)
- **Ràng buộc liên kết bắt buộc**:
  - Khi tạo `inbound_transaction` (loại `ADJUSTMENT_IN`), `outbound_transaction` (loại `ADJUSTMENT_OUT`), hoặc `adjustment_movement` (lý do `STOCKTAKE_DIFF`), trường `source_code` **bắt buộc phải tham chiếu đến một `INVENTORY_CHECK.check_code` đang tồn tại** (ví dụ: `CHK-UNIT-000001`).
- **Ràng buộc phạm vi sản phẩm**:
  - Toàn bộ sản phẩm trong các chứng từ điều chỉnh gắn với kiểm kho phải thuộc danh mục sản phẩm đã được khai báo trong `inventory_check_quantity` của đợt kiểm kê đó.
- **Quy tắc lực lượng duy nhất (Cardinality Invariant)**:
  - `source_code` mang ràng buộc **UNIQUE (`[UQ]`)**. Một đợt kiểm kê (`check_code`) chỉ được gắn với tối đa **01 đơn nhập điều chỉnh (`ADJUSTMENT_IN`)**, tối đa **01 đơn xuất điều chỉnh (`ADJUSTMENT_OUT`)**, và tối đa **01 phiếu điều chuyển nội bộ (`ADJUSTMENT_MOVEMENT`)**.
- **Không có khách hàng**:
  - Cả đơn nhập và xuất điều chỉnh đều có `customer_id == NULL`.

### 5.5 Lifecycle, Status Transition & Cancellation Rules (Khung trạng thái thống nhất cho Giao dịch & Điều chỉnh)
Áp dụng chung khung trạng thái thống nhất: **`PENDING`**, **`PROCESSING`**, **`COMPLETED`**, **`CANCELLED`** cho `inbound_transaction`, `outbound_transaction`, và `adjustment_movement`:

- **Khởi tạo**: Mọi đơn/phiếu mới tạo đều ở trạng thái **`PENDING`**.
- **Hủy bỏ (`CANCELLED`)**:
  - Chỉ được phép hủy đơn khi đơn đang ở **`PENDING`** và **tiến độ thực tế bằng 0** (`actual_quantity == 0` trên toàn bộ chi tiết, chưa có delivery/shipment nào tiếp nhận/bốc hàng).
  - Khi hủy đơn, toàn bộ các đợt giao/nhận liên quan chưa hoàn tất sẽ tự động chuyển sang `CANCELLED`.
  - Đơn đã `CANCELLED` là trạng thái kết thúc vĩnh viễn, không thể mở lại.
- **Tiến trình xử lý (`PENDING` ➔ `PROCESSING` ➔ `COMPLETED`)**:
  - Khi bắt đầu tiếp nhận/bốc hàng/điều chuyển: chuyển sang **`PROCESSING`**.
  - **Hoàn thành (`COMPLETED`)**:
    - Bình thường: Đơn chuyển sang `COMPLETED` khi đã hoàn thành 100% số lượng dự kiến.
    - **Quy tắc Under-Fulfillment**: Nếu không thể đáp ứng 100% số lượng (do thiếu hàng, chênh lệch không thu hồi được), cho phép hoàn tất sang **`COMPLETED`** với điều kiện người dùng phải xác nhận qua hộp thoại cảnh báo có **hộp kiểm xác nhận (checkbox)** rõ ràng.
    - **Khóa vĩnh viễn khi COMPLETED**: Khi đơn chuyển sang `COMPLETED`, toàn bộ đơn và các đợt giao/nhận đi kèm (`delivery`, `shipment`) bị khóa bất biến: không thể thêm đợt mới, không thể sửa đợt hiện có, và không thể thay đổi trạng thái unit nào nữa.
- **Trạng thái các đợt giao nhận liên quan**:
  - `delivery_status`: `PENDING` -> `RECEIVING` -> `COMPLETED`, hoặc `CANCELLED`.
  - `shipment_status`: `PENDING` -> `PICKING` -> `COMPLETED`, hoặc `CANCELLED`.
  - Người dùng được phép cập nhật trạng thái các đợt giao/nhận khi đơn cha chưa ở trạng thái kết thúc (`COMPLETED` / `CANCELLED`).

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
   - Format: `CHK-UNIT-<xxxxxx>` (6 digits, e.g., `CHK-UNIT-000001`), derived from `CHECK_TYPE.next_code_no` (chỉ dùng duy nhất loại `UNIT`).
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
     - **Unified Order Lifecycle & Status Transition Rules (Rule 5.5)**:
       - Applicable to `inbound_transaction`, `outbound_transaction`, and `adjustment_movement`:
       - **Khung trạng thái thống nhất**: `PENDING`, `PROCESSING`, `COMPLETED`, `CANCELLED`.
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
- **Inventory Check Interface (Kiểm Kê Kho - Duy Nhất `CHK-UNIT`)**:
  - **Upfront Product Scope Declaration**: Khi khởi tạo đợt kiểm kê mới, người dùng chọn trước danh mục sản phẩm tham gia kiểm kê. Sau khi xác nhận, danh mục này bị khóa bất biến vào `inventory_check_quantity`.
  - **Giao diện Kiểm kê chi tiết theo đơn vị (`CHK-UNIT`)**:
    - **Master–Detail Split View Layout**:
      - *Bảng Tổng hợp trên (`inventory_check_quantity`)*: Theo dõi tiến độ theo từng sản phẩm: Snapshot ban đầu vs. Thực tế kiểm đếm, các cột riêng biệt `Thừa (+)` / `Thiếu (-)` / `Lệch trạng thái (⇄)` (tuyệt đối không bù trừ ngang).
      - *Khung Chi tiết dưới (`inventory_check_unit` + Thanh quét Barcode/Serial)*: Quét liên tục số Serial kèm bảng so khớp từng đơn vị (`Serial | System Status | Scanned Status | Huy hiệu chênh lệch`).
    - **Huy hiệu màu sắc & Thông báo khi quét**:
      - `Khớp (Matched)`: Huy hiệu xanh lá (`scanned_status == system_status`).
      - `Thiếu (Missing)`: Huy hiệu đỏ/cam khi hoàn tất đợt mà `scanned_status == NULL`.
      - `Thừa - Tái phát hiện (Surplus - Rediscovered OUT)`: Huy hiệu xanh dương/cam khi quét serial có `system_status == OUT`; mở hộp thoại cho người dùng chọn tình trạng thực tế (`AVAILABLE` / `WARRANTY` / `SCRAPPED`).
      - `Thừa - Chưa định danh (Surplus - Unrecorded)`: Ghi nhận qua nút **"+ Ghi nhận hàng thừa chưa định danh"** trên dòng sản phẩm mà không sinh serial nháp trước khi lập đơn `ADJUSTMENT_IN`.
      - `Lệch trạng thái (Status Shift)`: Huy hiệu tím khi `scanned_status != system_status` giữa các trạng thái trong kho.
      - Quét serial ngoài danh mục kiểm kê sẽ bị cảnh báo và chặn ngay.
    - **Phễu tổng kết bất thường khi COMPLETED**:
      - Hiển thị thẻ KPI tổng quan (Sổ sách, Kiểm đếm, Khớp, Thiếu, Thừa, Lệch) và tự động bật bộ lọc **`[✓] Chỉ hiển thị bất thường`**.
      - Phân chia thành **3 Tab xử lý chứng từ** tương ứng với 3 nút: **"Tạo đơn xuất điều chỉnh (ADJ-OUT)"**, **"Tạo đơn nhập điều chỉnh (ADJ-IN)"**, và **"Tạo phiếu điều chuyển nội bộ (Movement)"**. Chỉ tạo đơn khi người dùng bấm xác nhận; sau khi tạo, nút chuyển sang trạng thái đã khóa và hiển thị mã chứng từ.
- **Internal Adjustment Movement Interface (Điều Chỉnh Nội Bộ)**:
  - **Upfront Detail Declaration**: Khi tạo phiếu điều chuyển, người dùng khai báo toàn bộ các dòng điều chuyển (`product_id`, `from_status`, `to_status`, `adjust_quantity`). Sau khi lưu, bảng chi tiết `adjust_movement_detail` bị khóa hoàn toàn.
  - Hiển thị thông tin từ `ADJUSTMENT_MOVEMENT` và `adjust_movement_detail`.
  - Hiển thị `adjust_code`, `source_code` (tham chiếu `check_code`), `reason` (`STOCKTAKE_DIFF`, `DAMAGED`, `OTHER`), và `status` (`PENDING`, `PROCESSING`, `COMPLETED`, `CANCELLED`).

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
