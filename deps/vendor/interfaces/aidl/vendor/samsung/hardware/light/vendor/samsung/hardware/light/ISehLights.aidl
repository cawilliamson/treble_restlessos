package vendor.samsung.hardware.light;

import android.hardware.light.HwLightState;
import vendor.samsung.hardware.light.SehHwLight;

@VintfStability
interface ISehLights {
    void setLightState(in int id, in HwLightState i2, int nits);
    SehHwLight[] getLights();
}

