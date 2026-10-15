# Warehouse Transaction Data Generator (PBL2)

Tuân thủ nghiêm ngặt **Quy tắc 7 (Generator Rules)** trong [project_rules.md](../project_rules.md) và thiết kế lược đồ [QuanLyKhoHang_schema.json](../QuanLyKhoHang_schema.json).

---

## 1. Mục đích và Phạm vi Tuân thủ

1. **Phạm vi (Rule 7.1)**:
   - Generator **chỉ chịu trách nhiệm sinh dữ liệu liên quan đến luồng giao dịch/kho (Transaction & Inventory Flow)** và lịch sử trạng thái (`status_history`).
   - Tuyệt đối **không tự ý sinh dữ liệu danh mục gốc/master** (không tự sinh Category, Brand, Product, hay Hardware Spec Tables vì các dữ liệu này thuộc phạm vi của Crawler và Master Data).
   - Tuyệt đối không can thiệp thay đổi cấu trúc bảng, lược đồ database hay join-table.

2. **Quy trình sinh dữ liệu (Rule 7.3 & 7.4)**:
   - Cung cấp giao diện Console tương tác trực tiếp (`Console Interface`).
   - Người dùng có thể chọn:
     - Loại giao dịch: Nhập (`Inbound`) hoặc Xuất (`Outbound`).
     - Sản phẩm tham gia giao dịch (chọn từ danh sách sản phẩm hiện có trong CSDL).
     - Số lượng tương ứng cho từng sản phẩm.
     - Số lượng phiếu nhận hàng (`Delivery`) hoặc phiếu xuất hàng (`Shipment`) cần tách đợt.
   - Quản lý trạng thái và lịch sử trạng thái theo logic chặt chẽ:
     - Khi Nhập kho: Tạo các đơn vị hiện vật (`unit_inventory`) với serial duy nhất, trạng thái `AVAILABLE` (hoặc `WARRANTY`), ghi nhận `status_history` (`old_status = NULL`, `curr_status = AVAILABLE`, `transaction = delivery_id`, `type = INBOUND`).
     - Khi Xuất kho: Kiểm tra số lượng tồn kho khả dụng; chuyển trạng thái từ `AVAILABLE` sang `OUT`, ghi nhận `status_history` (`old_status = AVAILABLE`, `curr_status = OUT`, `transaction = shipment_id`, `type = OUTBOUND`).
     - Cập nhật số lượng tồn kho `product.quantity`.

3. **Cài đặt thuần C++17 (Rule 1)**:
   - Không sử dụng các container có sẵn của STL (`std::vector`, `std::map`, `std::unordered_map`...).
   - Sử dụng các cấu trúc dữ liệu tự viết: `datastructure/vector/Vector.h`, `datastructure/hashmap/HashMap.h`, `datastructure/hashmap/Pair.h`.

---

## 2. Cách biên dịch và sử dụng

### 2.1 Biên dịch
```bash
mingw32-make generator.exe
```

### 2.2 Chạy giao diện Console tương tác (Interactive Mode)
```bash
.\generator.exe
```
Menu tương tác hiển thị:
```
======================================================
     WAREHOUSE TRANSACTION GENERATOR (PBL2)           
======================================================
  1. Generate Inbound Transaction (Receiving Flow)    
  2. Generate Outbound Transaction (Dispatch Flow)    
  3. Display Products & Stock Levels                  
  4. View Inventory & Transaction Summary             
  5. Auto Generate Random Batch                       
  6. Validate Database Referential Integrity          
  7. Save Database Changes to CSV                     
  0. Exit                                             
======================================================
```

### 2.3 Chạy chế độ Batch tự động (Headless Batch Mode)
```bash
.\generator.exe --batch <numInbound> <numOutbound> --save
```
Ví dụ:
```bash
.\generator.exe --batch 5 5 --save
```
