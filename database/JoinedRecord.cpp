#include "JoinedRecord.h"


void JoinedRecord::set(
    const string& columnName,
    const JoinedValue& value
)
{
    values[columnName] = value;
}


bool JoinedRecord::has(
    const string& columnName
) const
{
    return values.find(columnName)
        != values.end();
}


JoinedValue JoinedRecord::get(
    const string& columnName
) const
{
    auto it =
        values.find(columnName);

    if (it == values.end())
    {
        throw runtime_error(
            "Column not found: "
            + columnName
        );
    }

    return it->second;
}


const unordered_map<
    string,
    JoinedValue
>&
JoinedRecord::getValues() const
{
    return values;
}


int JoinedRecord::size() const
{
    return static_cast<int>(
        values.size()
    );
}


void JoinedRecord::clear()
{
    values.clear();
}


void JoinedRecord::print() const
{
    for (
        const auto& item
        :
        values
    )
    {
        cout
            << item.first
            << " = ";

        visit(
            [](const auto& value)
            {
                cout << value;
            },
            item.second
        );

        cout << endl;
    }
}
