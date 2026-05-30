package vendor.samsung.hardware.sysinput;

import vendor.samsung.hardware.sysinput.ISehSysInputCallback;
import vendor.samsung.hardware.sysinput.SehIntStringParcel;

/*
Type can be:
- 0 CURRENT_TSP
- 1 DEFAULT_TSP
- 2 EXTRA_TSP/TSP_SUB
- 11 SPEN
- 21 KEY
- 31 KEYBOARD
- 41 TAAS
- 100 UNSPECIFIED
*/

/*
Commands (setProperty?):
- 0 NONE
- 1 GAME
- 2 SCAN_RATE
- 3 REFRESH_RATE
- 4 GLOVE
- 5 CLEAR_COVER
- 6 ORIENTATION
- 7 PROX_LP_SCAN
- 8 GRIP_DATA
- 9 SIP
- 10 NOTE_APP
- 11 TEMPERATURE
- 12 SPAY
- 13 STYLUS
- 14 BRUSH
- 15 AOD_RECT
- 16 AOD
- 17 FOD // if mode == 1 mode,pressFast,strictMode ; else mode
- 18 FOD_ICON_VISIBLE
- 19 FOD_RECT
- 20 FOD_LP
- 21 SINGLETAP
- 22 EAR_DETECT
- 23 EXTERNAL_NOISE
- 24 TOUCHABLE_AREA
- 25 FP_INT_CONTROL
- 26 SYNC_CHANGED
- 27 POCKET_MODE
- 28 LOW_SENSITIVITY
- 29 CHARGER
- 30 AOT
- 31 FOLD_STATE
- 32 WIRELESS_CHARGER
- 33 TWO_FINGER_DOUBLETAP
- 34 SPEN_COVER_TYPE
- 35 SPEN_SAVING_MODE
- 36 SPEN_POWER
- 37 SPEN_BLE_CHARGING
- 38 SPEN_SCREEN_OFF_MEMO
- 39 SPEN_PDCT_LOWSENSITIVITY
- 40 SPEN_LOWCURRENT
*/

/*
Properties:
- 0 NONE
- 1 feature
- 2 cmd_list
- 3 scrub_pos
- 4 fod_info
- 5 fod_pos
- 6 aod_active_area
- 7 aod_enable
- 8 epen_pos
- 9 prox_off
- 10 hw_param
- 11 lp_dump
- 12 ble_charging
- 13 epen_saving
- 14 epen_memo
- 15 hand_edge
- 16 epen_wcharging
- 17 enabled
- 18 cmd
*/

@VintfStability
interface ISehSysInputDev {
    int[] getDeviceList(boolean force);
    int registerCallback(ISehSysInputCallback callback);
    int streamRawdata(int type, int mode);
    int injectRawdata(int type, in int[] inputData, int inputSize);
    int activate(int type, int enable, boolean isBefore);
    void runCommand(int type, String cmdname, out SehIntStringParcel outbuf);
    int setProperty(int type, int property, String vaule /* sic */);
    String getProperty(int type, int property);
    void getKeyState(int keycode, out SehIntStringParcel outbuf);
    void readTaas(out SehIntStringParcel outbuf);
    int writeTaas(String buf);
}
