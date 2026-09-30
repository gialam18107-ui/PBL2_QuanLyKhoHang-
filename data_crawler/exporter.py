"""
CSV Exporter for Warehouse Management System database tables.
Exports all 32 tables adhering strictly to QuanLyKhoHang_schema.json and C++ Schema.cpp.
"""

import os
import csv
from typing import List, Dict, Any

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


class SchemaExporter:
    """Exports crawled data into CSV files adhering to database schema."""

    def __init__(self, output_dir: str = "data"):
        self.output_dir = output_dir

    def _write_csv(self, filename: str, headers: List[str], rows: List[Dict[str, Any]]) -> None:
        os.makedirs(self.output_dir, exist_ok=True)
        path = os.path.join(self.output_dir, filename)
        with open(path, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(headers)
            for row in rows:
                writer.writerow([row.get(h, "") for h in headers])

    def export_all(self, crawler: DataCrawler) -> Dict[str, int]:
        """Export all 32 schema tables + status_history and compatibility CSV files."""
        summary: Dict[str, int] = {}

        # 1. CATEGORY
        cat_headers = ["category_id", "category_name", "code"]
        self._write_csv("category.csv", cat_headers, CATEGORIES)
        summary["category"] = len(CATEGORIES)

        # 2. BRAND
        brand_headers = ["brand_id", "brand", "code"]
        self._write_csv("brand.csv", brand_headers, BRANDS)
        summary["brand"] = len(BRANDS)

        # 3. UNIT_STATUS
        unit_status_headers = ["status_id", "status"]
        self._write_csv("unit_status.csv", unit_status_headers, UNIT_STATUSES)
        summary["unit_status"] = len(UNIT_STATUSES)

        # 4. INBOUND_TYPE
        inbound_type_headers = ["id", "type", "code", "next_code_no"]
        self._write_csv("inbound_type.csv", inbound_type_headers, INBOUND_TYPES)
        summary["inbound_type"] = len(INBOUND_TYPES)

        # 5. INBOUND_STATUS
        inbound_status_headers = ["status_id", "status"]
        self._write_csv("inbound_status.csv", inbound_status_headers, INBOUND_STATUSES)
        summary["inbound_status"] = len(INBOUND_STATUSES)

        # 6. DELIVERY_STATUS
        delivery_status_headers = ["status_id", "status"]
        self._write_csv("delivery_status.csv", delivery_status_headers, DELIVERY_STATUSES)
        summary["delivery_status"] = len(DELIVERY_STATUSES)

        # 7. OUTBOUND_TYPE
        outbound_type_headers = ["id", "type", "code", "next_code_no"]
        self._write_csv("outbound_type.csv", outbound_type_headers, OUTBOUND_TYPES)
        summary["outbound_type"] = len(OUTBOUND_TYPES)

        # 8. OUTBOUND_STATUS
        outbound_status_headers = ["status_id", "status"]
        self._write_csv("outbound_status.csv", outbound_status_headers, OUTBOUND_STATUSES)
        summary["outbound_status"] = len(OUTBOUND_STATUSES)

        # 9. SHIPMENT_STATUS
        shipment_status_headers = ["status_id", "status"]
        self._write_csv("shipment_status.csv", shipment_status_headers, SHIPMENT_STATUSES)
        summary["shipment_status"] = len(SHIPMENT_STATUSES)

        # 10. CUSTOMER
        customer_headers = ["customer_id", "customer_code", "name", "type"]
        self._write_csv("customer.csv", customer_headers, CUSTOMERS)
        summary["customer"] = len(CUSTOMERS)

        # 11. INDIVIDUAL_INFO
        indiv_headers = ["customer_id", "phone_number", "email", "next_code_no"]
        self._write_csv("individual_info.csv", indiv_headers, INDIVIDUAL_INFO)
        summary["individual_info"] = len(INDIVIDUAL_INFO)

        # 12. BUSINESS_INFO
        biz_headers = ["customer_id", "phone_number", "email", "tax_code", "next_code_no"]
        self._write_csv("business_info.csv", biz_headers, BUSINESS_INFO)
        summary["business_info"] = len(BUSINESS_INFO)

        # 13. PRODUCT
        prod_headers = [
            "product_id",
            "product_name",
            "product_code",
            "barcode",
            "sku",
            "brand",
            "category",
            "is_sold",
            "quantity",
        ]
        self._write_csv("product.csv", prod_headers, crawler.products)
        summary["product"] = len(crawler.products)

        # 14. PRODUCT_QUANTITY
        prod_quan_headers = ["product_id", "available_quan", "warranty_quan", "scrapped_quan"]
        self._write_csv("product_quantity.csv", prod_quan_headers, crawler.product_quantities)
        summary["product_quantity"] = len(crawler.product_quantities)

        # 15. CPU
        cpu_headers = ["cpu_id", "product_id", "socket", "tier", "series"]
        self._write_csv("cpu.csv", cpu_headers, crawler.specs_by_category["CPU"])
        summary["cpu"] = len(crawler.specs_by_category["CPU"])

        # 16. GPU
        gpu_headers = ["gpu_id", "product_id", "series", "producer", "vram"]
        self._write_csv("gpu.csv", gpu_headers, crawler.specs_by_category["GPU"])
        summary["gpu"] = len(crawler.specs_by_category["GPU"])

        # 17. MAINBOARD
        mb_headers = ["mainboard_id", "product_id", "cpu_support", "socket", "size", "chipset", "ram_support"]
        self._write_csv("mainboard.csv", mb_headers, crawler.specs_by_category["MAINBOARD"])
        summary["mainboard"] = len(crawler.specs_by_category["MAINBOARD"])

        # 18. RAM
        ram_headers = ["ram_id", "product_id", "generation", "quantity", "capacity", "bus"]
        self._write_csv("ram.csv", ram_headers, crawler.specs_by_category["RAM"])
        summary["ram"] = len(crawler.specs_by_category["RAM"])

        # 19. SSD
        ssd_headers = ["ssd_id", "product_id", "type", "gen", "capacity"]
        self._write_csv("ssd.csv", ssd_headers, crawler.specs_by_category["SSD"])
        summary["ssd"] = len(crawler.specs_by_category["SSD"])

        # 20. PSU
        psu_headers = ["psu_id", "product_id", "quality", "capacity", "size"]
        self._write_csv("psu.csv", psu_headers, crawler.specs_by_category["PSU"])
        summary["psu"] = len(crawler.specs_by_category["PSU"])

        # 21. COOLING
        cooling_headers = ["cooling_id", "product_id", "type"]
        self._write_csv("cooling.csv", cooling_headers, crawler.specs_by_category["COOLING"])
        summary["cooling"] = len(crawler.specs_by_category["COOLING"])

        # 22. CASE
        case_headers = ["case_id", "product_id", "product_name", "size"]
        self._write_csv("case.csv", case_headers, crawler.specs_by_category["CASE"])
        summary["case"] = len(crawler.specs_by_category["CASE"])

        # 23. UNIT_INVENTORY
        unit_headers = ["unit_id", "product_id", "serial", "status"]
        self._write_csv("unit_inventory.csv", unit_headers, crawler.units)
        summary["unit_inventory"] = len(crawler.units)

        # Baseline transactions for initial inventory intake and outbound flow
        # 24. INBOUND_TRANSACTION
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

        # 25. INBOUND_DETAIL
        inbound_details = [
            {
                "inbound_detail_id": 1,
                "inbound_transaction_id": 1,
                "product_id": 1,
                "ordered_quantity": 25,
            }
        ]
        inbound_detail_headers = [
            "inbound_detail_id",
            "inbound_transaction_id",
            "product_id",
            "ordered_quantity",
        ]
        self._write_csv("inbound_detail.csv", inbound_detail_headers, inbound_details)
        summary["inbound_detail"] = len(inbound_details)

        # 26. DELIVERY
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

        # 27. DELIVERY_DETAIL (Schema JSON)
        delivery_details = [
            {
                "id": "IN-PUR-000001-D01-P001",
                "delivery_id": 1,
                "product_id": 1,
                "received_quantity": 25,
            }
        ]
        delivery_detail_headers = ["id", "delivery_id", "product_id", "received_quantity"]
        self._write_csv("delivery_detail.csv", delivery_detail_headers, delivery_details)
        summary["delivery_detail"] = len(delivery_details)

        # 28. DELIVERY_UNIT_DETAIL (Compatibility)
        first_unit_serial = crawler.units[0]["serial"] if crawler.units else "SN-TEST001"
        delivery_units = [
            {
                "delivery_unit_detail_id": 1,
                "delivery_id": 1,
                "product_id": 1,
                "serial": first_unit_serial,
            }
        ]
        delivery_unit_headers = [
            "delivery_unit_detail_id",
            "delivery_id",
            "product_id",
            "serial",
        ]
        self._write_csv("delivery_unit_detail.csv", delivery_unit_headers, delivery_units)
        summary["delivery_unit_detail"] = len(delivery_units)

        # 29. OUTBOUND_TRANSACTION
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

        # 30. OUTBOUND_DETAIL
        outbound_details = [
            {
                "outbound_detail_id": 1,
                "outbound_transaction_id": 1,
                "product_id": 1,
                "ordered_quantity": 1,
            }
        ]
        outbound_detail_headers = [
            "outbound_detail_id",
            "outbound_transaction_id",
            "product_id",
            "ordered_quantity",
        ]
        self._write_csv("outbound_detail.csv", outbound_detail_headers, outbound_details)
        summary["outbound_detail"] = len(outbound_details)

        # 31. SHIPMENT
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

        # 32. SHIPMENT_DETAIL (Schema JSON)
        shipment_details = [
            {
                "id": "OUT-SEL-000001-S01-P001",
                "shipment_id": 1,
                "product_id": 1,
                "received_quantity": 1,
            }
        ]
        shipment_detail_headers = ["id", "shipment_id", "product_id", "received_quantity"]
        self._write_csv("shipment_detail.csv", shipment_detail_headers, shipment_details)
        summary["shipment_detail"] = len(shipment_details)

        # 33. SHIPMENT_UNIT_DETAIL (Compatibility)
        shipped_unit = crawler.units[-1] if crawler.units else {"product_id": 1, "serial": "SN-TEST002"}
        shipped_unit["status"] = "OUT"
        shipment_units = [
            {
                "shipment_unit_detail_id": 1,
                "shipment_id": 1,
                "product_id": shipped_unit["product_id"],
                "serial": shipped_unit["serial"],
            }
        ]
        shipment_unit_headers = [
            "shipment_unit_detail_id",
            "shipment_id",
            "product_id",
            "serial",
        ]
        self._write_csv("shipment_unit_detail.csv", shipment_unit_headers, shipment_units)
        summary["shipment_unit_detail"] = len(shipment_units)

        # 34. STATUS_HISTORY
        hist_headers = ["history_id", "unit_id", "old_status", "curr_status", "transaction", "type"]
        self._write_csv("status_history.csv", hist_headers, crawler.status_history)
        summary["status_history"] = len(crawler.status_history)

        return summary
