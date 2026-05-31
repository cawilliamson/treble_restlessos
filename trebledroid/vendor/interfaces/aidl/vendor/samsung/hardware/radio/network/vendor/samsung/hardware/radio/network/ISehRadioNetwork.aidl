package vendor.samsung.hardware.radio.network;

import vendor.samsung.hardware.radio.network.ISehRadioNetworkResponse;
import vendor.samsung.hardware.radio.network.ISehRadioNetworkIndication;
import vendor.samsung.hardware.radio.network.SehVendorConfiguration;

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
    oneway void setResponseFunctions(in ISehRadioNetworkResponse response, in ISehRadioNetworkIndication indication);
    oneway void setRoamingNetworkInfoViaBLE();
    oneway void setScanResultViaBLE();
    oneway void setVendorSpecificConfiguration(int serial, in SehVendorConfiguration[] configurations);
}
