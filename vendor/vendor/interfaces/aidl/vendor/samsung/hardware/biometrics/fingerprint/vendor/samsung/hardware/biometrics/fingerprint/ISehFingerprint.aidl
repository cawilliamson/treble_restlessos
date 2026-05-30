package vendor.samsung.hardware.biometrics.fingerprint;

import vendor.samsung.hardware.biometrics.fingerprint.SehResult;

/* Commands:
- 6 getSensorStatus
- 11 isTemplateDbCorrupted
- 12 getUserIdList
- 15 turn on gesture mode
- 16 turn off gesture mode
- 17 {0 screen-off, 1 screen-on}
- 19 getSensorTestResult
- 21 force CBGE
- 34 handleQcomFocreQDB
- 37 SemFpScreenStatusNotifier.start()
- 36 SemFpScreenStatusNotifier.notifyScreenStatus() 2 = interactive ; 0 not interactive
- 40 getOpticalCalibrationTime
- 10000 getTrustAppVersion
*/

@VintfStability
interface ISehFingerprint {
    SehResult sehRequest(in int sensorId, in int command, in int param, in byte[] inputbuffer);
}
