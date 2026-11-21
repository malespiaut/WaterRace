# WaterRace Linux port, P0: compile and link the retail English game against Pomme.
# Source list, exclusions and -fpermissive files come from port/*.txt (see port/check-compile.sh).
cmake_minimum_required(VERSION 3.16)
project(WaterRace CXX)
set(CMAKE_CXX_STANDARD 17)

# Pomme: Mac API layer. Sound mixer, video and QD3D stay off, as in PORTING.md §5.5.
set(POMME_NO_SOUND_MIXER ON CACHE BOOL "")
set(POMME_NO_VIDEO ON CACHE BOOL "")
set(POMME_NO_QD3D ON CACHE BOOL "")
add_subdirectory(Pomme)

function(read_list file out)
  file(STRINGS "${CMAKE_SOURCE_DIR}/${file}" lines REGEX "^[^#].*")
  set(${out} ${lines} PARENT_SCOPE)
endfunction()
read_list(port/excluded.txt EXCLUDED)
read_list(port/permissive.txt PERMISSIVE)

file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/Source/*.cp" "${CMAKE_SOURCE_DIR}/Source/*.c")
set(GAME_SOURCES "")
foreach(src IN LISTS ALL_SOURCES)
  file(RELATIVE_PATH rel "${CMAKE_SOURCE_DIR}/Source" "${src}")
  set_source_files_properties("${src}" PROPERTIES LANGUAGE CXX)
  if(NOT rel IN_LIST EXCLUDED)
    list(APPEND GAME_SOURCES "${src}")
  endif()
  if(rel IN_LIST PERMISSIVE)
    set_source_files_properties("${src}" PROPERTIES COMPILE_OPTIONS "-fpermissive")
  endif()
endforeach()

file(GLOB_RECURSE SOURCE_DIRS LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/Source/*")
set(INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Source")
foreach(d IN LISTS SOURCE_DIRS)
  if(IS_DIRECTORY "${d}")
    list(APPEND INCLUDE_DIRS "${d}")
  endif()
endforeach()

list(LENGTH GAME_SOURCES N_GAME)
message(STATUS "game sources: ${N_GAME}")
add_executable(WaterRace ${GAME_SOURCES} port/TextureLoader.cpp port/link-stubs.cpp)
target_include_directories(WaterRace PRIVATE ${INCLUDE_DIRS} port port/stubs Pomme/src)
target_compile_definitions(WaterRace PRIVATE POMME_NO_SOUND_MIXER POMME_NO_VIDEO POMME_NO_QD3D)
target_compile_options(WaterRace PRIVATE -w -include MacHeadersPPC++)
find_package(OpenGL REQUIRED)
target_link_libraries(WaterRace PRIVATE Pomme OpenGL::GL)  # Pomme brings SDL3 (PUBLIC)

# Headless PNG texture loader test (no Pomme, no GL): ctest --test-dir build
enable_testing()
add_executable(test-texture-loader port/test-texture-loader.cpp port/TextureLoader.cpp)
target_compile_definitions(test-texture-loader PRIVATE "WATERRACE_TEXTURES_DIR=\"${CMAKE_SOURCE_DIR}/WaterRace Data/Textures\"")
target_compile_options(test-texture-loader PRIVATE -Wall -Wno-multichar -Wno-unused-function)
add_test(NAME texture-loader COMMAND test-texture-loader)

# Headless Textures.cp test: real MetaFiles -> Texture_NewFromCompressedTexture -> TQAImage chain vs manifest.json
add_executable(test-textures port/test-textures.cpp "${CMAKE_SOURCE_DIR}/Source/Infinity Engine/Core/Textures.cp" port/TextureLoader.cpp)
target_include_directories(test-textures PRIVATE ${INCLUDE_DIRS} port port/stubs Pomme/src)
target_compile_definitions(test-textures PRIVATE POMME_NO_SOUND_MIXER POMME_NO_VIDEO POMME_NO_QD3D "WATERRACE_ROOT=\"${CMAKE_SOURCE_DIR}\"")
target_compile_options(test-textures PRIVATE -w -include MacHeadersPPC++)
target_link_libraries(test-textures PRIVATE Pomme)
add_test(NAME textures COMMAND test-textures)

# OGL_Support.cp test: RAVE Gouraud triangles, meshes, lines and real textures on GL, checked per pixel. Needs a display (xvfb-run).
add_executable(test-ogl-support port/test-ogl-support.cpp "${CMAKE_SOURCE_DIR}/Source/Infinity Engine/Core/OGL_Support.cp" port/TextureLoader.cpp)
target_compile_definitions(test-ogl-support PRIVATE "WATERRACE_TEXTURES_DIR=\"${CMAKE_SOURCE_DIR}/WaterRace Data/Textures\"")
target_include_directories(test-ogl-support PRIVATE ${INCLUDE_DIRS} port port/stubs Pomme/src)
target_compile_definitions(test-ogl-support PRIVATE POMME_NO_SOUND_MIXER POMME_NO_VIDEO POMME_NO_QD3D)
target_compile_options(test-ogl-support PRIVATE -w -include MacHeadersPPC++)
target_link_libraries(test-ogl-support PRIVATE Pomme OpenGL::GL)
find_program(XVFB_RUN xvfb-run)
if(XVFB_RUN)
  add_test(NAME ogl-support COMMAND ${XVFB_RUN} -a $<TARGET_FILE:test-ogl-support>)
else()
  add_test(NAME ogl-support COMMAND test-ogl-support)
endif()

# MetaFile Read.cp test: Logo Intro tag table and its 16 shapes read from the big-endian layout (PORTING.md 5.2)
add_executable(test-metafile port/test-metafile.cpp)
target_include_directories(test-metafile PRIVATE ${INCLUDE_DIRS} port port/stubs Pomme/src)
target_compile_definitions(test-metafile PRIVATE POMME_NO_SOUND_MIXER POMME_NO_VIDEO POMME_NO_QD3D "WATERRACE_ROOT=\"${CMAKE_SOURCE_DIR}\"")
target_compile_options(test-metafile PRIVATE -w -include MacHeadersPPC++)
target_link_libraries(test-metafile PRIVATE Pomme)
add_test(NAME metafile COMMAND test-metafile)

# Text Engine.cp test (#64): text lines on 32-bit pixels in a 32-bit colour, checked per pixel
add_executable(test-text-engine port/test-text-engine.cpp "${CMAKE_SOURCE_DIR}/Source/WaterRace final/Text Engine.cp")
target_include_directories(test-text-engine PRIVATE ${INCLUDE_DIRS} port port/stubs Pomme/src)
target_compile_definitions(test-text-engine PRIVATE POMME_NO_SOUND_MIXER POMME_NO_VIDEO POMME_NO_QD3D)
target_compile_options(test-text-engine PRIVATE -w -include MacHeadersPPC++)
target_link_libraries(test-text-engine PRIVATE Pomme)
add_test(NAME text-engine COMMAND test-text-engine)

# Practice Zone load test (#45): Terrain, TerrainMesh, SeaMesh, View and the texture lists read from the big-endian layout
add_executable(test-terrain port/test-terrain.cpp "${CMAKE_SOURCE_DIR}/Source/Infinity Engine/Core/Textures.cp" port/TextureLoader.cpp)
target_include_directories(test-terrain PRIVATE ${INCLUDE_DIRS} port port/stubs Pomme/src)
target_compile_definitions(test-terrain PRIVATE POMME_NO_SOUND_MIXER POMME_NO_VIDEO POMME_NO_QD3D "WATERRACE_ROOT=\"${CMAKE_SOURCE_DIR}\"")
target_compile_options(test-terrain PRIVATE -w -include MacHeadersPPC++)
target_link_libraries(test-terrain PRIVATE Pomme)
add_test(NAME terrain COMMAND test-terrain)

# P4 data check (#70): every MetaFile through the game's loaders under ASan/UBSan, dump diffed against tools/metadump.py
add_executable(test-datacheck port/test-datacheck.cpp "${CMAKE_SOURCE_DIR}/Source/Infinity Engine/Core/Textures.cp" port/TextureLoader.cpp)
target_include_directories(test-datacheck PRIVATE ${INCLUDE_DIRS} port port/stubs Pomme/src)
target_compile_definitions(test-datacheck PRIVATE POMME_NO_SOUND_MIXER POMME_NO_VIDEO POMME_NO_QD3D "WATERRACE_ROOT=\"${CMAKE_SOURCE_DIR}\"")
target_compile_options(test-datacheck PRIVATE -w -include MacHeadersPPC++ -fsanitize=address,undefined -fno-sanitize-recover=all)
target_link_options(test-datacheck PRIVATE -fsanitize=address,undefined)
target_link_libraries(test-datacheck PRIVATE Pomme)
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME datacheck COMMAND sh -c "\"$1\" tools/metadump.py 'WaterRace Data' > \"$3/metadump.txt\" && \"$2\" > \"$3/datacheck.txt\" && diff \"$3/metadump.txt\" \"$3/datacheck.txt\""
           sh ${Python3_EXECUTABLE} $<TARGET_FILE:test-datacheck> ${CMAKE_CURRENT_BINARY_DIR} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  # the loaders' callers own what they return; the test keeps it all
  set_tests_properties(datacheck PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

# Logo Intro load test (#43): InfinityPlayer_LoadAnimation with the default texture set, on GL. Every game file but Boot.cp, which the test includes.
set(PLAYER_TEST_SOURCES ${GAME_SOURCES})
list(FILTER PLAYER_TEST_SOURCES EXCLUDE REGEX "/WaterRace final/Boot\\.cp$")
add_executable(test-player port/test-player.cpp ${PLAYER_TEST_SOURCES} port/TextureLoader.cpp port/link-stubs.cpp)
target_include_directories(test-player PRIVATE ${INCLUDE_DIRS} port port/stubs Pomme/src)
target_compile_definitions(test-player PRIVATE POMME_NO_SOUND_MIXER POMME_NO_VIDEO POMME_NO_QD3D "WATERRACE_ROOT=\"${CMAKE_SOURCE_DIR}\"")
target_compile_options(test-player PRIVATE -w -include MacHeadersPPC++)
target_link_libraries(test-player PRIVATE Pomme OpenGL::GL)
if(XVFB_RUN)
  add_test(NAME player COMMAND ${XVFB_RUN} -a $<TARGET_FILE:test-player>)
else()
  add_test(NAME player COMMAND test-player)
endif()

# FX shaders on GL (#46): sky, sprite, particle bursts, lens, wireframe and Post FX lens flare through the engine's shaders,
# checked against a software composite of the submitted quads; writes fx-scene.png. Needs a display (xvfb-run).
add_executable(test-fx port/test-fx.cpp ${PLAYER_TEST_SOURCES} port/TextureLoader.cpp port/link-stubs.cpp)
target_include_directories(test-fx PRIVATE ${INCLUDE_DIRS} port port/stubs Pomme/src)
target_compile_definitions(test-fx PRIVATE POMME_NO_SOUND_MIXER POMME_NO_VIDEO POMME_NO_QD3D "WATERRACE_ROOT=\"${CMAKE_SOURCE_DIR}\"")
target_compile_options(test-fx PRIVATE -w -include MacHeadersPPC++)
target_link_libraries(test-fx PRIVATE Pomme OpenGL::GL)
if(XVFB_RUN)
  add_test(NAME fx COMMAND ${XVFB_RUN} -a $<TARGET_FILE:test-fx>)
else()
  add_test(NAME fx COMMAND test-fx)
endif()

# Ship and location previews (#74): Preload_DataFolders loads every 'shds'/'trds' preview from its PNG, sizes checked against the 'imag' PICT frames
set(PREVIEW_TEST_SOURCES ${PLAYER_TEST_SOURCES})
list(FILTER PREVIEW_TEST_SOURCES EXCLUDE REGEX "/WaterRace final/Data Files\\.cp$")
add_executable(test-previews port/test-previews.cpp ${PREVIEW_TEST_SOURCES} port/TextureLoader.cpp port/link-stubs.cpp)
target_include_directories(test-previews PRIVATE ${INCLUDE_DIRS} port port/stubs Pomme/src)
target_compile_definitions(test-previews PRIVATE POMME_NO_SOUND_MIXER POMME_NO_VIDEO POMME_NO_QD3D "WATERRACE_ROOT=\"${CMAKE_SOURCE_DIR}\"")
target_compile_options(test-previews PRIVATE -w -include MacHeadersPPC++ -fsanitize=address,undefined -fno-sanitize-recover=all)
target_link_options(test-previews PRIVATE -fsanitize=address,undefined)
target_link_libraries(test-previews PRIVATE Pomme OpenGL::GL)
add_test(NAME previews COMMAND test-previews)
set_tests_properties(previews PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")  # the preloaded data lives until exit
