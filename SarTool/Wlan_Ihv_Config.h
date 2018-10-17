/*++

    Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    Wlan_Ihv_Config.h

Abstract:

    WLAN IHV-specific configuration definitions.

--*/

#pragma once

// These values are used to ensure the two endpoints (SarMgr and the IHV driver) are in sync.
// Additionally, the corresponding version of the IHV Doc describes features and expectations.
// For example, see WLAN_SAR_WP_IHV_DOC_ver1.3_revXYZ.pdf for more information about the structs defined here.
// The IHV Major/Minor Version should match the corresponding values in Dmf_Wlan_Public.h.
//
static const int WDI_SAR_IHV_VERSION_MAJOR = 1;
static const int WDI_SAR_IHV_VERSION_MINOR = 3;

static const int MAX_NUM_SAR_WIFI_POWER_TABLE = 12;
static const int MAX_NUM_SAR_WIFI_POWER_VALUES_PER_TABLE= 5;

// The GUID used to store IHV-specific WLAN configuration variables in UEFI. These variables are
// only read by the IHV WLAN driver (e.g. REGION_CONFIG_VALUES and SAR_POWER_TABLE.)
// {8949533B-7EDA-4D90-A876-BF16215B0C9C}
//
DEFINE_GUID(WDI_SAR_UEFI_IHV_PARAMS, 
            0x8949533b, 0x7eda, 0x4d90, 0xa8, 0x76, 0xbf, 0x16, 0x21, 0x5b, 0xc, 0x9c);

// The named UEFI variables to be read from WDI_SAR_UEFI_IHV_PARAMS.
//
static const LPCWSTR WifiRegionConfig = L"WifiRegionConfig";
static const LPCWSTR WifiSARTable = L"WifiSARTable";

#pragma pack(push)
#pragma pack(1)
typedef struct _WDI_GEO_STATE_ALPHA
{
    UINT16 AsciiChars;
    UINT16 Reserved;
} WDI_GEO_STATE_ALPHA;
#pragma pack(pop)

typedef enum _WDI_DYNAMIC_GEO_STATE
{
    WDI_DYNAMIC_GEO_VALUE_DISABLED = 0,
    WDI_DYNAMIC_GEO_VALUE_ENABLED = 1,
} WDI_DYNAMIC_GEO_STATE;

typedef enum _WDI_DYNAMIC_GEO_TYPE
{
    WDI_DYNAMIC_GEO_TYPE_DYNAMIC_ONLY = 0,         // Dynamically determine Geo at runtime (e.g. using 802.11d)
    WDI_DYNAMIC_GEO_TYPE_STATIC_THEN_DYNAMIC = 1,  // Use GeoLocationValue if not 0xFFFFFFFF; otherwise dynamic...
    WDI_DYNAMIC_GEO_TYPE_DYNAMIC_THEN_STATIC = 2,  // Use dynamic result if available; otherwise GeoLocationValue
    WDI_DYNAMIC_GEO_TYPE_UNASSIGNED = 3,
} WDI_DYNAMIC_GEO_TYPE;

typedef struct _REGION_CONFIG_VALUES
{
    WDI_GEO_STATE_ALPHA GeoCountryString;
    UINT32 GeoLocationValue;
    UINT8 DynamicGeoState; // WDI_DYNAMIC_GEO_STATE
    UINT8 DynamicGeoType;  // WDI_DYNAMIC_GEO_TYPE
} REGION_CONFIG_VALUES;

typedef struct _SAR_POWER_TABLE
{
    UINT8 PowerValues[MAX_NUM_SAR_WIFI_POWER_TABLE][MAX_NUM_SAR_WIFI_POWER_VALUES_PER_TABLE];
} SAR_POWER_TABLE;

// eof: Wlan_Ihv_Config.h
//
