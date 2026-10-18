#ifndef QUERY_H
#define QUERY_H

#include "database/Record.h"
#include "datastructure/hashmap/Pair.h"
#include "datastructure/vector/Vector.h"
#include <algorithm>
#include <cstddef>
#include <variant>

using namespace std;

namespace Query {
    Vector<Record> sum(Vector<Record> &data, string groupedColumn, string calculatedColumn);
    Vector<Record> sum(Vector<Record> &data, Pair<string, string> groupedColumns, string calculatedColumn);
    Vector<Record> sum(Vector<Record> &data, Pair<string, string> groupedColumns, Pair<string, string> calculatedColumn);

    Vector<Record> count(Vector<Record> &data, string groupedColumn, string calculatedColumn);
    Vector<Record> count(Vector<Record> &data, Pair<string, string> groupedColumns, string calculatedColumn);
    Vector<Record> count(Vector<Record> &data, Pair<string, string> groupedColumns, Pair<string, string> calculatedColumn);

    void querySort(Vector<Record> &data, string groupedColumn, bool ascending);
    void querySort(Vector<Record> &data, Pair<string, string> groupedColumns, bool ascending);
}

#endif // QUERY_H