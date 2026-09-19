#ifndef QUERY_IO_H
#define QUERY_IO_H

#include "database/JoinedRecord.h"
#include "database/Query.h"
#include <string>
#include <vector>

using namespace std;

namespace DataIO {
class QueryIO {
public:
  // Execute query and write results to the specified CSV file
  static bool write(const Query &query, const string &filePath);

  // Execute query and write results to folderPath/<baseTable>.csv (or
  // folderPath/<baseTable>_query.csv)
  static bool writeToFolder(const Query &query,
                            const string &folderPath = "data");

  // Write arbitrary query results with headers to CSV
  static bool writeResults(const vector<JoinedRecord> &records,
                           const vector<string> &headers,
                           const string &filePath);

  // Read the data then execute query: ensures required tables are loaded from
  // folderPath, then executes query
  static vector<JoinedRecord> read(Query &query,
                                   const string &folderPath = "data");

  // Read query results CSV file back into a vector of JoinedRecord
  static vector<JoinedRecord> readResults(const string &filePath);
};
} // namespace DataIO

#endif // QUERY_IO_H
