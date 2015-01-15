# -*- cmake -*- -----------------------------------------------------------
# @@
#*-------------------------------------------------------------------------
# @file
# @brief

# Build settings to use CML in an external project.

# Load the original settings used to build CML:
IF(CML_BUILD_SETTINGS_FILE)
  INCLUDE(CMakeImportBuildSettings)
  CMAKE_IMPORT_BUILD_SETTINGS(${CML_BUILD_SETTINGS_FILE})
ENDIF(CML_BUILD_SETTINGS_FILE)

# Setup include paths:
INCLUDE_DIRECTORIES(BEFORE ${CML_HEADER_PATH})

# --------------------------------------------------------------------------
# vim:ft=cmake
