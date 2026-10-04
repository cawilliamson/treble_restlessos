package vendor.samsung.hardware.radio.network;

import vendor.samsung.hardware.radio.network.SehArfcnInfo;

@VintfStability
parcelable SehRoamingNetworkInfo {
byte[] plmn;
byte rat;
byte regState;
byte opResult;
byte[] date;
SehArfcnInfo[] arfcnInfo;
}
