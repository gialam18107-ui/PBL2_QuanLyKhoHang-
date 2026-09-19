#ifndef TABLE_H
#define TABLE_H

#include <string>
#include <vector>
#include <memory>

#include "Column.h"
#include "Record.h"
#include "Constraint.h"
#include "Condition.h"

using namespace std;

class Table
{
private:
    string name;

    vector<Column> columns;
    vector<Record> rows;

    vector<shared_ptr<Constraint>> constraints;

    bool checkRecord(
        const Record& record
    ) const;

    bool checkDataType(
        const Column& column,
        const Value& value
    ) const;

    bool checkConstraints(
        const Record& record,
        int ignoreRowIndex = -1
    ) const;

public:
    Table(const string& name);

    string getName() const;


    // ========================================================
    // COLUMN
    // ========================================================

    void addColumn(
        const Column& column
    );

    bool hasColumn(
        const string& columnName
    ) const;

    Column* getColumn(
        const string& columnName
    );

    const Column* getColumn(
        const string& columnName
    ) const;

    int getColumnCount() const;

    const vector<Column>& getColumns() const;


    // ========================================================
    // CONSTRAINT
    // ========================================================

    bool addConstraint(
        const shared_ptr<Constraint>& constraint
    );

    const vector<shared_ptr<Constraint>>&
    getConstraints() const;


    // ========================================================
    // RECORD
    // ========================================================

    bool insert(
        const Record& record
    );

    bool update(
        const Condition& condition,
        const Record& newValues
    );

    int getRowCount() const;

    const vector<Record>& getRows() const;


    // ========================================================
    // DISPLAY
    // ========================================================

    void printStructure() const;

    void printRows() const;


    // ========================================================
    // OTHER
    // ========================================================

    void clear();

    bool remove(
    const Condition& condition);
};

#endif
