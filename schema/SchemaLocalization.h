#ifndef SCHEMA_LOCALIZATION_H
#define SCHEMA_LOCALIZATION_H

#include <cctype>
#include <string>

namespace schema_i18n {

inline std::string toUpperAscii(const std::string& raw)
{
    std::string up;
    up.reserve(raw.size());
    for (unsigned char ch : raw) {
        if (!std::isspace(ch)) {
            up.push_back(static_cast<char>(std::toupper(ch)));
        }
    }
    return up;
}

inline std::string statusVi(const std::string& raw)
{
    std::string up = toUpperAscii(raw);
    if (up.empty() || up == "NULL") return "Chưa có trong kho";
    if (up == "AVAILABLE") return "Khả dụng";
    if (up == "WARRANTY") return "Bảo hành";
    if (up == "SCRAPPED") return "Lỗi/hỏng";
    if (up == "OUT") return "Đã xuất";
    if (up == "PENDING") return "Chờ xử lí";
    if (up == "PROCESSING" || up == "IN_PROGRESS") return "Đang xử lí";
    if (up == "RECEIVING") return "Đang nhận hàng";
    if (up == "PICKING") return "Đang lấy hàng";
    if (up == "COMPLETED") return "Hoàn thành";
    if (up == "CANCELLED") return "Đã hủy";
    return raw;
}

inline std::string orderTypeVi(const std::string& raw)
{
    std::string up = toUpperAscii(raw);
    if (up == "PURCHASING" || up == "PUR") return "Mua mới";
    if (up == "ADJUSTMENT_IN") return "Nhập điều chỉnh";
    if (up == "WARRANTY_CHECKING" || up == "WCK") return "Nhận bảo hành";
    if (up == "WARRANTY_REPLACEMENT" || up == "WRP") return "Đổi bảo hành hãng";
    if (up == "CUSTOMER_RETURN" || up == "CRT") return "Khách trả hàng";
    if (up == "SELLING" || up == "SEL") return "Bán hàng";
    if (up == "ADJUSTMENT_OUT") return "Xuất điều chỉnh";
    if (up == "WARRANTY_RETURN" || up == "WRT") return "Gửi bảo hành hãng";
    if (up == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || up == "WRC") return "Đổi bảo hành khách";
    if (up == "INBOUND") return "Nhập kho";
    if (up == "OUTBOUND") return "Xuất kho";
    if (up == "ADJUSTMENT" || up == "ADJ") return "Điều chỉnh";
    return raw;
}

inline std::string reasonVi(const std::string& raw)
{
    std::string up = toUpperAscii(raw);
    if (up == "STOCKTAKE_DIFF" || up == "STOCK_DIFF" || up == "STD") return "Lệch kiểm kê";
    if (up == "DAMAGED" || up == "DMG") return "Hư hỏng";
    if (up == "OTHER" || up == "OTH") return "Khác";
    return raw;
}

} // namespace schema_i18n

#endif // SCHEMA_LOCALIZATION_H
