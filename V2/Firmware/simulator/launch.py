#!/usr/bin/env python3
"""Build and run using the installed Nordic SDK without changing shell settings."""
import argparse
import json
import os
import hashlib
import shutil
import tempfile
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--bench", action="store_true", help="Calculated experimental OSTC4 diagnostics and depth graph")
parser.add_argument("--bench-profile", help="Replay CSV: timestamp_seconds,absolute_pressure_bar (implies --bench)")
parser.add_argument("--desaturation-check", action="store_true", help="Estimated desaturation summary live/render acceptance")
parser.add_argument("--checkpoint-check", action="store_true", help="Disk checkpoint controls, explicit clock assertion and recovered lock captures")
parser.add_argument("--gf-check", action="store_true", help="Atomic GF preparation editor and recorded-dive lock captures")
parser.add_argument("--restart-check", action="store_true", help="Synthetic checkpoint recovery and fault captures")
parser.add_argument("--calculated-check", action="store_true", help="Calculated watch and automatic diver captures")
parser.add_argument("--bench-check", action="store_true", help="Headless bench interaction and visible two-dive replay check")
parser.add_argument("--build-only", action="store_true")
parser.add_argument("--smoke", action="store_true", help="Run headless for two seconds")
parser.add_argument("--check-controls", action="store_true", help="Check wheel and tap controls headlessly")
parser.add_argument("--capture", help="Save a BMP screenshot during --check-controls")
parser.add_argument("--gallery", help="Check all V1 pages and save screenshots to this directory")
parser.add_argument("--scale-check", action="store_true", help="Check repeated Scale clicks, rendered frames and input responsiveness")
parser.add_argument("--water-check", action="store_true")
parser.add_argument("--dive-check", action="store_true")
parser.add_argument("--apnea-settings-check", action="store_true")
parser.add_argument("--apnea-check", action="store_true", help="Deterministic freediving session and Main render journey")
parser.add_argument("--scuba-check", action="store_true", help="Exercise the scuba UI with accelerated scenarios")
args = parser.parse_args()
args.bench = args.bench or bool(args.bench_profile) or args.bench_check or args.calculated_check or args.restart_check or args.gf_check or args.checkpoint_check or args.desaturation_check
if args.bench and (args.gallery or args.scuba_check or args.scale_check or args.check_controls or args.water_check or args.dive_check or args.apnea_check or args.apnea_settings_check):
    parser.error("Bench checks use --bench-check; legacy gallery/control checks run without --bench")
if (args.apnea_check or args.apnea_settings_check) and not args.gallery:
    parser.error("--apnea-check requires --gallery DIRECTORY")
if args.scuba_check and not args.gallery:
    parser.error("--scuba-check requires --gallery DIRECTORY")
root = Path(__file__).resolve().parent
# Zephyr's build scripts do not support spaces in application/build paths.
# Keep editable sources in the user's folder and build a fresh source copy.
tag = hashlib.sha256((str(root) + ("-bench" if args.bench else "")).encode()).hexdigest()[:10]
workspace = Path(tempfile.gettempdir()) / ("dive-watch-lvgl-" + tag)
source = workspace / "app"
source.mkdir(parents=True, exist_ok=True)
source_copy_stamp = workspace / ".content-aware-source-copy-v1"
refresh_source_timestamps = not source_copy_stamp.exists()

def copy_changed_source(src, dst):
    """Preserve incremental builds without importing stale checkout timestamps.

    A newly checked-out header can be older than a host runner object. copy2
    would retain that old time and make would silently reuse an incompatible ABI.
    Changed contents get a fresh destination timestamp; equal files stay untouched.
    """
    src, dst = Path(src), Path(dst)
    if refresh_source_timestamps or not dst.exists() or src.read_bytes() != dst.read_bytes():
        shutil.copy(src, dst)
    return str(dst)

for name in ("CMakeLists.txt", "prj.conf", "app.overlay"):
    copy_changed_source(root / name, source / name)
shutil.copytree(root / "src", source / "src", dirs_exist_ok=True, copy_function=copy_changed_source)
bench_options = ["-DDIVE_BENCH=OFF"]
if args.bench:
    core = workspace / "ostc4_baseline"
    shutil.copytree(root.parent / "ostc4_baseline", core, dirs_exist_ok=True, copy_function=copy_changed_source)
    core_build = workspace / "ostc4-build"
    core_env = os.environ.copy()
    core_env["CCACHE_DISABLE"] = "1"
    subprocess.run(["cmake", "-S", str(core), "-B", str(core_build), "-DBUILD_TESTING=OFF"], env=core_env, check=True)
    subprocess.run(["cmake", "--build", str(core_build), "--target", "ostc4_exposure", "-j4"], env=core_env, check=True)
    overlay = source / "app.overlay"
    overlay.write_text(overlay.read_text().replace("<620>","<1200>").replace("<420>","<800>"))
    with (source / "prj.conf").open("a") as config:
        config.write("\nCONFIG_CBPRINTF_FP_SUPPORT=y\n")
    bench_options = ["-DDIVE_BENCH=ON", "-DDIVE_OSTC4_LIBDIR=" + str(core_build)]
source_copy_stamp.touch()
sdk = Path(os.environ.get("DIVE_NCS_ROOT", str(Path.home() / "ncs/v3.4.1")))
toolchain = Path(os.environ.get("DIVE_TOOLCHAIN_ROOT", str(Path.home() / "ncs/toolchains/8285d8ad56")))
env = os.environ.copy()
for entry in json.loads((toolchain / "environment.json").read_text())["env_vars"]:
    key = entry["key"]
    value = (os.pathsep.join(str(toolchain / p) for p in entry["values"])
             if entry["type"] == "relative_paths" else entry["value"])
    if entry.get("existing_value_treatment") == "prepend_to" and env.get(key):
        value += os.pathsep + env[key]
    env[key] = value
env["ZEPHYR_BASE"] = str(sdk / "zephyr")
env["ZEPHYR_TOOLCHAIN_VARIANT"] = "host"
env["CCACHE_DISABLE"] = "1"
# Use Fedora's system libraries for native tools (readelf, SDL, GCC).
# Only the SDK's Python shared library directory is needed here.
env["LD_LIBRARY_PATH"] = str(toolchain / "usr/local/lib")
env["XDG_CACHE_HOME"] = str(workspace / ".cache")
subprocess.run([
    str(toolchain / "usr/local/bin/west"), "build", "--no-sysbuild", "-b", "native_sim/native/64",
    "-d", str(workspace / "build"), str(source), "--",
    *bench_options, "-DBUILD_VERSION=local", "-DUSER_CACHE_DIR=" + str(workspace / ".cache/zephyr"),
], cwd=sdk, env=env, check=True)
if not args.build_only:
    command = [str(workspace / "build/zephyr/zephyr.exe")]
    # Every automated run starts from isolated storage, even when the caller has
    # a normal preference directory configured in their environment.
    storage_fixture = None
    if (args.gallery or args.smoke or args.check_controls or args.scale_check or args.water_check or
        args.dive_check or args.apnea_check or args.apnea_settings_check or args.scuba_check or
        args.bench_check or args.calculated_check or args.restart_check or args.gf_check or args.checkpoint_check or args.desaturation_check):
        storage_fixture = tempfile.TemporaryDirectory(prefix="dive-watch-state-")
        env["DIVE_WATCH_STATE_DIR"] = storage_fixture.name
    if args.bench_profile:
        env["DIVE_SIM_BENCH_PROFILE"] = str(Path(args.bench_profile).resolve())
    if args.bench_check or args.calculated_check or args.restart_check or args.gf_check or args.checkpoint_check or args.desaturation_check:
        env["DIVE_SIM_BENCH_CHECK"] = "desaturation" if args.desaturation_check else "checkpoint" if args.checkpoint_check else "gf" if args.gf_check else "restart" if args.restart_check else "calculated" if args.calculated_check else "1"
        env["SDL_VIDEODRIVER"] = "dummy"
        command += ["--stop_at=10" if args.desaturation_check or args.checkpoint_check or args.gf_check else "--stop_at=20" if args.calculated_check else "--stop_at=15" if args.restart_check else "--stop_at=90", "--no-rt"]
    if args.scale_check:
        env["DIVE_SIM_SCALE_CHECK"] = "1"
    if args.scuba_check:
        env["DIVE_SIM_SCUBA_CHECK"] = "1"
    if args.apnea_settings_check:
        env["DIVE_SIM_APNEA_SETTINGS_CHECK"] = "1"
    if args.apnea_check:
        env["DIVE_SIM_APNEA_CHECK"] = "1"
    if args.dive_check:
        env["DIVE_SIM_DIVE_CHECK"] = "1"
    if args.scale_check:
        env["SDL_VIDEODRIVER"] = "dummy"
        command += ["--stop_at=13"]
    elif args.water_check:
        env["DIVE_SIM_WATER_CHECK"] = "1"
        env["SDL_VIDEODRIVER"] = "dummy"
        command += ["--stop_at=3"]
    if args.gallery:
        gallery = Path(args.gallery).resolve()
        gallery.mkdir(parents=True, exist_ok=True)
        env["SDL_VIDEODRIVER"] = "dummy"
        env["DIVE_SIM_GALLERY"] = str(gallery)
        if not args.water_check:
            command += ["--stop_at=22" if args.apnea_settings_check else "--stop_at=24" if args.apnea_check else "--stop_at=44" if args.scuba_check else "--stop_at=23" if args.dive_check else "--stop_at=58"]
    elif args.smoke or args.check_controls:
        env["SDL_VIDEODRIVER"] = "dummy"
        command += ["--stop_at=2"]
    if args.check_controls:
        env["DIVE_SIM_CHECK_CONTROLS"] = "1"
    if args.capture:
        env["DIVE_SIM_CAPTURE"] = str(Path(args.capture).resolve())
    try:
        subprocess.run(command, cwd=root, env=env, check=True)
    except KeyboardInterrupt:
        pass
    finally:
        if storage_fixture is not None:
            storage_fixture.cleanup()
