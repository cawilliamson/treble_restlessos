# GrapheneOS GSI

GrapheneOS as a Generic System Image (GSI) for Project Treble devices.

For discussion and support, join the Telegram group: https://t.me/grapheneosgsi

## Known Issues

### MediaTek BPF bug (kernel 4.14 / 4.19)

Some MediaTek devices running kernel 4.14 or 4.19 have a vendor kernel
patch ([ALPS05247589]) that breaks BPF array map updates. The patch adds
an incorrect bounds check to `array_map_update_elem` which silently
skips the `memcpy`, causing BPF map writes to be dropped without error.

This affects Android's BPF-based networking stack, including the firewall
and GrapheneOS's per-app network permission. Symptoms include apps
appearing to have no internet access despite being allowed, or firewall
rules not taking effect.

**This cannot be fixed from the GSI** — the bug is in the vendor kernel
binary. To fix it, patch the kernel using
[mtk-bpf-patcher](https://github.com/R0rt1z2/mtk-bpf-patcher) by
R0rt1z2.

[ALPS05247589]: https://gist.github.com/R0rt1z2/8af7735c6c3802148fa4da61b3cba506
