# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/Bilal/Documents/GitHub/Dive_Computer/V2/Firmware")
  file(MAKE_DIRECTORY "/home/Bilal/Documents/GitHub/Dive_Computer/V2/Firmware")
endif()
file(MAKE_DIRECTORY
  "/home/Bilal/Documents/GitHub/Dive_Computer/V2/Firmware/build/Firmware"
  "/home/Bilal/Documents/GitHub/Dive_Computer/V2/Firmware/build/_sysbuild/sysbuild/images/Firmware-prefix"
  "/home/Bilal/Documents/GitHub/Dive_Computer/V2/Firmware/build/_sysbuild/sysbuild/images/Firmware-prefix/tmp"
  "/home/Bilal/Documents/GitHub/Dive_Computer/V2/Firmware/build/_sysbuild/sysbuild/images/Firmware-prefix/src/Firmware-stamp"
  "/home/Bilal/Documents/GitHub/Dive_Computer/V2/Firmware/build/_sysbuild/sysbuild/images/Firmware-prefix/src"
  "/home/Bilal/Documents/GitHub/Dive_Computer/V2/Firmware/build/_sysbuild/sysbuild/images/Firmware-prefix/src/Firmware-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/Bilal/Documents/GitHub/Dive_Computer/V2/Firmware/build/_sysbuild/sysbuild/images/Firmware-prefix/src/Firmware-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/Bilal/Documents/GitHub/Dive_Computer/V2/Firmware/build/_sysbuild/sysbuild/images/Firmware-prefix/src/Firmware-stamp${cfgdir}") # cfgdir has leading slash
endif()
