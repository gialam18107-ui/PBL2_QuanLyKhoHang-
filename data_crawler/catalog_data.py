"""
Authentic PC hardware catalog data for data crawler.
Accurately categorized with specifications conforming strictly to QuanLyKhoHang_schema.json.
"""

from typing import List, Dict, Any

CATALOG_PRODUCTS: List[Dict[str, Any]] = [
    # =========================================================================
    # 1. CPU (Intel & AMD)
    # Fields: socket, tier, series
    # =========================================================================
    {
        "category": "CPU",
        "brand": "INTEL",
        "product_name": "Intel Core i9-14900K Processor",
        "specs": {"socket": "LGA1700", "tier": "I9", "series": "intel 14th"}
    },
    {
        "category": "CPU",
        "brand": "INTEL",
        "product_name": "Intel Core i7-14700K Processor",
        "specs": {"socket": "LGA1700", "tier": "I7", "series": "intel 14th"}
    },
    {
        "category": "CPU",
        "brand": "INTEL",
        "product_name": "Intel Core i5-14600K Processor",
        "specs": {"socket": "LGA1700", "tier": "I5", "series": "intel 14th"}
    },
    {
        "category": "CPU",
        "brand": "INTEL",
        "product_name": "Intel Core i5-13400F Processor",
        "specs": {"socket": "LGA1700", "tier": "I5", "series": "intel 13th"}
    },
    {
        "category": "CPU",
        "brand": "INTEL",
        "product_name": "Intel Core i3-13100 Processor",
        "specs": {"socket": "LGA1700", "tier": "I3", "series": "intel 13th"}
    },
    {
        "category": "CPU",
        "brand": "INTEL",
        "product_name": "Intel Core Ultra 9 285K Processor",
        "specs": {"socket": "LGA1851", "tier": "U9", "series": "ultra 200"}
    },
    {
        "category": "CPU",
        "brand": "AMD",
        "product_name": "AMD Ryzen 9 9950X Processor",
        "specs": {"socket": "AM5", "tier": "R9", "series": "amd 9000"}
    },
    {
        "category": "CPU",
        "brand": "AMD",
        "product_name": "AMD Ryzen 7 7800X3D Processor",
        "specs": {"socket": "AM5", "tier": "R7", "series": "amd 7000"}
    },
    {
        "category": "CPU",
        "brand": "AMD",
        "product_name": "AMD Ryzen 5 7600X Processor",
        "specs": {"socket": "AM5", "tier": "R5", "series": "amd 7000"}
    },
    {
        "category": "CPU",
        "brand": "AMD",
        "product_name": "AMD Ryzen 5 5600 Processor",
        "specs": {"socket": "AM4", "tier": "R5", "series": "amd 5000"}
    },

    # =========================================================================
    # 2. GPU (NVIDIA & AMD via ASUS, MSI, GIGABYTE)
    # Fields: series, producer, vram
    # =========================================================================
    {
        "category": "GPU",
        "brand": "ASUS",
        "product_name": "ASUS ROG Strix GeForce RTX 4090 OC 24GB",
        "specs": {"series": "RTX 4090", "producer": "NVIDIA", "vram": 24}
    },
    {
        "category": "GPU",
        "brand": "MSI",
        "product_name": "MSI GeForce RTX 4080 SUPER 16G GAMING X SLIM",
        "specs": {"series": "RTX 4080", "producer": "NVIDIA", "vram": 16}
    },
    {
        "category": "GPU",
        "brand": "GIGABYTE",
        "product_name": "GIGABYTE GeForce RTX 4070 Ti SUPER GAMING OC 16G",
        "specs": {"series": "RTX 4070", "producer": "NVIDIA", "vram": 16}
    },
    {
        "category": "GPU",
        "brand": "ASUS",
        "product_name": "ASUS Dual GeForce RTX 4070 SUPER EVO OC 12GB",
        "specs": {"series": "RTX 4070", "producer": "NVIDIA", "vram": 12}
    },
    {
        "category": "GPU",
        "brand": "MSI",
        "product_name": "MSI GeForce RTX 4060 Ti VENTUS 2X BLACK 8G OC",
        "specs": {"series": "RTX 4060", "producer": "NVIDIA", "vram": 8}
    },
    {
        "category": "GPU",
        "brand": "GIGABYTE",
        "product_name": "GIGABYTE GeForce RTX 4060 WINDFORCE OC 8G",
        "specs": {"series": "RTX 4060", "producer": "NVIDIA", "vram": 8}
    },
    {
        "category": "GPU",
        "brand": "ASUS",
        "product_name": "ASUS TUF Gaming Radeon RX 7900 XTX OC 24GB",
        "specs": {"series": "RX 7900", "producer": "AMD", "vram": 24}
    },
    {
        "category": "GPU",
        "brand": "GIGABYTE",
        "product_name": "GIGABYTE Radeon RX 7800 XT GAMING OC 16G",
        "specs": {"series": "RX 7800", "producer": "AMD", "vram": 16}
    },
    {
        "category": "GPU",
        "brand": "MSI",
        "product_name": "MSI GeForce RTX 3060 VENTUS 2X 12G OC",
        "specs": {"series": "RTX 3060", "producer": "NVIDIA", "vram": 12}
    },

    # =========================================================================
    # 3. MAINBOARD (ASUS, MSI, GIGABYTE)
    # Fields: cpu_support, socket, size, chipset, ram_support
    # =========================================================================
    {
        "category": "MAINBOARD",
        "brand": "ASUS",
        "product_name": "ASUS ROG MAXIMUS Z790 HERO",
        "specs": {
            "cpu_support": "Intel 12th/13th/14th Gen",
            "socket": "LGA1700",
            "size": "ATX",
            "chipset": "Z790",
            "ram_support": "DDR5"
        }
    },
    {
        "category": "MAINBOARD",
        "brand": "ASUS",
        "product_name": "ASUS TUF GAMING B760-PLUS WIFI",
        "specs": {
            "cpu_support": "Intel 12th/13th/14th Gen",
            "socket": "LGA1700",
            "size": "ATX",
            "chipset": "B760",
            "ram_support": "DDR5"
        }
    },
    {
        "category": "MAINBOARD",
        "brand": "MSI",
        "product_name": "MSI MAG B760 TOMAHAWK WIFI",
        "specs": {
            "cpu_support": "Intel 12th/13th/14th Gen",
            "socket": "LGA1700",
            "size": "ATX",
            "chipset": "B760",
            "ram_support": "DDR5"
        }
    },
    {
        "category": "MAINBOARD",
        "brand": "MSI",
        "product_name": "MSI PRO B760M-A WIFI DDR4",
        "specs": {
            "cpu_support": "Intel 12th/13th/14th Gen",
            "socket": "LGA1700",
            "size": "Micro-ATX",
            "chipset": "B760",
            "ram_support": "DDR4"
        }
    },
    {
        "category": "MAINBOARD",
        "brand": "GIGABYTE",
        "product_name": "GIGABYTE Z790 AORUS MASTER",
        "specs": {
            "cpu_support": "Intel 12th/13th/14th Gen",
            "socket": "LGA1700",
            "size": "E-ATX",
            "chipset": "Z790",
            "ram_support": "DDR5"
        }
    },
    {
        "category": "MAINBOARD",
        "brand": "GIGABYTE",
        "product_name": "GIGABYTE B650 AORUS ELITE AX",
        "specs": {
            "cpu_support": "AMD Ryzen 7000/8000/9000",
            "socket": "AM5",
            "size": "ATX",
            "chipset": "B650",
            "ram_support": "DDR5"
        }
    },
    {
        "category": "MAINBOARD",
        "brand": "MSI",
        "product_name": "MSI MAG B650 TOMAHAWK WIFI",
        "specs": {
            "cpu_support": "AMD Ryzen 7000/8000/9000",
            "socket": "AM5",
            "size": "ATX",
            "chipset": "B650",
            "ram_support": "DDR5"
        }
    },
    {
        "category": "MAINBOARD",
        "brand": "ASUS",
        "product_name": "ASUS ROG STRIX B650E-I GAMING WIFI",
        "specs": {
            "cpu_support": "AMD Ryzen 7000/8000/9000",
            "socket": "AM5",
            "size": "Mini-ITX",
            "chipset": "B650",
            "ram_support": "DDR5"
        }
    },

    # =========================================================================
    # 4. RAM (CORSAIR & KINGSTON)
    # Fields: generation, quantity, capacity, bus
    # =========================================================================
    {
        "category": "RAM",
        "brand": "CORSAIR",
        "product_name": "CORSAIR VENGEANCE RGB DDR5 32GB (2x16GB) 6000MHz",
        "specs": {"generation": "DDR5", "quantity": 2, "capacity": 32, "bus": 6000}
    },
    {
        "category": "RAM",
        "brand": "CORSAIR",
        "product_name": "CORSAIR DOMINATOR TITANIUM RGB DDR5 64GB (2x32GB) 6400MHz",
        "specs": {"generation": "DDR5", "quantity": 2, "capacity": 64, "bus": 6400}
    },
    {
        "category": "RAM",
        "brand": "CORSAIR",
        "product_name": "CORSAIR VENGEANCE LPX DDR4 16GB (2x8GB) 3200MHz",
        "specs": {"generation": "DDR4", "quantity": 2, "capacity": 16, "bus": 3200}
    },
    {
        "category": "RAM",
        "brand": "KINGSTON",
        "product_name": "Kingston FURY Beast RGB DDR5 32GB (2x16GB) 5600MHz",
        "specs": {"generation": "DDR5", "quantity": 2, "capacity": 32, "bus": 5600}
    },
    {
        "category": "RAM",
        "brand": "KINGSTON",
        "product_name": "Kingston FURY Beast DDR4 16GB (2x8GB) 3200MHz",
        "specs": {"generation": "DDR4", "quantity": 2, "capacity": 16, "bus": 3200}
    },
    {
        "category": "RAM",
        "brand": "KINGSTON",
        "product_name": "Kingston FURY Renegade DDR5 32GB (2x16GB) 6400MHz",
        "specs": {"generation": "DDR5", "quantity": 2, "capacity": 32, "bus": 6400}
    },
    {
        "category": "RAM",
        "brand": "CORSAIR",
        "product_name": "CORSAIR VENGEANCE DDR5 16GB (1x16GB) 5200MHz",
        "specs": {"generation": "DDR5", "quantity": 1, "capacity": 16, "bus": 5200}
    },

    # =========================================================================
    # 5. SSD (SAMSUNG, WESTERN DIGITAL, KINGSTON)
    # Fields: type, gen, capacity``
    # =========================================================================
    {
        "category": "SSD",
        "brand": "SAMSUNG",
        "product_name": "Samsung 990 PRO NVMe M.2 SSD 2TB",
        "specs": {"type": "NVMe M.2", "gen": "PCIe 4.0", "capacity": "2TB"}
    },
    {
        "category": "SSD",
        "brand": "SAMSUNG",
        "product_name": "Samsung 990 PRO NVMe M.2 SSD 1TB",
        "specs": {"type": "NVMe M.2", "gen": "PCIe 4.0", "capacity": "1TB"}
    },
    {
        "category": "SSD",
        "brand": "SAMSUNG",
        "product_name": "Samsung 980 PRO NVMe M.2 SSD 2TB",
        "specs": {"type": "NVMe M.2", "gen": "PCIe 4.0", "capacity": "2TB"}
    },
    {
        "category": "SSD",
        "brand": "SAMSUNG",
        "product_name": "Samsung 870 EVO SATA 2.5 Inch SSD 1TB",
        "specs": {"type": "SATA 2.5", "gen": "SATA 3", "capacity": "1TB"}
    },
    {
        "category": "SSD",
        "brand": "WESTERN DIGITAL",
        "product_name": "WD Black SN850X NVMe SSD 2TB",
        "specs": {"type": "NVMe M.2", "gen": "PCIe 4.0", "capacity": "2TB"}
    },
    {
        "category": "SSD",
        "brand": "WESTERN DIGITAL",
        "product_name": "WD Black SN770 NVMe SSD 1TB",
        "specs": {"type": "NVMe M.2", "gen": "PCIe 4.0", "capacity": "1TB"}
    },
    {
        "category": "SSD",
        "brand": "KINGSTON",
        "product_name": "Kingston KC3000 PCIe 4.0 NVMe M.2 SSD 1TB",
        "specs": {"type": "NVMe M.2", "gen": "PCIe 4.0", "capacity": "1TB"}
    },
    {
        "category": "SSD",
        "brand": "KINGSTON",
        "product_name": "Kingston NV2 PCIe 4.0 NVMe SSD 500GB",
        "specs": {"type": "NVMe M.2", "gen": "PCIe 4.0", "capacity": "500GB"}
    },

    # =========================================================================
    # 6. PSU (CORSAIR, MSI, GIGABYTE, ASUS)
    # Fields: quality, capacity, size
    # =========================================================================
    {
        "category": "PSU",
        "brand": "CORSAIR",
        "product_name": "CORSAIR RM850x 850W Fully Modular Power Supply",
        "specs": {"quality": "80 Plus Gold", "capacity": 850, "size": "ATX"}
    },
    {
        "category": "PSU",
        "brand": "CORSAIR",
        "product_name": "CORSAIR RM1000e 1000W ATX 3.0 Power Supply",
        "specs": {"quality": "80 Plus Gold", "capacity": 1000, "size": "ATX"}
    },
    {
        "category": "PSU",
        "brand": "CORSAIR",
        "product_name": "CORSAIR SF750 750W SFX Platinum Power Supply",
        "specs": {"quality": "80 Plus Platinum", "capacity": 750, "size": "SFX"}
    },
    {
        "category": "PSU",
        "brand": "MSI",
        "product_name": "MSI MAG A850GL PCIE5 850W 80 Plus Gold",
        "specs": {"quality": "80 Plus Gold", "capacity": 850, "size": "ATX"}
    },
    {
        "category": "PSU",
        "brand": "MSI",
        "product_name": "MSI MAG A750GL PCIE5 750W 80 Plus Gold",
        "specs": {"quality": "80 Plus Gold", "capacity": 750, "size": "ATX"}
    },
    {
        "category": "PSU",
        "brand": "GIGABYTE",
        "product_name": "GIGABYTE UD850GM PG5 850W 80 Plus Gold",
        "specs": {"quality": "80 Plus Gold", "capacity": 850, "size": "ATX"}
    },
    {
        "category": "PSU",
        "brand": "ASUS",
        "product_name": "ASUS TUF Gaming 750W 80 Plus Bronze Power Supply",
        "specs": {"quality": "80 Plus Bronze", "capacity": 750, "size": "ATX"}
    },
    {
        "category": "PSU",
        "brand": "CORSAIR",
        "product_name": "CORSAIR HX1200 1200W 80 Plus Platinum Modular",
        "specs": {"quality": "80 Plus Platinum", "capacity": 1200, "size": "ATX"}
    },

    # =========================================================================
    # 7. COOLING (CORSAIR, MSI, ASUS)
    # Fields: type
    # =========================================================================
    {
        "category": "COOLING",
        "brand": "CORSAIR",
        "product_name": "CORSAIR iCUE H150i ELITE LCD XT 360mm AIO Liquid Cooler",
        "specs": {"type": "AIO Liquid 360mm"}
    },
    {
        "category": "COOLING",
        "brand": "CORSAIR",
        "product_name": "CORSAIR iCUE H100i RGB ELITE 240mm AIO Liquid Cooler",
        "specs": {"type": "AIO Liquid 240mm"}
    },
    {
        "category": "COOLING",
        "brand": "MSI",
        "product_name": "MSI MAG CORELIQUID E360 Liquid Cooler",
        "specs": {"type": "AIO Liquid 360mm"}
    },
    {
        "category": "COOLING",
        "brand": "MSI",
        "product_name": "MSI MAG CORELIQUID 240R V2 Liquid Cooler",
        "specs": {"type": "AIO Liquid 240mm"}
    },
    {
        "category": "COOLING",
        "brand": "ASUS",
        "product_name": "ASUS ROG RYUJIN III 360 ARGB Liquid Cooler",
        "specs": {"type": "AIO Liquid 360mm"}
    },
    {
        "category": "COOLING",
        "brand": "ASUS",
        "product_name": "ASUS TUF Gaming LC II 240 ARGB Liquid Cooler",
        "specs": {"type": "AIO Liquid 240mm"}
    },
    {
        "category": "COOLING",
        "brand": "CORSAIR",
        "product_name": "CORSAIR A115 High-Performance Dual-Fan Air Cooler",
        "specs": {"type": "Air Cooling"}
    },

    # =========================================================================
    # 8. CASE (CORSAIR, MSI, ASUS)
    # Fields: product_name, size
    # =========================================================================
    {
        "category": "CASE",
        "brand": "CORSAIR",
        "product_name": "CORSAIR 4000D AIRFLOW Tempered Glass Mid-Tower Case",
        "specs": {"product_name": "CORSAIR 4000D AIRFLOW", "size": "Mid Tower"}
    },
    {
        "category": "CASE",
        "brand": "CORSAIR",
        "product_name": "CORSAIR 5000D AIRFLOW Tempered Glass Mid-Tower Case",
        "specs": {"product_name": "CORSAIR 5000D AIRFLOW", "size": "Mid Tower"}
    },
    {
        "category": "CASE",
        "brand": "CORSAIR",
        "product_name": "CORSAIR 7000D AIRFLOW Full-Tower Case",
        "specs": {"product_name": "CORSAIR 7000D AIRFLOW", "size": "Full Tower"}
    },
    {
        "category": "CASE",
        "brand": "MSI",
        "product_name": "MSI MPG GUNGNIR 300R AIRFLOW Mid-Tower Case",
        "specs": {"product_name": "MSI MPG GUNGNIR 300R", "size": "Mid Tower"}
    },
    {
        "category": "CASE",
        "brand": "MSI",
        "product_name": "MSI MAG FORGE 100R Mid-Tower Gaming Case",
        "specs": {"product_name": "MSI MAG FORGE 100R", "size": "Mid Tower"}
    },
    {
        "category": "CASE",
        "brand": "ASUS",
        "product_name": "ASUS TUF Gaming GT502 Dual Chamber Mid-Tower Case",
        "specs": {"product_name": "ASUS TUF Gaming GT502", "size": "Mid Tower"}
    },
    {
        "category": "CASE",
        "brand": "ASUS",
        "product_name": "ASUS Prime AP201 Micro-ATX Small Tower Case",
        "specs": {"product_name": "ASUS Prime AP201", "size": "Mini Tower"}
    },
]
