# RestlessOS

RestlessOS is an **unofficial**, **unaffiliated** fork of
[GrapheneOS](https://grapheneos.org) packaged as a Generic System Image (GSI)
for Project Treble devices. It is not endorsed by, sponsored by, or in any way
connected to the GrapheneOS project or its developers.

For discussion and support, join the Telegram group: https://t.me/restlessosgsi

## Changes from GrapheneOS

GrapheneOS targets Pixel devices with known hardware. A GSI must run on
arbitrary vendor partitions, so several hardening features are disabled or
made optional to avoid boot loops, crashes, or broken vendor drivers.

### Not included — inherent GSI limitations

A GSI ships only a system image; device-specific components come from the
vendor partition. The following GrapheneOS features cannot be provided by
any GSI:

- **firmware updates** — GrapheneOS ships firmware updates for Pixels alongside OS updates; RestlessOS uses whatever firmware the vendor partition provides
- **kernel updates** — GrapheneOS ships patched kernels for Pixels; RestlessOS boots the vendor kernel
- **driver and userspace HAL updates** — device-specific binaries cannot be bundled in a generic system image
- **GrapheneOS kernel hardening** — GrapheneOS's kernel patches target Pixel kernels specifically and cannot be applied to arbitrary vendor kernels

### Features removed
- **hardened_malloc** — causes boot loops on devices with 39-bit virtual address space. replaced with AOSP Scudo.
- **Auditor** — requires hardware attestation which doesn't work on GSI
- **mtectrl / misctrl** — Pixel-specific memory tagging control; breaks vendor TEE drivers
- **USB protection** — the low-level USB port controls rely on Pixel-specific hardware and are non-functional on other devices
- **dynamic code loading protections** — not ported; relies on assumptions about the system image that don't hold on arbitrary vendor partitions
- **native debugging protection** — not ported; breaks compatibility with root solutions and vendor debugging tools

### Features disabled by default

These can be re-enabled in **TrebleApp → Hardening** or **Settings → Exploit protection**.
- **MTE/TBI for vendor processes** — memory tagging breaks some vendor drivers
- **hardened thread stacks** — non-standard memory layout breaks some vendor drivers
- **secure (exec-based) app spawning** — breaks root solutions (Magisk / KernelSU)


## Delta Updates with zsync2

Each release includes a `.zsync` file alongside the uncompressed `.img`,
enabling delta downloads via [zsync2](https://github.com/AppImageCommunity/zsync2).
Only the changed blocks are downloaded, saving significant bandwidth on
incremental updates.

```sh
zsync2 <url to .zsync file> -i <full path to previous .img file>
```

For example:

```sh
zsync2 https://build.chrisaw.io/RestlessOS-ab-16-202603261200/zsync/RestlessOS-arm64-ab-16-202603261200.img.zsync \
    -i ~/Downloads/RestlessOS-arm64-ab-16-202603201400.img
```

> **Note:** point `-i` at the uncompressed `.img` file, not the `.img.xz`
> archive. If you only have the `.xz`, decompress it first with
> `xz -dk <file>.img.xz`.

## Known Issues

### MediaTek BPF bug (kernel 4.14 / 4.19)

Some MediaTek devices running kernel 4.14 or 4.19 have a vendor kernel
patch ([ALPS05247589]) that breaks BPF array map updates. The patch adds
an incorrect bounds check to `array_map_update_elem` which silently
skips the `memcpy`, causing BPF map writes to be dropped without error.

This affects Android's BPF-based networking stack, including the firewall
and RestlessOS's per-app network permission. Symptoms include apps
appearing to have no internet access despite being allowed, or firewall
rules not taking effect.

**This cannot be fixed from the GSI** — the bug is in the vendor kernel
binary. To fix it, patch the kernel using
[mtk-bpf-patcher](https://github.com/R0rt1z2/mtk-bpf-patcher) by
R0rt1z2. There is also an [APK version](https://github.com/Osanosa/ThemedManager/raw/refs/heads/main/mtk-bpf-patcher/release/mtk-bpf-patcher-release.apk)
that applies the same patch on-device (untested by us — use at your own
risk). See the [XDA thread](https://xdaforums.com/t/mtk-4-14-kernel-bpf-patching.4717277/)
for more information and discussion.

[ALPS05247589]: https://gist.github.com/R0rt1z2/8af7735c6c3802148fa4da61b3cba506

## Credits

- **TrebleDroid team** — for all of their hard work in making all of this possible
- **GrapheneOS team** — for creating the ROM in the first place
- **@Nullvalue** — for providing the inspiration to start working on this in the first place
- **@Gero** — for the idea of the previous name
- **[@Ziednaga](https://github.com/Ziednaga)** — for the RestlessOS logo and boot animation artwork
