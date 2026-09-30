# Data Crawler (PBL2 Warehouse Management System)

Tuân thủ nghiêm ngặt **Quy tắc 8 (Data Crawler Rules)** trong [project_rules.md](../project_rules.md) và thiết kế lược đồ [QuanLyKhoHang_schema.json](../QuanLyKhoHang_schema.json).

---

## 1. Mục đích và Phạm vi

`data_crawler` được xây dựng bằng **Python 3 thuần** (sử dụng thư viện chuẩn `urllib`, `re`, `json`, `csv`, không phụ thuộc vào thư viện bên thứ 3) nhằm:
1. Thu thập và định dạng danh mục sản phẩm linh kiện máy tính theo đúng **8 danh mục chuẩn**:
   - `CPU`
   - `GPU`
   - `MAINBOARD`
   - `RAM`
   - `SSD`
   - `PSU`
   - `COOLING`
   - `CASE`
2. Trích xuất thông số kỹ thuật (specs) chuyên biệt cho từng danh mục sản phẩm (thay vì dùng một bộ thông số chung chung), tuân thủ định dạng và giá trị hợp lệ được quy định trong `QuanLyKhoHang_schema.json`.
3. Khởi tạo và đồng bộ hóa các bảng dữ liệu Master/Tham chiếu:
   - Danh mục (`category.csv`)
   - Thương hiệu (`brand.csv`)
   - Trạng thái sản phẩm (`unit_status.csv`)
   - Loại & trạng thái nhập/xuất kho (`inbound_type.csv`, `inbound_status.csv`, `delivery_status.csv`, `outbound_type.csv`, `outbound_status.csv`, `shipment_status.csv`)
4. Xuất các bảng thực thể và khởi tạo sẵn các bảng giao dịch để Database Engine C++ có thể đọc/ghi toàn vẹn.

---

## 2. Cấu trúc thư mục

```
data_crawler/
├── __init__.py          # Package initialization
├── categories.py        # Định nghĩa 8 danh mục, 10 thương hiệu & mã chuẩn
├── catalog_data.py      # Bộ catalog sản phẩm linh kiện máy tính thực tế
├── crawler.py           # Engine crawl và trích xuất thông số
├── exporter.py          # Xuất dữ liệu sang định dạng CSV chuẩn Schema C++
└── main.py              # CLI entry point
```

---

## 3. Cách chạy Data Crawler

### 3.1 Chạy mặc định (xuất ra thư mục `data/` và kiểm tra toàn vẹn)
```bash
python -m data_crawler.main
```

### 3.2 Tùy chọn tham số
```bash
python -m data_crawler.main --output-dir data --seed 42 --validate
```
- `--output-dir <path>`: Thư mục đích lưu các file CSV (mặc định: `data`).
- `--seed <int>`: Seed ngẫu nhiên để tái lập kết quả sinh mã.
- `--validate`: Tự động kiểm tra tính toàn vẹn khóa ngoại (Foreign Keys) và ràng buộc duy nhất (Unique constraints).
