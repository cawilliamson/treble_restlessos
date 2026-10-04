package vendor.samsung.hardware.wifi;

@VintfStability
interface ISehWifi {
    void writeFile(byte b, String str);
    String readFile(byte b);
    void setProperty(byte b, String str);
    String getProperty(byte b);
    void updateFile(byte b);
    void removeFile(byte b);
    void removeLogFiles();
    String getChipsetVendorName();
}
