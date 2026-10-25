#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
exec python3 simulator/ble_simulator.py "$@"
