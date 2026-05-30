package vendor.mediatek.hardware.agolddaemon;

import vendor.mediatek.hardware.agolddaemon.IAgoldDaemonCallback;

@VintfStability
interface IAgoldDaemon {
void CommonGetResult(IAgoldDaemonCallback callback, int i);
void GetGasData(IAgoldDaemonCallback callback);
int SendMessageToIoctl(int a, int b, int c, int d);
void SmartpaGetResult(IAgoldDaemonCallback cb);
boolean StartGasSensor(int a);
void WriteIntDataToIoCtrl(in byte[] a, int b);
void WriteStringDataToIoCtrl(String a, int b);
boolean checkGoogleKey();
boolean checkTeeKey();
void cmd(String a, IAgoldDaemonCallback cb);
String getCameraClientPackageName();
String getNotGsi();
String readSysFile(String path);
void setCameraClientPackageName(String pkgName);
void setNotGsi(String flag);
boolean smartpaCalibration();
}
