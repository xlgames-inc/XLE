file(GLOB Src "*.cpp")
file(GLOB Headers "*.h")

project (ShaderScan)
source_group("" FILES ${Src} ${Headers})       # Push all files into the root folder
add_executable(ShaderScan ${Src} ${Headers})
set_target_properties(ShaderScan PROPERTIES VS_USER_PROPS "${CMAKE_SOURCE_DIR}/Main.props")
include_directories(${XLE_DIR})

# The CMake Visual Studio generator has a hack that disables the LinkLibraryDependencies setting in 
# the output project (see cmGlobalVisualStudio8Generator::NeedLinkLibraryDependencies) unless there are
# external project dependencies. It's frustrating because we absolutely need that on. To get around the
# problem, we'll link in a dummy external project that just contains nothing. This causes cmake to 
# enable the LinkLibraryDependencies flag, and hopefully has no other side effects.
include_external_msproject(generator_dummy ${CMAKE_SOURCE_DIR}/generator_dummy.vcxproj)
add_dependencies(ShaderScan Math Utility Assets ConsoleRig ShaderScan RenderCore generator_dummy)
# Foreign

# Foreign Common 
include_directories(${FOREIGN_DIR}/half-1.9.2/include ${FOREIGN_DIR}/eigen ${FOREIGN_DIR}/easyloggingpp ${FOREIGN_DIR}/TinyThreadPP/source ${FOREIGN_DIR}/cml-1_0_2 ${FOREIGN_DIR})
