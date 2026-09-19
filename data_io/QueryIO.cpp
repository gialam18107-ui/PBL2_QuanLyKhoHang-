#include "QueryIO.h"
#include "CSVHelper.h"
#include "TableIO.h"
#include "schema/Schema.h"

#include <fstream>
#include <iostream>
#include <algorithm>

namespace DataIO
{
    bool QueryIO::write(const Query& query, const string& filePath)
    {
        vector<JoinedRecord> records = query.execute();
        vector<string> headers = query.getSelectedColumns();

        if (headers.empty() && !records.empty())
        {
            for (const auto& pair : records[0].getValues())
            {
                headers.push_back(pair.first);
            }
            sort(headers.begin(), headers.end());
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

    bool QueryIO::writeResults(const vector<JoinedRecord>& records,
                               const vector<string>& headers,
                               const string& filePath)
    {
        ofstream file(filePath);
        if (!file.is_open())
        {
            cerr << "[QueryIO Error] Cannot open file for writing: " << filePath << endl;
            return false;
        }

        file << CSVHelper::formatLine(headers) << "\n";

        for (const auto& record : records)
        {
            vector<string> rowFields;
            for (const auto& colName : headers)
            {
                if (record.has(colName))
                {
                    JoinedValue val = record.get(colName);
                    if (holds_alternative<int>(val))
                    {
                        rowFields.push_back(to_string(get<int>(val)));
                    }
                    else if (holds_alternative<double>(val))
                    {
                        rowFields.push_back(to_string(get<double>(val)));
                    }
                    else if (holds_alternative<string>(val))
                    {
                        rowFields.push_back(get<string>(val));
                    }
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

    vector<JoinedRecord> QueryIO::read(Query& query, const string& folderPath)
    {
        // 1. Identify all tables referenced by the query
        vector<string> requiredTables;
        auto addRequiredTable = [&](const string& tableName) {
            if (tableName.empty())
            {
                return;
            }
            for (const auto& name : requiredTables)
            {
                if (name == tableName)
                {
                    return;
                }
            }
            requiredTables.push_back(tableName);
        };

        addRequiredTable(query.getBaseTable());
        for (const auto& join : query.getJoins())
        {
            addRequiredTable(join.getLeftTable());
            addRequiredTable(join.getRightTable());
        }

        // 2. Ensure each table is registered in Database and loaded from CSV
        Database& db = query.getDatabase();
        for (const string& tableName : requiredTables)
        {
            auto table = db.getTable(tableName);
            if (table == nullptr)
            {
                table = Schema::createTable(tableName);
                if (table != nullptr)
                {
                    db.addTable(table);
                }
            }

            if (table != nullptr && table->getRowCount() == 0)
            {
                TableIO::read(*table, folderPath);
            }
        }

        // 3. Execute and return results
        return query.execute();
    }

    vector<JoinedRecord> QueryIO::readResults(const string& filePath)
    {
        vector<JoinedRecord> results;

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

        vector<string> headers = CSVHelper::parseLine(headerLine);
        if (headers.empty())
        {
            return results;
        }

        string line;
        while (getline(file, line))
        {
            if (line.empty()) continue;

            vector<string> fields = CSVHelper::parseLine(line);
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
