# PBL2 - Quản Lý Kho Hàng

Dự án đồ án PBL2: Xây dựng hệ thống cơ sở dữ liệu và ứng dụng quản lý kho hàng linh kiện điện tử bằng **C++17** (tự cài đặt cấu trúc dữ liệu, không dùng thư viện có sẵn như `std::vector`, `std::map`) kết hợp giao diện **Qt6**.

---

## 1. Cấu trúc thư mục

- `database/`: Định nghĩa cơ sở dữ liệu, bảng (`Table`), bản ghi (`Record`), chỉ mục (`Index`), điều kiện lọc...
- `datastructure/`: Các cấu trúc dữ liệu tự cài đặt (`Vector`, `HashMap`, `AvlTree`, `Pair`...).
- `schema/`: Định nghĩa lược đồ dữ liệu và các truy vấn nghiệp vụ.
- `data_io/`: Xử lý đọc/ghi file CSV và tương tác dữ liệu.
- `data/`: Dữ liệu bảng lưu trữ dưới dạng CSV.
- `ui/`: Giao diện ứng dụng Qt6.
- `generator/`: Công cụ sinh dữ liệu kiểm thử.
- `test/`: Các file kiểm thử đơn vị / tính năng (unit tests).

---

## 2. Hướng dẫn biên dịch và chạy

### 2.1. Biên dịch ứng dụng chính
```bash
# Biên dịch các mục tiêu chính (app.exe, generator.exe, test_io.exe)
mingw32-make all

# Hoặc chỉ biên dịch ứng dụng chính (giao diện Qt6)
mingw32-make app.exe
```

### 2.2. Biên dịch và chạy file test trong thư mục `test/`

Hệ thống Makefile hỗ trợ tự động tìm và liên kết các file mã nguồn test trong thư mục `test/` với các object core trong `database/`, `schema/`, `data_io/`:

- **Biên dịch file `.exe` cho bài test cụ thể:**
  ```bash
  # Cú pháp: mingw32-make <tên_test>.exe
  mingw32-make test_avl_index.exe

  # Hoặc chỉ định rõ thư mục test/
  mingw32-make test/test_avl_index.exe
  ```

- **Biên dịch và chạy ngay bài test (rút gọn):**
  ```bash
  # Gõ trực tiếp tên bài test (tự động build nếu cần rồi thực thi)
  mingw32-make test_avl_index

  # Hoặc dùng tiền tố run_
  mingw32-make run_test_avl_index
  ```

- **Chạy toàn bộ các bài test trong thư mục `test/`:**
  ```bash
  mingw32-make test
  ```

---

## 3. Các lệnh Makefile khác

| Lệnh | Ý nghĩa |
| :--- | :--- |
| `mingw32-make clean` | Dọn dẹp thư mục `build/`, file `.exe`, `.o`, `.d` |
| `mingw32-make crawl` | Chạy crawler thu thập dữ liệu sản phẩm từ web |
| `mingw32-make generator.exe` | Biên dịch công cụ sinh dữ liệu ngẫu nhiên |
| `mingw32-make test_io.exe` | Biên dịch chương trình kiểm tra đọc/ghi CSV |
| `mingw32-make main.exe` | Tạo alias `main.exe` từ `app.exe` |