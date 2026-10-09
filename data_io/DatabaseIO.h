#ifndef DATABASE_IO_H
#define DATABASE_IO_H

#include <string>
#include "database/Database.h"
#include "database/Vector.h"

using namespace std;

namespace DataIO
{
    class DatabaseIO
    {
    public:
        // ========================================================
        // DATABASE IO (Domain-aware)
        // ========================================================

        // Load all tables from CSV files in folderPath in topological dependency order
        // Automatically resolves domain subfolders (transactions/, unit/, products/, customers/, status_type/)
        static bool readAll(Database& db, const string& folderPath = "data");

        // Load a specific table from CSV file in folderPath into Database
        static bool readTable(Database& db, const string& tableName, const string& folderPath = "data");

        // Save all tables registered in Database to domain-organized CSV files in folderPath
        static bool writeAll(const Database& db, const string& folderPath = "data");

        // Save a specific table in Database to domain-organized CSV file in folderPath
        static bool writeTable(const Database& db, const string& tableName, const string& folderPath = "data");

        // ========================================================
        // DOMAIN MANAGEMENT (Rule 10)
        // ========================================================

        // Get the domain subfolder name for a table ("transactions", "unit", "products", "customers", "status_type")
        static string getTableDomain(const string& tableName);

        // Get the resolved folder path for a table (e.g. folderPath/products)
        static string getTableFolder(const string& tableName, const string& folderPath = "data");

        // Get all supported domain folder names
        static Vector<string> getDomains();

        // Get all schema table names belonging to a domain
        static Vector<string> getTablesInDomain(const string& domain);

        // Read all tables belonging to a specific domain into Database
        static bool readDomain(Database& db, const string& domain, const string& folderPath = "data");

        // Write all tables belonging to a specific domain from Database
        static bool writeDomain(const Database& db, const string& domain, const string& folderPath = "data");

        // ========================================================
        // CATEGORY SPECIFICATIONS MANAGEMENT (Rule 10.4)
        // ========================================================

        // Get all categories that have specifications
        static Vector<string> getCategoriesWithSpecs();

        // Get the specification sub-table names for a given category
        static Vector<string> getSpecNamesForCategory(const string& category);

        // Read all spec sub-tables for a specific category into Database
        // Registered in Database as <category>_<specName> (e.g. ssd_type, ssd_gen, ssd_capacity)
        static bool readCategorySpecs(Database& db, const string& category, const string& folderPath = "data");

        // Write all spec sub-tables for a category from Database to folderPath/<category>_specs/
        static bool writeCategorySpecs(const Database& db, const string& category, const string& folderPath = "data");

        // Read all spec sub-tables for all categories into Database
        static bool readAllCategorySpecs(Database& db, const string& folderPath = "data");

        // Write all spec sub-tables for all categories from Database
        static bool writeAllCategorySpecs(const Database& db, const string& folderPath = "data");
    };
}

#endif // DATABASE_IO_H
