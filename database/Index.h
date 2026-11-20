#ifndef INDEX_H
#define INDEX_H

#include <string>
#include "datastructure/vector/Vector.h"
#include "datastructure/hashmap/HashMap.h"
#include "datastructure/hashmap/Pair.h"
#include "datastructure/avltree/AvlTree.h"
#include "Record.h"

using namespace std;

enum class IndexType
{
    HASH,
    AVL,
    HASH_TREE
};

/**
 * @brief Abstract base class for table indexes (HashIndex, AVLTreeIndex, HashTreeIndex).
 * Each column has at most one index, identified by columnName.
 */
class Index
{
protected:
    string columnName;
    IndexType type;

public:
    Index(
        const string& columnName,
        IndexType type
    );

    virtual ~Index() = default;

    virtual Index* clone() const = 0;

    /// Get indexed column name (also serves as the index key).
    const string& getColumnName() const;

    /// Get index type (HASH, AVL, HASH_TREE).
    IndexType getType() const;

    /**
     * @brief Insert an entry into the index.
     * @param value Column value to index.
     * @param rowIndex Row index pointing to location in Table::rows.
     */
    virtual void insert(const Value& value, size_t rowIndex) = 0;

    /**
     * @brief Remove an entry from the index.
     * @param value Column value.
     * @param rowIndex Row index to remove.
     */
    virtual void remove(const Value& value, size_t rowIndex) = 0;

    /**
     * @brief Clear all indexed entries.
     */
    virtual void clear() = 0;

    /**
     * @brief Zero-allocation check whether any active indexed entry matches value (optionally ignoring a row).
     */
    virtual bool contains(const Value& value, int ignoreRowIndex = -1) const = 0;

    /**
     * @brief Zero-allocation lookup for the first matching rowIndex.
     */
    virtual bool findFirst(const Value& value, size_t& outRowIndex) const = 0;

    /**
     * @brief Find all row indices matching an exact value.
     * @param value Value to search for.
     * @return Vector<size_t> Matching row indices.
     */
    virtual Vector<size_t> find(const Value& value) const = 0;

    /**
     * @brief Range query: find all row indices where minVal <= value <= maxVal (supported by AVLTreeIndex).
     * @param minVal Inclusive lower bound.
     * @param maxVal Inclusive upper bound.
     */
    virtual Vector<size_t> findRange(const Value& minVal, const Value& maxVal) const = 0;
};

// ============================================================
// HASH INDEX (Direct 1-1: HashMap<string, size_t>)
// ============================================================

/**
 * @brief Hash-based table index mapping each key to a single rowIndex.
 */
class HashIndex : public Index
{
private:
    HashMap<string, size_t> indexMap;

public:
    explicit HashIndex(const string& columnName);

    Index* clone() const override;

    void insert(const Value& value, size_t rowIndex) override;
    void remove(const Value& value, size_t rowIndex) override;
    void clear() override;

    bool contains(const Value& value, int ignoreRowIndex = -1) const override;
    bool findFirst(const Value& value, size_t& outRowIndex) const override;
    Vector<size_t> find(const Value& value) const override;
    Vector<size_t> findRange(const Value& minVal, const Value& maxVal) const override;
};

// ============================================================
// AVL TREE INDEX (Key: Pair<Value, size_t>, Value: dummy char)
// ============================================================

/**
 * @brief AVL tree index supporting exact match and range queries.
 * Key stores Pair<Value, size_t> (value and rowIndex), value of tree node is a dummy char.
 */
class AVLTreeIndex : public Index
{
private:
    AvlTree<Pair<Value, size_t>, char> tree;

public:
    explicit AVLTreeIndex(const string& columnName);

    Index* clone() const override;

    void insert(const Value& value, size_t rowIndex) override;
    void remove(const Value& value, size_t rowIndex) override;
    void clear() override;

    bool contains(const Value& value, int ignoreRowIndex = -1) const override;
    bool findFirst(const Value& value, size_t& outRowIndex) const override;
    Vector<size_t> find(const Value& value) const override;
    Vector<size_t> findRange(const Value& minVal, const Value& maxVal) const override;
};

// ============================================================
// HASH TREE INDEX (HashMap<string, AvlTree<size_t, char>>)
// ============================================================

/**
 * @brief Composite index combining HashMap for value buckets and AvlTree for row indices.
 */
class HashTreeIndex : public Index
{
private:
    HashMap<string, AvlTree<size_t, char>> indexMap;

public:
    explicit HashTreeIndex(const string& columnName);

    Index* clone() const override;

    void insert(const Value& value, size_t rowIndex) override;
    void remove(const Value& value, size_t rowIndex) override;
    void clear() override;

    bool contains(const Value& value, int ignoreRowIndex = -1) const override;
    bool findFirst(const Value& value, size_t& outRowIndex) const override;
    Vector<size_t> find(const Value& value) const override;
    Vector<size_t> findRange(const Value& minVal, const Value& maxVal) const override;
};

#endif // INDEX_H
