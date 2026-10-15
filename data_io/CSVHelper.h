#ifndef CSV_HELPER_H
#define CSV_HELPER_H

#include <string>
#include "datastructure/vector/Vector.h"

using namespace std;

namespace DataIO
{
    class CSVHelper
    {
    public:
        // Parse a CSV line into individual string fields
        static Vector<string> parseLine(const string& line);

        // Format a field for CSV (escapes quotes, adds enclosing quotes if needed)
        static string escapeField(const string& field);

        // Format a vector of fields into a comma-separated CSV line
        static string formatLine(const Vector<string>& fields);

        // Utility: trim whitespace from both ends
        static string trim(const string& str);

        // Utility: strip UTF-8 BOM if present at start of string
        static string stripBOM(const string& str);
    };
}

#endif // CSV_HELPER_H
