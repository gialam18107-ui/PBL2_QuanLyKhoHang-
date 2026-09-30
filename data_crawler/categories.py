"""
Categories, Brands, Customers, and Master Definitions for Warehouse Management System (PBL2).
Strictly complies with QuanLyKhoHang_schema.json and project_rules.md.
"""

from typing import Dict, List, Any

# 8 Fixed Categories
CATEGORIES: List[Dict[str, Any]] = [
    {"category_id": 1, "category_name": "CPU", "code": "CPU"},
    {"category_id": 2, "category_name": "GPU", "code": "GPU"},
    {"category_id": 3, "category_name": "MAINBOARD", "code": "MB"},
    {"category_id": 4, "category_name": "RAM", "code": "RAM"},
    {"category_id": 5, "category_name": "SSD", "code": "SSD"},
    {"category_id": 6, "category_name": "PSU", "code": "PSU"},
    {"category_id": 7, "category_name": "COOLING", "code": "COOL"},
    {"category_id": 8, "category_name": "CASE", "code": "CASE"},
]

CATEGORY_NAME_TO_CODE = {c["category_name"]: c["code"] for c in CATEGORIES}
CATEGORY_CODE_TO_ID = {c["code"]: c["category_id"] for c in CATEGORIES}
CATEGORY_NAME_TO_ID = {c["category_name"]: c["category_id"] for c in CATEGORIES}

# Master Brands
BRANDS: List[Dict[str, Any]] = [
    {"brand_id": 1, "brand": "INTEL", "code": "INTEL"},
    {"brand_id": 2, "brand": "AMD", "code": "AMD"},
    {"brand_id": 3, "brand": "NVIDIA", "code": "NVI"},
    {"brand_id": 4, "brand": "ASUS", "code": "ASUS"},
    {"brand_id": 5, "brand": "MSI", "code": "MSI"},
    {"brand_id": 6, "brand": "GIGABYTE", "code": "GIG"},
    {"brand_id": 7, "brand": "SAMSUNG", "code": "SAM"},
    {"brand_id": 8, "brand": "CORSAIR", "code": "COR"},
    {"brand_id": 9, "brand": "KINGSTON", "code": "KIN"},
    {"brand_id": 10, "brand": "WESTERN DIGITAL", "code": "WD"},
]

BRAND_NAME_TO_CODE = {b["brand"]: b["code"] for b in BRANDS}
BRAND_CODE_TO_ID = {b["code"]: b["brand_id"] for b in BRANDS}
BRAND_NAME_TO_ID = {b["brand"]: b["brand_id"] for b in BRANDS}

# Master Unit Statuses
UNIT_STATUSES: List[Dict[str, Any]] = [
    {"status_id": 1, "status": "AVAILABLE"},
    {"status_id": 2, "status": "WARRANTY"},
    {"status_id": 3, "status": "OUT"},
    {"status_id": 4, "status": "SCRAPPED"},
]

# Master Inbound Types
INBOUND_TYPES: List[Dict[str, Any]] = [
    {"id": 1, "type": "PURCHASING", "code": "PUR", "next_code_no": 2},
    {"id": 2, "type": "ADJUSTMENT_IN", "code": "ADJ", "next_code_no": 1},
    {"id": 3, "type": "WARRANTY_CHECKING", "code": "WCK", "next_code_no": 1},
    {"id": 4, "type": "WARRANTY_REPLACEMENT", "code": "WRP", "next_code_no": 1},
    {"id": 5, "type": "CUSTOMER_RETURN", "code": "CRT", "next_code_no": 1},
]

# Master Inbound Statuses
INBOUND_STATUSES: List[Dict[str, Any]] = [
    {"status_id": 1, "status": "PENDING"},
    {"status_id": 2, "status": "PROCESSING"},
    {"status_id": 3, "status": "PARTIALLY_CONFIRMED"},
    {"status_id": 4, "status": "CONFIRMED"},
    {"status_id": 5, "status": "CANCELLED"},
]

# Master Delivery Statuses
DELIVERY_STATUSES: List[Dict[str, Any]] = [
    {"status_id": 1, "status": "EXPECTED"},
    {"status_id": 2, "status": "RECEIVING"},
    {"status_id": 3, "status": "INSPECTING"},
    {"status_id": 4, "status": "COMPLETED"},
]

# Master Outbound Types
OUTBOUND_TYPES: List[Dict[str, Any]] = [
    {"id": 1, "type": "SELLING", "code": "SEL", "next_code_no": 2},
    {"id": 2, "type": "ADJUSTMENT_OUT", "code": "ADJ", "next_code_no": 1},
    {"id": 3, "type": "RETURN_TO_SUPPLIER", "code": "RTS", "next_code_no": 1},
    {"id": 4, "type": "WARRANTY_RETURN", "code": "WRT", "next_code_no": 1},
    {"id": 5, "type": "WARRANTY_REPLACEMENT_TO_CUSTOMER", "code": "WRC", "next_code_no": 1},
]

# Master Outbound Statuses
OUTBOUND_STATUSES: List[Dict[str, Any]] = [
    {"status_id": 1, "status": "PENDING"},
    {"status_id": 2, "status": "PROCESSING"},
    {"status_id": 3, "status": "CONFIRMED"},
    {"status_id": 4, "status": "PARTIALLY_CONFIRMED"},
    {"status_id": 5, "status": "CANCELLED"},
]

# Master Shipment Statuses
SHIPMENT_STATUSES: List[Dict[str, Any]] = [
    {"status_id": 1, "status": "PENDING"},
    {"status_id": 2, "status": "PICKING"},
    {"status_id": 3, "status": "COMPLETED"},
    {"status_id": 4, "status": "CANCELLED"},
]

# Master Customers (Individual: IN-xxxxxxx, Business: BS-xxxxxxx)
CUSTOMERS: List[Dict[str, Any]] = [
    {"customer_id": 1, "customer_code": "IN-0000001", "name": "Nguyen Van An", "type": "individual"},
    {"customer_id": 2, "customer_code": "IN-0000002", "name": "Tran Thi Binh", "type": "individual"},
    {"customer_id": 3, "customer_code": "IN-0000003", "name": "Le Hoang Nam", "type": "individual"},
    {"customer_id": 4, "customer_code": "BS-0000001", "name": "FPT Information System", "type": "business"},
    {"customer_id": 5, "customer_code": "BS-0000002", "name": "Phong Vu Technology JSC", "type": "business"},
    {"customer_id": 6, "customer_code": "BS-0000003", "name": "Viettel Commerce Corp", "type": "business"},
]

# Individual Details
INDIVIDUAL_INFO: List[Dict[str, Any]] = [
    {"customer_id": 1, "phone_number": "0912345678", "email": "nguyenvanan@gmail.com", "next_code_no": 2},
    {"customer_id": 2, "phone_number": "0987654321", "email": "tranbinh@outlook.com", "next_code_no": 3},
    {"customer_id": 3, "phone_number": "0905123456", "email": "lenam.it@gmail.com", "next_code_no": 4},
]

# Business Details
BUSINESS_INFO: List[Dict[str, Any]] = [
    {"customer_id": 4, "phone_number": "0243768900", "email": "contact@fis.com.vn", "tax_code": "0101234567", "next_code_no": 2},
    {"customer_id": 5, "phone_number": "0283834567", "email": "sales@phongvu.vn", "tax_code": "0309876543", "next_code_no": 3},
    {"customer_id": 6, "phone_number": "0246277889", "email": "procurement@viettel.vn", "tax_code": "0100109106", "next_code_no": 4},
]
