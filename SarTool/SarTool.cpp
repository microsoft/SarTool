/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.
    Permission is hereby granted, free of charge, to any person obtaining a copy of this software
    and associated documentation files (the Software), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge, publish, distribute,
    sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    The above copyright notice and this permission notice shall be included in all copies or
    substantial portions of the Software.
    THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
    BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
    DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Module Name 

    SarTool.cpp

Abstract:

    Test application for SAR API's on Windows devices.

Environment:

    User Mode

--*/

#include "stdafx.h"

// Avoid an error related to conflicting headers by defining NOMINMAX before including Windows headers.
//   Otherwise, we'd get "illegal token on right side of '::'" in winrt\base.h.
#define NOMINMAX 0

#include "targetver.h"
#include <windows.h>
#include <comdef.h>
#include <wlanapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iterator>
#include <vector>
#include <Roapi.h> // RO_INIT_MULTITHREADED
#include "winrt\Windows.Networking.NetworkOperators.h"

#include <initguid.h>
#include "Dmf_Wlan_Public.h"
#include "Wlan_Ihv_Config.h"

// link an umbrella app lib that resolves WINRT_SetRestrictedErrorInfo and other external symbols
#pragma comment(lib, "windowsapp")

using namespace winrt::Windows::Networking::NetworkOperators;

//
// The number of milliseconds to monitor for LTE transmit status updates
// 
const DWORD LteTxStatusMonitorPeriod = 60000;

//
// The "path" a user specifies for GetConfig/SetConfig when she wants to read from
// (or write to) UEFI instead of files on disk.
// 
LPCSTR UEFI = "uefi";

//
// Commands
// These are the commands a user can enter on the command-line to determine what functionality SarTool.exe exercises.
//
LPCSTR CMD_GETCONFIG = "getconfig";
LPCSTR CMD_SETCONFIG = "setconfig";
LPCSTR CMD_GETSAR = "getsar";
LPCSTR CMD_SETSAR = "setsar";
LPCSTR CMD_UNSOLMON = "unsolMon";

_Check_return_
HRESULT
SetProcessPrivilege()
/*++

Routine Description:

    Attempts to enable the SE_SYSTEM_ENVIRONMENT_NAME privilege in our process token.  Otherwise,
    Get/SetFirmwareEnvironmentVariable API's will not succeed.

Arguments:

    VOID

Return Value:

    S_OK on success or underlying failure code.

–*/
{
    HANDLE           hToken = NULL;
    TOKEN_PRIVILEGES tokenPrivileges = { 0 };
    HRESULT          hr = S_OK;
    BOOL             fOk = FALSE;

    // Get the token for this process. 
    fOk = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);
    if (!fOk)
    {
        hr = HRESULT_FROM_WIN32(::GetLastError());
        _tprintf(TEXT("OpenProcessToken() failed, hr = 0x%x\n"), hr);
        goto exit;
    }

    // Get the LUID for the UEFI-access privilege. 
    fOk = LookupPrivilegeValue(NULL, SE_SYSTEM_ENVIRONMENT_NAME, &tokenPrivileges.Privileges[0].Luid);
    if (!fOk)
    {
        hr = HRESULT_FROM_WIN32(::GetLastError());
        _tprintf(TEXT("LookupPrivilegeValue() failed, hr = 0x%x\n"), hr);
        goto exit;
    }

    tokenPrivileges.PrivilegeCount = 1;  // one privilege to set    
    tokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Set the UEFI-access privilege for this process. 
    fOk = AdjustTokenPrivileges(hToken, FALSE, &tokenPrivileges, 0, (PTOKEN_PRIVILEGES)NULL, 0);
    if (!fOk)
    {
        hr = HRESULT_FROM_WIN32(::GetLastError());
        _tprintf(TEXT("AdjustTokenPrivileges() failed, hr = 0x%x\n"), hr);
        goto exit;
    }

exit:
    return hr;
}

VOID
PrintLastError()
/*++

Routine Description:

    Prints a formatted message explaining the last encountered error to the screen.

Arguments:

    VOID

Return Value:

    VOID

–*/
{
    LPWSTR messageBuffer;

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  GetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPWSTR)&messageBuffer,
                  0,
                  NULL);

    printf("%S", messageBuffer);
    LocalFree(messageBuffer);
}

HRESULT
SetConfig(
    LPSTR path
    )
/*++

Routine Description:

    Writes config to a set of binary provisioning files or directly to UEFI.

Arguments:

    path - "UEFI" or the path to the folder where the .bin files should be written.

Return Value:

    S_OK on success or underlying failure code.

–*/
{
    HRESULT hr = S_OK;

    // Populate an example SAR_CONFIG_HEADER.
    SAR_CONFIG_HEADER sarConfigHeader = { 0 };

    sarConfigHeader.Size = sizeof(sarConfigHeader) + 2 * sizeof(SAR_CONFIG_VALUES);
    sarConfigHeader.HeaderOffset1 = sizeof(SAR_CONFIG_HEADER);
    sarConfigHeader.HeaderOffset2 = sizeof(SAR_CONFIG_HEADER) + sizeof(SAR_CONFIG_VALUES);
    sarConfigHeader.WLANTechnology = WDI_802_11_AD;
    sarConfigHeader.ProductID = 0x4;
    sarConfigHeader.Version = 0x5;
    sarConfigHeader.Revision = 0x6;
    sarConfigHeader.NumberSARTables = 0x7;
    sarConfigHeader.SARTablesCompressed = 0x8;
    sarConfigHeader.SARTimersFormat = 0x9;
    sarConfigHeader.ReservedA = 0xa;
    sarConfigHeader.ReservedB = 0xb;
    sarConfigHeader.ReservedC = 0xc;
    sarConfigHeader.ReservedD = 0xd;
    sarConfigHeader.ReservedE = 0xe;
    sarConfigHeader.ReservedF = 0xf;

    // The header should occupy 16 bytes in the output file.
    C_ASSERT(sizeof(SAR_CONFIG_HEADER) == 0x10);

    // Populate an example SAR_CONFIG_VALUES structure.
    SAR_CONFIG_VALUES sarConfigValues = { 0 };

    sarConfigValues.Size = sizeof(sarConfigValues);
    sarConfigValues.SARSafetyTimer = 0xabcdef01;
    sarConfigValues.SARSafetyRequestResponseTimeout = 0xbbbbbbbb;
    sarConfigValues.SARUnsolicitedUpdateTimer = 0xcccccccc;
    sarConfigValues.SARState = 0x55;
    sarConfigValues.SleepModeState = 0x44;
    sarConfigValues.SARPowerOnState = 0x33;
    sarConfigValues.SARPowerOnStateAfterFailure = 0x22;
    sarConfigValues.SARSafetyTableIndex = 0x11;
    sarConfigValues.SleepModeStateIndexTable = 0x05;

    // The values struct should occupy 26 bytes in the output file.
    C_ASSERT(sizeof(SAR_CONFIG_VALUES) == 0x13);

    // Populate a REGION_CONFIG_VALUES and SAR_POWER_TABLE (i.e. the IHV-only structs
    //  defined in Wlan_Ihv_Config.h)
    //
    REGION_CONFIG_VALUES regionConfigValues = {0};
    regionConfigValues.GeoCountryString.AsciiChars = 0x5048; // 'PH' == Philippines!
    regionConfigValues.GeoLocationValue = 0x11111111;
    regionConfigValues.DynamicGeoState = WDI_DYNAMIC_GEO_VALUE_ENABLED;
    regionConfigValues.DynamicGeoType = WDI_DYNAMIC_GEO_TYPE_DYNAMIC_THEN_STATIC;

    SAR_POWER_TABLE sarPowerTable = {0};
    for (int row = 0; row < MAX_NUM_SAR_WIFI_POWER_TABLE; row++)
    {
        for (int col = 0; col < MAX_NUM_SAR_WIFI_POWER_VALUES_PER_TABLE; col++)
        {
            // Uniquely number each entry in sarPowerTable.PowerValues.
            sarPowerTable.PowerValues[row][col] = 1 + col + (row*MAX_NUM_SAR_WIFI_POWER_VALUES_PER_TABLE);
        }
    }

    if (0 == _stricmp(path, UEFI))
    {
        WCHAR szGuid[39] = { 0 };

        if (!SUCCEEDED(SetProcessPrivilege()))
        {
            _tprintf(TEXT("Failed to add privilege to ProcessToken\r\n"));
        }

        // Write to WDI_SAR_UEFI_COMMON_PARAMS.
        //
        if (!StringFromGUID2(WDI_SAR_UEFI_COMMON_PARAMS, szGuid, ARRAYSIZE(szGuid)))
        {
            hr = E_NOT_SUFFICIENT_BUFFER;
            goto exit;
        }

        // Write SAR_CONFIG_HEADER to UEFI.
        if (!SetFirmwareEnvironmentVariable(WifiSARHeader,
                                            szGuid,
                                            &sarConfigHeader,
                                            sizeof(sarConfigHeader)))
        {
            _tprintf(TEXT("Failed to write %s to UEFI with error: - GetLastError returns:0x%08X\r\n"),
                     WifiSARHeader,
                     GetLastError());
        }

        // Write SAR_CONFIG_VALUES to UEFI.
        if (!SetFirmwareEnvironmentVariable(WifiSARConfig,
                                            szGuid,
                                            &sarConfigValues,
                                            sizeof(sarConfigValues)))
        {
            _tprintf(TEXT("Failed to write %s to UEFI with error: - GetLastError returns:0x%08X\r\n"),
                     WifiSARConfig,
                     GetLastError());
        }

        // Write to WDI_SAR_UEFI_IHV_PARAMS.
        //
        if (!StringFromGUID2(WDI_SAR_UEFI_IHV_PARAMS, szGuid, ARRAYSIZE(szGuid)))
        {
            hr = E_NOT_SUFFICIENT_BUFFER;
            goto exit;
        }

        // Write REGION_CONFIG_VALUES to UEFI.
        if (!SetFirmwareEnvironmentVariable(WifiRegionConfig,
                                            szGuid,
                                            &regionConfigValues,
                                            sizeof(regionConfigValues)))
        {
            _tprintf(TEXT("Failed to write %s to UEFI with error: - GetLastError returns:0x%08X\r\n"),
                     WifiRegionConfig,
                     GetLastError());
        }

        // Write SAR_POWER_TABLE to UEFI.
        if (!SetFirmwareEnvironmentVariable(WifiSARTable,
                                            szGuid,
                                            &sarPowerTable,
                                            sizeof(sarPowerTable)))
        {
            _tprintf(TEXT("Failed to write %s to UEFI with error: - GetLastError returns:0x%08X\r\n"),
                     WifiSARTable,
                     GetLastError());
        }
    }
    else
    {
        // Write to the appropriate file.
        char fullPath[MAX_PATH] = {0};
        sprintf_s(fullPath, sizeof(fullPath), "%s\\%ws.bin", path, WifiSARHeader);
        std::ofstream output(fullPath, std::ofstream::out | std::ofstream::binary);
        output.write((const char *)&sarConfigHeader, sizeof(sarConfigHeader));
        output.close();

        sprintf_s(fullPath, sizeof(fullPath), "%s\\%ws.bin", path, WifiSARConfig);
        output.open(fullPath, std::ofstream::out | std::ofstream::binary);
        output.write((const char *)&sarConfigValues, sizeof(sarConfigValues));
        output.close();

        sprintf_s(fullPath, sizeof(fullPath), "%s\\%ws.bin", path, WifiRegionConfig);
        output.open(fullPath, std::ofstream::out | std::ofstream::binary);
        output.write((const char *)&regionConfigValues, sizeof(regionConfigValues));
        output.close();

        sprintf_s(fullPath, sizeof(fullPath), "%s\\%ws.bin", path, WifiSARTable);
        output.open(fullPath, std::ofstream::out | std::ofstream::binary);
        output.write((const char *)&sarPowerTable, sizeof(sarPowerTable));
        output.close();
    }

exit:
    return hr;
}

HRESULT
GetConfig(
    LPSTR path
    )
/*++

Routine Description:

    Reads config from a binary PROVISION file on the local filesystem or from UEFI.

Arguments:

    path - "UEFI" or the path to the folder containing the .bin files containing provisioning info.

Return Value:

    S_OK on success or underlying failure code.

–*/
{
    HRESULT hr = S_OK;
    SAR_CONFIG_HEADER sarConfigHeader = { 0 };
    SAR_CONFIG_VALUES sarConfigValues = { 0 };
    REGION_CONFIG_VALUES regionConfigValues = {0};
    SAR_POWER_TABLE sarPowerTable = {0};

    if (0 == _stricmp(path, UEFI))
    {
        WCHAR szGuid[39] = { 0 };

        hr = SetProcessPrivilege();
        if (!SUCCEEDED(hr))
        {
            _tprintf(TEXT("Failed to add privilege to ProcessToken\r\n"));
            goto exit;
        }

        // Read from WDI_SAR_UEFI_COMMON_PARAMS.
        //
        if (!StringFromGUID2(WDI_SAR_UEFI_COMMON_PARAMS, szGuid, ARRAYSIZE(szGuid)))
        {
            hr = E_NOT_SUFFICIENT_BUFFER;
            goto exit;
        }

        // Read SAR_CONFIG_HEADER from UEFI.
        if (!GetFirmwareEnvironmentVariable(WifiSARHeader,
                                            szGuid,
                                            &sarConfigHeader,
                                            sizeof(sarConfigHeader)))
        {
            PrintLastError();
            _tprintf(TEXT("Failed to read %s from UEFI with error: - GetLastError returns:0x%08X\r\n"),
                     WifiSARHeader,
                     GetLastError());
        }

        // Read SAR_CONFIG_VALUES from UEFI.
        if (!GetFirmwareEnvironmentVariable(WifiSARConfig,
                                            szGuid,
                                            &sarConfigValues,
                                            sizeof(sarConfigValues)))
        {
            _tprintf(TEXT("Failed to read %s from UEFI with error: - GetLastError returns:0x%08X\r\n"),
                     WifiSARConfig,
                     GetLastError());
        }

        // Read from WDI_SAR_UEFI_IHV_PARAMS.
        //
        if (!StringFromGUID2(WDI_SAR_UEFI_IHV_PARAMS, szGuid, ARRAYSIZE(szGuid)))
        {
            hr = E_NOT_SUFFICIENT_BUFFER;
            goto exit;
        }


        // Read REGION_CONFIG_VALUES from UEFI.
        if (!GetFirmwareEnvironmentVariable(WifiRegionConfig,
                                            szGuid,
                                            &regionConfigValues,
                                            sizeof(regionConfigValues)))
        {
            _tprintf(TEXT("Failed to read %s from UEFI with error: - GetLastError returns:0x%08X\r\n"),
                     WifiRegionConfig,
                     GetLastError());
        }

        // Read SAR_POWER_TABLE from UEFI.
        if (!GetFirmwareEnvironmentVariable(WifiSARTable,
                                            szGuid,
                                            &sarPowerTable,
                                            sizeof(sarPowerTable)))
        {
            _tprintf(TEXT("Failed to read %s from UEFI with error: - GetLastError returns:0x%08X\r\n"),
                     WifiSARTable,
                     GetLastError());
        }
    }
    else
    {
        // The specified path is a folder.  We look for hard-coded file names that match the UEFI variable names.
        // For each file, copy the contents of the into the buffer and then copy the buffer into the struct.
        char fullPath[MAX_PATH] = {0};
        sprintf_s(fullPath, sizeof(fullPath), "%s\\%ws.bin", path, WifiSARHeader);
        std::vector<char> buffer;
        std::ifstream input(fullPath, std::ios::binary);
        std::vector<char>::iterator it;
        it = buffer.begin();
        buffer.insert(it,
                      std::istreambuf_iterator<char>(input),
                      std::istreambuf_iterator<char>());
        input.close();
        if (buffer.size() >= sizeof(sarConfigHeader))
        {
            memcpy(&sarConfigHeader, buffer.data(), sizeof(sarConfigHeader));
        }
        buffer.clear();

        // SAR_CONFIG_VALUES
        sprintf_s(fullPath, sizeof(fullPath), "%s\\%ws.bin", path, WifiSARConfig);
        input.open(fullPath, std::ios::binary);
        it = buffer.begin();
        buffer.insert(it,
                      std::istreambuf_iterator<char>(input),
                      std::istreambuf_iterator<char>());
        input.close();
        memcpy(&sarConfigValues, buffer.data(), sizeof(sarConfigValues));
        buffer.clear();

        // REGION_CONFIG_VALUES
        sprintf_s(fullPath, sizeof(fullPath), "%s\\%ws.bin", path, WifiRegionConfig);
        input.open(fullPath, std::ios::binary);
        it = buffer.begin();
        buffer.insert(it,
                      std::istreambuf_iterator<char>(input),
                      std::istreambuf_iterator<char>());
        input.close();
        memcpy(&regionConfigValues, buffer.data(), sizeof(regionConfigValues));
        buffer.clear();

        // SAR_POWER_TABLE 
        sprintf_s(fullPath, sizeof(fullPath), "%s\\%ws.bin", path, WifiSARTable);
        input.open(fullPath, std::ios::binary);
        it = buffer.begin();
        buffer.insert(it,
                      std::istreambuf_iterator<char>(input),
                      std::istreambuf_iterator<char>());
        input.close();

#define SPEW_EACH_BYTE Yeah!
#ifdef SPEW_EACH_BYTE
        wprintf(L"\nSAR_POWER_TABLE rawData \n");
        for (DWORD i = 0; i < buffer.size(); i++)
        {
            if (i % MAX_NUM_SAR_WIFI_POWER_TABLE == 0)
            {
                printf("\n");
            }
            printf("%02x ", (uint8_t)buffer[i]);
        }
        printf("\n");
#endif

        // Read SAR_POWER_TABLE (a fixed-sized PowerValues 2-D array.)
        memcpy(&sarPowerTable, buffer.data(), sizeof(sarPowerTable));
        buffer.clear();
    }

    // Print the contents of the SAR_CONFIG_HEADER.
    wprintf(L"\n\n");
    wprintf(L"Size = 0x%02x\n", sarConfigHeader.Size);
    wprintf(L"HeaderOffset1 = 0x%02x\n", sarConfigHeader.HeaderOffset1);
    wprintf(L"HeaderOffset2 = 0x%02x\n", sarConfigHeader.HeaderOffset2);
    wprintf(L"WLANTechnology = 0x%02x\n", sarConfigHeader.WLANTechnology);
    wprintf(L"ProductID = 0x%02x\n", sarConfigHeader.ProductID);
    wprintf(L"Version = 0x%02x\n", sarConfigHeader.Version);
    wprintf(L"Revision = 0x%02x\n", sarConfigHeader.Revision);
    wprintf(L"NumberSARTables = 0x%02x\n", sarConfigHeader.NumberSARTables);
    wprintf(L"SARTablesCompressed = 0x%02x\n", sarConfigHeader.SARTablesCompressed);
    wprintf(L"SARTimersFormat = 0x%02x\n", sarConfigHeader.SARTimersFormat);
    wprintf(L"ReservedA = 0x%02x\n", sarConfigHeader.ReservedA);
    wprintf(L"ReservedB = 0x%02x\n", sarConfigHeader.ReservedB);
    wprintf(L"ReservedC = 0x%02x\n", sarConfigHeader.ReservedC);
    wprintf(L"ReservedD = 0x%02x\n", sarConfigHeader.ReservedD);
    wprintf(L"ReservedE = 0x%02x\n", sarConfigHeader.ReservedE);
    wprintf(L"ReservedF = 0x%02x\n", sarConfigHeader.ReservedF);

    // Print the contents of the SAR_CONFIG_VALUES.
    wprintf(L"\nSAR_CONFIG_VALUES 1\n");
    wprintf(L"Size = 0x%02x\n", sarConfigValues.Size);
    wprintf(L"SARSafetyTimer = 0x%08x\n", sarConfigValues.SARSafetyTimer);
    wprintf(L"SARSafetyRequestResponseTimeout = 0x%08x\n", sarConfigValues.SARSafetyRequestResponseTimeout);
    wprintf(L"SARUnsolicitedUpdateTimer = 0x%08x\n", sarConfigValues.SARUnsolicitedUpdateTimer);
    wprintf(L"SARState = 0x%02x\n", sarConfigValues.SARState);
    wprintf(L"SleepModeState = 0x%02x\n", sarConfigValues.SleepModeState);
    wprintf(L"SARPowerOnState = 0x%02x\n", sarConfigValues.SARPowerOnState);
    wprintf(L"SARPowerOnStateAfterFailure = 0x%02x\n", sarConfigValues.SARPowerOnStateAfterFailure);
    wprintf(L"SARSafetyTableIndex = 0x%02x\n", sarConfigValues.SARSafetyTableIndex);
    wprintf(L"SleepModeStateIndexTable = 0x%02x\n", sarConfigValues.SleepModeStateIndexTable);

    // REGION_CONFIG_VALUES and SAR_POWER_TABLE (i.e. the IHV-only structs defined in Wlan_Ihv_Config.h)
    //
    wprintf(L"\nREGION_CONFIG_VALUES\n");
    wprintf(L"GeoCountryString.AsciiChars = 0x%04x\n", regionConfigValues.GeoCountryString.AsciiChars);
    wprintf(L"GeoLocationValue = 0x%08x\n", regionConfigValues.GeoLocationValue);
    wprintf(L"DynamicGeoState = 0x%02x\n", regionConfigValues.DynamicGeoState);
    wprintf(L"DynamicGeoType = 0x%02x\n", regionConfigValues.DynamicGeoType);

    wprintf(L"\nSAR_POWER_TABLE\n");
    for (int row = 0; row < MAX_NUM_SAR_WIFI_POWER_TABLE; row++)
    {
        for (int col = 0; col < MAX_NUM_SAR_WIFI_POWER_VALUES_PER_TABLE; col++)
        {
            wprintf(L"%6.3f", sarPowerTable.PowerValues[row][col]/8.0);
            if ((col+1) < MAX_NUM_SAR_WIFI_POWER_VALUES_PER_TABLE)
            {
                wprintf(L" - ");
            }
        }
        wprintf(L"\n");
    }

exit:
    return hr;
}

VOID
PrintGuid(
    REFGUID guid
    )
/*++

Routine Description:

    Prints the specified GUID to the screen.

Arguments:

    guid - The GUID to print to the screen.

Return Value:

    VOID

–*/
{
    WCHAR szGuid[39] = { 0 };

// Ignore compiler Warning C6031 Return value ignored
//
#pragma warning (push)
#pragma warning (disable : 6031)
    StringFromGUID2(guid, szGuid, ARRAYSIZE(szGuid));
#pragma warning (pop)

    wprintf(szGuid);

    return;
}

HRESULT
GetSetSARWiFi(
    DWORD dwOpCode,
    WDI_SAR_BACKOFF_STATE wdiSARBackoffState,
    UINT32 mimoConfigType,
    _In_ int argc,
    _In_reads_(argc) LPSTR argv[]
    )
/*++

Routine Description:

    Gets or sets the SAR configuration on the Wi-Fi radio using the WlanDeviceServiceCommand API
    available since Windows 10 version 1809 (build 17763.)

Arguments:

    dwOpCode - WDI_SET_SAR_STATE or WDI_GET_SAR_STATE
    wdiSARBackoffState - WDI_SARBACKOFF_DISABLED or WDI_SARBACKOFF_ENABLED
    mimoConfigType - Antenna selection bit mask.
    argc - Count of arguments.
    argv - Array of arguments.

Return Value:

    S_OK on success or underlying failure code.

–*/
{
    HRESULT hr = S_OK;

#if (NTDDI_WIN10_RS4 && (NTDDI_VERSION >= NTDDI_WIN10_RS4))
    HANDLE hClient = NULL;
    DWORD dwMaxClient = 2;
    DWORD dwCurVersion = 0;
    DWORD dwResult = 0;
    GUID ifaceGuid = { 0 };
    WDI_SAR_STATE * pwdiSARState;
    WDI_SAR_CONFIG_SET * pwdiSARConfig;

    GUID deviceServiceGuid = WDI_SAR_DEVICE_SERVICE;
    DWORD dwInBufferSize = 0;
    PVOID pInBuffer = nullptr;
    DWORD dwOutBuffer = { 0 };
    DWORD dwOutBufferSize = sizeof(dwOutBuffer);
    PVOID pOutBuffer = &dwOutBuffer;
    DWORD dwBytesReturned;

    int antennaPairs = argc / 2;
    int32_t antennaIndex1 = 0;
    LONG sarBackoffIndex1 = 0;
    int32_t antennaIndex2 = 0;
    int32_t sarBackoffIndex2 = 0;

    dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);
    if (dwResult != ERROR_SUCCESS)
    {
        hr = HRESULT_FROM_WIN32(dwResult);
        goto exit;
    }

    PWLAN_INTERFACE_INFO_LIST pInterfaceList;
    dwResult = WlanEnumInterfaces(hClient, nullptr, &pInterfaceList);
    if (dwResult != ERROR_SUCCESS)
    {
        hr = HRESULT_FROM_WIN32(dwResult);
        goto exit;
    }

    ifaceGuid = pInterfaceList->InterfaceInfo[pInterfaceList->dwIndex].InterfaceGuid;

#undef GET_SERVICES
#ifdef GET_SERVICES
    PWLAN_DEVICE_SERVICE_GUID_LIST pServiceGuidList = NULL;
    dwResult = WlanGetSupportedDeviceServices(hClient, &ifaceGuid, &pServiceGuidList);
    if (dwResult != ERROR_SUCCESS)
    {
        printf("WlanGetSupportedDeviceServices returned %u\r\n", dwResult);
        hr = HRESULT_FROM_WIN32(dwResult);
        goto exit;
    }

    // iterate over returned service GUID list
    for (DWORD i = 0; i < pServiceGuidList->dwNumberOfItems; i++)
    {
        PrintGuid(pServiceGuidList->DeviceService[i]);
        printf("\r\n");
    }
    WlanFreeMemory(pServiceGuidList);
#endif

    if (antennaPairs >= 1)
    {
        antennaIndex1 = strtoul(argv[0], nullptr, 16);
        sarBackoffIndex1 = atoi(argv[1]);
    }

    if (antennaPairs == 2)
    {
        antennaIndex2 = strtoul(argv[2], nullptr, 16);
        sarBackoffIndex2 = atoi(argv[3]);
    }

    if (dwOpCode == WDI_SET_SAR_STATE)
    {
        dwInBufferSize = sizeof(WDI_SAR_STATE) + 2*sizeof(WDI_SAR_CONFIG_SET);
        pwdiSARState = (WDI_SAR_STATE *)malloc(dwInBufferSize);
        if (!pwdiSARState)
        {
            hr = E_OUTOFMEMORY;
            goto exit;
        }
        memset(pwdiSARState, 0, dwInBufferSize);
        pwdiSARConfig = (WDI_SAR_CONFIG_SET *)(pwdiSARState + 1);

        pwdiSARState->SarBackoffStatus = wdiSARBackoffState;
        pwdiSARState->MIMOConfigType = mimoConfigType;
        pwdiSARState->NumWdiSarConfigElements = antennaPairs;

        pwdiSARConfig->WDI_SARAntennaIndex = antennaIndex1;
        pwdiSARConfig->WDI_SARBackOffIndex = sarBackoffIndex1;

        pwdiSARConfig++;

        pwdiSARConfig->WDI_SARAntennaIndex = antennaIndex2;
        pwdiSARConfig->WDI_SARBackOffIndex = sarBackoffIndex2;

        pInBuffer = (PVOID)pwdiSARState;
    }
    else
    {
        dwOutBufferSize = sizeof(WDI_SAR_STATE) + 2*sizeof(WDI_SAR_CONFIG_SET);
        pOutBuffer = (WDI_SAR_STATE *)malloc(dwOutBufferSize);
        if (!pOutBuffer)
        {
            hr = E_OUTOFMEMORY;
            goto exit;
        }
        memset(pOutBuffer, 0, dwOutBufferSize);
    }

#undef SPEW_EACH_BYTE
#ifdef SPEW_EACH_BYTE
    for (DWORD i = 0; i < dwInBufferSize; i++)
    {
        if (i % 8 == 0)
        {
            printf("\n");
        }
        printf("%02x ", ((UINT8*)pInBuffer)[i]);
    }
    printf("\n");
#endif

    dwResult = WlanDeviceServiceCommand(
        hClient,
        &ifaceGuid,
        &deviceServiceGuid,
        dwOpCode,
        dwInBufferSize,
        pInBuffer,
        dwOutBufferSize,
        pOutBuffer,
        &dwBytesReturned
    );

    if (dwResult != ERROR_SUCCESS)
    {
        printf("WlanDeviceServiceCommand returned %u\r\n", dwResult);
        hr = HRESULT_FROM_WIN32(dwResult);
        goto exit;
    }

    printf("WlanDeviceServiceCommand returned %u, dwOutBuffer=%u, dwBytesReturned=%u\r\n", dwResult, dwOutBuffer, dwBytesReturned);
    if (dwOpCode == WDI_GET_SAR_STATE)
    {
        if (pOutBuffer)
        {
            printf("WlanDeviceServiceCommand returned a null output buffer.u\r\n");
            hr = E_UNEXPECTED;
            goto exit;
        }

        pwdiSARState = (WDI_SAR_STATE *)pOutBuffer;
        pwdiSARConfig = (WDI_SAR_CONFIG_SET *)(pwdiSARState + 1);

        printf("WlanDeviceServiceCommand SarBackoffStatus %u, MIMOConfigType=%u, NumWdiSarConfigElements=%u\r\n",
            pwdiSARState->SarBackoffStatus,
            pwdiSARState->MIMOConfigType,
            pwdiSARState->NumWdiSarConfigElements);

        for (UINT32 i=0; i<pwdiSARState->NumWdiSarConfigElements; i++)
        {
            printf("    WDI_SARAntennaIndex %u, WDI_SARBackOffIndex=%u\r\n",
                pwdiSARConfig->WDI_SARAntennaIndex,
                pwdiSARConfig->WDI_SARBackOffIndex);
            pwdiSARConfig++;
        }
    }
    else
    {
        // Print the UINT32 result returned in pOutputBuffer.
        if (dwBytesReturned == 4)
        {
            WDI_SAR_RESULT* pwdiSARResult = (WDI_SAR_RESULT*)pOutBuffer;
            printf("WlanDeviceServiceCommand WDI_SAR_RESULT = %u\r\n",
                *pwdiSARResult);
    
        }
    }

exit:
    if (NULL != hClient)
    {
        dwResult = WlanCloseHandle(hClient, NULL);
    }
#else
    _tprintf(TEXT("\n\n--->>>> Compiled against an RS3 SDK or older - so WlanDeviceServiceCommand is not defined\n\n\n"));
#endif

    return hr;
}

HRESULT
GetSetSARLTE(
    BOOLEAN fGet,
    _In_ int argc,
    _In_reads_(argc) LPSTR argv[]
    )
/*++

Routine Description:

    Gets or sets the SAR configuration on the LTE radio using the MobileBroadbandSarManager WinRT API.

Arguments:

    fGet - TRUE if we should get the config; FALSE if we should set the config.
    argc - Count of arguments.
    argv - Array of arguments.

Return Value:

    S_OK on success or underlying failure code.

–*/
{
    HRESULT hr = S_OK;

    WINRT_RoInitialize(RO_INIT_MULTITHREADED);

    try
    {
        auto modem = MobileBroadbandModem::GetDefault();
        auto config = modem.GetCurrentConfigurationAsync().get();
        auto sarManager = config.SarManager();

        if (!sarManager)
        {
            printf("\nERROR: couldn't get valid SarManager.\n");
            hr = E_POINTER;
            goto Exit;
        }

        if (fGet)
        {
            printf("\r\n");

            if (sarManager.IsBackoffEnabled())
            {
                printf("Backoff is ENabled.\r\n");
            }
            else
            {
                printf("Backoff is DISabled\r\n.");
            }

            printf("\r\n");

            // Iterate over antennas and determine what their current config is.
            for (auto antenna : sarManager.Antennas())
            {
                printf("AntennaIndex 0x%08x configed to use BackoffIndex %u\r\n",
                    antenna.AntennaIndex(),
                    antenna.SarBackoffIndex());
            }
        }
        else
        {
            int antennaPairs = argc / 2;
            if ((antennaPairs != 1) && (antennaPairs != 2))
            {
                printf("\nERROR: invalid set of {AntennaIndex, PowerTableIndex} pairs\n");
                hr = E_INVALIDARG;
                goto Exit;
            }

            std::vector<MobileBroadbandAntennaSar> antennas;
            int32_t antennaIndex = 0;
            int32_t sarBackoffIndex = 0;
            if (antennaPairs >= 1)
            {
                printf("\n setting {AntennaIndex=%s, PowerTableIndex=%s}\n", argv[0], argv[1]);
                antennaIndex = atoi(argv[0]);
                sarBackoffIndex = atoi(argv[1]);
                antennas.insert(antennas.end(), { antennaIndex, sarBackoffIndex });
            }
            if (antennaPairs == 2)
            {
                printf("\n setting {AntennaIndex=%s, PowerTableIndex=%s}\n", argv[2], argv[3]);
                antennaIndex = atoi(argv[2]);
                sarBackoffIndex = atoi(argv[3]);
                antennas.insert(antennas.end(), { antennaIndex, sarBackoffIndex });
            }

            sarManager.SetConfigurationAsync(std::move(antennas)).get();
        }
    }
    catch (winrt::hresult_error ex)
    {
        printf("0x%08x - %ws\n",
            ex.code().value,
            ex.message().c_str());
    }

Exit:
    WINRT_RoUninitialize();

    return hr;
}

static int s_nCallbacks = 0;
VOID
DeviceServiceNotificationCallback(
    PWLAN_NOTIFICATION_DATA pdata,
    PVOID pCtxt
    )
/*++

Routine Description:

    This callback is called when an 'unsolicited notification' is sent by the WLAN (Wi-Fi)
    transmitter.  Each call is a request for updated SAR status.  A status message will be
    printed to the screen.

Arguments:

    pdata - The information specific to this call; sent by Wi-Fi transmitter.
    pCtxt - The context passed along to the callback.

Return Value:

    S_OK on success or underlying failure code.

–*/
{
#if (NTDDI_WIN10_RS5 && (NTDDI_VERSION >= NTDDI_WIN10_RS5))
    DWORD i;
    PWLAN_DEVICE_SERVICE_NOTIFICATION_DATA pNotData = NULL;
    GUID deviceServiceGuid = WDI_SAR_DEVICE_SERVICE;

    s_nCallbacks++;

    pNotData = (PWLAN_DEVICE_SERVICE_NOTIFICATION_DATA)pdata->pData;

    if (!memcmp(&deviceServiceGuid, &pNotData->DeviceService, sizeof(GUID)))
    {
        SYSTEMTIME time;

        GetSystemTime(&time);

        printf("%2.2d:%2.2d:%2.2d.%3.3d : We got SAR unsolicited request 0x%x\n", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds, *(UINT16 *)pNotData->DataBlob);

        return;
    }

    PrintGuid(pNotData->DeviceService);
    printf("\nopcode 0x%x\n", pdata->NotificationCode);
    printf("data size %d\n", pNotData->dwDataSize);

    for (i = 0; i < pNotData->dwDataSize; i++) {
        printf("0x%2.2x ", pNotData->DataBlob[i]);
    }
    printf("\n");
#else
    _tprintf(TEXT("\n\n--->>>> Compiled against an RS4 SDK or older - so WlanDeviceServiceCommand is not defined\n\n\n"));
#endif
}

INT
UnsolicitedMonitor(
    HANDLE hClient
    )
/*++

Routine Description:

    Registers for 'unsolicited notifications' sent by the WLAN (Wi-Fi) transmitter.

Arguments:

    hClient - Handle to the WLAN (Wi-Fi) subsystem.

Return Value:

    S_OK on success or underlying failure code.

–*/
{
    DWORD nReturnVal = 0;
#if (NTDDI_WIN10_RS5 && (NTDDI_VERSION >= NTDDI_WIN10_RS5))
    DWORD dwResult = 0;
    PWLAN_DEVICE_SERVICE_GUID_LIST pGuidList = NULL;
    GUID deviceServiceGuid = WDI_SAR_DEVICE_SERVICE;

    pGuidList = (PWLAN_DEVICE_SERVICE_GUID_LIST)malloc(sizeof(WLAN_DEVICE_SERVICE_GUID_LIST) + sizeof(GUID));
    if (!pGuidList)
    {
        nReturnVal = 1;
        goto exit;
    }
    pGuidList->dwNumberOfItems = 1;
    pGuidList->dwIndex = 0;
    pGuidList->DeviceService[0] = deviceServiceGuid;

    dwResult = WlanRegisterDeviceServiceNotification(hClient, pGuidList);
    if (dwResult != ERROR_SUCCESS)
    {
        printf("registration of device service GUIDs failed\n");
        nReturnVal = 1;
        goto exit;
    }

    free(pGuidList);

    dwResult = WlanRegisterNotification(hClient,
        WLAN_NOTIFICATION_SOURCE_DEVICE_SERVICE,
        FALSE,
        DeviceServiceNotificationCallback,
        NULL,
        NULL,
        NULL);
    if (dwResult != ERROR_SUCCESS)
    {
        printf("registration of notification failed\n");
        nReturnVal = 1;
        goto exit;
    }

exit:
#else
    _tprintf(TEXT("\n\n--->>>> Compiled against an RS3 SDK or older - so WlanRegisterDeviceServiceNotification is not defined\n\n\n"));
#endif

    return nReturnVal;
}

HRESULT
LteTxStatusMonitor()
/*++

Routine Description:

    This method will register with the LTE transmitter for 'unsolicited notifications' that are
    sent by the LTE transmitter to request updated SAR status.  Each time a notification is received
    during the monitoring period, a status message will be printed to the screen.

Arguments:

    VOID

Return Value:

    S_OK on success or underlying failure code.

–*/
{
    HRESULT hr = S_OK;

    winrt::init_apartment();

    try
    {
        auto modem = MobileBroadbandModem::GetDefault();
        auto config = modem.GetCurrentConfigurationAsync().get();
        auto sarManager = config.SarManager();

        if (!sarManager)
        {
            printf("\nERROR: couldn't get valid SarManager.\n");
            hr = E_POINTER;
            goto Exit;
        }

        // Register for TransmissionStateChanged event.
        //
        // TimeSpan API takes value specified as number of 100-nanosecond units.
        // SetTransmissionStateChangedHysteresisAsync accepts values in range 1-5 seconds.
        // Therefore, values can range between 10,000,000 and 50,000,000.
        winrt::Windows::Foundation::TimeSpan timeSpan(20000000);
        sarManager.SetTransmissionStateChangedHysteresisAsync(timeSpan).get();

        sarManager.TransmissionStateChanged([&](MobileBroadbandSarManager sarMgr, MobileBroadbandTransmissionStateChangedEventArgs eventArgs)
        {
            printf("TransmissionStateChanged: %s\n",
                eventArgs.IsTransmitting()? "transmitting" : "not transmitting");
        });

        sarManager.StartTransmissionStateMonitoring();

        Sleep(LteTxStatusMonitorPeriod);

        sarManager.StopTransmissionStateMonitoring();
    }
    catch (winrt::hresult_error ex)
    {
        printf("0x%08x - %ws\n",
            ex.code().value,
            ex.message().c_str());
    }

Exit:

    return hr;
}

TCHAR*
GetVersionInfo()
/*++

Routine Description:

    Constructs and returns a string containing the version number of SarTool.exe.

Arguments:

    VOID

Return Value:

    String containing the version number of SarTool.exe.

–*/
{
    TCHAR *pRet = NULL;
    UCHAR *pBuffer = NULL;

    TCHAR szModule[MAX_PATH];

    if (!GetModuleFileName(NULL, szModule, sizeof(szModule)/sizeof(TCHAR)))
    {
        goto exit;
    }

    DWORD dwArg, dwInfoSize;

    dwInfoSize = GetFileVersionInfoSize(szModule, &dwArg);
    if (!dwInfoSize)
    {
        goto exit;
    }

    pBuffer = (UCHAR*) malloc(dwInfoSize);
    if (pBuffer == NULL)
    {
        goto exit;
    }

    if (!GetFileVersionInfo(szModule, 0, dwInfoSize, pBuffer))
    {
        goto exit;
    }

    VS_FIXEDFILEINFO *vInfo;
    UINT uInfoSize;

    if (!VerQueryValue(pBuffer, TEXT("\\"), (LPVOID*)&vInfo, &uInfoSize))
    {
        goto exit;
    }

    pRet = (TCHAR*)malloc(128 * sizeof(TCHAR));
    if (!pRet)
    {
        goto exit;
    }

    _stprintf_s(pRet, 128, TEXT("%hu.%hu.%hu.%hu"),
        HIWORD(vInfo->dwFileVersionMS),
        LOWORD(vInfo->dwFileVersionMS),
        HIWORD(vInfo->dwFileVersionLS),
        LOWORD(vInfo->dwFileVersionLS));

exit:
    if (pBuffer != NULL)
    {
        free(pBuffer);
        pBuffer = NULL;
    }
    return pRet;
}

VOID
PrintUsage(
    _In_ PCSTR exeName
    )
/*++

Routine Description:

    Prints help text to the screen so the user can decide which command-line parameters to specify.

Arguments:

    exeName - The name of the executable: "SarTool.exe".

Return Value:

    VOID

–*/
{
    TCHAR* pVer = GetVersionInfo();
    printf("\n\n %s version %ws\n\n",
        exeName,
        pVer);
    free(pVer);

    printf("\n\n------------------------------------------------------------\n\n");

    printf("Usage: %s getconfig {UEFI | <path>}\n  The getconfig command reads configuration from UEFI using GetFirmwareEnvironmentVariable or a binary file.",
        exeName);

    printf("\n\n------------------------------------------------------------\n\n");

    printf("Usage: %s setconfig {UEFI | <path>}\n  The setconfig command writes configuration to a binary file.",
        exeName);

    printf("\n\n------------------------------------------------------------\n\n");

    printf("Usage: %s getsar {WiFi | LTE}\n  The getsar command uses the WlanDeviceServiceCommand or MobileBroadbandSarManager API to get the current configuration.",
        exeName);

    printf("\n\n------------------------------------------------------------\n\n");

    printf("Usage:\n%s setsar LTE {AntennaIndex1 PowerTableIndex1} {AntennaIndex2 PowerTableIndex2} ...\t\t--or--\n%s setsar WiFi {on | off} {MIMO config} {AntennaIndex1 PowerTableIndex1} {AntennaIndex2 PowerTableIndex2} ...\n  The setsar command uses the WlanDeviceServiceCommand or MobileBroadbandSarManager API to set a new configuration.",
        exeName,
        exeName);

    printf("\n\n------------------------------------------------------------\n\n");

    printf("Usage: %s unsolMon {WiFi | LTE}\n  The unsolMon command registers for 'unsolicited notifications' sent by the transmitter to request updated SAR status.",
        exeName);

    printf("\n\n------------------------------------------------------------\n\n");
}

int
_cdecl
main(
    _In_ int argc,
    _In_reads_(argc) LPSTR  *argv
    )
/*++

Routine Description:

    Process command-line and call corresponding function.

Arguments:

    argc - Count of arguments.
    argv - Array of arguments.

Return Value:

    0 on success
    non-zero to indicate a failure

–*/
{
    HRESULT hr = S_OK;
    int nReturnVal = 1;

    if (argc < 2)
    {
        PrintUsage(argv[0]);
        hr = E_INVALIDARG;
        goto Exit;
    }

    // verify arg is "getconfig" or "getConfig" or "GeTcONfIG", etc.
    if (0 == _stricmp(argv[1], CMD_GETCONFIG))
    {
        if (argc < 3)
        {
            PrintUsage(argv[0]);
            hr = E_INVALIDARG;
            goto Exit;
        }

        hr = GetConfig(argv[2]);
    }
    else if (0 == _stricmp(argv[1], CMD_SETCONFIG))
    {
        if (argc < 3)
        {
            PrintUsage(argv[0]);
            hr = E_INVALIDARG;
            goto Exit;
        }

        hr = SetConfig(argv[2]);
    }
    else if (0 == _stricmp(argv[1], CMD_GETSAR))
    {
        BOOL fLte = FALSE;

        if (argc < 3)
        {
            PrintUsage(argv[0]);
            hr = E_INVALIDARG;
            goto Exit;
        }

        // verify 1st arg is "Lte" or "LTE" or "lte" or "wifi" or "WIFI", etc.
        if (0 == _stricmp(argv[2], "lte"))
        {
            fLte = TRUE;
        }
        else if (0 == _stricmp(argv[2], "wifi"))
        {
            fLte = FALSE;
        }
        else
        {
            PrintUsage(argv[0]);
            hr = E_INVALIDARG;
            goto Exit;
        }

        if (TRUE == fLte)
        {
            hr = GetSetSARLTE(TRUE,
                              argc-3,
                              &argv[3]);
        }
        else
        {
            hr = GetSetSARWiFi(WDI_GET_SAR_STATE,
                               WDI_SARBACKOFF_ENABLED,
                               0,
                               argc - 4,
                               &argv[4]);
        }
    }
    else if (0 == _stricmp(argv[1], CMD_SETSAR))
    {
        BOOL fOn = FALSE;
        BOOL fLte = FALSE;

        if (argc < 4)
        {
            PrintUsage(argv[0]);
            hr = E_INVALIDARG;
            goto Exit;
        }

        // verify 1st arg is "Lte" or "LTE" or "lte" or "wifi" or "WIFI", etc.
        if (0 == _stricmp(argv[2], "lte"))
        {
            fLte = TRUE;
        }
        else if (0 == _stricmp(argv[2], "wifi"))
        {
            fLte = FALSE;
        }
        else
        {
            PrintUsage(argv[0]);
            hr = E_INVALIDARG;
            goto Exit;
        }

        if (TRUE == fLte)
        {
            hr = GetSetSARLTE(FALSE,
                              argc - 3,
                              &argv[3]);
        }
        else
        {
            int argCount = argc - 4;
            LPSTR* argList = &argv[4];

            // verify 2nd arg is "On" or "on" or "oFf" or "OFF", etc.
            if (0 == _stricmp(argv[3], "on"))
            {
                fOn = TRUE;
            }
            else if (0 == _stricmp(argv[3], "off"))
            {
                fOn = FALSE;
            }
            else
            {
                PrintUsage(argv[0]);
                hr = E_INVALIDARG;
                goto Exit;
            }

            // verify 3rd arg is numerical MIMO config value
            UINT32 mimoConfigType = 0x0;
            if (fOn)
            {
                if (argc < 7)
                {
                    PrintUsage(argv[0]);
                    hr = E_INVALIDARG;
                    goto Exit;
                }
                _snscanf_s(argv[4], 5, "0x%x", &mimoConfigType);
                printf("mimoConfigType = %u\n", mimoConfigType);
                argCount--;
                argList = &argv[5];
            }

            hr = GetSetSARWiFi(WDI_SET_SAR_STATE,
                               fOn ? WDI_SARBACKOFF_ENABLED : WDI_SARBACKOFF_DISABLED,
                               mimoConfigType,
                               argCount,
                               argList);
        }
    }
    else if (0 == _stricmp(argv[1], CMD_UNSOLMON))
    {
        BOOL fLte = FALSE;

        if (argc < 3)
        {
            PrintUsage(argv[0]);
            hr = E_INVALIDARG;
            goto Exit;
        }

        // verify next arg is "Lte" or "LTE" or "lte" or "wifi" or "WIFI", etc.
        if (0 == _stricmp(argv[2], "lte"))
        {
            fLte = TRUE;
        }
        else if (0 == _stricmp(argv[2], "wifi"))
        {
            fLte = FALSE;
        }
        else
        {
            PrintUsage(argv[0]);
            hr = E_INVALIDARG;
            goto Exit;
        }

        if (TRUE == fLte)
        {
            hr = LteTxStatusMonitor();
        }
        else
        {
            DWORD dwMaxClient = 2;
            DWORD dwCurVersion = 0;
            HANDLE hClient;
            DWORD dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);
            if (dwResult != ERROR_SUCCESS)
            {
                printf("opening handle failed\n");
                hr = HRESULT_FROM_WIN32(dwResult);
                goto Exit;
            }

            nReturnVal = UnsolicitedMonitor(hClient);

            if (nReturnVal == ERROR_SUCCESS)
            {
                boolean bWait = true;
                while ((bWait) && (s_nCallbacks < 128))
                {
                    Sleep(5000);
                }
                printf("called back %d times\n",
                       s_nCallbacks);
            }
            else
            {
                printf("error registering for DeviceServiceNotifications\n");
                hr = E_FAIL;
            }
            WlanCloseHandle(hClient, NULL);
        }
    }
    else
    {
        PrintUsage(argv[0]);
        hr = E_INVALIDARG;
        goto Exit;
    }

Exit:

    if (hr == S_OK)
    {
        nReturnVal = 0;
    }

    return nReturnVal;
}

// eof: SarTool.cpp
//
