package vendor.samsung.hardware.radio.network;

import vendor.samsung.hardware.radio.network.SehCallDetails;
import vendor.samsung.hardware.radio.network.SehEriInfo;
import vendor.samsung.hardware.radio.network.SehExtendedRegStateResult;
import vendor.samsung.hardware.radio.network.SehSignalBar;
import vendor.samsung.hardware.radio.network.SehVendorConfiguration;
import vendor.samsung.hardware.radio.network.SehRadioResponseInfo;
import vendor.samsung.hardware.radio.network.SehOperatorInfo;
import vendor.samsung.hardware.radio.network.SehCsgInfo;
import vendor.samsung.hardware.radio.network.SehPreferredNetworkInfo;
import vendor.samsung.hardware.radio.network.SehRoamingNetworkInfo;

@VintfStability
interface ISehRadioNetworkResponse {
    oneway void emergencyControlResponse(in SehRadioResponseInfo info);
    oneway void emergencySearchResponse(in SehRadioResponseInfo info, int respEmergenySearch);
    oneway void getAvailableNetworksResponse(in SehRadioResponseInfo info, in SehOperatorInfo[] networksInfo);
    oneway void getCnapResponse(in SehRadioResponseInfo info, int m);
    oneway void getCsgListResponse(in SehRadioResponseInfo info, in SehCsgInfo[] csgInfos);
    oneway void getDisable2gResponse(in SehRadioResponseInfo info, int isDisable);
    oneway void getNrIconTypeResponse(in SehRadioResponseInfo info, int nrIconType);
    oneway void getNrModeResponse(in SehRadioResponseInfo info, int nrMode);
    oneway void getPreferredNetworkListResponse(in SehRadioResponseInfo info, in SehPreferredNetworkInfo[] infos);
    oneway void getRoamingNetworkInfoViaBLEResponse(in SehRadioResponseInfo info, in SehRoamingNetworkInfo[] networkInfo);
    oneway void getVendorSpecificConfigurationResponse(in SehRadioResponseInfo info, in SehVendorConfiguration configurations);
    oneway void selectCsgManualResponse(in SehRadioResponseInfo info);
    oneway void sendEncodedUssdResponse(in SehRadioResponseInfo info);
    oneway void sendRequestRawResponse(in SehRadioResponseInfo info, in byte[] data);
    oneway void sendRequestStringsResponse(in SehRadioResponseInfo info, in String[] data);
    oneway void setCurrentNetworkInfoViaBLEResponse(in SehRadioResponseInfo info);
    oneway void setDisable2gResponse(in SehRadioResponseInfo info);
    oneway void setImsCallListResponse(in SehRadioResponseInfo info);
    oneway void setNrModeResponse(in SehRadioResponseInfo info);
    oneway void setPreferredNetworkListResponse(in SehRadioResponseInfo info);
    oneway void setRoamingNetworkInfoViaBLEResponse(in SehRadioResponseInfo info);
    oneway void setScanResultViaBLEResponse(in SehRadioResponseInfo info);
    oneway void setVendorSpecificConfigurationResponse(in SehRadioResponseInfo info);

}
