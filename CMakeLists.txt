cmake_minimum_required(VERSION 3.6)



# SUBDIRLIST(SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})

#add_subdirectory("libjpeg")
#add_subdirectory("libopenjpeg")
#add_subdirectory("libpng")
#add_subdirectory("libraw")
#add_subdirectory("libtiff")
#add_subdirectory("openexr")

add_subdirectory("libogg")
add_subdirectory("libpsd")
add_subdirectory("libtheora")
add_subdirectory("libutp")
add_subdirectory("libvorbis")
add_subdirectory("minizip")
add_subdirectory("nvtristrip")
add_subdirectory("rijndael")
add_subdirectory("udt")
add_subdirectory("utfcvt")
add_subdirectory("zlib")
add_subdirectory("libfreeimage")
set_property(TARGET FreeImage PROPERTY FOLDER "freeimage")






