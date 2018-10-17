/*++

    Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name:

    Dmf_Wlan_Public.h

Abstract:

    Public Wlan Module definitions.

Environment:

    User-mode

--*/

#pragma once

// These values are used to ensure the two endpoints (SarMgr and the IHV driver) are in sync.
// Additionally, the corresponding version of the IHV Doc describes features and expectations.
// For example, see WLAN_SAR_WP_IHV_DOC_ver1.3_revXYZ.pdf for more information about the structs defined here.
//
static const int WDI_SAR_INTERFACE_VERSION_MAJOR = 1;
static const int WDI_SAR_INTERFACE_VERSION_MINOR = 3;

// The GUID for the SAR WlanDeviceServiceCommand.
// {504304B4-1941-4A95-B819-A2102B69E5CD}
//
DEFINE_GUID(WDI_SAR_DEVICE_SERVICE,
            0x504304b4, 0x1941, 0x4a95, 0xb8, 0x19, 0xa2, 0x10, 0x2b, 0x69, 0xe5, 0xcd);

// The GUID used to store common configuration variables in UEFI. These variables are used by both
// SarMgr and the IHV WLAN driver (e.g. SAR_CONFIG_HEADER and SAR_CONFIG_VALUES.)
// {4290AA92-CACE-449D-887B-ADC61B9E05D}
//
DEFINE_GUID(WDI_SAR_UEFI_COMMON_PARAMS,
            0x4290AA92, 0xCACE, 0x449D, 0x88, 0x7B, 0xAD, 0xC6, 0x1B, 0x49, 0xE0, 0x5D);

// The named UEFI variables to be read from WDI_SAR_UEFI_COMMON_PARAMS.
//
static const LPCWSTR WifiSARHeader = L"WifiSARHeader";
static const LPCWSTR WifiSARConfig = L"WifiSARConfig";

typedef enum _WDI_SAR_DEVICE_SERVICE_OPCODE
{
    WDI_SET_SAR_STATE = 0x01,
    WDI_GET_SAR_STATE = 0x02,
    WDI_GET_GEO_STATE = 0x10,
    WDI_GET_INTERFACE_VERSION = 0x80, // IHV driver should return a 2x32-bit (8-byte) blob containing:
                                      //    WDI_SAR_VERSION_MAJOR 
                                      //    WDI_SAR_VERSION_MINOR 
} WDI_SAR_DEVICE_SERVICE_OPCODE;

typedef struct _WDI_SAR_CONFIG_SET
{
    UINT32 WDI_SARAntennaIndex;
    UINT32 WDI_SARBackOffIndex;
} WDI_SAR_CONFIG_SET;

typedef enum _WDI_SAR_BACKOFF_STATE
{
    WDI_SARBACKOFF_DISABLED = 0x00,
    WDI_SARBACKOFF_ENABLED = 0x01,
} WDI_SAR_BACKOFF_STATE;

// Used for SET_SAR and GET_SAR operations.
typedef struct _WDI_SAR_STATE
{
    WDI_SAR_BACKOFF_STATE SarBackoffStatus;
    UINT32                MIMOConfigType;
    UINT32                NumWdiSarConfigElements;
} WDI_SAR_STATE;

typedef enum _WDI_SAR_RESULT
{
    WDI_SAR_SUCCESS = 0,
    WDI_SAR_INVALID_ANTENNA_INDEX = 1,
    WDI_SAR_INVALID_TABLE_INDEX = 2,
    WDI_SAR_STATE_ERROR = 4,
    WDI_SAR_MIMO_NOT_SET = 8,
} WDI_SAR_RESULT;

typedef enum _WDI_WIFI_TECHNOLOGY
{
    WDI_802_11_AC = 1,
    WDI_802_11_AX = 2,
    WDI_802_11_AD = 4,
} WDI_WIFI_TECHNOLOGY;

// UEFI Structures
#pragma pack(push)
#pragma pack(1)
typedef struct _SAR_CONFIG_HEADER
{
    UINT8 Size;
    UINT8 HeaderOffset1;
    UINT8 HeaderOffset2;
    UINT8 WLANTechnology; // WDI_WIFI_TECHNOLOGY
    UINT8 ProductID;
    UINT8 Version;
    UINT8 Revision;
    UINT8 NumberSARTables;
    UINT8 SARTablesCompressed;
    UINT8 SARTimersFormat;
    UINT8 ReservedA;
    UINT8 ReservedB;
    UINT8 ReservedC;
    UINT8 ReservedD;
    UINT8 ReservedE;
    UINT8 ReservedF;
} SAR_CONFIG_HEADER;
C_ASSERT(sizeof(SAR_CONFIG_HEADER) == 0x10);

typedef struct _SAR_CONFIG_VALUES
{
    UINT8 Size;
    UINT32 SARSafetyTimer;
    UINT32 SARSafetyRequestResponseTimeout;
    UINT32 SARUnsolicitedUpdateTimer;
    UINT8 SARState;
    UINT8 SleepModeState; // Add comment to clarify discrepancy between this .h and the IHV doc:
                          // The "WDI SAR IHV Architecture Document v1.3" has a comment about a
                          // 32-bit flag value of 0xFFFFFFFF - it should be an 8-bit flag value
                          // of 0xFFFF instead.
    UINT8 SARPowerOnState;
    UINT8 SARPowerOnStateAfterFailure;
    UINT8 SARSafetyTableIndex;
    UINT8 SleepModeStateIndexTable;
} SAR_CONFIG_VALUES;
C_ASSERT(sizeof(SAR_CONFIG_VALUES) == 0x13);
#pragma pack(pop)

// eof: Dmf_Wlan_Public.h
//
