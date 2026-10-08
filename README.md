# Multi-Domain Debugger Fixture

Test firmware for NCS v3.4.1 / bundled Zephyr 4.4.2. Each sysbuild produces five
independently linked ELF images. It is intentionally not an audio or radio application.

| Domain    | Core           | Role                                                       |
| --------- | -------------- | ---------------------------------------------------------- |
| `dbgtest` | CPUAPP         | Minimal test bootloader, validates vectors and selects A/B |
| `app_a`   | CPUAPP         | Thread workload, image ID 1                                |
| `app_b`   | CPUAPP         | Thread workload, image ID 2                                |
| `app_c`   | CPUNET or FLPR | Thread workload, image ID 3                                |
| `app_d`   | CPUNET or FLPR | Thread workload, image ID 4                                |

## Switching

On first boot with an invalid retained selection, A and C are selected.

- `sw0`: toggle A/B, then warm-reset the system through the CPUAPP bootloader.
- `sw1`: toggle C/D, then warm-reset the system through the CPUAPP bootloader.
- Selection and a boot counter live in a dedicated 4 KiB SRAM region outside all
  kernel stacks, data and BSS. Selection is not persistent storage; power loss
  normally clears it. The bootloader validates its magic and bounds before use.
- Buttons are debounced for 40 ms using delayable work. Holding a button does not
  repeatedly switch images. All images are programmed up front; switching writes no flash.
- Both cores restart on either button. This tests reset/reattach and boot handoff,
  not preservation of a running secondary core while CPUAPP changes images.

On the Audio DK, `sw0` is the volume-down button and `sw1` is volume-up. On the
nRF54L15 DK these are the board's `button0` and `button1` aliases.

CPUNET always starts from its fixed reset address. C redirects to D at the EARLY
initialization stage, before drivers and workload threads start, if D is selected.
Thus C's reset/initialization symbols can briefly appear on every CPUNET startup.
FLPR is RISC-V, not a network Cortex-M33. CPUAPP starts it directly at C or D's
RRAM XIP entry address using the VPR HAL. C/D share their core's RAM, but never run
simultaneously on that core. A/B likewise share CPUAPP RAM.

## Debugger Workload

Each application has `producer`, `consumer` and `observer` threads with separate
2048-byte stacks and priorities 5, 6 and 7, plus Zephyr's main/idle threads and,
on CPUAPP, the system workqueue. The threads exercise a message queue, mutex,
semaphore waits, sleeping and scheduling.

Useful symbols and breakpoint locations:

- `debugger_globals`: image identity, counters, checksum and a nested sample struct.
- `debugger_image_name`: a different string in each ELF.
- `debugger_nested` -> `debugger_leaf`: non-inlined call frames, stack structs,
  arrays, a volatile local cookie and 64-bit arithmetic. `debugger_leaf` sleeps
  with its local frame alive.
- `debugger_handoff` / `branch_to_image`: CPUAPP handoff and CPUNET C-to-D handoff.
- `network_select`: CPUNET early selection.
- `switch_image`: button handling and warm reset.

All images use `-O0` and contain DWARF debug info. Application images include
Zephyr debug-thread metadata, thread names and stack initialization. Use each
domain's ELF for symbols; do not keep interpreting a new image with the old ELF.

## Build

Use the installed matching NCS v3.4.1 toolchain. In nRF Connect for VS Code, add
the root folder as an application and enable sysbuild for either CPUAPP target:

```text
nrf5340_audio_dk/nrf5340/cpuapp
nrf54l15dk/nrf54l15/cpuapp
```

Equivalent commands from the NCS workspace/toolchain environment, with `APP`
set to this repository's absolute path:

```sh
west build --sysbuild -b nrf5340_audio_dk/nrf5340/cpuapp -d "$APP/build_audio" "$APP"
west build --sysbuild -b nrf54l15dk/nrf54l15/cpuapp -d "$APP/build_l15" "$APP"
```

CPUAPP `/ns` builds and nRF54L10/L05/LC10 targets are not supported. Nested
`images/` is not a standalone application: it receives its identity through sysbuild.
The earlier standalone workload build, if present, is obsolete.

Each build's `domains.yaml` lists the five domain directories and flash order.
Program the whole sysbuild through nRF Connect, not only the default bootloader
domain. The ELF/HEX/BIN files are under `<build>/<domain>/zephyr/`. Keep both
boards in separate build directories. Do not use erase-all for individual domains.

| Image      | Audio DK address / size | nRF54L15 DK address / size |
| ---------- | ----------------------- | -------------------------- |
| Bootloader | `0x00000000` / 128 KiB  | `0x00000000` / 128 KiB     |
| A          | `0x00020000` / 448 KiB  | `0x00020000` / 576 KiB     |
| B          | `0x00090000` / 448 KiB  | `0x000b0000` / 576 KiB     |
| C          | `0x01000000` / 128 KiB  | `0x00140000` / 128 KiB     |
| D          | `0x01020000` / 128 KiB  | `0x00160000` / 116 KiB     |

CPUAPP RAM is 448 KiB on Audio DK and 184 KiB on nRF54L15 DK. CPUNET uses its
own 64 KiB RAM; FLPR uses the top 68 KiB of shared SRAM. Selection is at
`0x20070000` on Audio DK and `0x2002e000` on nRF54L15 DK. Devicetree layouts
under `layouts/` own these assignments; C code derives addresses from devicetree.

## Validation

Run artifact validation with Python already supplied by the selected toolchain.
No virtual environment or package installation is needed; NCS supplies PyYAML,
pyelftools and IntelHex for its own build tools:

```sh
python3 "$APP/tests/validate_build.py" "$APP/build_audio" "$APP/build_l15"
```

The check verifies domain inventory, programmed-byte non-overlap, expected load
addresses, ARM vectors, FLPR entry points, ELF debug symbols, per-image identity,
thread metadata and exclusion of retained SRAM from loadable segments.

Hardware acceptance checklist, to execute separately on both DKs:

1. Program all five domains. Boot A/C and inspect both cores' thread lists.
2. Break in `debugger_leaf`. Inspect its locals and caller's `local_copy`; switch
   threads and check each stack and `debugger_globals`.
3. Press `sw0`. Observe reset, bootloader vector validation, and handoff to B.
   Reload B's symbols and confirm image ID 2. Press again to return to A.
4. Press `sw1`. Observe D (ID 4) on the secondary core. On CPUNET, also observe
   C's EARLY redirect; on FLPR, observe CPUAPP's selected start address.
5. Cycle all A/C, B/C, B/D and A/D combinations. Verify counters progress without
   assertions. Hold each button and verify only one transition per press.
6. Reset without changing selection and verify it remains selected. Power-cycle
   and verify default selection when retained RAM is invalid. Break at handoff,
   single-step, and test debugger reset/reconnect and symbol reload behavior.

## Limitations

This is an unsigned test bootloader, not MCUboot or a secure/DFU implementation.
There are no signatures, rollback handling or update transport. UART consoles
are disabled to avoid inter-core pin ownership and peripheral handoff conflicts;
observability is through memory and breakpoints. No hardware stack guard or fault
injection is enabled; stack fill and stack metadata are provided for inspection.

The bootloader is clockless and does not start peripheral drivers. NCS reports
that the nRF5340 anomaly-160 system-clock workaround is unavailable in that
domain. The workload images have the normal clock configuration. This limitation
requires hardware qualification for the particular Audio DK revision.

Both board builds and artifact checks can pass without proving runtime SRAM
retention, FLPR security permissions, reset behavior or debugger support. The
hardware acceptance checklist is required before treating this as a validated
debugger fixture. Nothing in this project changes debug protection settings.
