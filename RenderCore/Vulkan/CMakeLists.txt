
set(Src
	Annotator.cpp
	DeviceVulkan.cpp
	Metal/Buffer.cpp
	Metal/CompiledShaderByteCode.cpp
	Metal/DescriptorSet.cpp
	Metal/DeviceContext.cpp
	Metal/Format.cpp
	Metal/FrameBuffer.cpp
	Metal/InputLayout.cpp
	Metal/NumericInterface.cpp
	Metal/ObjectFactory.cpp
	Metal/PipelineLayout.cpp
	Metal/DescriptorSetSignatureFile.cpp
	Metal/Pools.cpp
	Metal/QueryPool.cpp
	Metal/Resource.cpp
	Metal/Shader.cpp
	Metal/ShaderReflection.cpp
	Metal/State.cpp
	Metal/TextureView.cpp)

add_library(RenderCore_Vulkan STATIC ${Src} ${Headers})
xle_configure_library(RenderCore_Vulkan)
target_link_libraries(RenderCore_Vulkan PRIVATE ForeignMisc)
target_sources(RenderCore_Vulkan INTERFACE ${CMAKE_CURRENT_LIST_DIR}/RegisterVulkan.cpp)

############################################################################################################################################
set(XLE_VULKAN_SDK_DIR "" CACHE PATH "Vulkan SDK directory")

macro(find_vulkan)

	if (NOT INTERNAL_LAST_XLE_VULKAN_SDK_DIR STREQUAL XLE_VULKAN_SDK_DIR)
		message("Detected change in Vulkan configuration, searching again")
		set(INTERNAL_FOUND_VULKAN_INCLUDE_DIR)
		set(INTERNAL_FOUND_VULKAN_LIBRARY)
		set(INTERNAL_LAST_XLE_VULKAN_SDK_DIR ${XLE_VULKAN_SDK_DIR} CACHE PATH INTERNAL FORCE)
	endif ()

	# attempt to use "find_package(Vulkan)" to locate Vulkan
	if (NOT CMAKE_VERSION VERSION_LESS 3.7.0)
		find_package(Vulkan)
	endif()

	if (Vulkan_FOUND)
		set(INTERNAL_FOUND_VULKAN_INCLUDE_DIR Vulkan_INCLUDE_DIRS)
		set(INTERNAL_FOUND_VULKAN_LIBRARY Vulkan_LIBRARY)
	else ()
		find_path(INTERNAL_FOUND_VULKAN_INCLUDE_DIR vulkan/vulkan.h HINT "${XLE_VULKAN_SDK_DIR}/Include")
		if (CMAKE_SIZEOF_VOID_P EQUAL 8)
			find_library(INTERNAL_FOUND_VULKAN_LIBRARY vulkan-1 HINTS "${XLE_VULKAN_SDK_DIR}/Lib")
		elseif ()
			find_library(INTERNAL_FOUND_VULKAN_LIBRARY vulkan-1 HINTS "${XLE_VULKAN_SDK_DIR}/Lib32")
		endif ()
	endif ()

	if (INTERNAL_FOUND_VULKAN_INCLUDE_DIR STREQUAL INTERNAL_FOUND_VULKAN_INCLUDE_DIR-NOTFOUND 
		OR INTERNAL_FOUND_VULKAN_LIBRARY STREQUAL INTERNAL_FOUND_VULKAN_LIBRARY-NOTFOUND)

		# Most of the time, the remedy will be to just set XLE_VULKAN_SDK_DIR in the cmake cache
		# You'll usually grab that from https://www.lunarg.com/vulkan-sdk/
		message("INTERNAL_FOUND_VULKAN_INCLUDE_DIR: ${INTERNAL_FOUND_VULKAN_INCLUDE_DIR}")
		message("INTERNAL_FOUND_VULKAN_LIBRARY: ${INTERNAL_FOUND_VULKAN_LIBRARY}")
		message("XLE_VULKAN_SDK_DIR: ${XLE_VULKAN_SDK_DIR}")
		message(FATAL_ERROR "Could not find Vulkan headers and libraries. Either compile without this target included, or set the XLE_VULKAN_SDK_DIR cmake cache variables to the correct directories.")
	endif ()

	mark_as_advanced(INTERNAL_FOUND_VULKAN_INCLUDE_DIR)
	mark_as_advanced(INTERNAL_FOUND_VULKAN_LIBRARY)
	mark_as_advanced(INTERNAL_LAST_XLE_VULKAN_SDK_DIR)

endmacro ()
############################################################################################################################################

find_vulkan()
target_link_libraries(RenderCore_Vulkan PUBLIC ${INTERNAL_FOUND_VULKAN_LIBRARY})
target_include_directories(RenderCore_Vulkan PUBLIC ${INTERNAL_FOUND_VULKAN_INCLUDE_DIR})
