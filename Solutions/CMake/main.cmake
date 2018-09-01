message("Build type: ${CMAKE_BUILD_TYPE}")

get_filename_component(MAIN_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(XLE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)
get_filename_component(FOREIGN_DIR "${XLE_DIR}/Foreign/" ABSOLUTE)

macro(BasicLibrary LibName Src)
    project (LibName)
	source_group("" FILES ${Src})       # Push all files into the root folder
    add_library(${LibName} STATIC ${Src})
    if (MSVC)
        set_target_properties(${LibName} PROPERTIES VS_USER_PROPS "${MAIN_CMAKE_DIR}/Main.props")
    endif ()
    include_directories(${XLE_DIR})
    include_directories(${FOREIGN_DIR} ${FOREIGN_DIR}/Antlr-3.4/libantlr3c-3.4/include ${FOREIGN_DIR}/Antlr-3.4/libantlr3c-3.4 ${FOREIGN_DIR}/cml-1_0_2)
endmacro()

macro (FindProjectFiles retVal)
    file(GLOB prefilteredFiles *.cpp *.h)
	set(${retVal})
	foreach(f ${prefilteredFiles})
		if (NOT f MATCHES ".*_WinAPI.*")
		    list(APPEND ${retVal} ${f})
		endif ()
    endforeach ()
endmacro ()

