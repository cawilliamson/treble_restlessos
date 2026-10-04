# RestlessOS

RestlessOS is an **unofficial**, **unaffiliated** fork of
[GrapheneOS](https://grapheneos.org) packaged as a Generic System Image (GSI)
for Project Treble devices. It is not endorsed by, sponsored by, or in any way
connected to the GrapheneOS project or its developers.

For discussion and support, join the Telegram group: <https://t.me/restlessos>

> [!WARNING]
> **RestlessOS makes no security guarantees.** GrapheneOS's security model only works because it controls the entire device stack — hardware, firmware, bootloader and kernel — on a small set of audited Pixel devices. RestlessOS replaces the system partition only. Everything else on the device comes from your vendor, is unaudited, and is outside our control. If your threat model requires genuine hardware-backed security, use a supported device running upstream GrapheneOS.

## Security expectations

RestlessOS inherits GrapheneOS's hardened userspace, but a GSI can only ever be
as trustworthy as the device it runs on. The parts that matter most are
the parts we don't ship:

- the vendor kernel and firmware are opaque binaries of unknown provenance,
  frequently years out of date, and cannot be audited or replaced by us
- the verified boot chain is rooted in your vendor's bootloader, not in keys
  we control
- hardware-backed attestation does not work, so neither you nor anyone else
  can cryptographically verify the integrity of a running device

In the current political climate this matters more than it used to: firmware
and supply-chain-level interference by state and commercial actors is a
documented reality, and a community project with no hardware audit capability
cannot rule it out on your behalf. RestlessOS is a hardened, privacy-respecting
system image — it is not a guarantee against a motivated, well-resourced
adversary, and it should not be marketed or understood as one.

If you need the full GrapheneOS security model, the only way to get it is
supported hardware with upstream GrapheneOS installed as intended.

## Changes from GrapheneOS

GrapheneOS targets Pixel devices with known hardware. A GSI must run on
arbitrary vendor partitions, so several hardening features are disabled or
made optional to avoid boot loops, crashes, or broken vendor drivers.

### Not included — inherent GSI limitations

A GSI ships only a system image; device-specific components come from the
vendor partition. The following GrapheneOS features cannot be provided by
any GSI:

These are not optional omissions — they are the components GrapheneOS's
security guarantees are built on. A GSI cannot reconstruct them, which is
why RestlessOS offers hardening, not guarantees.

- **firmware updates** — GrapheneOS ships firmware updates for Pixels alongside OS updates; RestlessOS uses whatever firmware the vendor partition provides
- **kernel updates** — GrapheneOS ships patched kernels for Pixels; RestlessOS boots the vendor kernel
- **driver and userspace HAL updates** — device-specific binaries cannot be bundled in a generic system image
- **GrapheneOS kernel hardening** — GrapheneOS's kernel patches target Pixel kernels specifically and cannot be applied to arbitrary vendor kernels

### Features removed

- **hardened_malloc** — causes boot loops on devices with 39-bit virtual address space. replaced with AOSP Scudo.
- **Auditor** — requires hardware attestation which doesn't work on GSI
- **mtectrl / misctrl** — Pixel-specific memory tagging control; breaks vendor TEE drivers
- **USB protection** — the low-level USB port controls rely on Pixel-specific hardware and are non-functional on other devices
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

## Building

There is no supported local build path — RestlessOS is built entirely through
the GitHub Actions pipeline in [.github/workflows/build.yml](.github/workflows/build.yml),
which handles EC2 spot-instance provisioning, source sync, patch application,
compilation, signing and release. That workflow is the canonical reference for
how the ROM is built; read it before attempting to replicate the process.

### Signing with your own keys

The release image is signed using a private `vendor_cawilliamson-priv` repo
(synced via [`configs/manifests/default.xml`](configs/manifests/default.xml) into
`vendor/cawilliamson-priv`) containing the Android build signing keys: one
`.pem`/`.pk8`/`.x509.pem` set per signing identity, plus two scripts:

- `keys/make_keys.sh` — generates the full set of signing keys
- `keys/sign.sh` — invokes `sign_target_files_apks` with the correct
  `--extra_apks` / `--extra_apex_payload_key` mapping for every APK and APEX
  in the image

Script-only copies of both are kept under
[`overlays/cawilliamson/vendor_cawilliamson-priv/`](overlays/cawilliamson/vendor_cawilliamson-priv/)
as a reference. To sign with your own keys:

1. generate a key set — adapt `make_keys.sh` (the `SUBJECT` line and the key
   list) to your own details and run it
2. create your own private repo with the same layout: a `keys/` directory
   holding the generated key files plus your `sign.sh`, with the
   `--extra_apks` / `--extra_apex_payload_key` lines matching your key names
3. point your local manifest at your repo (see
   [`configs/manifests/default.xml`](configs/manifests/default.xml)) and update
   the `Sign target files` step in the workflow to reference it

Never commit the private key material (`.pk8` and `.pem` files) to a public
repository — the copies under `overlays/` are deliberately scripts only.

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
R0rt1z2. There is also an [APK version](https://xdaforums.com/attachments/mtk-bpf-patcher-release-apk.6238876/)
that applies the same patch on-device (untested by us — use at your own
risk). See the [XDA thread](https://xdaforums.com/t/mtk-4-14-kernel-bpf-patching.4717277/)
for more information and discussion.

[ALPS05247589]: https://gist.github.com/R0rt1z2/8af7735c6c3802148fa4da61b3cba506

## Credits

- **TrebleDroid team** — for all of their hard work in making all of this possible
- **[@phhusson](https://github.com/phhusson)** — for lptools, magisk integration and vndk-tests, overlaid into the build
- **GrapheneOS team** — for creating the ROM in the first place
- **@Nullvalue** — for providing the inspiration to start working on this in the first place
- **@Gero** — for the idea of the previous name
- **[@Ziednaga](https://github.com/Ziednaga)** — for the RestlessOS logo and boot animation artwork
- **[@clangsdorff](https://github.com/clangsdorff)** — for endless debugging help and code merged into the ROM
- **[@andycgyan](https://github.com/andycgyan)** — for QcRilAm, overlaid into the build
- **[PixelOS](https://github.com/PixelOS-AOSP)** — for the gsans treble build scripts, overlaid into the build
