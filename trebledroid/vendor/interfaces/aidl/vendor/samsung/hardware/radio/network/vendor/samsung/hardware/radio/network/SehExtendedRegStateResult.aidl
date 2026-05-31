package vendor.samsung.hardware.radio.network;

@VintfStability
parcelable SehExtendedRegStateResult {
    int unprocessedDataRegState;
    int unprocessedVoiceRegState;
    boolean isValid;
    int snapshotStatus;
    int unprocessedDataRat;
    int mobileOptionalRat;
    int imsEmergencyCallBarring;
    boolean isPsOnlyReg;
}
