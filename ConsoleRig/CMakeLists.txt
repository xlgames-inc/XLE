
set(Src
    AttachableLibrary_WinAPI.cpp
    Console.cpp
    DebugUtil.cpp
    GlobalServices.cpp
    Log.cpp
    OutputStream.cpp
    Plugins.cpp
    ${CMAKE_BINARY_DIR}/ConsoleRig/Version.cpp)
BasicLibrary(ConsoleRig "${Src}")

# Foreign Common
target_include_directories(ConsoleRig PRIVATE ${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen ${FOREIGN_DIR}/easyloggingpp ${FOREIGN_DIR}/TinyThreadPP/source ${FOREIGN_DIR}/cml-2_1_2 ${FOREIGN_DIR})
target_include_directories(ConsoleRig PRIVATE ${FOREIGN_DIR}/LuaBridge/Source/LuaBridge)
target_include_directories(ConsoleRig PRIVATE ${FOREIGN_DIR}/Lua/src)

##################################################################################################################################

find_package(Git)
if(NOT Git_FOUND)
  message("Did not find git for version number checks. Build commit and tag information won't be correctly assigned")
endif()

# the commit's SHA1, and whether the building workspace was dirty or not
execute_process(
  COMMAND "${GIT_EXECUTABLE}" describe --match=NeVeRmAtCh --always --abbrev=40 --dirty
  WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
  OUTPUT_VARIABLE GIT_SHA1
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  
# Get the current working branch
execute_process(
  COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the latest abbreviated commit hash of the working branch
execute_process(
  COMMAND "${GIT_EXECUTABLE}" log -1 --format=%h
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND "${GIT_EXECUTABLE}" describe
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE GIT_TAG
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M")

configure_file(
  ${CMAKE_CURRENT_LIST_DIR}/Version.in.cpp
  ${CMAKE_BINARY_DIR}/ConsoleRig/Version.cpp
)
