# SarTool
SarTool uses public Windows API's to control SAR state in WLAN (Wi-Fi) and LTE subsystems.

## Build the Project
- [ ] install WDK version 1809 or later<br>
**-OR-**
- [ ] use an EWDK to LaunchBuildEnv.bat and SetupVSEnv
- [ ] open Visual Studio from that cmd.exe
 >**NOTE:** If building in Visual Studio does not work (it's not yet fully supported from EWDK), use a command line like the following:
  msbuild /t:rebuild SarTool.sln /p:configuration=debug /p:platform=arm64 /property:WindowsTargetPlatformVersion=%Version_Number%

## Example Commands
`sartool getsar wifi`<br>
`sartool setsar wifi off`<br>
`sartool setsar WiFi on 0x3 0xff 2`<br>

## Files
| File      |    Contents  |
| :-------- | :----------- |
| Dmf_Wlan_Public.h | contains struct and value definitions shared between SurfaceSarManager.dll and an IHV’s WLAN driver |
| Wlan_Ihv_Config.h | contains struct and value definitions provisioned by the OEM but otherwise only read by the IHV’s WLAN driver |

## UEFI GUID and variable names
<table>
  <tr>
    <td><b>GUID</b></td>
    <td><b>struct</b></td>
    <td><b>UEFI variable name</b></td>
  </tr>
  <tr>
    <td colspan="3">WDI_SAR_UEFI_COMMON_PARAMS (see Wlan_Ihv_Config.h)</td>
  </tr>
  <tr>
    <td/>
    <td>SAR_CONFIG_HEADER</td>
    <td>WifiSARHeader.bin</td>
  </tr>
  <tr>
    <td/>
    <td>SAR_CONFIG_VALUES</td>
    <td>WifiSARConfig.bin</td>
  </tr>
  <tr>
    <td colspan="3">WDI_SAR_UEFI_IHV_PARAMS (see Wlan_Ihv_Config.h)</td>
  </tr>
  <tr>
    <td/>
    <td>REGION_CONFIG_VALUES</td>
    <td>WifiRegionConfig.bin</td>
  </tr>
  <tr>
    <td/>
    <td>SAR_POWER_TABLE</td>
    <td>WifiSARTable.bin</td>
  </tr>
</table>

## See also
The document that describes the format of SarTool.exe payloads is WLAN_SAR_IHV_DOC.
Inside the document a Revision History table indicates the version (e.g. 1.3).<br>
The version numbers in Wlan_Ihv_Config.h and Dmf_Wlan_Public.h will kept in sync with the document’s version (revisions of a given version should not alter struct formats or contain other “material” changes.)<br>

## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us the rights to use your contribution. For details, visit https://cla.microsoft.com.

When you submit a pull request, a CLA-bot will automatically determine whether you need to provide a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the instructions provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
