package vendor.samsung.hardware.sysinput;

@VintfStability
interface ISehSysInputCallback {
    void onReportInformation(int type, in String data);
    void onReportRawData(int type, int count, in int[] data);
}
