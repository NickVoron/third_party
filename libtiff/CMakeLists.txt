file(GLOB_RECURSE allSourceFiles
    "*.h"
    "*.hpp"
    "*.hxx"
    "*.cpp"
    "*.c"
    "*.cxx"
)

list(REMOVE_ITEM allSourceFiles 
	${CMAKE_CURRENT_SOURCE_DIR}/tif_vms.c 
	${CMAKE_CURRENT_SOURCE_DIR}/tif_wince.c 
	${CMAKE_CURRENT_SOURCE_DIR}/tif_unix.c 
	)

if(NOT USE_WINDOWS)
list(REMOVE_ITEM allSourceFiles ${CMAKE_CURRENT_SOURCE_DIR}/tif_win32.c)
endif()

get_filename_component(LibraryId ${CMAKE_CURRENT_SOURCE_DIR} NAME)
string(REPLACE " " "_" LibraryId ${LibraryId})
add_library(${LibraryId} ${allSourceFiles})
set_target_properties(${LibraryId} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${SharedTecINT})