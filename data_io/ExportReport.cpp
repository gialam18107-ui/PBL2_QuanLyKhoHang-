#include "ExportReport.h"
#include "CSVHelper.h"
#include <fstream>
#include <cstdio>

using namespace std;

namespace DataIO {

namespace {

inline int safeGetInt(const Record &r, const string &col, int defaultVal = 0) {
  if (!r.has(col))
    return defaultVal;
  const auto &v = r.get(col);
  if (holds_alternative<int>(v))
    return get<int>(v);
  if (holds_alternative<double>(v))
    return static_cast<int>(get<double>(v));
  return defaultVal;
}

inline string safeGetString(const Record &r, const string &col, const string &defaultVal = "") {
  if (!r.has(col))
    return defaultVal;
  return valueToString(r.get(col));
}

} // anonymous namespace

bool export_periodic_inventory_report_csv(int month, int year,
                                          const Vector<Record> &results,
                                          const Record &grandTotal,
                                          const string &outPath) {
  string path = outPath;
  if (path.empty()) {
    char fname[128];
    snprintf(fname, sizeof(fname), "report/BaoCao_Thang_%02d_Nam_%04d.csv", month, year);
    path = fname;
  }

  ofstream outFile(path);
  if (!outFile.is_open())
    return false;

  Vector<string> headers = {
      "Mã sản phẩm", "Tên sản phẩm", "Tồn đầu kỳ", "Nhập trong tháng",
      "Xuất trong tháng", "Tồn cuối tháng", "Tồn khả dụng", "Hàng bảo hành", "Hàng lỗi hỏng"
  };
  outFile << CSVHelper::formatLine(headers) << "\n";

  Vector<string> fields;
  fields.reserve(9);

  for (size_t i = 0; i < results.size(); ++i) {
    const auto &r = results[i];
    fields.clear();
    fields.push_back(safeGetString(r, "product_code"));
    fields.push_back(safeGetString(r, "product_name"));
    fields.push_back(to_string(safeGetInt(r, "opening_stock")));
    fields.push_back(to_string(safeGetInt(r, "inbound_quantity")));
    fields.push_back(to_string(safeGetInt(r, "outbound_quantity")));
    fields.push_back(to_string(safeGetInt(r, "closing_stock")));
    fields.push_back(to_string(safeGetInt(r, "available_quan")));
    fields.push_back(to_string(safeGetInt(r, "warranty_quan")));
    fields.push_back(to_string(safeGetInt(r, "scrapped_quan")));

    outFile << CSVHelper::formatLine(fields) << "\n";
  }

  Vector<string> totalFields = {
      "TỔNG CỘNG",
      "",
      to_string(safeGetInt(grandTotal, "opening_stock")),
      to_string(safeGetInt(grandTotal, "inbound_quantity")),
      to_string(safeGetInt(grandTotal, "outbound_quantity")),
      to_string(safeGetInt(grandTotal, "closing_stock")),
      to_string(safeGetInt(grandTotal, "available_quan")),
      to_string(safeGetInt(grandTotal, "warranty_quan")),
      to_string(safeGetInt(grandTotal, "scrapped_quan"))
  };

  outFile << CSVHelper::formatLine(totalFields) << "\n";

  outFile.close();
  return true;
}

} // namespace DataIO
