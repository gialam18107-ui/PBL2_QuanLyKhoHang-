#include "Query.h"
#include "database/Database.h"
#include "database/Constraint.h"
#include <cctype>
#include <iostream>

using namespace std;

namespace {

bool g_queryDirty = false;

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

string toUpperStr(string s)
{
    for (char &c : s) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return s;
}

string toLowerStr(string s)
{
    for (char &c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}

} // anonymous namespace

namespace Query {

// ============================================================
// CACHE INVALIDATION & DIRTY FLAG
// ============================================================

bool isDirty()
{
    return g_queryDirty;
}

void setDirty(bool dirty)
{
    g_queryDirty = dirty;
}

void clearDirty()
{
    g_queryDirty = false;
}

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

// ============================================================
// INDEX HELPERS & FILTERING
// ============================================================

const Index* findIndexForFilter(const Table& table, const string& selectedColumn, bool isRange)
{
    if (isRange)
    {
        // For range queries, only AVL Tree index supports findRange
        for (const Index* idx : table.getIndexes())
        {
            if (idx != nullptr && idx->getColumnName() == selectedColumn && idx->getType() == IndexType::AVL)
            {
                return idx;
            }
        }
        const Index* index = table.getIndex(selectedColumn);
        if (index != nullptr && index->getType() == IndexType::AVL) return index;
        return nullptr;
    }

    // For exact equality matches, prioritize HASH > HASH_TREE > AVL
    for (const Index* idx : table.getIndexes())
    {
        if (idx != nullptr && idx->getColumnName() == selectedColumn && idx->getType() == IndexType::HASH)
        {
            return idx;
        }
    }

    for (const Index* idx : table.getIndexes())
    {
        if (idx != nullptr && idx->getColumnName() == selectedColumn && idx->getType() == IndexType::HASH_TREE)
        {
            return idx;
        }
    }

    for (const Index* idx : table.getIndexes())
    {
        if (idx != nullptr && idx->getColumnName() == selectedColumn && idx->getType() == IndexType::AVL)
        {
            return idx;
        }
    }

    const Index* index = table.getIndex(selectedColumn);
    if (index != nullptr) return index;

    for (const Index* idx : table.getIndexes())
    {
        if (idx != nullptr && idx->getColumnName() == selectedColumn)
        {
            return idx;
        }
    }

    return nullptr;
}

Vector<Record> filterRecords(const Table &table, string selectedColumn, const Value &value)
{
    const Index *index = findIndexForFilter(table, selectedColumn, false);
    if (index != nullptr)
    {
        Vector<size_t> data_id = index->find(value);
        const Vector<Record> &data = table.getRows();
        Vector<Record> results(data_id.size());

        for (size_t i : data_id)
        {
            if (!table.isRowDeleted(i))
            {
                results.push_back(data[i]);
            }
        }
        return results;
    }

    // Fallback: full table scan if no index exists on selectedColumn
    const Vector<Record>& data = table.getRows();
    Vector<Record> results;
    for (size_t i = 0; i < data.size(); ++i)
    {
        if (!table.isRowDeleted(i) && data[i].has(selectedColumn))
        {
            if (valueEqual(data[i].get(selectedColumn), value))
            {
                results.push_back(data[i]);
            }
        }
    }
    return results;
}

Vector<Record> filterRecords(const Table &table, string selectedColumn, const Value &minVal, const Value &maxVal)
{
    const Index *index = findIndexForFilter(table, selectedColumn, true);
    if (index != nullptr)
    {
        Vector<size_t> data_id = index->findRange(minVal, maxVal);
        const Vector<Record> &data = table.getRows();
        Vector<Record> results(data_id.size());

        for (size_t i : data_id)
        {
            if (!table.isRowDeleted(i))
            {
                results.push_back(data[i]);
            }
        }
        return results;
    }

    // Fallback: full table scan if no AVL index is available
    const Vector<Record>& data = table.getRows();
    Vector<Record> results;
    for (size_t i = 0; i < data.size(); ++i)
    {
        if (!table.isRowDeleted(i) && data[i].has(selectedColumn))
        {
            const Value& val = data[i].get(selectedColumn);
            if (!valueLess(val, minVal) && !valueLess(maxVal, val))
            {
                results.push_back(data[i]);
            }
        }
    }
    return results;
}

Vector<Record> filterRecords(const Table &table, const Condition &condition)
{
    // Try to optimize via available index
    if (condition.getOperation() == CompareOperator::EQUAL)
    {
        return filterRecords(table, condition.getColumnName(), condition.getCompareValue());
    }
    else if (condition.getOperation() == CompareOperator::BETWEEN)
    {
        return filterRecords(table, condition.getColumnName(), condition.getCompareValue(), condition.getSecondCompareValue());
    }
    else if (condition.getOperation() == CompareOperator::IN)
    {
        const Index* index = findIndexForFilter(table, condition.getColumnName(), false);
        if (index != nullptr)
        {
            const Vector<Record>& data = table.getRows();
            Vector<Record> results;
            HashMap<size_t, char> visited;
            for (const Value& v : condition.getInValues())
            {
                Vector<size_t> ids = index->find(v);
                for (size_t id : ids)
                {
                    if (!visited.contains(id) && !table.isRowDeleted(id))
                    {
                        visited[id] = 1;
                        results.push_back(data[id]);
                    }
                }
            }
            return results;
        }
    }

    // Fallback table scan
    const Vector<Record>& data = table.getRows();
    Vector<Record> results;
    for (size_t i = 0; i < data.size(); ++i)
    {
        if (!table.isRowDeleted(i) && condition.evaluate(data[i]))
        {
            results.push_back(data[i]);
        }
    }
    return results;
}

Vector<Record> filterRecords(const Table &table, const ConditionGroup &group)
{
    const Vector<Record>& data = table.getRows();
    Vector<Record> results;
    for (size_t i = 0; i < data.size(); ++i)
    {
        if (!table.isRowDeleted(i) && group.evaluate(data[i]))
        {
            results.push_back(data[i]);
        }
    }
    return results;
}

// ============================================================
// DELETION FUNCTIONS (FREE FUNCTIONS IN NAMESPACE QUERY)
// ============================================================

bool deleteRecords(Table &table, string selectedColumn, const Value &value, Database* db)
{
    Query q(table, db);
    q.where(selectedColumn, CompareOperator::EQUAL, value);
    return q.deleteRecords();
}

bool deleteRecords(Table &table, string selectedColumn, const Value &minVal, const Value &maxVal, Database* db)
{
    Query q(table, db);
    q.whereRange(selectedColumn, minVal, maxVal);
    return q.deleteRecords();
}

bool deleteRecords(Table &table, const Condition &condition, Database* db)
{
    Query q(table, db);
    q.where(condition);
    return q.deleteRecords();
}

bool deleteRecords(Table &table, const ConditionGroup &group, Database* db)
{
    Query q(table, db);
    q.where(group);
    return q.deleteRecords();
}

// ============================================================
// QUERY CLASS IMPLEMENTATION
// ============================================================

Query::Query()
    : table(nullptr), database(nullptr), logicalOp(LogicalOperator::AND)
{
}

Query::Query(Table& targetTable, Database* db)
    : table(&targetTable), database(db), logicalOp(LogicalOperator::AND)
{
}

Query::Query(Table* targetTable, Database* db)
    : table(targetTable), database(db), logicalOp(LogicalOperator::AND)
{
}

Query& Query::from(Table& targetTable)
{
    table = &targetTable;
    return *this;
}

Query& Query::from(Table* targetTable)
{
    table = targetTable;
    return *this;
}

Query& Query::setDatabase(Database* db)
{
    database = db;
    return *this;
}

Query& Query::where(const Condition& condition)
{
    conditions.push_back(condition);
    return *this;
}

Query& Query::where(const ConditionGroup& group)
{
    logicalOp = group.getOperation();
    for (const Condition& cond : group.getConditions())
    {
        conditions.push_back(cond);
    }
    return *this;
}

Query& Query::where(const string& column, CompareOperator op, const Value& val)
{
    conditions.push_back(Condition(column, op, val));
    return *this;
}

Query& Query::where(const string& column, const Value& val)
{
    conditions.push_back(Condition(column, CompareOperator::EQUAL, val));
    return *this;
}

Query& Query::whereRange(const string& column, const Value& minVal, const Value& maxVal)
{
    conditions.push_back(Condition(column, minVal, maxVal));
    return *this;
}

Vector<Record> Query::execute() const
{
    if (table == nullptr) return Vector<Record>();

    const Vector<Record>& allRows = table->getRows();
    if (conditions.empty())
    {
        Vector<Record> results;
        for (size_t i = 0; i < allRows.size(); ++i)
        {
            if (!table->isRowDeleted(i))
            {
                results.push_back(allRows[i]);
            }
        }
        return results;
    }

    if (conditions.size() == 1)
    {
        return filterRecords(*table, conditions[0]);
    }

    // Multiple conditions with AND logic: leverage available indexes
    if (logicalOp == LogicalOperator::AND)
    {
        int indexedCondIndex = -1;
        const Index* chosenIndex = nullptr;

        for (size_t i = 0; i < conditions.size(); ++i)
        {
            if (conditions[i].getOperation() == CompareOperator::EQUAL)
            {
                const Index* idx = findIndexForFilter(*table, conditions[i].getColumnName(), false);
                if (idx != nullptr)
                {
                    chosenIndex = idx;
                    indexedCondIndex = static_cast<int>(i);
                    break;
                }
            }
            else if (conditions[i].getOperation() == CompareOperator::BETWEEN)
            {
                const Index* idx = findIndexForFilter(*table, conditions[i].getColumnName(), true);
                if (idx != nullptr)
                {
                    chosenIndex = idx;
                    indexedCondIndex = static_cast<int>(i);
                    break;
                }
            }
        }

        Vector<Record> results;
        if (chosenIndex != nullptr && indexedCondIndex >= 0)
        {
            const Condition& indexedCond = conditions[indexedCondIndex];
            Vector<size_t> candidateIds;
            if (indexedCond.getOperation() == CompareOperator::EQUAL)
            {
                candidateIds = chosenIndex->find(indexedCond.getCompareValue());
            }
            else if (indexedCond.getOperation() == CompareOperator::BETWEEN)
            {
                candidateIds = chosenIndex->findRange(indexedCond.getCompareValue(), indexedCond.getSecondCompareValue());
            }

            for (size_t rowId : candidateIds)
            {
                if (table->isRowDeleted(rowId)) continue;
                const Record& r = allRows[rowId];
                bool allPass = true;
                for (size_t j = 0; j < conditions.size(); ++j)
                {
                    if (static_cast<int>(j) == indexedCondIndex) continue;
                    if (!conditions[j].evaluate(r))
                    {
                        allPass = false;
                        break;
                    }
                }
                if (allPass)
                {
                    results.push_back(r);
                }
            }
            return results;
        }

        // Fallback: full table scan for AND conditions
        for (size_t i = 0; i < allRows.size(); ++i)
        {
            if (table->isRowDeleted(i)) continue;
            bool pass = true;
            for (size_t j = 0; j < conditions.size(); ++j)
            {
                if (!conditions[j].evaluate(allRows[i]))
                {
                    pass = false;
                    break;
                }
            }
            if (pass)
            {
                results.push_back(allRows[i]);
            }
        }
        return results;
    }

    // Default fallback for OR or other logic
    ConditionGroup group(logicalOp);
    for (const Condition& c : conditions)
    {
        group.add(c);
    }
    return filterRecords(*table, group);
}

bool Query::isSafeToDelete(const Vector<Record>& targetRows) const
{
    if (table == nullptr) return false;
    string tblName = table->getName();
    string lowerName = toLowerStr(tblName);

    // Rule 4.4 & 2: Master data tables must NEVER be edited/deleted directly
    if (lowerName == "status" || lowerName == "type" || lowerName == "unit_status" ||
        lowerName == "inbound_status" || lowerName == "outbound_status" ||
        lowerName == "delivery_status" || lowerName == "shipment_status")
    {
        cout << "Delete rejected: master table '" << tblName << "' is read-only." << endl;
        return false;
    }

    // Rule 4.3: Confirmed transactions, deliveries, shipments must NEVER be deleted
    if (lowerName == "inbound_transaction" || lowerName == "outbound_transaction" ||
        lowerName == "delivery" || lowerName == "shipment" || lowerName == "unit")
    {
        for (size_t i = 0; i < targetRows.size(); ++i)
        {
            const Record& r = targetRows[i];
            if (r.has("status"))
            {
                const Value& sv = r.get("status");
                if (holds_alternative<string>(sv))
                {
                    string s = toUpperStr(get<string>(sv));
                    if (s == "CONFIRMED" || s == "COMPLETED" || s == "DELIVERED" || s == "SHIPPED")
                    {
                        cout << "Delete rejected: confirmed record in '" << tblName << "' cannot be deleted." << endl;
                        return false;
                    }
                }
            }
        }
    }

    // Rule 3: Referential integrity - record must not be deleted if referenced by another table
    if (database != nullptr)
    {
        Vector<string> allTableNames = database->getTableNames();
        for (size_t t = 0; t < allTableNames.size(); ++t)
        {
            Table* child = database->getTable(allTableNames[t]);
            if (child == nullptr || child->getName() == tblName) continue;

            for (size_t c = 0; c < child->getConstraints().size(); ++c)
            {
                auto* fk = dynamic_cast<ForeignKeyConstraint*>(child->getConstraints()[c]);
                if (fk == nullptr || fk->getReferenceTable() != tblName) continue;

                string refCol = fk->getReferenceColumn();
                string childCol = fk->getColumnName();

                for (size_t r = 0; r < targetRows.size(); ++r)
                {
                    if (!targetRows[r].has(refCol)) continue;
                    Value parentVal = targetRows[r].get(refCol);
                    if (isNull(parentVal)) continue;

                    // Automatically check child index if available for optimal referential integrity lookup
                    const Index* childIdx = findIndexForFilter(*child, childCol, false);
                    if (childIdx != nullptr)
                    {
                        Vector<size_t> childMatches = childIdx->find(parentVal);
                        for (size_t cm : childMatches)
                        {
                            if (!child->isRowDeleted(cm))
                            {
                                cout << "Delete rejected: record referenced by " << child->getName() << "." << childCol << endl;
                                return false;
                            }
                        }
                    }
                    else
                    {
                        const Vector<Record>& childRows = child->getRows();
                        for (size_t cr = 0; cr < childRows.size(); ++cr)
                        {
                            if (!child->isRowDeleted(cr) && childRows[cr].has(childCol))
                            {
                                if (valueEqual(childRows[cr].get(childCol), parentVal))
                                {
                                    cout << "Delete rejected: record referenced by " << child->getName() << "." << childCol << endl;
                                    return false;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return true;
}

bool Query::deleteRecords()
{
    if (table == nullptr) return false;

    // 1. Fetch matching rows
    Vector<Record> matchingRows = execute();
    if (matchingRows.empty()) return false;

    // 2. Validate safety and referential integrity
    if (!isSafeToDelete(matchingRows))
    {
        return false;
    }

    // 3. Remove records from the target table
    bool anyRemoved = false;
    if (conditions.empty())
    {
        // Deleting all table records without conditions is forbidden
        return false;
    }
    else if (conditions.size() == 1)
    {
        if (database != nullptr)
        {
            anyRemoved = database->remove(table->getName(), conditions[0]);
        }
        else
        {
            anyRemoved = table->remove(conditions[0]);
        }
    }
    else
    {
        ConditionGroup group(logicalOp);
        for (const Condition& c : conditions)
        {
            group.add(c);
        }
        if (database != nullptr)
        {
            anyRemoved = database->remove(table->getName(), group);
        }
        else
        {
            anyRemoved = table->remove(group);
        }
    }

    // 4. Trigger cache invalidation dirty flag
    if (anyRemoved)
    {
        setDirty(true);
    }

    return anyRemoved;
}

Table* Query::getTable() const
{
    return table;
}

Database* Query::getDatabase() const
{
    return database;
}

const Vector<Condition>& Query::getConditions() const
{
    return conditions;
}

void Query::clearConditions()
{
    conditions.clear();
}

} // namespace Query
