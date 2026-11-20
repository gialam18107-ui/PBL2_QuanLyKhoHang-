#include "Index.h"

Index::Index(
    const string& columnName,
    IndexType type
)
    : columnName(columnName), type(type)
{
}

const string& Index::getColumnName() const
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

HashIndex::HashIndex(const string& columnName)
    : Index(columnName, IndexType::HASH)
{
}

Index* HashIndex::clone() const
{
    return new HashIndex(*this);
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

bool HashIndex::contains(const Value& value, int ignoreRowIndex) const
{
    string key = valueToHashKey(value);
    auto it = indexMap.find(key);
    if (it == indexMap.end())
    {
        return false;
    }
    return static_cast<int>(it->second) != ignoreRowIndex;
}

bool HashIndex::findFirst(const Value& value, size_t& outRowIndex) const
{
    string key = valueToHashKey(value);
    auto it = indexMap.find(key);
    if (it == indexMap.end())
    {
        return false;
    }
    outRowIndex = it->second;
    return true;
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

AVLTreeIndex::AVLTreeIndex(const string& columnName)
    : Index(columnName, IndexType::AVL)
{
}

Index* AVLTreeIndex::clone() const
{
    return new AVLTreeIndex(*this);
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

bool AVLTreeIndex::contains(const Value& value, int ignoreRowIndex) const
{
    for (auto it = tree.lower_bound(value); it != tree.end() && it->key == value; ++it)
    {
        if (static_cast<int>(it->key.second) != ignoreRowIndex)
        {
            return true;
        }
    }
    return false;
}

bool AVLTreeIndex::findFirst(const Value& value, size_t& outRowIndex) const
{
    auto it = tree.lower_bound(value);
    if (it != tree.end() && it->key == value)
    {
        outRowIndex = it->key.second;
        return true;
    }
    return false;
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

HashTreeIndex::HashTreeIndex(const string& columnName)
    : Index(columnName, IndexType::HASH_TREE)
{
}

Index* HashTreeIndex::clone() const
{
    return new HashTreeIndex(*this);
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

bool HashTreeIndex::contains(const Value& value, int ignoreRowIndex) const
{
    string key = valueToHashKey(value);
    auto it = indexMap.find(key);
    if (it == indexMap.end() || it->second.empty())
    {
        return false;
    }
    if (ignoreRowIndex < 0)
    {
        return true;
    }
    for (const auto& node : it->second)
    {
        if (static_cast<int>(node.key) != ignoreRowIndex)
        {
            return true;
        }
    }
    return false;
}

bool HashTreeIndex::findFirst(const Value& value, size_t& outRowIndex) const
{
    string key = valueToHashKey(value);
    auto it = indexMap.find(key);
    if (it == indexMap.end() || it->second.empty())
    {
        return false;
    }
    auto treeIt = it->second.begin();
    if (treeIt == it->second.end())
    {
        return false;
    }
    outRowIndex = treeIt->key;
    return true;
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
