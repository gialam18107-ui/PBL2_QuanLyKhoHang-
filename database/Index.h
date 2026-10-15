#ifndef INDEX_H
#define INDEX_H

#include <string>
#include "datastructure/vector/Vector.h"
#include "datastructure/hashmap/HashMap.h"
#include "datastructure/hashmap/Pair.h"
#include "Record.h"

using namespace std;

enum class IndexType
{
    HASH,
    BTREE
};

/**
 * @brief Abstract base class for table indexes (HashIndex, BTreeIndex).
 */
class Index
{
protected:
    string name;
    string columnName;
    IndexType type;

public:
    Index(
        const string& name,
        const string& columnName,
        IndexType type
    );

    virtual ~Index() = default;

    virtual Index* clone() const = 0;

    /// Get index identifier name.
    string getName() const;

    /// Get indexed column name.
    string getColumnName() const;

    /// Get index type (HASH or BTREE).
    IndexType getType() const;

    /**
     * @brief Build/rebuild index from a list of records.
     * @param records Complete list of table records to index.
     */
    virtual void build(const Vector<Record>& records) = 0;

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
     * @brief Find all row indices matching an exact value (O(1) in HashIndex, O(log N) in BTree).
     * @param value Value to search for.
     * @return Vector<size_t> Matching row indices.
     */
    virtual Vector<size_t> find(const Value& value) const = 0;

    /**
     * @brief Range query: find all row indices where minVal <= value <= maxVal (supported by BTree).
     * @param minVal Inclusive lower bound.
     * @param maxVal Inclusive upper bound.
     */
    virtual Vector<size_t> findRange(const Value& minVal, const Value& maxVal) const = 0;
};

// ============================================================
// HASH INDEX
// ============================================================

/**
 * @brief Hash-based table index for O(1) exact equality lookups.
 */
class HashIndex : public Index
{
private:
    HashMap<string, Vector<size_t>> indexMap;

public:
    /**
     * @brief Construct a HashIndex on a column.
     * @param name Index name (e.g. "idx_product_name").
     * @param columnName Target column name.
     */
    HashIndex(
        const string& name,
        const string& columnName
    );

    Index* clone() const override;

    void build(const Vector<Record>& records) override;
    void insert(const Value& value, size_t rowIndex) override;
    void remove(const Value& value, size_t rowIndex) override;
    void clear() override;

    Vector<size_t> find(const Value& value) const override;
    Vector<size_t> findRange(const Value& minVal, const Value& maxVal) const override;
};

// ============================================================
// Ordered vector index. Lookups use binary search; insertions and removals
// may shift entries.
// ============================================================

/**
 * @brief Ordered B-Tree index supporting both equality and range queries (findRange).
 */
class BTreeIndex : public Index
{
private:
    Vector<Pair<Value, size_t>> entries;

    static bool valueLess(const Value& a, const Value& b);
    static bool valueEqual(const Value& a, const Value& b);
    size_t lowerBound(const Value& value) const;

public:
    /**
     * @brief Construct a BTreeIndex on a column.
     * @param name Index name (e.g. "idx_product_price").
     * @param columnName Target column name.
     */
    BTreeIndex(
        const string& name,
        const string& columnName
    );

    Index* clone() const override;

    void build(const Vector<Record>& records) override;
    void insert(const Value& value, size_t rowIndex) override;
    void remove(const Value& value, size_t rowIndex) override;
    void clear() override;

    Vector<size_t> find(const Value& value) const override;
    Vector<size_t> findRange(const Value& minVal, const Value& maxVal) const override;
};

#endif // INDEX_H
