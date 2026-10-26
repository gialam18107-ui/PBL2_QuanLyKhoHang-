"""
CSV Exporter for Warehouse Management System database tables.
Exports all 32 tables adhering strictly to QuanLyKhoHang_schema.json,
C++ Schema.cpp, and project_rules.md (Section 10 File Organization Rules).
"""

import os
import csv
from typing import List, Dict, Any, Set

from data_crawler.categories import (
    CATEGORIES,
    BRANDS,
    UNIT_STATUSES,
    INBOUND_TYPES,
    INBOUND_STATUSES,
    DELIVERY_STATUSES,
    OUTBOUND_TYPES,
    OUTBOUND_STATUSES,
    SHIPMENT_STATUSES,
    CUSTOMERS,
    INDIVIDUAL_INFO,
    BUSINESS_INFO,
)
from data_crawler.crawler import DataCrawler

# Business domain mapping according to project_rules.md Section 10
TABLE_DOMAINS: Dict[str, str] = {
    # 10.1 Transactions
    "inbound_transaction": "transactions",
    "inbound_detail": "transactions",
    "delivery": "transactions",
    "delivery_detail": "transactions",
    "outbound_transaction": "transactions",
    "outbound_detail": "transactions",
    "shipment": "transactions",
    "shipment_detail": "transactions",

    # 10.2 Units
    "status_history": "unit",
    "unit_inventory": "unit",

    # 10.3 Products
    "product_quantity": "products",
    "product": "products",
    "cpu": "products",
    "mainboard": "products",
    "gpu": "products",
    "ram": "products",
    "ssd": "products",
    "psu": "products",
    "cooling": "products",
    "case": "products",
    "brand": "products",
    "category": "products",

    # 10.5 Customers
    "customer": "customers",
    "individual_info": "customers",
    "business_info": "customers",

    # 10.6 Status and Type Tables
    "unit_status": "status_type",
    "inbound_type": "status_type",
    "inbound_status": "status_type",
    "delivery_status": "status_type",
    "outbound_type": "status_type",
    "outbound_status": "status_type",
    "shipment_status": "status_type",
}

# 10.4 Category Specifications sub-tables for each category
CATEGORY_SPECS_MAP: Dict[str, List[str]] = {
    "CPU": ["socket", "tier", "series"],
    "GPU": ["series", "producer", "vram"],
    "MAINBOARD": ["cpu_support", "socket", "size", "chipset", "ram_support"],
    "RAM": ["generation", "quantity", "capacity", "bus"],
    "SSD": ["type", "gen", "capacity"],
    "PSU": ["quality", "capacity", "size"],
    "COOLING": ["type"],
    "CASE": ["size", "product_name"],
}


class SchemaExporter:
    """Exports crawled data into CSV files adhering to database schema and domain folders."""

    def __init__(self, output_dir: str = "data"):
        self.output_dir = output_dir

    def _write_csv_file(self, full_path: str, headers: List[str], rows: List[Dict[str, Any]]) -> None:
        """Helper to write CSV to a specific full file path."""
        os.makedirs(os.path.dirname(full_path), exist_ok=True)
        with open(full_path, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(headers)
            for row in rows:
                writer.writerow([row.get(h, "") for h in headers])

    def _write_csv(self, filename: str, headers: List[str], rows: List[Dict[str, Any]]) -> None:
        """
        Write CSV file to its canonical business domain subfolder (Section 10).
        Also writes to root output_dir for backward compatibility (Section 3 protection).
        """
        table_name = filename[:-4] if filename.endswith(".csv") else filename
        domain = TABLE_DOMAINS.get(table_name)

        if domain:
            domain_path = os.path.join(self.output_dir, domain, filename)
            self._write_csv_file(domain_path, headers, rows)
        else:
            base_path = os.path.join(self.output_dir, filename)
            self._write_csv_file(base_path, headers, rows)

    def _write_spec_csv(self, category_dir: str, spec_field: str, unique_values: List[str]) -> int:
        """
        Write a 2-column specification sub-table (id, spec) according to Section 10.4.
        """
        folder_path = os.path.join(self.output_dir, category_dir)
        os.makedirs(folder_path, exist_ok=True)
        file_path = os.path.join(folder_path, f"{spec_field}.csv")

        headers = ["id", "spec"]
        rows = [{"id": i + 1, "spec": val} for i, val in enumerate(unique_values)]
        self._write_csv_file(file_path, headers, rows)
        return len(rows)

    def export_category_specs(self, crawler: DataCrawler) -> Dict[str, int]:
        """
        Export specification sub-tables into <category>_specs/ folders (Section 10.4).
        Each sub-table contains exactly two columns: id, spec.
        """
        spec_summary: Dict[str, int] = {}

        for cat_name, fields in CATEGORY_SPECS_MAP.items():
            cat_dir = f"{cat_name.lower()}_specs"
            cat_items = crawler.specs_by_category.get(cat_name, [])

            for field in fields:
                seen: Set[str] = set()
                unique_values: List[str] = []

                for item in cat_items:
                    raw_val = item.get(field)
                    if raw_val is not None:
                        val_str = str(raw_val).strip()
                        if val_str and val_str not in seen:
                            seen.add(val_str)
                            unique_values.append(val_str)

                # Sort values for consistency
                try:
                    # If all values are numeric, sort numerically
                    if all(v.isdigit() for v in unique_values):
                        unique_values.sort(key=lambda x: int(x))
                    else:
                        unique_values.sort()
                except Exception:
                    unique_values.sort()

                count = self._write_spec_csv(cat_dir, field, unique_values)
                spec_summary[f"{cat_dir}/{field}"] = count

        return spec_summary

    def export_all(self, crawler: DataCrawler) -> Dict[str, int]:
        """Export all 32 schema tables + status_history + compatibility tables + category specs."""
        summary: Dict[str, int] = {}

        # 1. CATEGORY (products/)
        cat_headers = ["category_id", "category_name", "code"]
        self._write_csv("category.csv", cat_headers, CATEGORIES)
        summary["category"] = len(CATEGORIES)

        # 2. BRAND (products/)
        brand_headers = ["brand_id", "brand", "code"]
        self._write_csv("brand.csv", brand_headers, BRANDS)
        summary["brand"] = len(BRANDS)

        # 3. UNIT_STATUS (status_type/)
        unit_status_headers = ["status_id", "status"]
        self._write_csv("unit_status.csv", unit_status_headers, UNIT_STATUSES)
        summary["unit_status"] = len(UNIT_STATUSES)

        # 4. INBOUND_TYPE (status_type/)
        inbound_type_headers = ["id", "type", "code", "next_code_no"]
        self._write_csv("inbound_type.csv", inbound_type_headers, INBOUND_TYPES)
        summary["inbound_type"] = len(INBOUND_TYPES)

        # 5. INBOUND_STATUS (status_type/)
        inbound_status_headers = ["status_id", "status"]
        self._write_csv("inbound_status.csv", inbound_status_headers, INBOUND_STATUSES)
        summary["inbound_status"] = len(INBOUND_STATUSES)

        # 6. DELIVERY_STATUS (status_type/)
        delivery_status_headers = ["status_id", "status"]
        self._write_csv("delivery_status.csv", delivery_status_headers, DELIVERY_STATUSES)
        summary["delivery_status"] = len(DELIVERY_STATUSES)

        # 7. OUTBOUND_TYPE (status_type/)
        outbound_type_headers = ["id", "type", "code", "next_code_no"]
        self._write_csv("outbound_type.csv", outbound_type_headers, OUTBOUND_TYPES)
        summary["outbound_type"] = len(OUTBOUND_TYPES)

        # 8. OUTBOUND_STATUS (status_type/)
        outbound_status_headers = ["status_id", "status"]
        self._write_csv("outbound_status.csv", outbound_status_headers, OUTBOUND_STATUSES)
        summary["outbound_status"] = len(OUTBOUND_STATUSES)

        # 9. SHIPMENT_STATUS (status_type/)
        shipment_status_headers = ["status_id", "status"]
        self._write_csv("shipment_status.csv", shipment_status_headers, SHIPMENT_STATUSES)
        summary["shipment_status"] = len(SHIPMENT_STATUSES)

        # 10. CUSTOMER (customers/)
        customer_headers = ["customer_id", "customer_code", "name", "type"]
        self._write_csv("customer.csv", customer_headers, CUSTOMERS)
        summary["customer"] = len(CUSTOMERS)

        # 11. INDIVIDUAL_INFO (customers/)
        indiv_headers = ["customer_id", "phone_number", "email", "next_code_no"]
        self._write_csv("individual_info.csv", indiv_headers, INDIVIDUAL_INFO)
        summary["individual_info"] = len(INDIVIDUAL_INFO)

        # 12. BUSINESS_INFO (customers/)
        biz_headers = ["customer_id", "phone_number", "email", "tax_code", "next_code_no"]
        self._write_csv("business_info.csv", biz_headers, BUSINESS_INFO)
        summary["business_info"] = len(BUSINESS_INFO)

        # 13. PRODUCT (products/)
        prod_headers = [
            "product_id",
            "product_name",
            "product_code",
            "barcode",
            "sku",
            "brand",
            "category",
            "is_sold",
        ]
        self._write_csv("product.csv", prod_headers, crawler.products)
        summary["product"] = len(crawler.products)

        # 14. PRODUCT_QUANTITY (products/)
        prod_quan_headers = ["product_id", "available_quan", "warranty_quan", "scrapped_quan"]
        self._write_csv("product_quantity.csv", prod_quan_headers, crawler.product_quantities)
        summary["product_quantity"] = len(crawler.product_quantities)

        # 15. CPU (products/)
        cpu_headers = ["cpu_id", "product_id", "socket", "tier", "series"]
        self._write_csv("cpu.csv", cpu_headers, crawler.specs_by_category["CPU"])
        summary["cpu"] = len(crawler.specs_by_category["CPU"])

        # 16. GPU (products/)
        gpu_headers = ["gpu_id", "product_id", "series", "producer", "vram"]
        self._write_csv("gpu.csv", gpu_headers, crawler.specs_by_category["GPU"])
        summary["gpu"] = len(crawler.specs_by_category["GPU"])

        # 17. MAINBOARD (products/)
        mb_headers = ["mainboard_id", "product_id", "cpu_support", "socket", "size", "chipset", "ram_support"]
        self._write_csv("mainboard.csv", mb_headers, crawler.specs_by_category["MAINBOARD"])
        summary["mainboard"] = len(crawler.specs_by_category["MAINBOARD"])

        # 18. RAM (products/)
        ram_headers = ["ram_id", "product_id", "generation", "quantity", "capacity", "bus"]
        self._write_csv("ram.csv", ram_headers, crawler.specs_by_category["RAM"])
        summary["ram"] = len(crawler.specs_by_category["RAM"])

        # 19. SSD (products/)
        ssd_headers = ["ssd_id", "product_id", "type", "gen", "capacity"]
        self._write_csv("ssd.csv", ssd_headers, crawler.specs_by_category["SSD"])
        summary["ssd"] = len(crawler.specs_by_category["SSD"])

        # 20. PSU (products/)
        psu_headers = ["psu_id", "product_id", "quality", "capacity", "size"]
        self._write_csv("psu.csv", psu_headers, crawler.specs_by_category["PSU"])
        summary["psu"] = len(crawler.specs_by_category["PSU"])

        # 21. COOLING (products/)
        cooling_headers = ["cooling_id", "product_id", "type"]
        self._write_csv("cooling.csv", cooling_headers, crawler.specs_by_category["COOLING"])
        summary["cooling"] = len(crawler.specs_by_category["COOLING"])

        # 22. CASE (products/)
        case_headers = ["case_id", "product_id", "product_name", "size"]
        self._write_csv("case.csv", case_headers, crawler.specs_by_category["CASE"])
        summary["case"] = len(crawler.specs_by_category["CASE"])

        # 23. UNIT_INVENTORY (unit/)
        unit_headers = ["unit_id", "product_id", "serial", "status"]
        self._write_csv("unit_inventory.csv", unit_headers, crawler.units)
        summary["unit_inventory"] = len(crawler.units)

        # Baseline transactions for initial inventory intake and outbound flow
        # 24. INBOUND_TRANSACTION (transactions/)
        inbound_txns = [
            {
                "inbound_transaction_id": 1,
                "inbound_code": "IN-PUR-000001",
                "customer_id": 4,  # Business partner: FPT Information System
                "order_at": "2026-09-01",
                "type": "PURCHASING",
                "status": "CONFIRMED",
                "next_delivery_no": 2,
                "note": "Initial warehouse inventory intake",
            }
        ]
        inbound_txn_headers = [
            "inbound_transaction_id",
            "inbound_code",
            "customer_id",
            "order_at",
            "type",
            "status",
            "next_delivery_no",
            "note",
        ]
        self._write_csv("inbound_transaction.csv", inbound_txn_headers, inbound_txns)
        summary["inbound_transaction"] = len(inbound_txns)

        # 25. INBOUND_DETAIL (transactions/)
        inbound_details = [
            {
                "inbound_detail_id": 1,
                "inbound_transaction_id": 1,
                "product_id": 1,
                "ordered_quantity": 25,
                "actual_quantity": 25,
            }
        ]
        inbound_detail_headers = [
            "inbound_detail_id",
            "inbound_transaction_id",
            "product_id",
            "ordered_quantity",
            "actual_quantity",
        ]
        self._write_csv("inbound_detail.csv", inbound_detail_headers, inbound_details)
        summary["inbound_detail"] = len(inbound_details)

        # 26. DELIVERY (transactions/)
        deliveries = [
            {
                "delivery_id": 1,
                "delivery_code": "IN-PUR-000001-D01",
                "inbound_transaction_id": 1,
                "delivery_at": "2026-09-02",
                "expected_at": "2026-09-02",
                "status": "COMPLETED",
            }
        ]
        delivery_headers = [
            "delivery_id",
            "delivery_code",
            "inbound_transaction_id",
            "delivery_at",
            "expected_at",
            "status",
        ]
        self._write_csv("delivery.csv", delivery_headers, deliveries)
        summary["delivery"] = len(deliveries)

        # 27. DELIVERY_DETAIL (transactions/)
        delivery_details = [
            {
                "delivery_detail_id": 1,
                "delivery_id": 1,
                "product_id": 1,
                "received_quantity": 25,
                "actual_quantity": 25,
            }
        ]
        delivery_detail_headers = ["delivery_detail_id", "delivery_id", "product_id", "received_quantity", "actual_quantity"]
        self._write_csv("delivery_detail.csv", delivery_detail_headers, delivery_details)
        summary["delivery_detail"] = len(delivery_details)

        # 28. OUTBOUND_TRANSACTION (transactions/)
        outbound_txns = [
            {
                "outbound_trainsaction_id": 1,
                "outbound_code": "OUT-SEL-000001",
                "customer_id": 1,  # Individual customer: Nguyen Van An
                "order_at": "2026-09-05",
                "type": "SELLING",
                "status": "CONFIRMED",
                "next_ship_no": 2,
                "note": "Initial baseline outbound retail order",
            }
        ]
        outbound_txn_headers = [
            "outbound_trainsaction_id",
            "outbound_code",
            "customer_id",
            "order_at",
            "type",
            "status",
            "next_ship_no",
            "note",
        ]
        self._write_csv("outbound_transaction.csv", outbound_txn_headers, outbound_txns)
        summary["outbound_transaction"] = len(outbound_txns)

        # 30. OUTBOUND_DETAIL (transactions/)
        outbound_details = [
            {
                "outbound_detail_id": 1,
                "outbound_transaction_id": 1,
                "product_id": 1,
                "ordered_quantity": 1,
                "actual_quantity": 1,
            }
        ]
        outbound_detail_headers = [
            "outbound_detail_id",
            "outbound_transaction_id",
            "product_id",
            "ordered_quantity",
            "actual_quantity",
        ]
        self._write_csv("outbound_detail.csv", outbound_detail_headers, outbound_details)
        summary["outbound_detail"] = len(outbound_details)

        # 31. SHIPMENT (transactions/)
        shipments = [
            {
                "shipment_id": 1,
                "shipment_code": "OUT-SEL-000001-S01",
                "sell_id": 1,
                "ship_at": "2026-09-06",
                "expected_at": "2026-09-06",
                "status": "COMPLETED",
            }
        ]
        shipment_headers = [
            "shipment_id",
            "shipment_code",
            "sell_id",
            "ship_at",
            "expected_at",
            "status",
        ]
        self._write_csv("shipment.csv", shipment_headers, shipments)
        summary["shipment"] = len(shipments)

        # 32. SHIPMENT_DETAIL (transactions/)
        shipment_details = [
            {
                "shipment_detail_id": 1,
                "shipment_id": 1,
                "product_id": 1,
                "received_quantity": 1,
                "actual_quantity": 1,
            }
        ]
        shipment_detail_headers = ["shipment_detail_id", "shipment_id", "product_id", "received_quantity", "actual_quantity"]
        self._write_csv("shipment_detail.csv", shipment_detail_headers, shipment_details)
        summary["shipment_detail"] = len(shipment_details)

        # 32. STATUS_HISTORY (unit/)
        hist_headers = ["history_id", "unit_id", "old_status", "curr_status", "transaction", "type"]
        self._write_csv("status_history.csv", hist_headers, crawler.status_history)
        summary["status_history"] = len(crawler.status_history)

        # Export Category Specifications (Section 10.4)
        specs_summary = self.export_category_specs(crawler)
        summary.update(specs_summary)

        return summary
