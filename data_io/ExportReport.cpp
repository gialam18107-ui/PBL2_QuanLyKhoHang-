#include "ExportReport.h"
#include "CSVHelper.h"
#include <fstream>
#include <cstdio>
#include <filesystem>

using namespace std;

namespace DataIO {

bool export_periodic_inventory_report_csv(int month, int year,
                                          const Vector<Record> &results,
                                          const Record &grandTotal,
                                          const string &outPath) {
  string path = outPath;
  if (path.empty()) {
    std::filesystem::create_directories("report");
    char fname[128];
    snprintf(fname, sizeof(fname), "report/BaoCao_Thang_%02d_Nam_%04d.csv", month, year);
    path = fname;
  } else {
    try {
      std::filesystem::path p(path);
      if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
      }
    } catch (...) {}
  }

  ofstream outFile(path);
  if (!outFile.is_open())
    return false;

  Vector<string> headers = {
      "Mã sản phẩm", "Tên sản phẩm", "Tồn đầu kỳ", "Nhập trong tháng",
      "Xuất trong tháng", "Tồn cuối tháng", "Tồn khả dụng", "Hàng bảo hành", "Hàng lỗi hỏng"
  };
  outFile << CSVHelper::formatLine(headers) << "\n";

  Vector<string> reportCols = {
      "product_code", "product_name", "opening_stock", "inbound_quantity",
      "outbound_quantity", "closing_stock", "available_quan", "warranty_quan", "scrapped_quan"
  };

  for (size_t i = 0; i < results.size(); ++i) {
    outFile << CSVHelper::formatLine(results[i], reportCols) << "\n";
  }

  Record totalRec = grandTotal;
  totalRec.set("product_code", string("TỔNG CỘNG"));
  totalRec.set("product_name", string(""));
  outFile << CSVHelper::formatLine(totalRec, reportCols) << "\n";

  outFile.close();
  return true;
}

} // namespace DataIO
