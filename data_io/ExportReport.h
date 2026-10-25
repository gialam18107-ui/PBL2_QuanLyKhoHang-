#ifndef EXPORT_REPORT_H
#define EXPORT_REPORT_H

#include <string>
#include "database/Record.h"
#include "datastructure/vector/Vector.h"

using namespace std;

namespace DataIO {

/**
 * @brief Xuất báo cáo xuất - nhập - tồn định kỳ ra định dạng CSV (Section 9)
 * @param month Tháng báo cáo (1-12)
 * @param year Năm báo cáo
 * @param results Danh sách kết quả từng sản phẩm
 * @param grandTotal Record chứa dòng tổng cộng (Grand Total)
 * @param outPath Đường dẫn file đích (mặc định: report/BaoCao_Thang_MM_Nam_YYYY.csv)
 * @return true nếu ghi file thành công
 */
bool export_periodic_inventory_report_csv(int month, int year,
                                          const Vector<Record> &results,
                                          const Record &grandTotal,
                                          const string &outPath = "");

} // namespace DataIO

#endif // EXPORT_REPORT_H
