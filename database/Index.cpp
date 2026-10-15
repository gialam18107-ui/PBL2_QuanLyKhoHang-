#include "Index.h"

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
    indexMap[key] = rowIndex;
}

void HashIndex::remove(const Value& value, size_t rowIndex)
{
    string key = valueToHashKey(value);
    auto it = indexMap.find(key);
    if (it != indexMap.end() && it->second == rowIndex)
    {
        indexMap.erase(key);
    }
}

void HashIndex::clear()
{
    indexMap.clear();
}

Vector<size_t> HashIndex::find(const Value& value) const
{
    Vector<size_t> result;
    string key = valueToHashKey(value);
    auto it = indexMap.find(key);
    if (it != indexMap.end())
    {
        result.push_back(it->second);
    }
    return result;
}

Vector<size_t> HashIndex::findRange(const Value& /*minVal*/, const Value& /*maxVal*/) const
{
    return {};
}

// ============================================================
// AVL TREE INDEX
// ============================================================

AVLTreeIndex::AVLTreeIndex(
    const string& name,
    const string& columnName
)
    : Index(name, columnName, IndexType::AVL)
{
}

Index* AVLTreeIndex::clone() const
{
    return new AVLTreeIndex(*this);
}

void AVLTreeIndex::build(const Vector<Record>& records)
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

void AVLTreeIndex::insert(const Value& value, size_t rowIndex)
{
    tree.insert(makePair(value, rowIndex), 0);
}

void AVLTreeIndex::remove(const Value& value, size_t rowIndex)
{
    tree.remove(makePair(value, rowIndex));
}

void AVLTreeIndex::clear()
{
    tree.clear();
}

Vector<size_t> AVLTreeIndex::find(const Value& value) const
{
    Vector<size_t> result;
    for (auto it = tree.lower_bound(value); it != tree.end() && it->key == value; ++it)
    {
        result.push_back(it->key.second);
    }
    return result;
}

Vector<size_t> AVLTreeIndex::findRange(const Value& minVal, const Value& maxVal) const
{
    Vector<size_t> result;
    if (maxVal < minVal) return result;

    for (auto it = tree.lower_bound(minVal); it != tree.end() && !(maxVal < it->key); ++it)
    {
        result.push_back(it->key.second);
    }
    return result;
}

// ============================================================
// HASH TREE INDEX
// ============================================================

HashTreeIndex::HashTreeIndex(
    const string& name,
    const string& columnName
)
    : Index(name, columnName, IndexType::HASH_TREE)
{
}

Index* HashTreeIndex::clone() const
{
    return new HashTreeIndex(*this);
}

void HashTreeIndex::build(const Vector<Record>& records)
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

void HashTreeIndex::insert(const Value& value, size_t rowIndex)
{
    string key = valueToHashKey(value);
    indexMap[key].insert(rowIndex, 0);
}

void HashTreeIndex::remove(const Value& value, size_t rowIndex)
{
    string key = valueToHashKey(value);
    auto it = indexMap.find(key);
    if (it != indexMap.end())
    {
        it->second.remove(rowIndex);
        if (it->second.empty())
        {
            indexMap.erase(key);
        }
    }
}

void HashTreeIndex::clear()
{
    indexMap.clear();
}

Vector<size_t> HashTreeIndex::find(const Value& value) const
{
    Vector<size_t> result;
    string key = valueToHashKey(value);
    auto it = indexMap.find(key);
    if (it != indexMap.end())
    {
        for (const auto& node : it->second)
        {
            result.push_back(node.key);
        }
    }
    return result;
}

Vector<size_t> HashTreeIndex::findRange(const Value& /*minVal*/, const Value& /*maxVal*/) const
{
    return {};
}
