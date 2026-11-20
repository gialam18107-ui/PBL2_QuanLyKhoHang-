# Warehouse Transaction Data Generator (PBL2)

Tuân thủ nghiêm ngặt **Quy tắc 7 (Generator Rules)** trong [project_rules.md](../project_rules.md) và thiết kế lược đồ [QuanLyKhoHang_schema.json](../QuanLyKhoHang_schema.json).

---

## 1. Mục đích và Phạm vi Tuân thủ

1. **Phạm vi (Rule 7.1)**:
   - Generator **chỉ chịu trách nhiệm sinh dữ liệu liên quan đến luồng giao dịch/kho (Transaction & Inventory Flow)**, đợt kiểm kho (`inventory_check`), và lịch sử trạng thái (`status_history`).
   - Tuyệt đối **không tự ý sinh dữ liệu danh mục gốc/master** (không tự sinh Category, Brand, Product, hay Hardware Spec Tables vì các dữ liệu này thuộc phạm vi của Crawler và Master Data).
   - Tuyệt đối không can thiệp thay đổi cấu trúc bảng, lược đồ database hay join-table.

2. **Quy trình sinh dữ liệu mới (New Rules & Schema 2.0)**:
   - **Giao dịch điều chỉnh (ADJ)**:
     - `ADJUSTMENT_IN` và `ADJUSTMENT_OUT` **bắt buộc không có khách hàng (`customer_id = NULL`)**.
     - `source_code` **bắt buộc tham chiếu đến mã kiểm kê `check_code`** (tự động liên kết hoặc tạo phiếu kiểm kho snapshot).
   - **Tiền điều kiện Nhập kho (Inbound Precondition)**:
     - Mọi thao tác nhập kho chỉ được phép triển khai từ unit có `old_status` là `OUT` (đưa hàng trở lại kho) hoặc `NULL` (hàng mới từ nhà cung cấp).
     - `WARRANTY_CHECKING` (`WCK`) và `CUSTOMER_RETURN` (`CRT`) chỉ nhận lại các unit đang ở trạng thái `OUT` (không tự sinh unit mới từ `NULL`).
     - `ADJUSTMENT_IN` được phép phục hồi unit từ `OUT`/`NULL` sang `AVAILABLE`, `WARRANTY`, hoặc `SCRAPPED`.
   - **Quy tắc ngừng bán sản phẩm (`is_sold`)**:
     - `product.is_sold` chỉ có thể chuyển sang `FALSE` khi tổng số lượng tồn kho bằng 0 (`available_quantity + warranty_quantity + scrapped_quantity == 0`).
     - Mọi thay đổi trạng thái `is_sold` được tự động ghi nhận vào bảng `is_sold_changing`.
     - Sinh giao dịch tự động chỉ chọn các sản phẩm đang được phép bán (`is_sold == TRUE`) và không bị đóng băng kiểm kho (Stock Freeze).
   - **Kiểm kho chi tiết duy nhất (`CHK-UNIT`)**:
     - Hệ thống chỉ sử dụng chế độ kiểm kê chi tiết theo từng Serial (`CHK-UNIT`), tạo đồng thời cả `inventory_check_quantity` (mốc theo dõi số lượng) và `inventory_check_unit` (quét chi tiết từng serial hiện vật).
   - **Điều chuyển trạng thái nội bộ (`ADJUSTMENT_MOVEMENT`)**:
     - `STOCKTAKE_DIFF` (`STD`): bắt buộc liên kết `source_code` tới một đợt `inventory_check` đã `COMPLETED` có ghi nhận lệch trạng thái.
     - `DAMAGED` (`DMG`): `to_status` bắt buộc là `SCRAPPED`, `source_code` để `NULL` (trừ khi chỉ định).
     - `OTHER` (`OTH`): `source_code` để `NULL` (trừ khi chỉ định).

3. **Cài đặt thuần C++17 (Rule 1)**:
   - Không sử dụng các container có sẵn của STL (`std::vector`, `std::map`, `std::unordered_map`...).
   - Sử dụng các cấu trúc dữ liệu tự viết: `datastructure/vector/Vector.h`, `datastructure/hashmap/HashMap.h`, `datastructure/hashmap/Pair.h`.

---

## 2. Cách biên dịch và sử dụng

### 2.1 Biên dịch
Mở terminal tại thư mục gốc của project và chạy lệnh:
```bash
mingw32-make generator.exe
```

---

### 2.2 Chạy chế độ tương tác (Interactive Console Mode)
Chạy trực tiếp file thực thi (hoặc thêm cờ `-i` / `--interactive`):
```bash
.\generator.exe
```
Menu tương tác 11 chức năng (0–10) hiển thị:
```text
======================================================
     WAREHOUSE TRANSACTION GENERATOR (PBL2)           
======================================================
  1. Generate Inbound Transaction (Receiving Flow)    
  2. Generate Outbound Transaction (Dispatch Flow)    
  3. Perform Inventory Check (Unit Audit: CHK-UNIT)   
  4. Generate Adjustment Movement (Internal Shift)    
  5. Toggle Product is_sold Status                    
  6. Display Products & Stock Levels                  
  7. View Inventory & Transaction Summary             
  8. Auto Generate Random Batch                       
  9. Validate Database Referential Integrity          
 10. Save Database Changes to CSV                     
  0. Exit                                             
======================================================
```

#### Hướng dẫn các tùy chọn trên menu:
- **1. Generate Inbound Transaction**: Chọn loại nhập kho (`PURCHASING`, `ADJUSTMENT_IN`, `WARRANTY_CHECKING`, `WARRANTY_REPLACEMENT`, `CUSTOMER_RETURN`), chọn sản phẩm, số lượng và số đợt nhận hàng (`Delivery`). Nếu là `ADJUSTMENT_IN`, hệ thống tự đặt `customer_id = NULL` và liên kết `source_code`.
- **2. Generate Outbound Transaction**: Chọn loại xuất kho (`SELLING`, `ADJUSTMENT_OUT`, `WARRANTY_RETURN`, `WARRANTY_REPLACEMENT_TO_CUSTOMER`), chọn sản phẩm và số đợt giao (`Shipment`). Nếu là `ADJUSTMENT_OUT`, hệ thống tự đặt `customer_id = NULL` và liên kết `source_code`.
- **3. Perform Inventory Check**: Tạo phiếu kiểm kê chi tiết theo Serial (`CHK-UNIT-xxxxxx`), tạo đồng thời `inventory_check_quantity` và `inventory_check_unit`.
- **4. Generate Adjustment Movement**: Tạo phiếu điều chuyển trạng thái nội bộ (`STOCKTAKE_DIFF`, `DAMAGED`, `OTHER`).
- **5. Toggle Product is_sold Status**: Bật/tắt trạng thái bán của sản phẩm. Kiểm tra nghiêm ngặt tồn kho = 0 trước khi cho phép tắt và ghi log vào `is_sold_changing`.
- **6. Display Products & Stock Levels**: Bảng hiển thị thông tin sản phẩm: tồn khả dụng (Avail), bảo hành (Warr), phế phẩm (Scrap), và trạng thái Active (`is_sold`).
- **7. View Inventory & Transaction Summary**: Xem thống kê tổng quan toàn bộ kho và số lượng giao dịch.
- **8. Auto Generate Random Batch**: Nhập số lượng đơn Inbound, Outbound và số đợt kiểm kho (Inventory Checks) để hệ thống tự động sinh dữ liệu mô phỏng ngẫu nhiên hợp lệ.
- **9. Validate Database Referential Integrity**: Kiểm tra 100% tính toàn vẹn quan hệ, khóa ngoại, số lượng tồn và các ràng buộc nghiệp vụ.
- **10. Save Database Changes to CSV**: Ghi toàn bộ các thay đổi vừa sinh về các file CSV trong thư mục `data/`.
- **0. Exit**: Thoát chương trình.

---

### 2.3 Chạy chế độ dòng lệnh tự động (Headless Batch Mode)

Cú pháp tổng quát:
```bash
.\generator.exe --batch <numInbound> <numOutbound> [numChecks] [--save]
.\generator.exe --checks <numChecks> [--save]
```

#### Các ví dụ thường dùng:

1. **Sinh giao dịch kèm phiếu kiểm kho (Dry-Run)**:
   Sinh 5 đơn nhập, 5 đơn xuất và 2 phiếu kiểm kho (`CHK-UNIT`), kiểm tra toàn vẹn trên RAM:
   ```bash
   .\generator.exe --batch 5 5 2
   ```

2. **Sinh giao dịch kèm phiếu kiểm kho và LƯU vào đĩa (`data/`)**:
   ```bash
   .\generator.exe --batch 5 5 2 --save
   ```

3. **Chỉ sinh phiếu kiểm kho độc lập**:
   Sinh 3 phiếu kiểm kho (`CHK-UNIT`) và lưu vào CSDL:
   ```bash
   .\generator.exe --checks 3 --save
   ```

4. **Chạy chỉ đơn nhập và xuất (bỏ qua checks)**:
   ```bash
   .\generator.exe --batch 5 5 --save
   ```

5. **Chỉ kiểm tra tính toàn vẹn CSDL hiện tại (không sinh thêm)**:
   ```bash
   .\generator.exe --batch 0 0
   ```
