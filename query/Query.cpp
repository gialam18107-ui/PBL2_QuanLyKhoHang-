#include "Query.h"

using namespace std;

namespace {

bool isNumeric(const Value& v)
{
    return holds_alternative<int>(v) || holds_alternative<double>(v);
}

double toDouble(const Value& v)
{
    return holds_alternative<int>(v)
        ? static_cast<double>(get<int>(v))
        : get<double>(v);
}

bool valueLess(const Value& a, const Value& b)
{
    if (holds_alternative<monostate>(a)) return !holds_alternative<monostate>(b);
    if (holds_alternative<monostate>(b)) return false;
    if (isNumeric(a) && isNumeric(b)) return toDouble(a) < toDouble(b);
    if (a.index() != b.index()) return a.index() < b.index();

    switch (a.index())
    {
        case 3: return get<string>(a) < get<string>(b);
        case 4: return get<bool>(a) < get<bool>(b);
        default: return false;
    }
}

bool valueEqual(const Value& a, const Value& b)
{
    if (isNumeric(a) && isNumeric(b)) return toDouble(a) == toDouble(b);
    return a == b;
}

bool addNumeric(Value& acc, const Value& val)
{
    if (val.index() == 0) return true; // NULL is ignored in SUM
    switch (val.index())
    {
        case 1: // int
        {
            int v = get<int>(val);
            if (acc.index() == 0) acc = v;
            else if (acc.index() == 1) acc = get<int>(acc) + v;
            else if (acc.index() == 2) acc = get<double>(acc) + v;
            return true;
        }
        case 2: // double
        {
            double v = get<double>(val);
            if (acc.index() == 0) acc = v;
            else if (acc.index() == 2) acc = get<double>(acc) + v;
            else if (acc.index() == 1) acc = static_cast<double>(get<int>(acc)) + v;
            return true;
        }
        case 4: // bool
        {
            int v = get<bool>(val) ? 1 : 0;
            if (acc.index() == 0) acc = v;
            else if (acc.index() == 1) acc = get<int>(acc) + v;
            else if (acc.index() == 2) acc = get<double>(acc) + v;
            return true;
        }
        default:
            return false; // Non-numeric type cannot be summed
    }
}

} // anonymous namespace

namespace Query {

// ============================================================
// SUM FUNCTIONS
// ============================================================

Vector<Record> sum(Vector<Record> &data, string groupedColumn, string calculatedColumn)
{
    HashMap<Value, Value> map;

    for (Record &item : data)
    {
        if (!item.has(groupedColumn) || !item.has(calculatedColumn))
        {
            return Vector<Record>();
        }
        Value grouped_key = item.get(groupedColumn);
        Value tmp_value = item.get(calculatedColumn);
        Value &acc = map[grouped_key];
        if (!addNumeric(acc, tmp_value))
        {
            return Vector<Record>();
        }
    }

    Vector<Record> result(map.size() + 1);
    for (const auto& entry : map)
    {
        Record tmp_r;
        tmp_r.set(groupedColumn, entry.first);
        Value val = entry.second;
        if (val.index() == 0) val = 0;
        tmp_r.set("sum_" + calculatedColumn, val);
        result.push_back(tmp_r);
    }
    return result;
}

Vector<Record> sum(Vector<Record> &data, Pair<string, string> groupedColumns, string calculatedColumn)
{
    HashMap<Pair<Value, Value>, Value> map;

    for (Record &item : data)
    {
        if (!item.has(groupedColumns.first) || !item.has(groupedColumns.second) || !item.has(calculatedColumn))
        {
            return Vector<Record>();
        }
        Pair<Value, Value> grouped_key(item.get(groupedColumns.first), item.get(groupedColumns.second));
        Value tmp_value = item.get(calculatedColumn);
        Value &acc = map[grouped_key];
        if (!addNumeric(acc, tmp_value))
        {
            return Vector<Record>();
        }
    }

    Vector<Record> result(map.size() + 1);
    for (const auto& entry : map)
    {
        Record tmp_r;
        tmp_r.set(groupedColumns.first, entry.first.first);
        tmp_r.set(groupedColumns.second, entry.first.second);
        Value val = entry.second;
        if (val.index() == 0) val = 0;
        tmp_r.set("sum_" + calculatedColumn, val);
        result.push_back(tmp_r);
    }
    return result;
}

Vector<Record> sum(Vector<Record> &data, Pair<string, string> groupedColumns, Pair<string, string> calculatedColumn)
{
    HashMap<Pair<Value, Value>, Pair<Value, Value>> map;

    for (Record &item : data)
    {
        if (!item.has(groupedColumns.first) || !item.has(groupedColumns.second) ||
            !item.has(calculatedColumn.first) || !item.has(calculatedColumn.second))
        {
            return Vector<Record>();
        }
        Pair<Value, Value> grouped_key(item.get(groupedColumns.first), item.get(groupedColumns.second));
        Value val1 = item.get(calculatedColumn.first);
        Value val2 = item.get(calculatedColumn.second);
        Pair<Value, Value> &accPair = map[grouped_key];
        if (!addNumeric(accPair.first, val1) || !addNumeric(accPair.second, val2))
        {
            return Vector<Record>();
        }
    }

    Vector<Record> result(map.size() + 1);
    for (const auto& entry : map)
    {
        Record tmp_r;
        tmp_r.set(groupedColumns.first, entry.first.first);
        tmp_r.set(groupedColumns.second, entry.first.second);
        Value v1 = entry.second.first;
        Value v2 = entry.second.second;
        if (v1.index() == 0) v1 = 0;
        if (v2.index() == 0) v2 = 0;
        tmp_r.set("sum_" + calculatedColumn.first, v1);
        tmp_r.set("sum_" + calculatedColumn.second, v2);
        result.push_back(tmp_r);
    }
    return result;
}

// ============================================================
// COUNT FUNCTIONS
// ============================================================

Vector<Record> count(Vector<Record> &data, string groupedColumn, string calculatedColumn)
{
    HashMap<Value, int> map;

    for (Record &item : data)
    {
        if (!item.has(groupedColumn) || !item.has(calculatedColumn))
        {
            return Vector<Record>();
        }
        Value grouped_key = item.get(groupedColumn);
        Value tmp_value = item.get(calculatedColumn);
        if (!map.contains(grouped_key))
        {
            map[grouped_key] = 0;
        }
        if (!isNull(tmp_value))
        {
            map[grouped_key]++;
        }
    }

    Vector<Record> result(map.size() + 1);
    for (const auto& entry : map)
    {
        Record tmp_r;
        tmp_r.set(groupedColumn, entry.first);
        tmp_r.set("count_" + calculatedColumn, entry.second);
        result.push_back(tmp_r);
    }
    return result;
}

Vector<Record> count(Vector<Record> &data, Pair<string, string> groupedColumns, string calculatedColumn)
{
    HashMap<Pair<Value, Value>, int> map;

    for (Record &item : data)
    {
        if (!item.has(groupedColumns.first) || !item.has(groupedColumns.second) || !item.has(calculatedColumn))
        {
            return Vector<Record>();
        }
        Pair<Value, Value> grouped_key(item.get(groupedColumns.first), item.get(groupedColumns.second));
        Value tmp_value = item.get(calculatedColumn);
        if (!map.contains(grouped_key))
        {
            map[grouped_key] = 0;
        }
        if (!isNull(tmp_value))
        {
            map[grouped_key]++;
        }
    }

    Vector<Record> result(map.size() + 1);
    for (const auto& entry : map)
    {
        Record tmp_r;
        tmp_r.set(groupedColumns.first, entry.first.first);
        tmp_r.set(groupedColumns.second, entry.first.second);
        tmp_r.set("count_" + calculatedColumn, entry.second);
        result.push_back(tmp_r);
    }
    return result;
}

Vector<Record> count(Vector<Record> &data, Pair<string, string> groupedColumns, Pair<string, string> calculatedColumn)
{
    HashMap<Pair<Value, Value>, Pair<int, int>> map;

    for (Record &item : data)
    {
        if (!item.has(groupedColumns.first) || !item.has(groupedColumns.second) ||
            !item.has(calculatedColumn.first) || !item.has(calculatedColumn.second))
        {
            return Vector<Record>();
        }
        Pair<Value, Value> grouped_key(item.get(groupedColumns.first), item.get(groupedColumns.second));
        Value val1 = item.get(calculatedColumn.first);
        Value val2 = item.get(calculatedColumn.second);

        if (!map.contains(grouped_key))
        {
            map[grouped_key] = Pair<int, int>(0, 0);
        }
        Pair<int, int>& counts = map[grouped_key];
        if (!isNull(val1)) counts.first++;
        if (!isNull(val2)) counts.second++;
    }

    Vector<Record> result(map.size() + 1);
    for (const auto& entry : map)
    {
        Record tmp_r;
        tmp_r.set(groupedColumns.first, entry.first.first);
        tmp_r.set(groupedColumns.second, entry.first.second);
        tmp_r.set("count_" + calculatedColumn.first, entry.second.first);
        tmp_r.set("count_" + calculatedColumn.second, entry.second.second);
        result.push_back(tmp_r);
    }
    return result;
}

// ============================================================
// SORT FUNCTIONS
// ============================================================

void querySort(Vector<Record> &data, string groupedColumn, bool ascending)
{
    std::sort(data.begin(), data.end(), [&](const Record& a, const Record& b) {
        if (!a.has(groupedColumn) || !b.has(groupedColumn)) return false;
        Value va = a.get(groupedColumn);
        Value vb = b.get(groupedColumn);
        if (valueEqual(va, vb)) return false;
        bool less = valueLess(va, vb);
        return ascending ? less : !less;
    });
}

void querySort(Vector<Record> &data, Pair<string, string> groupedColumns, bool ascending)
{
    std::sort(data.begin(), data.end(), [&](const Record& a, const Record& b) {
        if (!a.has(groupedColumns.first) || !b.has(groupedColumns.first)) return false;
        Value va1 = a.get(groupedColumns.first);
        Value vb1 = b.get(groupedColumns.first);
        if (!valueEqual(va1, vb1))
        {
            bool less = valueLess(va1, vb1);
            return ascending ? less : !less;
        }

        if (!a.has(groupedColumns.second) || !b.has(groupedColumns.second)) return false;
        Value va2 = a.get(groupedColumns.second);
        Value vb2 = b.get(groupedColumns.second);
        if (valueEqual(va2, vb2)) return false;
        bool less = valueLess(va2, vb2);
        return ascending ? less : !less;
    });
}

} // namespace Query