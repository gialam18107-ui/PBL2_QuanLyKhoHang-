#include "Index.h"
#include <algorithm>

Index::Index(
    const string& name,
    const string& columnName,
    IndexType type
)
    : name(name), columnName(columnName), type(type)
{
}

string Index::getName() const
{
    return name;
}

string Index::getColumnName() const
{
    return columnName;
}

IndexType Index::getType() const
{
    return type;
}

// ============================================================
// HASH INDEX
// ============================================================

HashIndex::HashIndex(
    const string& name,
    const string& columnName
)
    : Index(name, columnName, IndexType::HASH)
{
}

Index* HashIndex::clone() const
{
    return new HashIndex(*this);
}

void HashIndex::build(const Vector<Record>& records)
{
    clear();
    for (size_t i = 0; i < records.size(); i++)
    {
        if (records[i].has(columnName))
        {
            insert(records[i].get(columnName), i);
        }
    }
}

void HashIndex::insert(const Value& value, size_t rowIndex)
{
    string key = valueToString(value);
    indexMap[key].push_back(rowIndex);
}

void HashIndex::remove(const Value& value, size_t rowIndex)
{
    string key = valueToString(value);
    auto it = indexMap.find(key);
    if (it != indexMap.end())
    {
        Vector<size_t>& rowIndices = it->second;
        for (size_t i = 0; i < rowIndices.size(); i++)
        {
            if (rowIndices[i] == rowIndex)
            {
                rowIndices.erase(i);
                break;
            }
        }
        if (rowIndices.empty())
        {
            indexMap.erase(key);
        }
    }
}

void HashIndex::clear()
{
    indexMap.clear();
}

Vector<size_t> HashIndex::find(const Value& value) const
{
    string key = valueToString(value);
    auto it = indexMap.find(key);
    if (it != indexMap.end())
    {
        return it->second;
    }
    return {};
}

Vector<size_t> HashIndex::findRange(const Value& /*minVal*/, const Value& /*maxVal*/) const
{
    return {};
}

// ============================================================
// BTREE INDEX (Ordered Index Implementation)
// ============================================================

bool BTreeIndex::valueLess(const Value& a, const Value& b)
{
    auto isNum = [](const Value& v) {
        return holds_alternative<int>(v) || holds_alternative<double>(v);
    };
    auto toDbl = [](const Value& v) -> double {
        if (holds_alternative<int>(v)) return static_cast<double>(get<int>(v));
        if (holds_alternative<double>(v)) return get<double>(v);
        return 0.0;
    };

    if (isNum(a) && isNum(b))
    {
        return toDbl(a) < toDbl(b);
    }
    if (a.index() != b.index())
    {
        return a.index() < b.index();
    }
    if (holds_alternative<string>(a))
    {
        return get<string>(a) < get<string>(b);
    }
    if (holds_alternative<bool>(a))
    {
        return !get<bool>(a) && get<bool>(b);
    }
    return false;
}

bool BTreeIndex::valueEqual(const Value& a, const Value& b)
{
    auto isNum = [](const Value& v) {
        return holds_alternative<int>(v) || holds_alternative<double>(v);
    };
    auto toDbl = [](const Value& v) -> double {
        if (holds_alternative<int>(v)) return static_cast<double>(get<int>(v));
        if (holds_alternative<double>(v)) return get<double>(v);
        return 0.0;
    };

    if (isNum(a) && isNum(b))
    {
        return toDbl(a) == toDbl(b);
    }
    return a == b;
}

BTreeIndex::BTreeIndex(
    const string& name,
    const string& columnName
)
    : Index(name, columnName, IndexType::BTREE)
{
}

Index* BTreeIndex::clone() const
{
    return new BTreeIndex(*this);
}

void BTreeIndex::build(const Vector<Record>& records)
{
    clear();
    for (size_t i = 0; i < records.size(); i++)
    {
        if (records[i].has(columnName))
        {
            insert(records[i].get(columnName), i);
        }
    }
}

void BTreeIndex::insert(const Value& value, size_t rowIndex)
{
    size_t left = 0;
    size_t right = entries.size();
    while (left < right)
    {
        size_t mid = left + (right - left) / 2;
        if (valueLess(entries[mid].first, value))
        {
            left = mid + 1;
        }
        else
        {
            right = mid;
        }
    }
    entries.insert(left, makePair(value, rowIndex));
}

void BTreeIndex::remove(const Value& value, size_t rowIndex)
{
    for (size_t i = 0; i < entries.size(); i++)
    {
        if (valueEqual(entries[i].first, value) && entries[i].second == rowIndex)
        {
            entries.erase(i);
            break;
        }
    }
}

void BTreeIndex::clear()
{
    entries.clear();
}

Vector<size_t> BTreeIndex::find(const Value& value) const
{
    Vector<size_t> result;
    size_t left = 0;
    size_t right = entries.size();
    while (left < right)
    {
        size_t mid = left + (right - left) / 2;
        if (valueLess(entries[mid].first, value))
        {
            left = mid + 1;
        }
        else
        {
            right = mid;
        }
    }
    while (left < entries.size() && valueEqual(entries[left].first, value))
    {
        result.push_back(entries[left].second);
        left++;
    }
    return result;
}

Vector<size_t> BTreeIndex::findRange(const Value& minVal, const Value& maxVal) const
{
    Vector<size_t> result;
    size_t left = 0;
    size_t right = entries.size();
    while (left < right)
    {
        size_t mid = left + (right - left) / 2;
        if (valueLess(entries[mid].first, minVal))
        {
            left = mid + 1;
        }
        else
        {
            right = mid;
        }
    }
    while (left < entries.size() && !valueLess(maxVal, entries[left].first))
    {
        result.push_back(entries[left].second);
        left++;
    }
    return result;
}
