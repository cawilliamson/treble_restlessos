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

package vendor.samsung.hardware.sysinput;
@VintfStability
interface ISehSysInputDev {
  int[] getDeviceList(boolean force);
  int registerCallback(vendor.samsung.hardware.sysinput.ISehSysInputCallback callback);
  int streamRawdata(int type, int mode);
  int injectRawdata(int type, in int[] inputData, int inputSize);
  int activate(int type, int enable, boolean isBefore);
  void runCommand(int type, String cmdname, out vendor.samsung.hardware.sysinput.SehIntStringParcel outbuf);
  int setProperty(int type, int property, String vaule);
  String getProperty(int type, int property);
  void getKeyState(int keycode, out vendor.samsung.hardware.sysinput.SehIntStringParcel outbuf);
  void readTaas(out vendor.samsung.hardware.sysinput.SehIntStringParcel outbuf);
  int writeTaas(String buf);
}
