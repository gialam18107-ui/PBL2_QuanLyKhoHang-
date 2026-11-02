#include "TableIO.h"
#include "CSVHelper.h"
#include "schema/Schema.h"

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
        // If table has no columns, populate them from Schema
        if (table.getColumnCount() == 0)
        {
            Table* schemaTable = Schema::createTable(table.getName());
            if (schemaTable != nullptr)
            {
                for (const auto& col : schemaTable->getColumns())
                {
                    table.addColumn(col);
                }
                for (const auto& constraint : schemaTable->getConstraints())
                {
                    table.addConstraint(constraint->clone());
                }
                delete schemaTable;
            }
        }

        ifstream file(filePath);
        table.clear();
        if (!file.is_open())
        {
            cerr << "[TableIO Error] Cannot open file for reading: " << filePath << endl;
            return false;
        }

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

        // If table still has no columns (e.g. dynamic spec table), initialize from headers
        if (table.getColumnCount() == 0)
        {
            for (const auto& h : headers)
            {
                if (h == "id")
                {
                    table.addColumn(Column(h, DataType::INTEGER, 0, true, true, true, true));
                }
                else
                {
                    table.addColumn(Column(h, DataType::VARCHAR, 255, false, false, true));
                }
            }
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
                const string& colName = headers[i];
                const string& valStr = fields[i];

                const Column* col = table.getColumn(colName);
                if (col != nullptr)
                {
                    if (col->getType() == DataType::INTEGER)
                    {
                        if (valStr.empty())
                        {
                            if (!col->getDefaultValue().empty())
                            {
                                record.set(colName, stoi(col->getDefaultValue()));
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
                        if (valStr.empty())
                        {
                            if (!col->getDefaultValue().empty())
                            {
                                bool b = (col->getDefaultValue() == "1" || col->getDefaultValue() == "true" || col->getDefaultValue() == "TRUE");
                                record.set(colName, b);
                            }
                        }
                        else
                        {
                            bool b = (valStr == "1" || valStr == "true" || valStr == "TRUE" || valStr == "True");
                            record.set(colName, b);
                        }
                    }
                    else
                    {
                        if (valStr.empty())
                        {
                            if (col->isNotNull())
                            {
                                if (!col->getDefaultValue().empty())
                                {
                                    record.set(colName, col->getDefaultValue());
                                }
                                else
                                {
                                    record.set(colName, string(""));
                                }
                            }
                        }
                        else
                        {
                            record.set(colName, valStr);
                        }
                    }
                }
                else
                {
                    record.set(colName, valStr);
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

        const Vector<Column>& columns = table.getColumns();
        Vector<string> headerNames;
        for (const auto& col : columns)
        {
            headerNames.push_back(col.getName());
        }

        file << CSVHelper::formatLine(headerNames) << "\n";

        const auto& rows = table.getRows();
        for (size_t i = 0; i < rows.size(); ++i)
        {
            // Check if record is deleted before writing
            if (table.isRowDeleted(i))
            {
                continue;
            }
            file << CSVHelper::formatLine(rows[i], columns) << "\n";
        }

        return true;
    }

    string TableIO::formatLine(const Record& record, const Vector<string>& columns)
    {
        return CSVHelper::formatLine(record, columns);
    }

    string TableIO::formatLine(const Record& record, const Vector<Column>& columns)
    {
        return CSVHelper::formatLine(record, columns);
    }

    string TableIO::formatLine(const Record& record)
    {
        return CSVHelper::formatLine(record);
    }
}
