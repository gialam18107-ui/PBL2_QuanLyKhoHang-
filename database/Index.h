#ifndef INDEX_H
#define INDEX_H

#include <string>
#include "Vector.h"
#include "HashMap.h"
#include "Pair.h"
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

    string getName() const;
    string getColumnName() const;
    IndexType getType() const;

    /**
     * @brief Build/rebuild index from a list of records.
     */
    virtual void build(const Vector<Record>& records) = 0;

    /**
     * @brief Insert an entry into the index.
     */
    virtual void insert(const Value& value, size_t rowIndex) = 0;

    /**
     * @brief Remove an entry from the index.
     */
    virtual void remove(const Value& value, size_t rowIndex) = 0;

    /**
     * @brief Clear all indexed entries.
     */
    virtual void clear() = 0;

    /**
     * @brief Find all row indices matching value.
     */
    virtual Vector<size_t> find(const Value& value) const = 0;

    /**
     * @brief Range query (used by BTree).
     */
    virtual Vector<size_t> findRange(const Value& minVal, const Value& maxVal) const = 0;
};

// ============================================================
// HASH INDEX
// ============================================================
class HashIndex : public Index
{
private:
    HashMap<string, Vector<size_t>> indexMap;

public:
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
class BTreeIndex : public Index
{
private:
    Vector<Pair<Value, size_t>> entries;

    static bool valueLess(const Value& a, const Value& b);
    static bool valueEqual(const Value& a, const Value& b);
    size_t lowerBound(const Value& value) const;

public:
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
