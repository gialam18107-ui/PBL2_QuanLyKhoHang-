#include "ExportReport.h"
#include "CSVHelper.h"
#include <fstream>
#include <cstdio>

using namespace std;

namespace DataIO {

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

  for (size_t i = 0; i < results.size(); ++i) {
    outFile << CSVHelper::formatLine(results[i]) << "\n";
  }

  outFile << CSVHelper::formatLine(grandTotal) << "\n";

  outFile.close();
  return true;
}

} // namespace DataIO
