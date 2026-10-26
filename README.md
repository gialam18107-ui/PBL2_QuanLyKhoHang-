# PBL2 - Quản Lý Kho Hàng

Dự án đồ án PBL2: Xây dựng hệ thống cơ sở dữ liệu và ứng dụng quản lý kho hàng linh kiện điện tử bằng **C++17** (tự cài đặt cấu trúc dữ liệu, không dùng thư viện có sẵn như `std::vector`, `std::map`) kết hợp giao diện **Qt6**.

---

## 1. Cấu trúc thư mục

- `database/`: Định nghĩa cơ sở dữ liệu, bảng (`Table`), bản ghi (`Record`), chỉ mục (`Index`)...
- `datastructure/`: Các cấu trúc dữ liệu tự cài đặt (`Vector`, `HashMap`, `AvlTree`, `Pair`...).
- `query/`: Xử lý truy vấn, sắp xếp và thống kê dữ liệu.
- `schema/`: Định nghĩa lược đồ dữ liệu và các nghiệp vụ truy vấn/CRUD.
- `data_io/`: Xử lý đọc/ghi file CSV và xuất báo cáo.
- `data/` & `report/`: Dữ liệu CSV của các bảng và thư mục lưu báo cáo định kỳ.
- `data_crawler/`: Công cụ Python thu thập linh kiện và khởi tạo bộ dữ liệu CSV gốc.
- `generator/`: Công cụ C++ sinh dữ liệu giao dịch Nhập/Xuất kho.
- `ui/`: Giao diện ứng dụng Qt6.
- `test/`: Các file kiểm thử đơn vị và quy tắc nghiệp vụ.

---

## 2. Hướng dẫn biên dịch và chạy

> Yêu cầu: **MinGW-w64 / MSYS2 UCRT64** (`g++`, `mingw32-make`, `Qt6Widgets`) và **Python 3**.

### 2.1. Thu thập dữ liệu gốc (`data_crawler`)
Khởi tạo danh mục sản phẩm, thông số kỹ thuật và dữ liệu nền vào `data/`:
```bash
# Chạy nhanh qua Makefile
mingw32-make crawl

# Hoặc chạy trực tiếp bằng Python
python -m data_crawler.main --output-dir data --seed 42 --validate
```

### 2.2. Sinh dữ liệu giao dịch (`generator`)
Đọc dữ liệu trong `data/` và sinh thêm các đơn Nhập/Xuất kho, tồn kho đơn vị, lịch sử trạng thái:
```bash
# Biên dịch
mingw32-make generator.exe

# Chạy chế độ menu tương tác trên Console
./generator.exe

# Chạy tự động theo lô: --batch <số_đơn_nhập> <số_đơn_xuất> (thêm --save để ghi vào CSV)
./generator.exe --batch 10 5 --save
```

### 2.3. Biên dịch và chạy ứng dụng chính (`app.exe`)
```bash
# Biên dịch toàn bộ (app.exe, generator.exe, test_io.exe) hoặc chỉ riêng app.exe
mingw32-make all
mingw32-make app.exe

# Khởi chạy giao diện Qt6
./app.exe
```

### 2.4. Biên dịch và chạy kiểm thử (`test/`)
```bash
# Kiểm tra đọc/ghi CSV 32 bảng
mingw32-make test_io.exe
./test_io.exe

# Biên dịch và chạy ngay một bài test cụ thể trong test/
mingw32-make test_avl_index
mingw32-make test_user_query

# Chạy toàn bộ các bài test trong test/
mingw32-make test
```

---

## 3. Các lệnh Makefile nhanh

| Lệnh | Ý nghĩa |
| :--- | :--- |
| `mingw32-make all` | Biên dịch `app.exe`, `generator.exe`, `test_io.exe` |
| `mingw32-make crawl` | Chạy `data_crawler` khởi tạo dữ liệu CSV vào `data/` |
| `mingw32-make generator.exe` | Biên dịch công cụ sinh dữ liệu giao dịch |
| `mingw32-make app.exe` | Biên dịch ứng dụng giao diện chính Qt6 |
| `mingw32-make main.exe` | Tạo bản sao `main.exe` từ `app.exe` |
| `mingw32-make test_io.exe` | Biên dịch chương trình kiểm tra đọc/ghi CSV |
| `mingw32-make test` | Biên dịch và chạy toàn bộ bài test trong `test/` |
| `mingw32-make clean` | Dọn dẹp thư mục `build/`, các file `.exe`, `.o`, `.d` |
