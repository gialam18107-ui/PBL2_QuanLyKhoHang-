#ifndef TABLEVIEW_H
#define TABLEVIEW_H

#include <cstddef>
#include <utility>
#include "datastructure/vector/Vector.h"
#include "Record.h"

class Table;

/**
 * @brief Lightweight cursor/view representing a sequence of non-deleted records in a Table.
 * Supports both Full Table Scan and Index Scan under the exact same interface.
 * Does not copy Record objects during iteration (yields const Record&).
 */
class TableView
{
    friend class Table;

public:
    class Iterator
    {
    private:
        const Table* table;
        const Vector<size_t>* matches; // nullptr for full table scan
        size_t pos;

        void skipDeleted();

    public:
        Iterator(const Table* tbl, const Vector<size_t>* m, size_t p);

        const Record& operator*() const;
        const Record* operator->() const;
        Iterator& operator++();
        Iterator operator++(int);
        bool operator==(const Iterator& other) const;
        bool operator!=(const Iterator& other) const;

        size_t rowIndex() const;
    };

private:
    const Table* table;
    Vector<size_t> indexMatches;
    bool useIndex;

    // Full scan view (only callable by Table::view)
    explicit TableView(const Table* tbl);

    // Index scan view (only callable by Table::view)
    TableView(const Table* tbl, Vector<size_t>&& matches);

public:
    // Default constructor: empty view
    TableView();

    Iterator begin() const;
    Iterator end() const;

    bool empty() const;
    size_t size() const;
};

#endif // TABLEVIEW_H
