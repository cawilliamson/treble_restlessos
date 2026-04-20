# RestlessOS Issue Fix Plan

Generated: 2026-04-20
Based on analysis of 13 open GitHub issues.

Issues grouped by root cause. Each fix includes: target patch tier, target
repo, approach, and verification steps. Patches MUST follow the android-patch
skill workflow -- never hand-edit `.patch` files.

---

## Fix 1: SELinux xperms on kernels lacking ioctl extended-permission support

**Issues:** #29, #37
**Severity:** BLOCKING (devices unbootable)
**Root cause:** secilc fails when compiled policy contains AVTAB_XPERMS entries
but the kernel cannot load them. Current patch caps policy version 30→29 for
kernels <4.9, but secilc -c 29 still emits xperms in some code paths, and
kernels between 4.9 and ~5.4 may lack the actual xperms backport despite
reporting version ≥4.9.

### 1a: Fix secilc -c 29 not stripping xperms

Current patch `trebledroid-staging/platform_system_core/0005` caps the `-c`
version argument to 29, but secilc at version 29 still writes xperms entries
if the CIL input contains `allowxperm`/`auditallowxperm`/`dontauditxperm`
statements. The fix: **strip xperms CIL statements before compilation** on
affected kernels.

**Target repo:** `platform/system/core`
**Patch tier:** `trebledroid-staging`
**Approach:**
- In `OpenSplitPolicy()` (selinux.cpp), after capping the version to 29,
  add a step that filters CIL files to remove lines matching
  `(allow|auditallow|dontaudit)xperm` before passing them to secilc.
- Use a temporary directory for filtered CIL files.
- Log the number of stripped xperms rules for debugging.

### 1b: Extend xperms detection to cover broken 4.9–5.4 kernels

4.14 vendor kernels (issue #37) don't get capped because `KernelOlderThan(4,
9)` returns false, but they still lack xperms support. The version check alone
is insufficient.

**Target repo:** `platform/system/core`
**Patch tier:** `trebledroid-staging`
**Approach:**
- Add a runtime probe: attempt to load a minimal xperms policy into the
  kernel and check for EINVAL. If it fails, set a flag and apply the same
  version cap + xperms stripping.
- Alternative (simpler): add a `ro.boot.selinux_cap_policy_version` property
  that can be set from `rw-system.sh` based on device detection. When set,
  override the auto-detection.
- Fallback: extend `KernelOlderThan` to cover known-bad kernel ranges. A
  conservative approach: cap for any kernel <5.4 unless
  `ro.boot.selinux_xperms_supported=1` is explicitly set.

### 1c: Ensure plat_sepolicy_genfs_202404.cil is present

The nubia NX563J log shows: `Missing
/system/etc/selinux/plat_sepolicy_genfs_202404.cil; skipping`. Commit
`fe9b7c3` shipped this file, but it may not be included in the GSI build if
the build system doesn't pick it up from the sepolicy directory.

**Action:** Verify the file exists in the built image. If missing, add an
explicit install rule in the sepolicy build configuration.

### Verification

- Test on nubia NX563J (4.4.302): boot should succeed, `dmesg` should show
  "capping policy version" and "stripped N xperms rules"
- Test on a 4.14 MTK device: same, with the extended detection
- Test on a 5.10+ device: no change in behaviour (xperms loaded normally)

---

## Fix 2: FUSE BPF fallback for /sdcard/ access on old kernels

**Issues:** #28, #30, #33
**Severity:** BLOCKING (storage broken, camera video save fails)
**Root cause:** `fuseMedia.bpf` fails to load on kernels without FUSE BPF
program type support (<5.4 without backport). FuseDaemon falls back to "Not
using FUSE BPF" but the legacy FUSE path on Android 16 QPR2 doesn't work
correctly -- /sdcard/ access fails with EIO, video recording returns error
code 7.

### 2a: Force FuseDaemon into working legacy mode

**Target repo:** `platform/packages/providers/MediaProvider`
**Patch tier:** `rom`
**Approach:**
- Add a system property `persist.sys.fuse_legacy=true` that forces the
  FuseDaemon to use the pre-FUSE-BPF code path unconditionally.
- In the FuseDaemon native code (FuseDaemon.cc / fuse_jni), check this
  property at startup. When set, skip the FUSE BPF initialisation entirely
  and use the legacy passthrough path.
- Set this property automatically from `phh-on-boot.sh` when the kernel
  doesn't support FUSE BPF (detect by checking if
  `/sys/fs/bpf/tethering/map_fuseMedia_*` exists, or if
  `bpfloader` logged the fuseMedia.bpf failure).

### 2b: Fix the legacy FUSE path itself

If the legacy path is supposed to work but doesn't, the issue may be in how
the lower filesystem path is resolved. Android 16 added a `lowerfs_path`
mechanism for FUSE BPF -- when FUSE BPF is absent, the FuseDaemon may still
try to use it and fail.

**Target repo:** `platform/packages/providers/MediaProvider`
**Patch tier:** `rom`
**Approach:**
- In the FuseDaemon, when FUSE BPF is unavailable, ensure the
  `lowerfs_path` is set correctly (typically `/data/media`).
- Check if the `FuseDaemon::MaybeInitializeBpf()` function correctly falls
  back and whether subsequent operations assume BPF is available.
- May need to patch `MediaProvider` Java code to not attempt BPF-dependent
  operations when the property indicates legacy mode.

### 2c: Skip fuseMedia.bpf.o from bpfloader on unsupported kernels

**Target repo:** `platform/system/bpfprogs`
**Patch tier:** `trebledroid-staging`
**Approach:**
- In the `Android.bp` for `fuseMedia.o`, add a `max_kver` directive so
  bpfloader doesn't attempt to load it on kernels below 5.4.
- Alternatively, set the min kernel version requirement in the BPF program
  definition so it's skipped gracefully rather than failing with an error.
- This prevents the scary error log even though our bpfloader patch already
  makes it non-fatal.

### Verification

- Test on SDM660 (4.14) device: /sdcard/ should be accessible from apps,
  camera video recording should succeed
- Test on MTK6768 device: same
- Test on 5.10+ device: FUSE BPF should load normally, no regression

---

## Fix 3: Neutralise OPPO/Realme root detection reboot

**Issues:** #38
**Severity:** BLOCKING (device reboots to recovery)
**Root cause:** OPPO/Realme vendor init includes `oppo_root_check` which
detects the GSI as "rooted" (unlocked bootloader, su binary, etc.) and
forces `reboot("recovery")`. The call trace shows `oppo_root_check+0x20/0xf0`
→ `SyS_reboot` → `arch_reset: cmd = recovery`.

### 3a: Block oppo_root_check from rw-system.sh

**Target repo:** `platform/device/phh/treble`
**Patch tier:** `trebledroid-staging`
**Approach:**
- In `rw-system.sh`, detect OPPO/Realme devices (check
  `ro.product.brand` or `ro.build.oppo.brand` or presence of
  `/vendor/etc/init/hw/init.oppo.rc`).
- Mount a dummy `oppo_root_check` binary over the vendor one using
  `mount --bind` or replace the init rc action that triggers it.
- Alternative: set `ro.boot.verifiedbootstate=green` and
  `ro.boot.vbmeta.device_state=locked` early enough that the root check
  passes. This may not be sufficient on all OPPO devices.
- Most reliable: find the property or trigger that launches
  `oppo_root_check` and disable the corresponding init service by
  writing `stop <service>` or overriding its `.rc` file.

### Verification

- Test on RMX2040 (Realme 6, MTK Helio P90): device should boot into
  RestlessOS instead of recovery
- Verify other Realme/OPPO devices aren't affected

---

## Fix 4: Allow crash_dump to ptrace init for tombstone capture

**Issues:** #35
**Severity:** DEGRADED (can't diagnose the SIGSEGV without tombstones)
**Root cause:** `crash_dump64` is denied `ptrace` on init process by SELinux
policy (`permissive=0`). Without tombstones, we can't determine why init
receives SIGSEGV.

### 4a: Add SELinux allow rule for crash_dump → init ptrace

**Target repo:** `platform/system/sepolicy`
**Patch tier:** `rom`
**Approach:**
- Add: `allow crash_dump init:process ptrace;`
- This is already standard in some AOSP policy configurations but
  GrapheneOS may have restricted it further.
- Also add: `allow crash_dump init:file { read open getattr };` so crash_dump
  can read init's executable for symbol resolution.

### 4b: Investigate init SIGSEGV on Samsung Exynos

After tombstones are captured, the actual init crash can be diagnosed.
Likely causes:
- GrapheneOS exec-based spawning incompatibility with Samsung vendor init
- Scudo allocator issue on Exynos 1280 (unlikely since we use Scudo not
  hardened_malloc)
- Samsung's `sec_reboot.ko` kernel module interfering with init subprocesses
- KernelSU interaction (request re-test without KernelSU)

**Target repo:** TBD (depends on tombstone analysis)
**Verification:** Test on Galaxy M33, capture tombstone, analyse crash

---

## Fix 5: Hotspot DNS resolution for tethered clients

**Issues:** #14, #34
**Severity:** DEGRADED (hotspot connects but no DNS for clients)
**Root cause:** `dnsmasq` communication fails (broken pipe = dnsmasq not
running). Our existing patch `trebledroid-staging/platform_system_netd/0002`
prevents tethering teardown but DNS still doesn't work because no resolver
listens on port 53 for hotspot clients.

### 5a: Ensure NetworkStackService DNS listener starts when dnsmasq is down

**Target repo:** `platform/packages/modules/Connectivity` (tethering module)
**Patch tier:** `trebledroid-staging`
**Approach:**
- The modern Android tethering stack (Tethering module) has its own DNS
  resolver that should work without dnsmasq. Investigate why it isn't
  binding on port 53.
- Check if `Tethering` module's `DnsServer` class starts correctly when
  `usingLegacyDnsProxy` is true (which it is in the logs).
- May need to patch Tethering to start its own DNS server on port 53 when
  dnsmasq is unavailable, or explicitly set
  `tethering_using_legacy_dns_proxy=false` to use the modern path.
- Alternative: start a minimal dnsmasq instance from `phh-on-boot.sh`
  specifically for tethering.

### 5b: Document MTK BPF patcher as workaround for MTK 4.14 devices

For MTK 4.14 devices specifically, the root cause may be broken BPF
support. The mtk-bpf-patcher tool
(https://github.com/R0rt1z2/mtk-bpf-patcher) patches the kernel to fix BPF
issues. Already documented in #14 comments.

**Action:** Add a note in the TrebleApp or wiki about this workaround.

### Verification

- Enable hotspot on affected device, connect client, verify DNS resolution
  works (dig/browser test)
- Verify on both MTK and Qualcomm devices

---

## Fix 6: Fingerprint HAL support for AIDL-based devices

**Issues:** #25
**Severity:** DEGRADED (fingerprint not working)
**Root cause:** Realme GT5 (SD 8 Gen 2) uses AIDL fingerprint HAL but our
Oplus compatibility shim only supports HIDL
(`vendor.oplus.hardware.biometrics.fingerprint@2.1`).

### 6a: Add AIDL fingerprint HAL bridge for Oplus devices

**Target repo:** `platform/device/phh/treble`
**Patch tier:** `trebledroid-staging`
**Approach:**
- Detect AIDL FP HAL availability by checking for
  `android.hardware.biometrics.fingerprint.IFingerprint/default` in the
  VINTF manifest.
- When AIDL FP HAL is present, register a `FingerprintService` connector
  that bridges to the vendor's AIDL implementation.
- The Oplus Udfps fix patch (0004) handles HIDL; extend it to also handle
  AIDL.
- May need to check if TrebleDroid upstream has AIDL FP support.

### Prerequisite

Need a full `adb logcat -b all` from the Realme GT5 to confirm which AIDL
interface the vendor provides. Already requested in #25 comments.

### Verification

- Test on Realme GT5: fingerprint enrollment should work
- Verify existing HIDL FP devices (Unihertz Jelly Max etc.) aren't broken

---

## Fix 7: Fingerprint regression on Jelly Max (0412 build)

**Issues:** #31
**Severity:** DEGRADED (regression)
**Root cause:** Fingerprint works on 0411, breaks on 0412. Key 0412 changes
that could affect FP: `blocked_power_modes` auto-detection, C2 input surface
disable, auto_reboot timer changes.

### 7a: Investigate blocked_power_modes impact on fingerprint

**Target repo:** `platform/frameworks/native`
**Patch tier:** `trebledroid-staging`
**Approach:**
- The `blocked_power_modes` feature suppresses display power states that
  vendor HWCs claim to support but actually mishandle. If the fingerprint
  HAL requires a specific display power state (e.g., `DOZE` for under-display
  FP), blocking that state would break FP.
- Check if the Jelly Max FP HAL uses `PowerAdvisor` to request a display
  state for the FP sensor. If so, add an exception for FP-triggered power
  state changes.
- Add logging: when `blocked_power_modes` blocks a state transition, log
  which component requested it.

### Prerequisite

Need `adb logcat -b all` from 0411 (working) and 0412 (broken) builds.
Already requested in #31 comments.

### Verification

- Flash 0411 + 0412, compare FP HAL initialisation
- Test fix: FP should work with blocked_power_modes active

---

## Fix 8: MVNO APN configuration

**Issues:** #36
**Severity:** DEGRADED (data not working on specific MVNO)
**Root cause:** MTK RIL reports `no match APN type` for Walmart Family Mobile
/ Access Wireless (Verizon MVNO). Another SIM works. Likely missing APN entry
in the carrier database.

### 8a: Verify and add missing MVNO APN entries

**Target repo:** `platform/device/phh/treble`
**Patch tier:** `trebledroid-staging`
**Approach:**
- Check the APN database included via patch 0003 for Walmart Family Mobile
  and Access Wireless entries.
- If missing, add the APN configuration. Key details needed from reporter:
  - APN name
  - APN value
  - MCC/MNC
  - Authentication type
- The MTK RIL `no match APN type` error may also be a Telephony framework
  issue where the APN type bitmask doesn't match what the modem expects.
  Investigate if `ro.telephony.default_network` needs to be set correctly
  for Verizon MVNOs.

### Verification

- Insert Walmart Family Mobile SIM, verify mobile data works
- Verify other SIMs still work

---

## Fix 9: SystemUI battery drain on A15/A16 vendor

**Issues:** #21
**Severity:** DEGRADED (battery drain)
**Root cause:** Works on A14 vendor, broken on A15/A16 vendor. Most likely a
vendor HAL incompatibility where the HAL expects certain power state
transitions that the GSI's framework handles differently.

### 9a: Compare A14 vs A16 vendor behaviour

**Prerequisite:** Need A14 and A16 bugreport ZIPs from the same device.
Already requested in #21 comments.

### 9b: Investigate vendor HAL wake-lock patterns

**Target repo:** `platform/frameworks/native`
**Patch tier:** `trebledroid-staging`
**Approach:**
- Once bugreports are available, compare:
  - Wake-lock holders between A14 and A16
  - Power state transition frequency
  - SystemUI process CPU time
- Likely culprit: a vendor display HAL or power HAL that can't enter deep
  idle states on the newer GSI, keeping the CPU awake.
- The `blocked_power_modes` feature may need device-specific tuning for
  the POCO X3 (the reporter's device).

### Verification

- Compare battery usage over 1 hour on A14 vs A16 vendor
- Verify reduced wake-lock time after fix

---

## Implementation Order

Phase 1 (immediate, BLOCKING fixes) — IMPLEMENTED:
1. **Fix 1** -- SELinux xperms → patch `trebledroid-staging/platform_system_core/0006`
   - Strips xperms CIL rules before secilc compilation
   - Extends detection from <4.9 to <5.4 with property overrides
   - `ro.boot.selinux_xperms_supported=1` opts back in on 4.9-5.4
   - `ro.boot.selinux_no_xperms=1` opts out on 5.4+
2. **Fix 2** -- FUSE BPF fallback → patch `trebledroid-staging/platform_device_phh_treble/0022`
   - Sets `persist.sys.fuse.bpf.override=false` when `/sys/fs/fuse/features/fuse_bpf` absent
   - Forces legacy FUSE path from start, avoids broken BPF load attempt
   - vold correctly sets up bind-mounts for Android/data and Android/obb
3. **Fix 3** -- OPPO root check → patch `trebledroid-staging/platform_device_phh_treble/0023`
   - Sets `ro.boot.flash.locked=1`, `ro.boot.vbmeta.device_state=locked`, `ro.boot.verifiedbootstate=green` in `on early-init`
   - Targets OPPO and realme brands
   - NOTE: kernel-level fuse check may still bypass this on some devices

Phase 2 (next, DEGRADED fixes) — PARTIALLY IMPLEMENTED:
4. **Fix 4** -- crash_dump ptrace → patch `rom/platform_system_sepolicy/0002`
   - Allows crash_dump to ptrace init for tombstone capture on GSI
   - Removed init from exclusion list and neverallow in crash_dump.te
5. **Fix 5** -- Hotspot DNS — NEEDS DEEPER INVESTIGATION
   - dnsmasq broken pipe (not running), Tethering module doesn't start own DNS resolver
   - `usingLegacyDnsProxy: true` but no fallback DNS server on port 53
   - Requires Tethering module source investigation
6. **Fix 7** -- FP regression (#31, once logs received)
7. **Fix 6** -- AIDL FP bridge (#25, once full logcat received)
8. **Fix 8** -- MVNO APN (#36, once APN details confirmed)
9. **Fix 9** -- Battery drain (#21, once A14/A16 bugreports received)

---

## Notes

- All patches must follow the android-patch skill workflow
- Never edit `.patch` diff hunks, `@@` lines, index lines, or hunk counts
- Verify patches with `git am` if `src/` is synced; otherwise state
  "WARNING: cannot verify -- src/ is not synced"
- Test each fix independently before merging
- Do not trigger CI builds without explicit instruction
