OSTC4 custom nRF52840-QIAA-F development build
============================================

This is the entire currently configured Zephyr application for dc_v2_2/nrf52840,
not a DK image or an isolated archive. It is NOT the complete watch product:
main previously returned immediately; the target JDI driver/LVGL presentation,
Bluetooth application/services, sensor acquisition and persisted clock/history
adapter are not implemented/enabled here. The previous active MCU target is replaced; the current pin plan is recorded below;
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
  python3 V2/Firmware/tests/target_board/check_resources.py /tmp/dive-watch-target
  python3 V2/Firmware/tests/target_board/check_configuration.py /tmp/dive-watch-target

Run from the repository root. Paths to NCS/toolchain can be overridden through
--ncs and --toolchain. Defaults: ~/ncs/v3.4.1, ~/ncs/toolchains/8285d8ad56.
Use source/build paths without spaces: this NCS devicetree overlay discovery
splits a source path containing spaces. Cache/build output is outside the SDK.
Use a fresh build directory after changing SDK/toolchain/board configuration.
The public builder accepts repeatable --extra-conf PATH and --overlay PATH.
These append Kconfig fragments and DT overlays after the normal app defaults;
use separate fresh build directories for each controller-validation variant.

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
Historical evidence in evidence/nrf52833/ identifies the earlier configured image
and configuration; its identity and measurements remain unchanged. report.json records ELF hash, generated-source hashes, compilation
commands, math members, ABI attributes, actual linker capacities and stack
objects. Zephyr linker _flash_used and _image_ram_size are the used-byte sources;
the map supplies region origin/length. ELF debug/file size is not flash use.
RAM used includes initialized data, BSS, RTT buffers, noinit stack objects and
alignment/reservations. Per-stack configured usable sizes differ from allocated
objects because of protection/FPU/alignment overhead. The configured system
workqueue size does not imply an allocated workqueue stack in this application.
No dynamic heap is reserved; COMMON_LIBC_MALLOC_ARENA_SIZE=0 and
HEAP_MEM_POOL_SIZE=0. Remaining RAM is static headroom, not runtime headroom.

The nRF52840-QIAA has 1 MiB flash and 256 KiB RAM. QIAA-F is the procurement
variant; QIAA is the installed SDK package identifier. The derived aligned layout
preserves boot (48 KiB) and storage (24 KiB), enlarging both images to 476 KiB:

  boot       0x000000..0x00c000   size 0x0c000
  image0     0x00c000..0x083000   size 0x77000
  image1     0x083000..0x0fa000   size 0x77000
  storage    0x0fa000..0x100000   size 0x06000

These exact byte values are an implementation proposal derived from the specified
reservations, not a separately recorded owner choice. USE_DT_CODE_PARTITION=y
honors image0 at 0xc000. Reports distinguish chip capacity from this link region.
This un-signed development ELF has no MCUboot header/trailer or bootloader build;
it is not a deployable update package. Do not treat compilation as boot proof.
The reserved main/IRQ/idle stacks are compile-time choices, not measured safe
sizes. No target timing, high-water, physical sensor/display/BLE or dive evidence.
The desktop simulator's 1 MiB LVGL pool alone exceeds all target RAM; this ticket
does not port that desktop configuration or claim it can fit unchanged.

Power, clocks and programming boundary
-------------------------------------
Battery feeds VDDH. The supported SDK reg0 status and reg1 initial-mode select
both DC/DC stages. Hardware requires the Nordic reference LC networks for both
stages (DCC/DCCH/DEC connections); software cannot replace their populated parts.
Nominal MCU VDD/GPIO is 1.8 V. MCU VDD and the independently switched external
peripheral 1.8 V regulator output are separate nets and must not be shorted.

The generated zephyr,user node records the intended REGOUT0 VOUT=0 (1.8 V).
This SDK has no REGOUT0 DT/Kconfig programming property: no startup UICR write
has been added. The report inspects whether the HEX actually contains REGOUT0.
An absent REGOUT0 image word does NOT enforce 1.8 V on a previously programmed
device. Actual VOUT=0 or erased/default VOUT=7 gives 1.8 V; other pre-existing
values must be detected/resolved during later authorized hardware qualification.
No UICR read, write, erase, probe connection, programming or execution occurs here.
The SDK NFC/reset selections may themselves write their UICR fields when this
image eventually executes; compilation does not establish actual pin state.

External 32 MHz and 32.768 kHz crystals remain required. LF selection is XTAL
with conservative 50 ppm temperature/aging budget. P0.00/P0.01 remain crystal
pins; P0.18 is nRESET. NFC P0.09/P0.10 are selected as GPIO; parallel trace stays
disabled. SWD/RTT is the debug boundary. J-Link and pyOCD metadata name nRF52840.

Current pin plan — 2026-11-10
-----------------------------
Reference: boards/nordic/dc_v2_2/source-pin-plan.json (owner export, 19 signals).
Effective plan: boards/nordic/dc_v2_2/pin-plan.json. The MCU PWM fallback is
disabled by default, as explicitly reconfirmed by the owner. CHG has an external
pull-up to MCU VDD, also confirmed; no internal pull-up or output flags are used.

  SCREEN_SDI    P0.26  G1    SPIM0_MOSI       8000000 Hz  pull=none
  SCREEN_SCL    P0.04  J1    SPIM0_SCK        8000000 Hz  pull=none
  SCREEN_NRST   P0.06  L1    GPIO_OUTPUT            0 Hz  pull=up
  SCREEN_CS     P0.08  N1    GPIO_OUTPUT            0 Hz  pull=down
  SCREEN_SYNC   P1.09  R1    GPIO_INPUT             0 Hz  pull=none
  BL_PWM_MCU    P0.12  U1    PWM0_CH0             250 Hz  pull=none
  INT_LIGHT     P1.13  A16   GPIO_INPUT             0 Hz  pull=none
  INT_MAG       P1.15  A14   GPIO_INPUT             0 Hz  pull=none
  INT_ACC1      P0.02  A12   GPIO_INPUT             0 Hz  pull=none
  INT_ACC2      P0.29  A10   GPIO_INPUT             0 Hz  pull=none
  INT_HAPTIC    P0.31  A8    GPIO_INPUT             0 Hz  pull=none
  INT_ENC       P0.30  B9    GPIO_INPUT             0 Hz  pull=none
  1.8V_EN       P1.14  B15   GPIO_OUTPUT            0 Hz  pull=down
  1.2V_EN       P0.03  B13   GPIO_OUTPUT            0 Hz  pull=down
  3.1V_EN       P0.28  B11   GPIO_OUTPUT            0 Hz  pull=down
  CHG           P1.02  W24   GPIO_INPUT             0 Hz  pull=none
  QI_EN         P1.01  Y23   GPIO_OUTPUT            0 Hz  pull=none
  SCL           P0.27  H2    TWIM1_SCL         400000 Hz  pull=none
  SDA           P0.05  K2    TWIM1_SDA         400000 Hz  pull=none

SPI0 and I2C1 use different peripheral IDs. SPI0 and I2C0 cannot run together.
P0.00/P0.01 remain LF crystal pins; P0.18 remains reset. SWD/RTT is preserved,
NFC is GPIO and parallel trace is disabled. Supply/decoupling/RF pads are fixed.

1.2 V, 1.8 V and 3.1 V now have independent enables. P1.14 controls the switched
peripheral 1.8 V supply (sensor/I2C/interrupt pull-ups and panel VDDI/DCX), separate
from MCU VDD; P0.03 controls 1.2 V and P0.28 controls VCI 3.1 V. No runtime rail
sequence is implemented. Quiesce interfaces before rail-off and avoid backfeeding.

IRQ properties have a single owner on each sensor node. ACC/MAG use the SDK
bindings' intended active-high polarity; Hall/light/haptic use active low.
The JSON does not configure sensor registers. DRV2624 TRIG/INTZ in interrupt
mode needs an external pull-up; hardware presence remains to be checked:
https://www.ti.com/lit/ds/symlink/drv2624.pdf (SLOS893D, section 7.3.16.3).
Custom haptic/light/display nodes remain passive declarations, not new drivers.

GPIO pulls apply after configuration, not before initialization. Requested CS
pull-down is preserved, but active-low CS must be driven inactive high before
transfers; it does not guarantee deselection during MCU reset. Display RESET
uses active low plus the requested pull-up. TE moved to unrestricted P1.09.
LF-restricted GPIO events still require <=10 kHz and standard drive.

All six I2C addresses remain: LIS2MDL 0x1e, LIS2DUX12 0x19, MS5837-30BA 0x76,
TMAG3001 0x34 (ADDR grounded), VEML6035 0x10, DRV2624 0x5a. I2C1 is 400 kHz,
open-drain with external pull-ups. Actual rise times, DC and power domains need
physical validation; no hardware is accessed by this update.

Controller build (no sensor acquisition or actual SPI transfer):

  python3 V2/Firmware/tools/build_target.py --build-dir V2/Firmware/build_pinplan \
      --extra-conf V2/Firmware/validation/sensors.conf \
      --extra-conf V2/Firmware/validation/display.conf
  python3 V2/Firmware/tests/target_board/check_sensors.py V2/Firmware/build_pinplan
  python3 V2/Firmware/tests/target_board/check_display.py V2/Firmware/build_pinplan --variant spi8
  python3 V2/Firmware/tests/target_board/check_integrated.py V2/Firmware/build_pinplan --variant combined8

nRF Connect: application V2/Firmware, target dc_v2_2/nrf52840, build_pinplan.
Select that new configuration rather than the older build/build_1 profiles.
Native graphical Pin Overview inspection and physical qualification are distinct
from generated devicetree, Kconfig, compiled controller and ownership checks.

Historical nRF52833 revalidation (2026-10-28)
-------------------------------------------
The shared endpoint-ceiling calculation added after ticket 08 failed the target
build's -Werror=double-promotion checks. Explicit double casts now document the
same conversions already required by the C expressions; formulas, source
precision, warning policy and independent expectations are unchanged.

Rebuilt dc_v2_1/nrf52833 on base e0c963d with this fix: 39,620 B flash out of
225,280 B (185,660 B remaining), 15,104 B static/reserved RAM out of 131,072 B
(115,968 B remaining). Main/IRQ/idle allocated stacks remain 8,320/2,176/512 B.
The firmware host suite passes 14/14, the current PC bench passes 26/26, and
independent_schedule_acceptance succeeds with the existing comparison policy.
The evidence/nrf52833 bundle above remains the original ticket08 snapshot;
these new figures describe the later shared implementation. The configured-image
and physical-validation limitations above still apply. No target execution.

Display and backlight build-only variants
----------------------------------------
Current routing is in the 2026-11-10 table above. SPI mode 0, no MISO. 8 MHz is
the requested ceiling; the previously documented worst-case setup margin remains
unqualified. validation/spi4mhz.overlay selects 4 MHz/high drive on the SAME
SCK/MOSI pins. It no longer changes PWM routing in either power state.
Three-wire nine-bit panel framing needs a future transport over the installed
eight-bit SPIM driver; no visible-pixel or actual-transfer test is implemented.

Normal assembly: panel PWM, R8 fitted/R7 DNP. PWM0 ch0 P0.12 stays disabled,
with no duplicate GPIO output owner. Manual fallback: R7 fitted/R8 removed.
Never fit both. The optional validation profile uses 250 Hz (4 ms), as requested,
but no application starts PWM, sets duty or implements automatic failover.

To the combined build above, append the following only for the manual variant:

  --extra-conf V2/Firmware/validation/pwm-fallback.conf \
  --overlay V2/Firmware/validation/spi4mhz.overlay \
  --overlay V2/Firmware/validation/pwm-fallback.overlay

Then use check_display.py --variant spi4-pwm and check_integrated.py --variant
combined4-pwm. Hardware timing/DC/slew/power/TE/PWM qualification remains pending.

Historical nRF52840 migration handoff (2026-11-06)
----------------------------------------------
Its older GPIO allocation is superseded by the 2026-11-10 plan above.
Physical reference, all 74 pads, historical actual-vs-intended wiring, qualification
ledger, all 17 acceptance scenarios, four combined build variants and repeatable
commands:

  /home/Bilal/Documents/apps/Markdown files/migration-nrf52840-delivery.md

The corresponding retained evidence is evidence/migration-nrf52840/ticket04/.
Run tests/target_board/check_integrated.py BUILD_DIR --variant normal|combined8|
combined4|combined4-pwm after the applicable public build. It spans GPIO and bus
pinctrl owners and shared serial instances against independent pad research.
Optional --connection-reference PATH also checks the complete physical table.
Use check_sensors.py for combined builds and check_display.py --variant
normal|spi8|spi4|spi4-pwm respectively; the final variant explicitly composes the
4 MHz bring-up selection and PWM fallback. This does not activate app transfers.
Native nRF Connect graphical inspection is PENDING, separately from successful
generated-artifact acceptance. Hardware qualification remains unestablished.
