#include "CSVHelper.h"
#include <cctype>

namespace DataIO
{
    string CSVHelper::trim(const string& str)
    {
        size_t first = 0;
        while (first < str.size() && (str[first] == ' ' || str[first] == '\t' || str[first] == '\r' || str[first] == '\n'))
        {
            first++;
        }
        if (first == str.size())
        {
            return "";
        }

        size_t last = str.size() - 1;
        while (last > first && (str[last] == ' ' || str[last] == '\t' || str[last] == '\r' || str[last] == '\n'))
        {
            last--;
        }

        return str.substr(first, last - first + 1);
    }

    string CSVHelper::stripBOM(const string& str)
    {
        if (str.size() >= 3 &&
            static_cast<unsigned char>(str[0]) == 0xEF &&
            static_cast<unsigned char>(str[1]) == 0xBB &&
            static_cast<unsigned char>(str[2]) == 0xBF)
        {
            return str.substr(3);
        }
        return str;
    }

    Vector<string> CSVHelper::parseLine(const string& line)
    {
        Vector<string> result;
        string cleanedLine = stripBOM(line);

        while (!cleanedLine.empty() && (cleanedLine.back() == '\r' || cleanedLine.back() == '\n'))
        {
            cleanedLine.pop_back();
        }

        if (cleanedLine.empty())
        {
            return result;
        }

        string currentField = "";
        bool inQuotes = false;
        size_t i = 0;

        while (i < cleanedLine.size())
        {
            char c = cleanedLine[i];

            if (inQuotes)
            {
                if (c == '"')
                {
                    if (i + 1 < cleanedLine.size() && cleanedLine[i + 1] == '"')
                    {
                        currentField += '"';
                        i += 2;
                        continue;
                    }
                    else
                    {
                        inQuotes = false;
                        i++;
                        continue;
                    }
                }
                else
                {
                    currentField += c;
                    i++;
                }
            }
            else
            {
                if (c == '"')
                {
                    inQuotes = true;
                    i++;
                }
                else if (c == ',')
                {
                    result.push_back(trim(currentField));
                    currentField.clear();
                    i++;
                }
                else
                {
                    currentField += c;
                    i++;
                }
            }
        }

        result.push_back(trim(currentField));
        return result;
    }

    string CSVHelper::escapeField(const string& field)
    {
        bool needsQuotes = false;
        for (char c : field)
        {
            if (c == ',' || c == '"' || c == '\n' || c == '\r')
            {
                needsQuotes = true;
                break;
            }
        }

        if (!needsQuotes)
        {
            return field;
        }

        string escaped = "\"";
        for (char c : field)
        {
            if (c == '"')
            {
                escaped += "\"\"";
            }
            else
            {
                escaped += c;
            }
        }
        escaped += "\"";
        return escaped;
    }

    string CSVHelper::formatLine(const Vector<string>& fields)
    {
        string line = "";
        for (size_t i = 0; i < fields.size(); i++)
        {
            line += escapeField(fields[i]);
            if (i + 1 < fields.size())
            {
                line += ",";
            }
        }
        return line;
    }

    string CSVHelper::formatLine(const Record& record, const Vector<string>& columns)
    {
        Vector<string> fields;
        fields.reserve(columns.size());
        for (const auto& colName : columns)
        {
            if (record.has(colName))
            {
                const Value& val = record.get(colName);
                if (holds_alternative<monostate>(val))
                {
                    fields.push_back("");
                }
                else
                {
                    fields.push_back(valueToString(val));
                }
            }
            else
            {
                fields.push_back("");
            }
        }
        return formatLine(fields);
    }

    string CSVHelper::formatLine(const Record& record, const Vector<Column>& columns)
    {
        Vector<string> colNames;
        colNames.reserve(columns.size());
        for (const auto& col : columns)
        {
            colNames.push_back(col.getName());
        }
        return formatLine(record, colNames);
    }

    string CSVHelper::formatLine(const Record& record)
    {
        Vector<string> fields;
        for (const auto& item : record.getValues())
        {
            if (holds_alternative<monostate>(item.second))
            {
                fields.push_back("");
            }
            else
            {
                fields.push_back(valueToString(item.second));
            }
        }
        return formatLine(fields);
    }
}
