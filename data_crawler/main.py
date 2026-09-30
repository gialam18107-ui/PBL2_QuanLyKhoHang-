"""
Entry point for Data Crawler (PBL2 Warehouse Management System).
Crawls products, builds categorized specs, and exports CSV database files.
"""

import sys
import argparse
from data_crawler.crawler import DataCrawler
from data_crawler.exporter import SchemaExporter


def validate_exported_data(data_dir: str) -> bool:
    """Validate referential integrity and constraints of exported CSV files."""
    import csv
    import os

    print(f"\n[+] Validating CSV data integrity in: {data_dir}")

    # Check products
    prod_path = os.path.join(data_dir, "product.csv")
    if not os.path.exists(prod_path):
        print(f"[-] Missing product.csv")
        return False

    product_ids = set()
    categories = set()
    brands = set()

    with open(prod_path, encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            pid = int(row["product_id"])
            product_ids.add(pid)
            categories.add(row["category"])
            brands.add(row["brand"])
            if not row["product_code"] or not row["barcode"] or not row["sku"]:
                print(f"[-] Product {pid} missing code/barcode/sku")
                return False

    print(f"  [OK] Loaded {len(product_ids)} products across {len(categories)} categories and {len(brands)} brands.")

    # Check spec tables
    spec_tables = ["cpu", "gpu", "mainboard", "ram", "ssd", "psu", "cooling", "case"]
    total_specs = 0
    for tbl in spec_tables:
        tbl_path = os.path.join(data_dir, f"{tbl}.csv")
        if not os.path.exists(tbl_path):
            print(f"[-] Missing {tbl}.csv")
            return False
        with open(tbl_path, encoding="utf-8") as f:
            reader = csv.DictReader(f)
            count = 0
            for row in reader:
                pid = int(row["product_id"])
                if pid not in product_ids:
                    print(f"[-] {tbl} row references non-existent product_id: {pid}")
                    return False
                count += 1
            total_specs += count
            print(f"  [OK] {tbl}.csv: {count} records linked.")

    # Check product_quantity
    pq_path = os.path.join(data_dir, "product_quantity.csv")
    if os.path.exists(pq_path):
        with open(pq_path, encoding="utf-8") as f:
            reader = csv.DictReader(f)
            pq_count = sum(1 for _ in reader)
            print(f"  [OK] product_quantity.csv: {pq_count} records.")

    # Check customers
    cust_path = os.path.join(data_dir, "customer.csv")
    if os.path.exists(cust_path):
        customer_ids = set()
        with open(cust_path, encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                customer_ids.add(int(row["customer_id"]))
        print(f"  [OK] customer.csv: {len(customer_ids)} customers verified.")

    # Check units
    units_path = os.path.join(data_dir, "unit_inventory.csv")
    unit_serials = set()
    with open(units_path, encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            serial = row["serial"]
            if serial in unit_serials:
                print(f"[-] Duplicate serial in unit_inventory: {serial}")
                return False
            unit_serials.add(serial)
    print(f"  [OK] unit_inventory.csv: {len(unit_serials)} unique physical units.")

    print("[PASS] All integrity checks passed successfully!\n")
    return True


def main():
    parser = argparse.ArgumentParser(description="PBL2 Warehouse Data Crawler")
    parser.add_argument(
        "--output-dir",
        default="data",
        help="Target folder for CSV database files (default: data)",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Random seed for deterministic generation",
    )
    parser.add_argument(
        "--validate",
        action="store_true",
        help="Run integrity validation after export",
    )

    args = parser.parse_args()

    print("======================================================")
    print("       WAREHOUSE DATA CRAWLER INITIALIZING            ")
    print("======================================================")
    print(f"Target Output Directory: {args.output_dir}")

    crawler = DataCrawler(seed=args.seed)
    print("[+] Fetching hardware specifications & building catalog...")
    crawler.build_dataset()

    print(f"[+] Crawled & structured {len(crawler.products)} products across 8 categories.")
    print(f"[+] Generated {len(crawler.units)} inventory units.")

    print(f"\n[+] Exporting database tables to '{args.output_dir}'...")
    exporter = SchemaExporter(output_dir=args.output_dir)
    summary = exporter.export_all(crawler)

    print("\n======================================================")
    print("               CRAWLER EXPORT SUMMARY                 ")
    print("======================================================")
    for table_name, count in summary.items():
        print(f"  - {table_name:25s}: {count:4d} rows")
    print("======================================================")

    if args.validate or True:
        valid = validate_exported_data(args.output_dir)
        if not valid:
            sys.exit(1)

    print("[+] Data Crawler completed successfully!")


if __name__ == "__main__":
    main()
