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
interface ISehRadioNetwork {
  oneway void emergencyControl();
  oneway void emergencySearch();
  oneway void getAvailableNetworks();
  oneway void getCnap();
  oneway void getCsgList();
  oneway void getDisable2g();
  oneway void getNrIconType();
  oneway void getNrMode();
  oneway void getPreferredNetworkList();
  oneway void getRoamingNetworkInfoViaBLE();
  oneway void getVendorSpecificConfiguration();
  oneway void selectCsgManual();
  oneway void sendEncodedUssd();
  oneway void sendRequestRaw();
  oneway void sendRequestStrings();
  oneway void setCurrentNetworkInfoViaBLE();
  oneway void setDisable2g();
  oneway void setImsCallList();
  oneway void setNrMode(int serial, int mode, boolean force);
  oneway void setPreferredNetworkList();
  oneway void setResponseFunctions(in vendor.samsung.hardware.radio.network.ISehRadioNetworkResponse response, in vendor.samsung.hardware.radio.network.ISehRadioNetworkIndication indication);
  oneway void setRoamingNetworkInfoViaBLE();
  oneway void setScanResultViaBLE();
  oneway void setVendorSpecificConfiguration(int serial, in vendor.samsung.hardware.radio.network.SehVendorConfiguration[] configurations);
}
