Here are few things I've found while reverse-engineering fingerprint code.

Everything seems to boil down to calls to
vendor.oneplus.fingerprint.extension@1.0.updateStatus()

The value set is output in stock ROM with FingerprintService logcat TAG:
12-27 12:24:07.631   907  1509 D FingerprintService: updateStatus , 8

In real life firmware, the following values have been found:
- 3
- 4
- 8
- 9

Here are few places, and values found in the code:

- services-decompile/sources/com/android/server/fingerprint/FingerprintService.java:
startClient can set status to 3 if client == keyguard, else to 4
It also centralizes statuses (cf next entry).
This indicates some extra values: 8 or 9, though it doesn't set those.
8 and 9 seem to be specific to enrolling

- OPSystemUI/sources/com/android/keyguard/KeyguardUpdateMonitor.java
the mPocketListener can set status to 1 or 2 (probably to suspend the sensor when the phone is in pocket), or event 0

When reading stock ROM logcat, it seems this is the behaviour:
Authentication by SystemUI (== keyguard) sets value to 3
End of auth set values to 4
Settings seem to set status to 8 BEFORE starting enrollment, then when enrolling is called, FingerprintService calls updateStatus to 4
When enrollment is paused, status is set to 9, then back to 8 to resume
