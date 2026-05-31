///////////////////////////////////////////////////////////////////////////////
// THIS FILE IS IMMUTABLE. DO NOT EDIT IN ANY CASE.                          //
///////////////////////////////////////////////////////////////////////////////

// This file is a snapshot of an AIDL file. Do not edit it manually. There are
// two cases:
// 1). this is a frozen version file - do not edit this in any case.
// 2). this is a 'current' file. If you make a backwards compatible change to
//     the interface (from the latest frozen version), the build system will
//     prompt you to update this file with `m <name>-update-api`.
//
// You must not make a backward incompatible change to any AIDL file built
// with the aidl_interface module type with versions property set. The module
// type is used to build AIDL files in a way that they can be used across
// independently updatable components of the system. If a device is shipped
// with such a backward incompatible change, it has a high risk of breaking
// later when a module using the interface is updated, e.g., Mainline modules.

package vendor.samsung.hardware.radio.network;
@VintfStability
interface ISehRadioNetworkResponse {
  oneway void emergencyControlResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info);
  oneway void emergencySearchResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info, int respEmergenySearch);
  oneway void getAvailableNetworksResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info, in vendor.samsung.hardware.radio.network.SehOperatorInfo[] networksInfo);
  oneway void getCnapResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info, int m);
  oneway void getCsgListResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info, in vendor.samsung.hardware.radio.network.SehCsgInfo[] csgInfos);
  oneway void getDisable2gResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info, int isDisable);
  oneway void getNrIconTypeResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info, int nrIconType);
  oneway void getNrModeResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info, int nrMode);
  oneway void getPreferredNetworkListResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info, in vendor.samsung.hardware.radio.network.SehPreferredNetworkInfo[] infos);
  oneway void getRoamingNetworkInfoViaBLEResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info, in vendor.samsung.hardware.radio.network.SehRoamingNetworkInfo[] networkInfo);
  oneway void getVendorSpecificConfigurationResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info, in vendor.samsung.hardware.radio.network.SehVendorConfiguration configurations);
  oneway void selectCsgManualResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info);
  oneway void sendEncodedUssdResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info);
  oneway void sendRequestRawResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info, in byte[] data);
  oneway void sendRequestStringsResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info, in String[] data);
  oneway void setCurrentNetworkInfoViaBLEResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info);
  oneway void setDisable2gResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info);
  oneway void setImsCallListResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info);
  oneway void setNrModeResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info);
  oneway void setPreferredNetworkListResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info);
  oneway void setRoamingNetworkInfoViaBLEResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info);
  oneway void setScanResultViaBLEResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info);
  oneway void setVendorSpecificConfigurationResponse(in vendor.samsung.hardware.radio.network.SehRadioResponseInfo info);
}
