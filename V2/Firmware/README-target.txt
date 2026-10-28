OSTC4 custom nRF52833 development build (ticket 08)
================================================

This is the entire currently configured Zephyr application for dc_v2_1/nrf52833,
not a DK image or an isolated archive. It is NOT the complete watch product:
main previously returned immediately; the target JDI driver/LVGL presentation,
Bluetooth application/services, sensor acquisition and persisted clock/history
adapter are not implemented/enabled here. The existing board descriptions,
drivers and desktop LVGL presentation are preserved. No full-watch fit claim.

At boot this development image runs a clearly labelled SYNTHETIC self-test via
the public pressure/time boundary, prints its status over RTT, then leaves all
exposure guidance INVALID. It never initializes real exposure from fresh tissues.
Only compilation is demonstrated on ARM; this boot behavior has not been run on
hardware. A future real-sensor adapter must establish continuous exposure,
including restart recovery, before enabling results. A pressure read alone is not
proof of exposure history. The ticket07 portable recovery API remains available;
its deployment/storage/clock integration is not provided by this build.

Repeat on the installed environment, without flashing:

  python3 V2/Firmware/tools/build_target.py --build-dir /tmp/dive-watch-target
  python3 V2/Firmware/tools/report_target.py /tmp/dive-watch-target \
      --output /tmp/dive-watch-target/report.json

Run from the repository root. Paths to NCS/toolchain can be overridden through
--ncs and --toolchain. Defaults: ~/ncs/v3.4.1, ~/ncs/toolchains/8285d8ad56.
Use source/build paths without spaces: this NCS devicetree overlay discovery
splits a source path containing spaces. Cache/build output is outside the SDK.
Use a fresh build directory after changing SDK/toolchain/board configuration.

Host verification of the same target smoke and the adapter:

  CCACHE_DISABLE=1 cmake -S V2/Firmware/tests/firmware -B /tmp/dive-fw-host
  CCACHE_DISABLE=1 cmake --build /tmp/dive-fw-host -j 4
  ctest --test-dir /tmp/dive-fw-host --output-on-failure
  CCACHE_DISABLE=1 cmake --build /tmp/dive-fw-host \
      --target independent_schedule_acceptance

The confirmed seam is timestamped absolute pressure in, observed state/results
out. Synthetic initialization is explicit. The adapter accepts absolute pascals
and elapsed uint64 milliseconds, converts Pa / 100000.0f and ms / 1000, and
rejects fractional seconds and uint32-second overflow rather than rounding time.
The producer must supply whole-second endpoints for known continuous linear
segments; missing/invalid samples latch invalidity. It is not yet a sensor driver
or a resampler for the actual sensor/clock. No ordinary runtime path clears the
fault; only an explicit synthetic-fixture start may reset the development state.

Numerical identity and math
---------------------------
ostc4_baseline/exposure.cmake supplies both PC and target production targets:
corrected decom.c, contained buehlmann planner, ostc4_baseline.c continuous API.
The two checksum-guarded generators are unchanged; pinned upstream is unchanged.
Revision: 7198d203520f3a3cdccf1ccc0bf8af6475ee4819 (experimental beta snapshot).
Correction set: cached-10s-and-stable-schreiner-v1.
Exposure policy: ostc4-continuous-air-gf30-70-v1.
Numerical details remain in ostc4_baseline/{policy,corrected-policy}.json.

Source binary32 float and binary64 double are enforced by existing static
assertions. No fast math; FP contraction off; no single-precision-constant flag.
The PC source target requests -O2; Zephyr appends -Os (effective on ARM).
ARM GCC 14.3.0; Zephyr SDK 1.0.1; NCS 3.4.1 / Zephyr 4.4.2.
Cortex-M4, -mfpu=fpv4-sp-d16, -mfloat-abi=hard, AAPCS/VFP arguments. The M4F
accelerates single precision only. The linked ELF retains __aeabi_d* software
double helpers; expf/expm1f/ceilf remain as named math functions in the final ELF, supplied by
SDK Picolibc 1.8.10 libc.a math members. The link map also names pow/logf/sqrt
archive members whose unused sections may be discarded; that list alone does
not prove those functions remain in the ELF. Transcendentals use library
algorithms, not native FPU operations.
Picolibc is the supported SDK libc selected here, not the PC's libm. Successful
linking and common generated source do not prove target numerical equivalence.
Upstream -Wdouble-promotion warnings are retained because those conversions are
part of the preserved source arithmetic, not "fixed" to shrink the image.

Memory evidence and limitations
-------------------------------
Evidence in evidence/nrf52833/ identifies the complete configured image and
configuration. report.json records ELF hash, generated-source hashes, compilation
commands, math members, ABI attributes, actual linker capacities and stack
objects. Zephyr linker _flash_used and _image_ram_size are the used-byte sources;
the map supplies region origin/length. ELF debug/file size is not flash use.
RAM used includes initialized data, BSS, RTT buffers, noinit stack objects and
alignment/reservations. Per-stack configured usable sizes differ from allocated
objects because of protection/FPU/alignment overhead. The configured system
workqueue size does not imply an allocated workqueue stack in this application.
No dynamic heap is reserved; COMMON_LIBC_MALLOC_ARENA_SIZE=0 and
HEAP_MEM_POOL_SIZE=0. Remaining RAM is static headroom, not runtime headroom.

The board partitions 512 KiB flash into boot (48 KiB), image0 (220 KiB), image1
(220 KiB) and storage (24 KiB). USE_DT_CODE_PARTITION=y honors image0 at 0xc000.
This un-signed development ELF has no MCUboot header/trailer or bootloader build;
it is not a deployable update package. Do not treat compilation as boot proof.
The reserved main/IRQ/idle stacks are compile-time choices, not measured safe
sizes. No target timing, high-water, physical sensor/display/BLE or dive evidence.
The desktop simulator's 1 MiB LVGL pool alone exceeds all target RAM; this ticket
does not port that desktop configuration or claim it can fit unchanged.
