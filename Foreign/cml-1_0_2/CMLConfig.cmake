# -*- cmake -*- -----------------------------------------------------------
# @@
#*-------------------------------------------------------------------------
# @file
# @brief

# The path to CMLConfig.cmake, set by FIND_PACKAGE(CML):
IF(CML_DIR)

  # Compute CML_INSTALL_DIR from CML_DIR:
  IF(WIN32)

    # On Windows, CMLConfig.cmake is in <install>:
    SET(CML_INSTALL_DIR "${CML_DIR}")
  ELSE(WIN32)

    # On UNIX, includes are in <install>/include/cml and libraries are in
    # <install>/lib/cml:
    SET(CML_INSTALL_DIR "${CML_DIR}/../..")
  ENDIF(WIN32)

  # Use the configured relative paths when CML was built to
  # initialize the library and Boost header paths:
  SET(CML_LIBRARY_DIR "${CML_INSTALL_DIR}/")
  SET(CML_HEADER_DIR "${CML_INSTALL_DIR}/")
ELSE(CML_DIR)
  MESSAGE(FATAL_ERROR "Can't find a CML installation.")
ENDIF(CML_DIR)

# The configured build type for this installation:
SET(CML_BUILD_TYPE "Release")

# File included from CMakeLists.txt to setup CML:
SET(CML_USE_FILE "${CML_LIBRARY_DIR}/UseCML.cmake")

# XXX Importing build settings for a header-only library leads to annoying
# "Setting CML_BUILD_TYPE to T" warnings:
# File containing the original CML build configuration:
# SET(CML_BUILD_SETTINGS_FILE "${CML_DIR}/CMLBuildSettings.cmake")

# Setup variables for external builds against CML:
SET(CML_HEADER_PATH "${CML_HEADER_DIR}")
SET(CML_LIBRARY_PATH "${CML_LIBRARY_DIR}")

# --------------------------------------------------------------------------
# vim:ft=cmake
