#include <iostream>
#include <string>
#include "database/Database.h"
#include "data_io/DatabaseIO.h"
#include "schema/Schema.h"
#include "generator/Generator.h"

using namespace std;
using namespace DataIO;
using namespace Generator;

void printUsage(const char* progName)
{
    cout << "Warehouse Transaction Data Generator (PBL2)\n";
    cout << "Usage: " << progName << " [options]\n";
    cout << "Options:\n";
    cout << "  --interactive, -i                           Launch interactive console interface (default if no args)\n";
    cout << "  --batch <numInbound> <numOutbound> [checks] Run batch generation non-interactively (checks optional)\n";
    cout << "  --checks, -c <numChecks>                    Generate standalone inventory checks\n";
    cout << "  --data-dir <path>                     Data directory (default: data)\n";
    cout << "  --save                                Write generated data back to CSV\n";
    cout << "  --help, -h                            Show this help message\n";
}

int main(int argc, char* argv[])
try
{
    cout << "======================================================\n"
         << "     WAREHOUSE TRANSACTION GENERATOR (PBL2)           \n"
         << "======================================================\n" << flush;

    string dataDir = "data";
    int numInbound = 0;
    int numOutbound = 0;
    int numChecks = 0;
    bool isBatchMode = false;
    bool shouldSave = false;
    bool forceInteractive = false;

    for (int i = 1; i < argc; i++)
    {
        string arg = argv[i];
        if (arg == "--batch" && i + 2 < argc)
        {
            numInbound = stoi(argv[++i]);
            numOutbound = stoi(argv[++i]);
            isBatchMode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                try
                {
                    numChecks = stoi(argv[i + 1]);
                    i++;
                }
                catch (...) {}
            }
        }
        else if ((arg == "--checks" || arg == "-c") && i + 1 < argc)
        {
            numChecks = stoi(argv[++i]);
            isBatchMode = true;
        }
        else if (arg == "--interactive" || arg == "-i")
        {
            forceInteractive = true;
        }
        else if (arg == "--data-dir" && i + 1 < argc)
        {
            dataDir = argv[++i];
        }
        else if (arg == "--save")
        {
            shouldSave = true;
        }
        else if (arg == "--help" || arg == "-h")
        {
            printUsage(argv[0]);
            return 0;
        }
    }

    // 1. Initialize Database & load schema
    Database db;
    cout << "[+] Loading database schema and existing data from: " << dataDir << " ...\n" << flush;
    Schema::initDatabase(db, false);
    DatabaseIO dbIO = Schema::createDatabaseIO(dataDir, false);
    bool loaded = dbIO.readAll(db);
    if (!loaded)
    {
        cout << "[!] Notice: Some tables were empty or initialized fresh.\n" << flush;
    }
    cout << "[+] Loaded " << db.getTableCount() << " tables successfully.\n" << flush;

    // 2. Initialize Data Generator
    DataGenerator generator(db);
    cout << "[+] Generator initialized and database scanned.\n" << flush;

    // 3. Determine Execution Mode
    if (forceInteractive || (!isBatchMode && argc == 1))
    {
        // Interactive Console Interface (Section 7.3 of project_rules.md)
        generator.runConsoleInterface();
        return 0;
    }

    // 4. Batch Mode execution
    cout << "\n[Before Generation]";
    generator.printInventorySummary();

    cout << "\n[+] Generating batch: "
         << numInbound << " inbounds, "
         << numOutbound << " outbounds, "
         << numChecks << " inventory checks...\n";

    GenerationSummary summary = generator.generateBatch(numInbound, numOutbound, numChecks);

    cout << "\n======================================================\n";
    cout << "               GENERATION SUMMARY REPORT              \n";
    cout << "======================================================\n";
    cout << "  - Inbound Transactions Generated: " << summary.inboundTxnsGenerated << "\n";
    cout << "  - Deliveries Completed:           " << summary.deliveriesGenerated << "\n";
    cout << "  - Inventory Units Added:          " << summary.unitsCreated << "\n";
    cout << "  - Outbound Transactions Generated:" << summary.outboundTxnsGenerated << "\n";
    cout << "  - Shipments Completed:            " << summary.shipmentsGenerated << "\n";
    cout << "  - Inventory Units Shipped (OUT):  " << summary.unitsShipped << "\n";
    cout << "  - Inventory Checks Triggered:     " << summary.inventoryChecksGenerated << "\n";
    cout << "  - Adjustment Movements Created:   " << summary.adjustmentMovementsGenerated << "\n";
    cout << "======================================================\n";

    // 5. Validate Integrity
    bool valid = generator.validateIntegrity();
    cout << "[+] Database integrity validation: " << (valid ? "[PASSED] ALL CONSTRAINTS OK" : "[FAILED]") << "\n";

    cout << "\n[After Generation]";
    generator.printInventorySummary();

    // 6. Save if requested
    if (shouldSave)
    {
        cout << "\n[+] Saving updated database to: " << dataDir << " ...\n";
        bool saved = dbIO.writeAll(db);
        cout << "[+] Save result: " << (saved ? "SUCCESS" : "FAILED") << "\n";
    }
    else
    {
        cout << "\n[i] Dry-run completed (pass --save to persist changes to CSV).\n";
    }

    return 0;
}
catch (const std::exception& e)
{
    cerr << "[Generator Fatal Exception]: " << e.what() << endl;
    return 1;
}
catch (...)
{
    cerr << "[Generator Fatal Error]: Unknown exception occurred." << endl;
    return 1;
}
