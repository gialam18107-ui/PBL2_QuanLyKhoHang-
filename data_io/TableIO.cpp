#include "TableIO.h"
#include "CSVHelper.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace DataIO
{
    bool TableIO::read(Table& table, const string& folderPath)
    {
        string filePath = folderPath;
        if (!filePath.empty() && filePath.back() != '/' && filePath.back() != '\\')
        {
            filePath += "/";
        }
        filePath += table.getName() + ".csv";
        return readFromFile(table, filePath);
    }

    bool TableIO::readFromFile(Table& table, const string& filePath)
    {
        // TableIO is schema-independent: only load data for an already-initialized table.
        if (table.getColumnCount() == 0)
        {
            cerr << "[TableIO Error] Table '" << table.getName()
                 << "' has no initialized columns." << endl;
            return false;
        }

        ifstream file(filePath);
        if (!file.is_open())
        {
            cerr << "[TableIO Error] Cannot open file for reading: " << filePath << endl;
            return false;
        }

        table.clear();

        string headerLine;
        if (!getline(file, headerLine))
        {
            return false;
        }

        Vector<string> headers = CSVHelper::parseLine(headerLine);
        if (headers.empty())
        {
            return false;
        }

        // Resolve column descriptors once per file for O(1) lookup per cell
        Vector<const Column*> headerColumns;
        headerColumns.reserve(headers.size());
        for (size_t i = 0; i < headers.size(); ++i)
        {
            headerColumns.push_back(table.getColumn(headers[i]));
        }

        string line;
        while (getline(file, line))
        {
            if (line.empty()) continue;

            Vector<string> fields = CSVHelper::parseLine(line);
            if (fields.empty()) continue;

            Record record;
            for (size_t i = 0; i < headers.size() && i < fields.size(); i++)
            {
                const Column* col = headerColumns[i];
                if (col == nullptr)
                {
                    continue;
                }

                const string& colName = col->getName();
                const string& valStr = fields[i];
                bool isNullToken = (valStr.empty() || valStr == "NULL");

                if (col->getType() == DataType::INTEGER)
                {
                    if (isNullToken)
                    {
                        if (!isNull(col->getDefaultValue()))
                        {
                            record.set(colName, col->getDefaultValue());
                        }
                    }
                    else
                    {
                        try
                        {
                            record.set(colName, stoi(valStr));
                        }
                        catch (...)
                        {
                            cerr << "[TableIO Warning] Failed to parse integer for column " << colName << ": " << valStr << endl;
                        }
                    }
                }
                else if (col->getType() == DataType::BOOLEAN)
                {
                    if (isNullToken)
                    {
                        if (!isNull(col->getDefaultValue()))
                        {
                            record.set(colName, col->getDefaultValue());
                        }
                    }
                    else
                    {
                        if (valStr == "1")
                        {
                            record.set(colName, true);
                        }
                        else if (valStr == "0")
                        {
                            record.set(colName, false);
                        }
                        else
                        {
                            cerr << "[TableIO Warning] Invalid boolean literal for column " << colName << ": " << valStr << " (expected 0 or 1)" << endl;
                        }
                    }
                }
                else
                {
                    if (isNullToken)
                    {
                        if (!isNull(col->getDefaultValue()))
                        {
                            record.set(colName, col->getDefaultValue());
                        }
                    }
                    else
                    {
                        record.set(colName, valStr);
                    }
                }
            }

            table.insert(record);
        }

        return true;
    }

    bool TableIO::write(const Table& table, const string& folderPath)
    {
        string filePath = folderPath;
        if (!filePath.empty() && filePath.back() != '/' && filePath.back() != '\\')
        {
            filePath += "/";
        }
        filePath += table.getName() + ".csv";
        return writeToFile(table, filePath);
    }

    bool TableIO::writeToFile(const Table& table, const string& filePath)
    {
        // Ensure parent directory exists
        try {
            filesystem::path p(filePath);
            if (p.has_parent_path())
            {
                filesystem::create_directories(p.parent_path());
            }
        } catch (...) {}

        ofstream file(filePath);
        if (!file.is_open())
        {
            cerr << "[TableIO Error] Cannot open file for writing: " << filePath << endl;
            return false;
        }

        const HashMap<string, Column>& columns = table.getColumns();
        Vector<string> headerNames;
        headerNames.reserve(columns.size());
        for (const auto& entry : columns)
        {
            headerNames.push_back(entry.first);
        }

        file << CSVHelper::formatLine(headerNames) << "\n";

        for (const auto& r : table.view())
        {
            file << CSVHelper::formatLine(r, headerNames) << "\n";
        }

        return true;
    }
}
