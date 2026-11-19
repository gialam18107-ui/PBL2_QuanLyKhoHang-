#ifndef UI_LOCALIZATION_H
#define UI_LOCALIZATION_H

#include <QColor>
#include <QString>
#include <string>

namespace UiLocalization {

inline QString stripTablePrefix(const std::string& colKey)
{
    size_t dot = colKey.rfind('.');
    std::string base = (dot == std::string::npos) ? colKey : colKey.substr(dot + 1);
    return QString::fromStdString(base);
}

inline QString localizeStatus(const QString& raw)
{
    QString s = raw.trimmed();
    QString up = s.toUpper();
    if (up == "PENDING") return "Chờ xử lí";
    if (up == "PROCESSING" || up == "IN_PROGRESS") return "Đang xử lí";
    if (up == "RECEIVING") return "Đang nhận hàng";
    if (up == "PICKING") return "Đang lấy hàng";
    if (up == "COMPLETED") return "Hoàn thành";
    if (up == "CANCELLED") return "Đã hủy";

    if (up == "AVAILABLE") return "Khả dụng";
    if (up == "WARRANTY") return "Bảo hành";
    if (up == "SCRAPPED") return "Lỗi/hỏng";
    if (up == "OUT") return "Đã xuất";
    return s;
}

inline QString localizeOrderType(const QString& raw)
{
    QString s = raw.trimmed();
    QString up = s.toUpper();
    if (up == "PURCHASING" || up == "PUR") return "Mua mới";
    if (up == "ADJUSTMENT_IN") return "Nhập điều chỉnh";
    if (up == "WARRANTY_CHECKING" || up == "WCK") return "Nhận bảo hành";
    if (up == "WARRANTY_REPLACEMENT" || up == "WRP") return "Đổi BH hãng";
    if (up == "CUSTOMER_RETURN" || up == "CRT") return "Khách trả hàng";

    if (up == "SELLING" || up == "SEL") return "Bán hàng";
    if (up == "ADJUSTMENT_OUT") return "Xuất điều chỉnh";
    if (up == "WARRANTY_RETURN" || up == "WRT") return "Gửi BH hãng";
    if (up == "WARRANTY_REPLACEMENT_TO_CUSTOMER" || up == "WRC") return "Đổi BH khách";
    if (up == "RETURN_TO_SUPPLIER" || up == "RTS") return "Trả NCC";

    if (up == "INBOUND") return "Nhập kho";
    if (up == "OUTBOUND") return "Xuất kho";
    if (up == "ADJUSTMENT" || up == "ADJ") return "Điều chuyển";
    return s;
}

inline QString localizeReason(const QString& raw)
{
    QString s = raw.trimmed();
    QString up = s.toUpper();
    if (up == "STOCKTAKE_DIFF" || up == "STOCK_DIFF" || up == "STD") return "Lệch kiểm kê";
    if (up == "DAMAGED" || up == "DMG") return "Hư hỏng";
    if (up == "OTHER" || up == "OTH") return "Khác";
    return s;
}

inline QString localizeCheckType(const QString& raw)
{
    QString s = raw.trimmed();
    QString up = s.toUpper();
    if (up == "UNIT") return "Theo Serial";
    if (up == "QUAN") return "Theo số lượng";
    return s;
}

inline bool isStatusColumnKey(const std::string& colKey)
{
    QString col = stripTablePrefix(colKey);
    return (col == "status" || col == "system_status" || col == "scanned_status" ||
            col == "from_status" || col == "to_status" || col == "old_status" ||
            col == "curr_status");
}

inline QString localizeColumnValue(const std::string& colKey, const QString& raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty() || s == "-") return raw;

    QString col = stripTablePrefix(colKey);
    if (col == "status" || col == "system_status" || col == "scanned_status" ||
        col == "from_status" || col == "to_status" || col == "old_status" ||
        col == "curr_status") {
        return localizeStatus(s);
    }
    if (col == "type") {
        return localizeOrderType(s);
    }
    if (col == "reason") {
        return localizeReason(s);
    }
    if (col == "check_type") {
        return localizeCheckType(s);
    }
    return raw;
}

inline QString unlocalizeFilterValue(const std::string& colKey, const QString& displayText)
{
    QString s = displayText.trimmed();
    if (s.isEmpty()) return s;
    QString low = s.toLower();
    QString col = stripTablePrefix(colKey);

    if (isStatusColumnKey(colKey)) {
        if (low == "chờ xử lí" || low == "chờ xử lý") return "PENDING";
        if (low == "đang xử lí" || low == "đang xử lý" || low == "đang thực hiện") return "PROCESSING";
        if (low == "đang nhận hàng") return "RECEIVING";
        if (low == "đang lấy hàng") return "PICKING";
        if (low == "hoàn thành" || low == "đã hoàn thành") return "COMPLETED";
        if (low == "đã hủy") return "CANCELLED";
        if (low == "khả dụng") return "AVAILABLE";
        if (low == "bảo hành" || low == "đang bảo hành") return "WARRANTY";
        if (low == "lỗi/hỏng") return "SCRAPPED";
        if (low == "đã xuất") return "OUT";
    }
    if (col == "type") {
        if (low == "mua mới") return "PURCHASING";
        if (low == "nhập điều chỉnh") return "ADJUSTMENT_IN";
        if (low == "nhận bảo hành") return "WARRANTY_CHECKING";
        if (low == "đổi bh hãng") return "WARRANTY_REPLACEMENT";
        if (low == "khách trả hàng") return "CUSTOMER_RETURN";
        if (low == "bán hàng") return "SELLING";
        if (low == "xuất điều chỉnh") return "ADJUSTMENT_OUT";
        if (low == "gửi bh hãng") return "WARRANTY_RETURN";
        if (low == "đổi bh khách") return "WARRANTY_REPLACEMENT_TO_CUSTOMER";
        if (low == "nhập kho") return "INBOUND";
        if (low == "xuất kho") return "OUTBOUND";
        if (low == "điều chuyển") return "ADJUSTMENT";
    }
    if (col == "reason") {
        if (low == "lệch kiểm kê") return "STOCKTAKE_DIFF";
        if (low == "hư hỏng") return "DAMAGED";
        if (low == "khác") return "OTHER";
    }
    if (col == "check_type") {
        if (low == "theo serial") return "UNIT";
        if (low == "theo số lượng") return "QUAN";
    }
    return s;
}

inline QColor statusTextColor(const QString& statusRawOrLocalized)
{
    QString up = statusRawOrLocalized.trimmed().toUpper();
    QString low = statusRawOrLocalized.trimmed().toLower();
    if (up == "COMPLETED" || low == "hoàn thành" || low == "đã hoàn thành") {
        return QColor(16, 185, 129);
    }
    if (up == "PROCESSING" || up == "RECEIVING" || up == "PICKING" || up == "IN_PROGRESS" ||
        low == "đang xử lí" || low == "đang xử lý" || low == "đang nhận hàng" || low == "đang lấy hàng") {
        return QColor(37, 99, 235);
    }
    if (up == "CANCELLED" || up == "SCRAPPED" || low == "đã hủy" || low == "lỗi/hỏng") {
        return QColor(220, 38, 38);
    }
    if (up == "PENDING" || up == "WARRANTY" || low == "chờ xử lí" || low == "chờ xử lý" || low == "bảo hành") {
        return QColor(234, 88, 12);
    }
    if (up == "AVAILABLE" || low == "khả dụng") {
        return QColor(22, 163, 74);
    }
    if (up == "OUT" || low == "đã xuất") {
        return QColor(100, 116, 139);
    }
    return QColor(30, 41, 59);
}

inline bool isCompletedStatus(const QString& statusRawOrLocalized)
{
    QString up = statusRawOrLocalized.trimmed().toUpper();
    QString low = statusRawOrLocalized.trimmed().toLower();
    return (up == "COMPLETED" || low == "hoàn thành" || low == "đã hoàn thành");
}

} // namespace UiLocalization

#endif // UI_LOCALIZATION_H
