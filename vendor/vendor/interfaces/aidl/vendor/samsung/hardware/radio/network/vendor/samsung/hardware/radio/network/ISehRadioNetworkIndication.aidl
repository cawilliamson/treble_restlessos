package vendor.samsung.hardware.radio.network;

import vendor.samsung.hardware.radio.network.SehCallDetails;
import vendor.samsung.hardware.radio.network.SehEriInfo;
import vendor.samsung.hardware.radio.network.SehExtendedRegStateResult;
import vendor.samsung.hardware.radio.network.SehSignalBar;
import vendor.samsung.hardware.radio.network.SehVendorConfiguration;

@VintfStability
interface ISehRadioNetworkIndication {
    oneway void acbInfo(int type, in int[] acbInfo);
    oneway void callDetailsChanged(int type, in SehCallDetails[] callDetails);
    oneway void csFallback(int type, int state);
    oneway void currentNetworkScanIsrequested(int type, byte mode);
    oneway void eriInfoReceived(int type, in SehEriInfo eriInfo);
    oneway void execute(int type, String cmd);
    oneway void extendedRegistrationState(int type, in SehExtendedRegStateResult state);
    oneway void imsPreferenceChanged(int type, in int[] imsPref);
    oneway void needTurnOnRadioIndication(int type);
    oneway void nrBearerAllocationChanged(int type, int status);
    oneway void nrIconTypeChanged(int type, int nrIconType);
    oneway void nrNetworkTypeAdded(int type, int nrNetworkType);
    oneway void roamingNetworkScanIsRequested(int type, in byte[] scanData);
    oneway void signalLevelInfoChanged(int type, in SehSignalBar signalBarInfo);
    oneway void vendorConfigurationChanged(int type, in SehVendorConfiguration[] configurations);
}
