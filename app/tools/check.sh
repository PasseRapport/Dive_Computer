#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python3 -m unittest discover -s simulator -v
./gradlew :core:test :android:testDebugUnitTest :android:assembleDebug :android:lintDebug
