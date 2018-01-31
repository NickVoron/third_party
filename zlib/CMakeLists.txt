file(GLOB_RECURSE allSourceFiles
    "*.h"
    "*.c"
)

list(REMOVE_ITEM allSourceFiles 
	${CMAKE_CURRENT_SOURCE_DIR}/gzlib.c 
	${CMAKE_CURRENT_SOURCE_DIR}/gzread.c 
	${CMAKE_CURRENT_SOURCE_DIR}/gzwrite.c 
	)


get_filename_component(LibraryId ${CMAKE_CURRENT_SOURCE_DIR} NAME)
string(REPLACE " " "_" LibraryId ${LibraryId})
add_library(${LibraryId} ${allSourceFiles})
set_target_properties(${LibraryId} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${SharedTecINT})