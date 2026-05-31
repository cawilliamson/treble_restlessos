package vendor.xiaomi.hw.touchfeature;

@VintfStability
interface ITouchFeature {
int get_mode_cur_value(int touchId, int mode);
int get_mode_def_value(int touchId, int mode);
int get_mode_max_value(int touchId, int mode);
int get_mode_min_value(int touchId, int mode);
int[] get_modes_values(int touchId, int mode);
void get_touch_event();
int mode_reset(int touchId, int mode); //also called resetTouchMode
int set_mode_edge_value(int touchId, int mode, int length, in int[] arr);
int set_mode_value(int touchid, int mode, int value);
String get_mode_cur_value_string(int touchid, int mode);
String getmodewhitelist(int touchId, in int[] mode);
void register();
void unregister();
void set_mode_packagename(int touchid, int mode, String packagename);
}
