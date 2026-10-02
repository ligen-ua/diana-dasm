cmake_minimum_required(VERSION 3.16)

# One version for both builds: the one orthia --version and the Windows version resource show
file(STRINGS "src/orthia/orthia_disasm_ui/orthia_version.h" ORTHIA_VERSION_LINES
     REGEX "#define ORTHIA_UI_VER_(MAJOR|MINOR|PATCH|REVISION) ")
foreach(line ${ORTHIA_VERSION_LINES})
    string(REGEX MATCH "ORTHIA_UI_VER_([A-Z]+) +([0-9]+)" _ "${line}")
    set(ORTHIA_VER_${CMAKE_MATCH_1} ${CMAKE_MATCH_2})
endforeach()

project(diana_dasm
    VERSION ${ORTHIA_VER_MAJOR}.${ORTHIA_VER_MINOR}.${ORTHIA_VER_PATCH}.${ORTHIA_VER_REVISION}
    LANGUAGES C CXX)

# Default to Release if no build type specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug|Release)" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}, version ${PROJECT_VERSION}")

# std::any, std::optional, structured bindings: not every supported compiler defaults to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(_DEBUG)
endif()

# Release keeps debug info in the build tree: `cmake --install <dir> --strip` strips the installed
# copies, and distribution packagers strip and split the debug info themselves
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    string(APPEND CMAKE_C_FLAGS_RELEASE " -g")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " -g")
endif()

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

# Install layout (prefix /usr/local by default):
#   bin/orthia
#   lib/orthia/lib*.so      private libraries, found through RUNPATH $ORIGIN/../lib/orthia
#   share/doc/orthia/       README, license, notices, docs
set(CMAKE_INSTALL_DOCDIR "share/doc/orthia" CACHE PATH "Documentation directory")
include(GNUInstallDirs)
set(ORTHIA_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}/orthia")

enable_testing()

add_subdirectory(src)

set(ORTHIA_SHARED_LIBRARIES diana_core utf8rewind orthia_lib orthia_ui)
set_target_properties(${ORTHIA_SHARED_LIBRARIES} PROPERTIES INSTALL_RPATH "$ORIGIN")
set_target_properties(orthia_disasm_ui PROPERTIES INSTALL_RPATH "$ORIGIN/../${ORTHIA_INSTALL_LIBDIR}")

install(TARGETS orthia_disasm_ui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS ${ORTHIA_SHARED_LIBRARIES} LIBRARY DESTINATION ${ORTHIA_INSTALL_LIBDIR})
install(FILES README.md LICENSE THIRD_PARTY_NOTICES.md docs/orthia.md docs/databases.md
        DESTINATION ${CMAKE_INSTALL_DOCDIR})

# `cpack` in the build folder: a relocatable tarball of the install tree, stripped
set(CPACK_PACKAGE_NAME "orthia")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
string(TOLOWER "orthia-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}" CPACK_PACKAGE_FILE_NAME)
set(CPACK_PACKAGE_VENDOR "diana-dasm")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Orthia: console disassembler with a text UI for PE and ELF files and live processes")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ligen-ua/diana-dasm")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR "TGZ")
set(CPACK_STRIP_FILES ON)
set(CPACK_SOURCE_GENERATOR "")
include(CPack)
