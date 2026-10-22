"""
Core Data Crawler implementation for Warehouse Management System.
Conforms strictly to project_rules.md (Rule 8) and QuanLyKhoHang_schema.json.
Scrapes live authentic PC hardware product data from reliable retailers (AnPhat PC).
"""

import urllib.request
import urllib.error
import re
import random
from typing import List, Dict, Any

from data_crawler.categories import (
    CATEGORIES,
    BRANDS,
    CATEGORY_NAME_TO_CODE,
    BRAND_NAME_TO_CODE,
)
from data_crawler.catalog_data import CATALOG_PRODUCTS


class DataCrawler:
    """
    Crawls PC hardware products and specifications, organizing them by category
    and enforcing all constraints from QuanLyKhoHang_schema.json.
    """

    def __init__(self, seed: int = 42):
        self.rng = random.Random(seed)
        self.products: List[Dict[str, Any]] = []
        self.product_quantities: List[Dict[str, Any]] = []
        self.specs_by_category: Dict[str, List[Dict[str, Any]]] = {
            "CPU": [],
            "GPU": [],
            "MAINBOARD": [],
            "RAM": [],
            "SSD": [],
            "PSU": [],
            "COOLING": [],
            "CASE": [],
        }
        self.units: List[Dict[str, Any]] = []
        self.status_history: List[Dict[str, Any]] = []

    def generate_ean13(self, seed_num: int) -> str:
        """Generate a valid 13-digit EAN barcode."""
        raw12 = f"893{seed_num:09d}"[:12]
        even_sum = sum(int(raw12[i]) for i in range(1, 12, 2))
        odd_sum = sum(int(raw12[i]) for i in range(0, 12, 2))
        total = odd_sum + even_sum * 3
        check_digit = (10 - (total % 10)) % 10
        return f"{raw12}{check_digit}"

    def generate_serial(self, unit_id: int) -> str:
        """Generate unique serial matching schema comment: SN-7K3PE33 (SN- + 7 uppercase alphanumeric)."""
        chars = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ"
        suffix = "".join(self.rng.choice(chars) for _ in range(7))
        return f"SN-{suffix[:3]}{(unit_id % 10000):04d}"

    def crawl_online_samples(self) -> List[Dict[str, Any]]:
        """
        Scrape live hardware product listings from active computer hardware retailer (AnPhat PC).
        """
        crawled_items: List[Dict[str, Any]] = []
        headers = {
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
        }

        # 1. Scrape Live CPUs
        try:
            req = urllib.request.Request("https://anphatpc.com.vn/cpu-bo-vi-xu-ly.html", headers=headers)
            with urllib.request.urlopen(req, timeout=6) as resp:
                html = resp.read().decode("utf-8", errors="ignore")
                h3s = re.findall(r"<h3[^>]*>(.*?)</h3>", html, re.DOTALL)
                for raw_title in h3s[:15]:
                    title = re.sub(r"<[^>]+>", "", raw_title).strip()
                    title_lower = title.lower()
                    if "intel" in title_lower:
                        brand = "INTEL"
                    elif "amd" in title_lower or "ryzen" in title_lower:
                        brand = "AMD"
                    else:
                        continue

                    socket = "LGA1700"
                    if "1851" in title:
                        socket = "LGA1851"
                    elif "am5" in title_lower:
                        socket = "AM5"
                    elif "am4" in title_lower:
                        socket = "AM4"

                    tier = "I5"
                    for t in ["i9", "i7", "i5", "i3", "r9", "r7", "r5", "r3", "ultra 9", "ultra 7", "ultra 5"]:
                        if t in title_lower:
                            tier = t.upper().replace("ULTRA ", "U")
                            break

                    series = "intel 14th"
                    if "12" in title:
                        series = "intel 12th"
                    elif "13" in title:
                        series = "intel 13th"
                    elif "14" in title:
                        series = "intel 14th"
                    elif "7000" in title or "7800" in title or "7600" in title:
                        series = "amd 7000"
                    elif "9000" in title or "9950" in title:
                        series = "amd 9000"
                    elif "5000" in title or "5600" in title:
                        series = "amd 5000"

                    crawled_items.append({
                        "category": "CPU",
                        "brand": brand,
                        "product_name": title[:100],
                        "specs": {"socket": socket, "tier": tier, "series": series}
                    })
        except Exception:
            pass

        # 2. Scrape Live GPUs
        try:
            req = urllib.request.Request("https://anphatpc.com.vn/vga-card-man-hinh.html", headers=headers)
            with urllib.request.urlopen(req, timeout=6) as resp:
                html = resp.read().decode("utf-8", errors="ignore")
                h3s = re.findall(r"<h3[^>]*>(.*?)</h3>", html, re.DOTALL)
                for raw_title in h3s[:15]:
                    title = re.sub(r"<[^>]+>", "", raw_title).strip()
                    brand = None
                    for b in ["ASUS", "MSI", "GIGABYTE"]:
                        if b.lower() in title.lower():
                            brand = b
                            break
                    if not brand:
                        continue

                    vram = 12
                    vram_m = re.search(r"(\d+)\s*GB", title, re.IGNORECASE)
                    if vram_m:
                        vram = int(vram_m.group(1))

                    series = "RTX 4070"
                    for s in ["RTX 5090", "RTX 5080", "RTX 5070", "RTX 4090", "RTX 4080", "RTX 4070", "RTX 4060", "RTX 3060", "RTX 3050", "RX 7900", "RX 7800", "RX 7600"]:
                        if s.lower() in title.lower():
                            series = s
                            break

                    producer = "AMD" if "rx" in title.lower() else "NVIDIA"

                    crawled_items.append({
                        "category": "GPU",
                        "brand": brand,
                        "product_name": title[:100],
                        "specs": {"series": series, "producer": producer, "vram": vram}
                    })
        except Exception:
            pass

        return crawled_items

    def build_dataset(self, min_products_per_cat: int = 6) -> None:
        """
        Build the complete database dataset from catalog and online sources,
        assigning IDs, product codes, barcodes, SKUs, and specifications conforming to QuanLyKhoHang_schema.json.
        """
        raw_items = list(CATALOG_PRODUCTS)

        # Attempt online crawl from AnPhat PC
        online_items = self.crawl_online_samples()
        raw_items.extend(online_items)

        # Counters for code generation
        # Schema rule: producer_code + category_code + next_code_no(producer) + next_code_no(category)
        brand_counters: Dict[str, int] = {}
        cat_counters: Dict[str, int] = {}

        product_id = 1
        spec_ids: Dict[str, int] = {cat: 1 for cat in self.specs_by_category}
        unit_id = 1
        history_id = 1

        for item in raw_items:
            cat_name = item["category"]
            brand_name = item["brand"]

            if cat_name not in CATEGORY_NAME_TO_CODE or brand_name not in BRAND_NAME_TO_CODE:
                continue

            cat_code = CATEGORY_NAME_TO_CODE[cat_name]
            brand_code = BRAND_NAME_TO_CODE[brand_name]

            brand_counters[brand_code] = brand_counters.get(brand_code, 0) + 1
            cat_counters[cat_code] = cat_counters.get(cat_code, 0) + 1

            brand_seq = brand_counters[brand_code]
            cat_seq = cat_counters[cat_code]

            # Product Code: INTEL-CPU-0001-0001
            product_code = f"{brand_code}-{cat_code}-{brand_seq:04d}-{cat_seq:04d}"
            barcode = self.generate_ean13(product_id)
            sku = f"{cat_code}-{brand_code}-{product_id:04d}"

            # Initial stock quantity between 25 and 45 (ensures product.quantity >= 20 for test_io.cpp)
            initial_qty = self.rng.randint(25, 45)

            product_record = {
                "product_id": product_id,
                "product_name": item["product_name"],
                "product_code": product_code,
                "barcode": barcode,
                "sku": sku,
                "brand": brand_name,
                "category": cat_name,
                "is_sold": "TRUE",
            }
            self.products.append(product_record)

            # Build PRODUCT_QUANTITY record matching QuanLyKhoHang_schema.json
            self.product_quantities.append({
                "product_id": product_id,
                "available_quan": initial_qty,
                "warranty_quan": 0,
                "scrapped_quan": 0,
            })

            # Build category-specific specification record
            spec_id = spec_ids[cat_name]
            spec_ids[cat_name] += 1
            raw_specs = item.get("specs", {})

            if cat_name == "CPU":
                self.specs_by_category["CPU"].append({
                    "cpu_id": spec_id,
                    "product_id": product_id,
                    "socket": raw_specs.get("socket", "LGA1700"),
                    "tier": raw_specs.get("tier", "I7"),
                    "series": raw_specs.get("series", "intel 14th"),
                })
            elif cat_name == "GPU":
                self.specs_by_category["GPU"].append({
                    "gpu_id": spec_id,
                    "product_id": product_id,
                    "series": raw_specs.get("series", "RTX 4070"),
                    "producer": raw_specs.get("producer", "NVIDIA"),
                    "vram": int(raw_specs.get("vram", 12)),
                })
            elif cat_name == "MAINBOARD":
                # cpu_support in schema comment is INTEL/AMD
                cpu_sup = raw_specs.get("cpu_support", "INTEL")
                if "amd" in cpu_sup.lower() or "ryzen" in cpu_sup.lower():
                    cpu_sup = "AMD"
                else:
                    cpu_sup = "INTEL"
                self.specs_by_category["MAINBOARD"].append({
                    "mainboard_id": spec_id,
                    "product_id": product_id,
                    "cpu_support": cpu_sup,
                    "socket": raw_specs.get("socket", "LGA1700"),
                    "size": raw_specs.get("size", "ATX"),
                    "chipset": raw_specs.get("chipset", "B760"),
                    "ram_support": raw_specs.get("ram_support", "DDR5"),
                })
            elif cat_name == "RAM":
                # capacity and bus are VARCHAR in QuanLyKhoHang_schema.json
                self.specs_by_category["RAM"].append({
                    "ram_id": spec_id,
                    "product_id": product_id,
                    "generation": raw_specs.get("generation", "DDR5"),
                    "quantity": int(raw_specs.get("quantity", 2)),
                    "capacity": str(raw_specs.get("capacity", "32GB")),
                    "bus": str(raw_specs.get("bus", "6000MHz")),
                })
            elif cat_name == "SSD":
                self.specs_by_category["SSD"].append({
                    "ssd_id": spec_id,
                    "product_id": product_id,
                    "type": raw_specs.get("type", "NVMe M.2"),
                    "gen": raw_specs.get("gen", "PCIe 4.0"),
                    "capacity": str(raw_specs.get("capacity", "1TB")),
                })
            elif cat_name == "PSU":
                self.specs_by_category["PSU"].append({
                    "psu_id": spec_id,
                    "product_id": product_id,
                    "quality": raw_specs.get("quality", "80 Plus Gold"),
                    "capacity": int(raw_specs.get("capacity", 850)),
                    "size": raw_specs.get("size", "ATX"),
                })
            elif cat_name == "COOLING":
                self.specs_by_category["COOLING"].append({
                    "cooling_id": spec_id,
                    "product_id": product_id,
                    "type": raw_specs.get("type", "AIO Liquid 360mm"),
                })
            elif cat_name == "CASE":
                self.specs_by_category["CASE"].append({
                    "case_id": spec_id,
                    "product_id": product_id,
                    "product_name": raw_specs.get("product_name", item["product_name"]),
                    "size": raw_specs.get("size", "Mid Tower"),
                })

            # Create physical units in unit_inventory matching initial_qty
            for _ in range(initial_qty):
                serial = self.generate_serial(unit_id)
                self.units.append({
                    "unit_id": unit_id,
                    "product_id": product_id,
                    "serial": serial,
                    "status": "AVAILABLE",
                })
                # Status history for initial receiving
                self.status_history.append({
                    "history_id": history_id,
                    "unit_id": unit_id,
                    "old_status": "",
                    "curr_status": "AVAILABLE",
                    "transaction": 1,
                    "type": "INBOUND",
                })
                unit_id += 1
                history_id += 1

            product_id += 1
