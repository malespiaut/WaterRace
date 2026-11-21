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
