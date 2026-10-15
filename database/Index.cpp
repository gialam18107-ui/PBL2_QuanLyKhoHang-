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
    string key = valueToHashKey(value);
    indexMap[key].push_back(rowIndex);
}

void HashIndex::remove(const Value& value, size_t rowIndex)
{
    string key = valueToHashKey(value);
    auto it = indexMap.find(key);
    if (it != indexMap.end())
    {
        Vector<size_t>& rowIndices = it->second;
        for (size_t i = 0; i < rowIndices.size(); i++)
        {
            if (rowIndices[i] == rowIndex)
            {
                rowIndices[i] = rowIndices[rowIndices.size() - 1];
                rowIndices.pop_back();
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
    string key = valueToHashKey(value);
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
            entries.push_back(makePair(records[i].get(columnName), i));
        }
    }
    std::sort(entries.begin(), entries.end(), [](const Pair<Value, size_t>& left, const Pair<Value, size_t>& right) {
        if (valueLess(left.first, right.first)) return true;
        if (valueLess(right.first, left.first)) return false;
        return left.second > right.second;
    });
}

size_t BTreeIndex::lowerBound(const Value& value) const
{
    size_t left = 0;
    size_t right = entries.size();
    while (left < right)
    {
        const size_t mid = left + (right - left) / 2;
        if (valueLess(entries[mid].first, value))
        {
            left = mid + 1;
        }
        else
        {
            right = mid;
        }
    }
    return left;
}

void BTreeIndex::insert(const Value& value, size_t rowIndex)
{
    entries.insert(lowerBound(value), makePair(value, rowIndex));
}

void BTreeIndex::remove(const Value& value, size_t rowIndex)
{
    size_t entryIndex = lowerBound(value);
    while (entryIndex < entries.size() && valueEqual(entries[entryIndex].first, value))
    {
        if (entries[entryIndex].second == rowIndex)
        {
            entries.erase(entryIndex);
            return;
        }
        ++entryIndex;
    }
}

void BTreeIndex::clear()
{
    entries.clear();
}

Vector<size_t> BTreeIndex::find(const Value& value) const
{
    Vector<size_t> result;
    size_t entryIndex = lowerBound(value);
    while (entryIndex < entries.size() && valueEqual(entries[entryIndex].first, value))
    {
        result.push_back(entries[entryIndex].second);
        ++entryIndex;
    }
    return result;
}

Vector<size_t> BTreeIndex::findRange(const Value& minVal, const Value& maxVal) const
{
    Vector<size_t> result;
    if (valueLess(maxVal, minVal)) return result;

    size_t entryIndex = lowerBound(minVal);
    while (entryIndex < entries.size() && !valueLess(maxVal, entries[entryIndex].first))
    {
        result.push_back(entries[entryIndex].second);
        ++entryIndex;
    }
    return result;
}
