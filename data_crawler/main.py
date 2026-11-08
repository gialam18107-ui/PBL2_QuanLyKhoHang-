"""
Entry point for Data Crawler (PBL2 Warehouse Management System).
Crawls products, builds categorized specs, and exports domain-organized CSV database files.
Strictly complies with project_rules.md and QuanLyKhoHang_schema.json.
"""

import sys
import os
import csv
import argparse
from data_crawler.crawler import DataCrawler
from data_crawler.exporter import SchemaExporter, TABLE_DOMAINS, CATEGORY_SPECS_MAP


def resolve_file(data_dir: str, filename: str) -> str:
    """Find CSV file in its domain subfolder or root folder."""
    table_name = filename[:-4] if filename.endswith(".csv") else filename
    domain = TABLE_DOMAINS.get(table_name)
    if domain:
        domain_path = os.path.join(data_dir, domain, filename)
        if os.path.exists(domain_path):
            return domain_path
    root_path = os.path.join(data_dir, filename)
    if os.path.exists(root_path):
        return root_path
    return os.path.join(data_dir, domain if domain else "", filename)


def validate_exported_data(data_dir: str) -> bool:
    """Validate referential integrity, domain folders, and category specs."""
    print(f"\n[+] Validating CSV data integrity in: {data_dir}")

    # 1. Verify Domain Folders exist (Section 10)
    expected_domains = ["transactions", "unit", "products", "customers", "status_type", "inventory_check"]
    for dom in expected_domains:
        dom_dir = os.path.join(data_dir, dom)
        if not os.path.isdir(dom_dir):
            print(f"[-] Missing domain folder: {dom_dir}")
            return False
    print(f"  [OK] Verified {len(expected_domains)} domain directories: {', '.join(expected_domains)}")

    # 2. Check products
    prod_path = resolve_file(data_dir, "product.csv")
    if not os.path.exists(prod_path):
        print(f"[-] Missing product.csv (checked domain and root)")
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

    # 3. Check spec tables (Level 3 tables in products/)
    spec_tables = ["cpu", "gpu", "mainboard", "ram", "ssd", "psu", "cooling", "case"]
    total_specs = 0
    for tbl in spec_tables:
        tbl_path = resolve_file(data_dir, f"{tbl}.csv")
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

    # 5. Check customers
    cust_path = resolve_file(data_dir, "customer.csv")
    if os.path.exists(cust_path):
        customer_ids = set()
        with open(cust_path, encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                customer_ids.add(int(row["customer_id"]))
        print(f"  [OK] customer.csv: {len(customer_ids)} customers verified.")

    # 6. Check units and bucket synchronization (Rule 4.2 & 4.4)
    units_path = resolve_file(data_dir, "unit_inventory.csv")
    unit_serials = set()
    unit_counts = {}  # pid -> {status: count}
    unit_status_map = {}  # uid -> status
    with open(units_path, encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            uid = int(row["unit_id"])
            pid = int(row["product_id"])
            serial = row["serial"]
            status = row["status"]
            if serial in unit_serials:
                print(f"[-] Duplicate serial in unit_inventory: {serial}")
                return False
            unit_serials.add(serial)
            unit_status_map[uid] = status
            if pid not in unit_counts:
                unit_counts[pid] = {"AVAILABLE": 0, "WARRANTY": 0, "SCRAPPED": 0, "OUT": 0}
            unit_counts[pid][status] = unit_counts[pid].get(status, 0) + 1
    print(f"  [OK] unit_inventory.csv: {len(unit_serials)} unique physical units.")

    # Verify atomic bucket sync with product.csv (Rule 4.2)
    with open(prod_path, encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            pid = int(row["product_id"])
            act_avail = int(row["available_quantity"])
            act_warr = int(row["warranty_quantity"])
            act_scrap = int(row["scrapped_quantity"])
            counts = unit_counts.get(pid, {"AVAILABLE": 0, "WARRANTY": 0, "SCRAPPED": 0})
            if counts.get("AVAILABLE", 0) != act_avail or counts.get("WARRANTY", 0) != act_warr or counts.get("SCRAPPED", 0) != act_scrap:
                print(f"[-] Inventory bucket mismatch for product {pid}: CSV says ({act_avail}, {act_warr}, {act_scrap}) but unit_inventory has ({counts.get('AVAILABLE', 0)}, {counts.get('WARRANTY', 0)}, {counts.get('SCRAPPED', 0)})")
                return False
    print(f"  [OK] Rule 4.2 Verified: All product inventory buckets match physical units exactly.")

    # Verify status_history consistency (Rule 4.4)
    hist_path = resolve_file(data_dir, "status_history.csv")
    if os.path.exists(hist_path):
        latest_status_per_unit = {}
        with open(hist_path, encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                uid = int(row["unit_id"])
                curr_st = row["curr_status"]
                latest_status_per_unit[uid] = curr_st
        for uid, cur_st in unit_status_map.items():
            if uid not in latest_status_per_unit:
                print(f"[-] Unit {uid} has no status_history entry.")
                return False
            if latest_status_per_unit[uid] != cur_st:
                print(f"[-] Status mismatch for unit {uid}: unit_inventory={cur_st}, latest status_history={latest_status_per_unit[uid]}")
                return False
        print(f"  [OK] Rule 4.4 Verified: Unit status consistency with status_history integrity confirmed.")

    # 7. Check Category Specifications (Section 10.4)
    print(f"[+] Validating Category Specification sub-tables (<category>_specs/)...")
    total_cat_specs = 0
    for cat_name, fields in CATEGORY_SPECS_MAP.items():
        cat_dir = os.path.join(data_dir, f"{cat_name.lower()}_specs")
        if not os.path.isdir(cat_dir):
            print(f"[-] Missing category specs directory: {cat_dir}")
            return False
        for field in fields:
            spec_file = os.path.join(cat_dir, f"{field}.csv")
            if not os.path.exists(spec_file):
                print(f"[-] Missing spec sub-table: {spec_file}")
                return False
            with open(spec_file, encoding="utf-8") as f:
                reader = csv.reader(f)
                header = next(reader, None)
                if header != ["id", "spec"]:
                    print(f"[-] Invalid spec table headers in {spec_file}: {header} (expected ['id', 'spec'])")
                    return False
                row_count = sum(1 for _ in reader)
                if row_count == 0:
                    print(f"[-] Spec table {spec_file} has 0 records.")
                    return False
                total_cat_specs += row_count
        print(f"  [OK] {cat_name.lower()}_specs: {len(fields)} sub-tables verified.")

    print(f"  [OK] Total category specification entries verified: {total_cat_specs}")
    print("[PASS] All domain organization and integrity checks passed successfully!\n")
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
        "--online",
        action="store_true",
        help="Scrape additional online hardware items from retailers (default: False, use curated catalog)",
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
    crawler.build_dataset(allow_online=args.online)

    print(f"[+] Crawled & structured {len(crawler.products)} products across 8 categories.")
    print(f"[+] Generated {len(crawler.units)} inventory units.")

    print(f"\n[+] Exporting database tables to '{args.output_dir}' (organized by domain & specs)...")
    exporter = SchemaExporter(output_dir=args.output_dir)
    summary = exporter.export_all(crawler)

    print("\n======================================================")
    print("               CRAWLER EXPORT SUMMARY                 ")
    print("======================================================")
    for table_name, count in summary.items():
        print(f"  - {table_name:30s}: {count:4d} rows")
    print("======================================================")

    if args.validate or True:
        valid = validate_exported_data(args.output_dir)
        if not valid:
            sys.exit(1)

    print("[+] Data Crawler completed successfully!")


if __name__ == "__main__":
    main()
