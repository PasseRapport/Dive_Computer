OSTC4 custom nRF52840-QIAA-F development build
============================================

This is the entire currently configured Zephyr application for dc_v2_2/nrf52840,
not a DK image or an isolated archive. It is NOT the complete watch product:
main previously returned immediately; the target JDI driver/LVGL presentation,
Bluetooth application/services, sensor acquisition and persisted clock/history
adapter are not implemented/enabled here. The previous active MCU target is replaced; existing peripheral assignments,
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

Sensors, charger and shared peripheral supply
--------------------------------------------
Controller-validation build, still using the existing calculation application:

  python3 V2/Firmware/tools/build_target.py --build-dir /tmp/dive-sensor-controllers \
      --extra-conf V2/Firmware/validation/sensors.conf
  python3 V2/Firmware/tools/report_target.py /tmp/dive-sensor-controllers \
      --output /tmp/dive-sensor-controllers/report.json
  python3 V2/Firmware/tests/target_board/check_sensors.py /tmp/dive-sensor-controllers

This profile compiles existing TWIM0/pinctrl drivers without CONFIG_SENSOR. It
does not implement acquisition, sensor setup, wake handling or charge detection.
The normal calculation build can leave the controller/pinctrl compiled out;
source declarations alone are not evidence of a compiled controller. Inspect
the generated zephyr.dts/.config and compile_commands.json for the selected build.
Physical nRF Connect Devicetree Visual Editor/Pin Overview inspection remains
PENDING: no native editor UI is available in this execution environment. Text
checks do not count as a completed graphical pin inspection.

TWIM0 uses SCL P0.15/AD10 and SDA P0.17/AD12, with the declared 400 kHz ceiling.
Both active/sleep groups explicitly use NRF_DRIVE_S0D1: drive low, disconnect
high. Installed Nordic pinctrl also substitutes S0D1 for default TWIM S0S1. The
external 4.7 kohm pull-ups connect to switched peripheral 1.8 V, not MCU VDD.
The six 7-bit addresses are LIS2MDL 0x1e, LIS2DUX12 0x19, MS5837-30BA 0x76,
TMAG3001 0x34 (ADDR grounded), VEML6035 0x10 and DRV2624 0x5a. Magnetometer
INT/DRDY and haptic IRQ are not routed; no interrupt has been invented for them.

INT_ACC1 P0.02/A12 is active high for the intended accelerometer push-pull
setting. INT_ENC P0.03/B13 and INT_LIGHT P0.30/B9 are active low from open-drain
sensor outputs with external 10 kohm pull-ups to the switched peripheral rail.
Each interrupt has one owner on its sensor node, without duplicate gpio-keys.
These QIAA pins carry the standard-drive/low-frequency recommendation, <=10 kHz.
Hall is intended for event/change use; exporting every 20 kHz conversion is not
qualified. Polarity declarations do not configure sensor output stages or prove
their output guarantees against MCU VIH/VIL over rail tolerances/load.

Historical QI_EN P0.09/L24 reaches BQ51051B EN2. EN2 governs adapter-source
priority; AD is grounded in the current design. This signal is not a general
wireless-charge disable input. ~CHG P0.10/J24 is an active-low input from an
open-drain charger output. Its zephyr,user declaration specifies INPUT policy
and GPIO_ACTIVE_LOW | GPIO_PULL_UP: future initialization must call the GPIO
input configuration with those flags. The internal pull-up goes to always-
powered MCU VDD, not BAT+ or the switched peripheral supply. Passive DT policy
does not initialize it; idle is NOT guaranteed before initialization. No physical
resistor or charger control/runtime code has been added. NFC remains GPIO.

P0.28/B11 gates both external 1.2 V and external peripheral 1.8 V. That 1.8 V
rail supplies sensors, interrupt/I2C pull-ups and panel VDDI/DCX; MCU VDD is a
separate always-powered net. DRV2624 has a battery power input, while its shared
I2C pull-ups still use the switched 1.8 V domain. Turning P0.28 off removes
motion/Hall/light wake capability and pressure acquisition. Keep this rail on
for future firmware requiring those functions, unless hardware is redesigned.

Before a future rail shutdown: finish/stop bus transfers, suspend the controller
and select its disconnected-input sleep state with no internal pulls. Sensor
interrupt MCU inputs must also be high impedance with their MCU internal pulls
disabled. Do not leave live MCU pull-ups/driven highs feeding unpowered sensor
pins through protection structures. Check every other signal crossing the rail
boundary and follow panel-specific power sequencing separately. This is a future
initialization/shutdown contract; no runtime power manager or sequencing is
implemented here. GPIO sense alone cannot wake from an unpowered sensor.

Electrical reservations stay open: a 4.7 kohm/300 ns simple-RC limit gives about
75 pF bus loading; 400 kHz rise times need real loading measurements. MS5837's
nominal 0.36 V VIL versus some participants' 0.4 V guaranteed VOL at their test
current leaves an unclosed worst-case guarantee, not an observed failure at the
actual lower load. Independent regulator tolerance, interrupt/output DC bounds,
charge levels/startup and powered/unpowered behavior require hardware checks.
Successful compilation proves none of these electrical or wake-up properties.

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
The normal board retains SPIM1 SCK P0.04/J1, MOSI P0.05/K2, active-low GPIO
CS P0.11/T2, active-low reset P1.09/R1 and TE input P0.31/A8. No MISO/read
path is routed. SPI mode 0 is represented by absent CPOL/CPHA properties.
The minimal normal application does not compile/initialize a display controller.
Use the same public builder with a controller-validation profile:

  python3 V2/Firmware/tools/build_target.py --build-dir /tmp/dive-spi8 \
      --extra-conf V2/Firmware/validation/display.conf
  python3 V2/Firmware/tests/target_board/check_display.py /tmp/dive-spi8 \
      --variant spi8

8 MHz remains the previously confirmed declared ceiling, with an unresolved
published-bound setup margin: 62.5 ns half-cycle - 59 ns maximum MOSI delay =
3.5 ns, below JDI's 20 ns register and 5 ns GRAM requirements. This is not a
measurement of a failed physical unit. The audit recommends selectable 4 MHz:

  python3 V2/Firmware/tools/build_target.py --build-dir /tmp/dive-spi4 \
      --extra-conf V2/Firmware/validation/display.conf \
      --overlay V2/Firmware/validation/spi4mhz.overlay
  python3 V2/Firmware/tests/target_board/check_display.py /tmp/dive-spi4 \
      --variant spi4

That overlay keeps identical pins, selects high drive only on unrestricted
SCK/MOSI, and leaves 66 ns (125 - 59 ns) before board skew. It is an audit
recommendation, not a separately confirmed replacement of 8 MHz. The JDI 10 ns
edge requirement and independent nominal 1.8 V rail/DC margins remain unqualified;
high drive does not close voltage guarantees. Slow restricted signals remain
standard drive. TE is a frame input: documented 63.1 Hz, 560 us high; P0.31's
standard-drive/low-frequency recommendation is <=10 kHz, so unspecified high-rate
line TE cannot be inferred compatible. No frame capture has occurred.

The installed spi_nrfx_spim_common.c rejects word sizes other than eight bits.
Panel three-wire nine-bit framing still needs a future transport that handles
packing, exact command/GRAM clocks and CS continuity. No display driver or SPI
transfer is added by these profiles; compilation is not evidence of visible pixels.

PWM ecran is normal operation: R8 fitted, R7 not fitted. PWM0 is disabled and
P0.20/AD16 has no active GPIO output alias in the normal board. PWM MCU de secours
requires a manual hardware assembly choice: R7 fitted, R8 not fitted. The explicit
fallback variant is only for compiling/inspecting that alternative controller:

  python3 V2/Firmware/tools/build_target.py --build-dir /tmp/dive-pwm \
      --extra-conf V2/Firmware/validation/display.conf \
      --extra-conf V2/Firmware/validation/pwm-fallback.conf \
      --overlay V2/Firmware/validation/pwm-fallback.overlay
  python3 V2/Firmware/tests/target_board/check_display.py /tmp/dive-pwm \
      --variant pwm

PWM0 channel 0 routes to P0.20, active high. Its proposed period is 4 ms (250 Hz),
an audit proposal, not a measured or separately confirmed product frequency.
At 1%, 40 us exceeds TPS7510x's recommended 33 us on-time. The zephyr,user
pwms property records this proposal; no application consumes it,
sets duty cycle or starts PWM. There is no brightness driver or automatic failover.
Never fit R7 and R8 together: these are alternate sources, not concurrent drivers.
The test changes no resistor, BOM, KiCad flag or hardware.

P0.28/B11 gates shared 1.2 V and peripheral 1.8 V (sensors plus panel VDDI/DCX);
P0.29/A10 gates panel 3.1 V VCI. VDDI must precede VCI. Powered mode-0 idle
(SCK low, CS inactive, valid reset/TE behavior) and unpowered interface-at-GND
requirements are different phases. The SPI sleep state merely disconnects the
input buffer and requests weak pull-downs; it does not drive guaranteed GND or
implement safe CS/reset/TE shutdown. A future power sequence must establish
suitable levels before rail-off, avoid MCU pull-ups feeding unpowered inputs,
and account for losing sensor wake/acquisition when the shared rail is off.
No runtime power manager or operational sequence is implemented here.

Generated checks cover pin choices, frequency, mode flags, drive mode, fallback
period/polarity, ownership and linked controller objects. Native nRF Connect
Devicetree Visual Editor/Pin Overview inspection is PENDING: no suitable native
editor surface was available. Physical timing/DC/slew/rail/TE/PWM qualification,
probe connection, flashing and target execution remain unperformed.

Complete nRF52840 migration handoff (2026-11-06)
----------------------------------------------
Physical reference, all 74 pads, fresh actual-vs-intended wiring, qualification
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
