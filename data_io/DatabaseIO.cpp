#include "DatabaseIO.h"
#include "TableIO.h"
#include "schema/Schema.h"

#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace DataIO
{
    // ========================================================
    // DOMAIN MANAGEMENT (Rule 10)
    // ========================================================

    string DatabaseIO::getTableDomain(const string& tableName)
    {
        string name = tableName;
        for (char& c : name) c = tolower(c);

        // Section 10.1: Transactions
        if (name == "inbound_transaction" || name == "inbound_detail" ||
            name == "delivery" || name == "delivery_detail" || name == "delivery_unit_detail" ||
            name == "outbound_transaction" || name == "outbound_detail" ||
            name == "shipment" || name == "shipment_detail" || name == "shipment_unit_detail")
        {
            return "transactions";
        }

        // Section 10.2: Units
        if (name == "status_history" || name == "unit_inventory")
        {
            return "unit";
        }

        // Section 10.3: Products
        if (name == "product_quantity" || name == "product" ||
            name == "cpu" || name == "mainboard" || name == "gpu" ||
            name == "ram" || name == "ssd" || name == "psu" ||
            name == "cooling" || name == "case" || name == "brand" || name == "category")
        {
            return "products";
        }

        // Section 10.5: Customers
        if (name == "customer" || name == "individual_info" || name == "individual_infor" || name == "business_info")
        {
            return "customers";
        }

        // Section 10.6: Status and Type Tables
        if (name == "unit_status" || name == "inbound_type" || name == "inbound_status" ||
            name == "delivery_status" || name == "outbound_type" || name == "outbound_status" ||
            name == "shipment_status")
        {
            return "status_type";
        }

        return "";
    }

    string DatabaseIO::getTableFolder(const string& tableName, const string& folderPath)
    {
        string domain = getTableDomain(tableName);
        string base = folderPath;
        if (!base.empty() && base.back() != '/' && base.back() != '\\') base += "/";

        if (!domain.empty())
        {
            // If folderPath already includes domain, return as is
            if (base.find(domain + "/") != string::npos || base.find(domain + "\\") != string::npos)
            {
                return folderPath;
            }

            // Check if domain subfolder exists and contains the table
            string domainFolder = base + domain;
            if (filesystem::exists(domainFolder + "/" + tableName + ".csv"))
            {
                return domainFolder;
            }

            // Also check "units" fallback for "unit"
            if (domain == "unit" && filesystem::exists(base + "units/" + tableName + ".csv"))
            {
                return base + "units";
            }

            // Check if table is directly in base folder
            if (filesystem::exists(base + tableName + ".csv"))
            {
                return folderPath;
            }

            // Canonical target is domain folder
            return domainFolder;
        }

        return folderPath;
    }

    Vector<string> DatabaseIO::getDomains()
    {
        return {"transactions", "unit", "products", "customers", "status_type"};
    }

    Vector<string> DatabaseIO::getTablesInDomain(const string& domain)
    {
        Vector<string> result;
        string dom = domain;
        for (char& c : dom) c = tolower(c);

        for (const string& tableName : Schema::getTableNames())
        {
            if (getTableDomain(tableName) == dom)
            {
                result.push_back(tableName);
            }
        }

        // Include compatibility tables if domain matches
        if (dom == "transactions")
        {
            bool hasDeliv = false;
            bool hasShip = false;
            for (const auto& t : result)
            {
                if (t == "delivery_unit_detail") hasDeliv = true;
                if (t == "shipment_unit_detail") hasShip = true;
            }
            if (!hasDeliv) result.push_back("delivery_unit_detail");
            if (!hasShip) result.push_back("shipment_unit_detail");
        }
        return result;
    }

    // ========================================================
    // DATABASE IO (Domain-aware, reuses TableIO)
    // ========================================================

    bool DatabaseIO::readAll(Database& db, const string& folderPath)
    {
        bool allSuccess = true;
        for (const string& tableName : Schema::getTableNames())
        {
            bool ok = readTable(db, tableName, folderPath);
            if (!ok)
            {
                cerr << "[DatabaseIO Warning] Failed to read table: " << tableName << endl;
                allSuccess = false;
            }
        }
        return allSuccess;
    }

    bool DatabaseIO::readTable(Database& db, const string& tableName, const string& folderPath)
    {
        auto table = db.getTable(tableName);
        if (table == nullptr)
        {
            table = Schema::createTable(tableName);
            if (table != nullptr)
            {
                db.addTable(table);
            }
            else
            {
                cerr << "[DatabaseIO Error] Unknown table schema: " << tableName << endl;
                return false;
            }
        }

        // Resolve domain folder and delegate to TableIO::read
        string targetFolder = getTableFolder(tableName, folderPath);
        bool ok = TableIO::read(*table, targetFolder);
        if (!ok && targetFolder != folderPath)
        {
            // Fallback to base folder
            ok = TableIO::read(*table, folderPath);
        }
        return ok;
    }

    bool DatabaseIO::writeAll(const Database& db, const string& folderPath)
    {
        bool allSuccess = true;
        for (const string& tableName : Schema::getTableNames())
        {
            auto table = db.getTable(tableName);
            if (table != nullptr)
            {
                bool ok = writeTable(db, tableName, folderPath);
                if (!ok)
                {
                    cerr << "[DatabaseIO Warning] Failed to write table: " << tableName << endl;
                    allSuccess = false;
                }
            }
        }
        return allSuccess;
    }

    bool DatabaseIO::writeTable(const Database& db, const string& tableName, const string& folderPath)
    {
        auto table = db.getTable(tableName);
        if (table == nullptr)
        {
            cerr << "[DatabaseIO Error] Table not found in database: " << tableName << endl;
            return false;
        }

        string domain = getTableDomain(tableName);
        string base = folderPath;
        if (!base.empty() && base.back() != '/' && base.back() != '\\') base += "/";

        string targetFolder = folderPath;
        if (!domain.empty())
        {
            if (base.find(domain + "/") == string::npos && base.find(domain + "\\") == string::npos)
            {
                targetFolder = base + domain;
                try { filesystem::create_directories(targetFolder); } catch (...) {}
            }
        }

        // Delegate write to TableIO
        bool ok = TableIO::write(*table, targetFolder);

        // Also keep base file updated if written to domain subfolder
        if (targetFolder != folderPath)
        {
            TableIO::write(*table, folderPath);
        }

        return ok;
    }

    bool DatabaseIO::readDomain(Database& db, const string& domain, const string& folderPath)
    {
        Vector<string> tables = getTablesInDomain(domain);
        if (tables.empty())
        {
            cerr << "[DatabaseIO Warning] No tables found for domain: " << domain << endl;
            return false;
        }

        bool allSuccess = true;
        for (const string& tableName : tables)
        {
            bool ok = readTable(db, tableName, folderPath);
            if (!ok)
            {
                allSuccess = false;
            }
        }
        return allSuccess;
    }

    bool DatabaseIO::writeDomain(const Database& db, const string& domain, const string& folderPath)
    {
        Vector<string> tables = getTablesInDomain(domain);
        if (tables.empty())
        {
            cerr << "[DatabaseIO Warning] No tables found for domain: " << domain << endl;
            return false;
        }

        bool allSuccess = true;
        for (const string& tableName : tables)
        {
            if (db.getTable(tableName) != nullptr)
            {
                bool ok = writeTable(db, tableName, folderPath);
                if (!ok)
                {
                    allSuccess = false;
                }
            }
        }
        return allSuccess;
    }

    // ========================================================
    // CATEGORY SPECIFICATIONS MANAGEMENT (Rule 10.4)
    // ========================================================

    Vector<string> DatabaseIO::getCategoriesWithSpecs()
    {
        return {"cpu", "gpu", "mainboard", "ram", "ssd", "psu", "cooling", "case"};
    }

    Vector<string> DatabaseIO::getSpecNamesForCategory(const string& category)
    {
        string cat = category;
        for (char& c : cat) c = tolower(c);

        if (cat == "cpu") return {"socket", "tier", "series"};
        if (cat == "gpu") return {"series", "producer", "vram"};
        if (cat == "mainboard") return {"cpu_support", "socket", "size", "chipset", "ram_support"};
        if (cat == "ram") return {"generation", "quantity", "capacity", "bus"};
        if (cat == "ssd") return {"type", "gen", "capacity"};
        if (cat == "psu") return {"quality", "capacity", "size"};
        if (cat == "cooling") return {"type"};
        if (cat == "case") return {"size", "product_name"};
        return {};
    }

    bool DatabaseIO::readCategorySpecs(Database& db, const string& category, const string& folderPath)
    {
        string cat = category;
        for (char& c : cat) c = tolower(c);

        Vector<string> specNames = getSpecNamesForCategory(cat);
        if (specNames.empty())
        {
            cerr << "[DatabaseIO Warning] Unknown category for specs: " << category << endl;
            return false;
        }

        string base = folderPath;
        if (!base.empty() && base.back() != '/' && base.back() != '\\') base += "/";
        string specsFolder = base + cat + "_specs";

        bool allSuccess = true;
        for (const string& specName : specNames)
        {
            string dbTableName = cat + "_" + specName;
            auto table = db.getTable(dbTableName);
            if (table == nullptr)
            {
                table = Schema::createSpecTable(dbTableName);
                db.addTable(table);
            }

            string filePath = specsFolder + "/" + specName + ".csv";
            bool ok = TableIO::readFromFile(*table, filePath);
            if (!ok)
            {
                cerr << "[DatabaseIO Warning] Failed to read spec table: " << cat << "/" << specName << endl;
                allSuccess = false;
            }
        }
        return allSuccess;
    }

    bool DatabaseIO::writeCategorySpecs(const Database& db, const string& category, const string& folderPath)
    {
        string cat = category;
        for (char& c : cat) c = tolower(c);

        Vector<string> specNames = getSpecNamesForCategory(cat);
        if (specNames.empty())
        {
            return false;
        }

        string base = folderPath;
        if (!base.empty() && base.back() != '/' && base.back() != '\\') base += "/";
        string specsFolder = base + cat + "_specs";

        bool allSuccess = true;
        for (const string& specName : specNames)
        {
            string dbTableName = cat + "_" + specName;
            auto table = db.getTable(dbTableName);
            if (table != nullptr)
            {
                string filePath = specsFolder + "/" + specName + ".csv";
                bool ok = TableIO::writeToFile(*table, filePath);
                if (!ok)
                {
                    allSuccess = false;
                }
            }
        }
        return allSuccess;
    }

    bool DatabaseIO::readAllCategorySpecs(Database& db, const string& folderPath)
    {
        bool allSuccess = true;
        for (const string& cat : getCategoriesWithSpecs())
        {
            bool ok = readCategorySpecs(db, cat, folderPath);
            if (!ok)
            {
                allSuccess = false;
            }
        }
        return allSuccess;
    }

    bool DatabaseIO::writeAllCategorySpecs(const Database& db, const string& folderPath)
    {
        bool allSuccess = true;
        for (const string& cat : getCategoriesWithSpecs())
        {
            bool ok = writeCategorySpecs(db, cat, folderPath);
            if (!ok)
            {
                allSuccess = false;
            }
        }
        return allSuccess;
    }
}
