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
parser.add_argument("--build-only", action="store_true")
parser.add_argument("--smoke", action="store_true", help="Run headless for two seconds")
parser.add_argument("--check-controls", action="store_true", help="Check wheel and tap controls headlessly")
parser.add_argument("--capture", help="Save a BMP screenshot during --check-controls")
parser.add_argument("--gallery", help="Check all V1 pages and save screenshots to this directory")
parser.add_argument("--water-check", action="store_true")
parser.add_argument("--dive-check", action="store_true")
parser.add_argument("--scuba-check", action="store_true", help="Exercise the scuba UI with accelerated scenarios")
args = parser.parse_args()
if args.scuba_check and not args.gallery:
    parser.error("--scuba-check requires --gallery DIRECTORY")
root = Path(__file__).resolve().parent
# Zephyr's build scripts do not support spaces in application/build paths.
# Keep editable sources in the user's folder and build a fresh source copy.
tag = hashlib.sha256(str(root).encode()).hexdigest()[:10]
workspace = Path(tempfile.gettempdir()) / ("dive-watch-lvgl-" + tag)
source = workspace / "app"
source.mkdir(parents=True, exist_ok=True)
for name in ("CMakeLists.txt", "prj.conf", "app.overlay"):
    shutil.copy2(root / name, source / name)
shutil.copytree(root / "src", source / "src", dirs_exist_ok=True)
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
    "-DBUILD_VERSION=local", "-DUSER_CACHE_DIR=" + str(workspace / ".cache/zephyr"),
], cwd=sdk, env=env, check=True)
if not args.build_only:
    command = [str(workspace / "build/zephyr/zephyr.exe")]
    if args.scuba_check:
        env["DIVE_SIM_SCUBA_CHECK"] = "1"
    if args.dive_check:
        env["DIVE_SIM_DIVE_CHECK"] = "1"
    if args.water_check:
        env["DIVE_SIM_WATER_CHECK"] = "1"
        env["SDL_VIDEODRIVER"] = "dummy"
        command += ["--stop_at=3"]
    if args.gallery:
        gallery = Path(args.gallery).resolve()
        gallery.mkdir(parents=True, exist_ok=True)
        env["SDL_VIDEODRIVER"] = "dummy"
        env["DIVE_SIM_GALLERY"] = str(gallery)
        if not args.water_check:
            command += ["--stop_at=34" if args.scuba_check else "--stop_at=23" if args.dive_check else "--stop_at=58"]
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
