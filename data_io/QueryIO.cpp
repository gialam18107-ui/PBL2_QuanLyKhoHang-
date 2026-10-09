#include "QueryIO.h"
#include "CSVHelper.h"
#include "DatabaseIO.h"

#include <fstream>
#include <iostream>

namespace DataIO
{
    bool QueryIO::write(const Query& query, const string& filePath)
    {
        Vector<JoinedRecord> records = query.execute();
        Vector<string> headers = query.getSelectedColumns();

        if (headers.empty() && !records.empty())
        {
            for (const auto& pair : records[0].getValues())
            {
                headers.push_back(pair.first);
            }
        }

        return writeResults(records, headers, filePath);
    }

    bool QueryIO::writeToFolder(const Query& query, const string& folderPath)
    {
        string filePath = folderPath;
        if (!filePath.empty() && filePath.back() != '/' && filePath.back() != '\\')
        {
            filePath += "/";
        }
        string tableName = query.getBaseTable();
        if (tableName.empty())
        {
            tableName = "query_result";
        }
        filePath += tableName + ".csv";

        return write(query, filePath);
    }

    bool QueryIO::writeResults(
        const Vector<JoinedRecord>& records,
        const Vector<string>& headers,
        const string& filePath
    )
    {
        ofstream file(filePath);
        if (!file.is_open())
        {
            cerr << "[QueryIO Error] Cannot open file for writing: " << filePath << endl;
            return false;
        }

        file << CSVHelper::formatLine(headers) << "\n";

        for (size_t r = 0; r < records.size(); r++)
        {
            const auto& record = records[r];
            Vector<string> rowFields;
            for (size_t c = 0; c < headers.size(); c++)
            {
                const string& colName = headers[c];
                if (record.has(colName))
                {
                    JoinedValue val = record.get(colName);
                    rowFields.push_back(valueToString(val));
                }
                else
                {
                    rowFields.push_back("");
                }
            }
            file << CSVHelper::formatLine(rowFields) << "\n";
        }

        return true;
    }

    Vector<JoinedRecord> QueryIO::read(Query& query, const string& folderPath)
    {
        Vector<string> requiredTables;
        auto addRequiredTable = [&](const string& tableName) {
            if (tableName.empty())
            {
                return;
            }
            for (size_t i = 0; i < requiredTables.size(); i++)
            {
                if (requiredTables[i] == tableName)
                {
                    return;
                }
            }
            requiredTables.push_back(tableName);
        };

        addRequiredTable(query.getBaseTable());
        for (size_t i = 0; i < query.getJoins().size(); i++)
        {
            const auto& join = query.getJoins()[i];
            addRequiredTable(join.getLeftTable());
            addRequiredTable(join.getRightTable());
        }

        Database* db = &query.getDatabase();
        if (db != nullptr)
        {
            for (size_t i = 0; i < requiredTables.size(); i++)
            {
                const string& tableName = requiredTables[i];
                auto table = db->getTable(tableName);
                if (table == nullptr || table->getRowCount() == 0)
                {
                    DatabaseIO::readTable(*db, tableName, folderPath);
                }
            }
        }

        return query.execute();
    }

    Vector<JoinedRecord> QueryIO::readResults(const string& filePath)
    {
        Vector<JoinedRecord> results;

        ifstream file(filePath);
        if (!file.is_open())
        {
            cerr << "[QueryIO Error] Cannot open file for reading: " << filePath << endl;
            return results;
        }

        string headerLine;
        if (!getline(file, headerLine))
        {
            return results;
        }

        Vector<string> headers = CSVHelper::parseLine(headerLine);
        if (headers.empty())
        {
            return results;
        }

        string line;
        while (getline(file, line))
        {
            if (line.empty()) continue;

            Vector<string> fields = CSVHelper::parseLine(line);
            if (fields.empty()) continue;

            JoinedRecord record;
            for (size_t i = 0; i < headers.size() && i < fields.size(); i++)
            {
                const string& colName = headers[i];
                const string& valStr = fields[i];

                bool isInt = true;
                if (valStr.empty())
                {
                    isInt = false;
                }
                else
                {
                    size_t start = (valStr[0] == '-' || valStr[0] == '+') ? 1 : 0;
                    if (start >= valStr.size())
                    {
                        isInt = false;
                    }
                    for (size_t k = start; k < valStr.size(); k++)
                    {
                        if (!isdigit(static_cast<unsigned char>(valStr[k])))
                        {
                            isInt = false;
                            break;
                        }
                    }
                }

                if (isInt)
                {
                    try
                    {
                        record.set(colName, stoi(valStr));
                    }
                    catch (...)
                    {
                        record.set(colName, valStr);
                    }
                }
                else
                {
                    record.set(colName, valStr);
                }
            }

            results.push_back(record);
        }

        return results;
    }
}
