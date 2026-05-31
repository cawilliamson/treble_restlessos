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

package vendor.xiaomi.hw.touchfeature;
@VintfStability
interface ITouchFeature {
  int get_mode_cur_value(int touchId, int mode);
  int get_mode_def_value(int touchId, int mode);
  int get_mode_max_value(int touchId, int mode);
  int get_mode_min_value(int touchId, int mode);
  int[] get_modes_values(int touchId, int mode);
  void get_touch_event();
  int mode_reset(int touchId, int mode);
  int set_mode_edge_value(int touchId, int mode, int length, in int[] arr);
  int set_mode_value(int touchid, int mode, int value);
  String get_mode_cur_value_string(int touchid, int mode);
  String getmodewhitelist(int touchId, in int[] mode);
  void register();
  void unregister();
  void set_mode_packagename(int touchid, int mode, String packagename);
}
