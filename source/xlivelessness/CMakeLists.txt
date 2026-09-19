cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(
    XLiveLessNess
    VERSION 0.0.1
    # C is needed for vendored C sources (utils/puff.c): the *.c glob predates any C file, and
    # without the language declared the Makefile/clang-cl CI generates a C rule with uninitialized
    # flags that silently emits no object, failing only at link.
    LANGUAGES C CXX
)

# export compile_commands.json for editors like VSCode/Zed
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# use folder-hierarchy in VS
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(XLLN_BUILD_STATIC "Build XLLN as static library" OFF)

# source directory override guard
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory for build!")
endif()

string(REPLACE "\\" "/" WIN_DIR "$ENV{WINDIR}")

# Allow passing a custom path to a WINE Prefix using -DWINE_PREFIX_DIR=...
set(WINE_PREFIX_DIR "$ENV{WINEPREFIX}" CACHE PATH "Path to a WINE Prefix")

# Set the default WINE Prefix location if not already explicitly set.
if(NOT WINE_PREFIX_DIR AND DEFINED ENV{WINEPATH})
    set(WINE_PREFIX_DIR "$ENV{HOME}/.wine")
endif()

# Check if the WINE Prefix is 32 or 64 bit.
if(WINE_PREFIX_DIR)
    if(NOT EXISTS "${WINE_PREFIX_DIR}/drive_c/windows/syswow64/")
        set(WINE_PREFIX_ARCH_32 "32")
    endif()
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    if(WINE_PREFIX_DIR)
        if(WINE_PREFIX_ARCH_32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
            # If WINE Prefix only supports 32 bit or compiling for other architecture.
            set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE "${WINE_PREFIX_DIR}/drive_c/windows/system32")
        else()
            set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE "${WINE_PREFIX_DIR}/drive_c/windows/syswow64")
        endif()
    else()
        if(DEFINED ENV{ProgramFiles\(x86\)} AND CMAKE_SIZEOF_VOID_P EQUAL 4)
            # If Windows OS is 64 bit and compiling for 32 bit.
            set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE "${WIN_DIR}/SysWOW64")
        else()
            set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE "${WIN_DIR}/System32")
        endif()
        
    endif()
endif()

# Use the static debug CRT (like the previous Visual Studio projects) so that debug builds also
# load on machines without Visual Studio installed (the debug CRT is not redistributable). This
# must be set before the CPM packages are included so the dependencies use the same CRT.
add_compile_options(
    $<$<CONFIG:Debug>:/MTd>
)

# use https://github.com/cpm-cmake/CPM.cmake for dependency management
include(cmake/cpm.cmake)
# pull third-party packages using CPM (Opus/RapidXML/DXSDK)
include(cmake/packages.cmake)

# collect project files
file(GLOB_RECURSE headers_h CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/xlivelessness/*.h")
file(GLOB_RECURSE headers_hpp CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/xlivelessness/*.hpp")
file(GLOB_RECURSE sources_c CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/xlivelessness/*.c")
file(GLOB_RECURSE sources_cpp CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/xlivelessness/*.cpp")
list(APPEND sources "${CMAKE_CURRENT_SOURCE_DIR}/xlivelessness/xlive_exports.def"
     "${CMAKE_CURRENT_SOURCE_DIR}/xlivelessness/xlive.rc"
)

# Create automatic IDE folders based on the disk structure.
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${headers_h} ${headers_hpp} ${sources_c} ${sources_cpp} ${sources})

if(XLLN_BUILD_STATIC)
    set(LIBTYPE "STATIC")
else()
    set(LIBTYPE "SHARED")
endif()

add_library(${PROJECT_NAME} ${LIBTYPE})
target_sources(${PROJECT_NAME} PUBLIC ${headers_h} ${headers_hpp} ${sources_c} ${sources_cpp} ${sources})

# link necessary libraries
target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE Opus::opus
            DXSDK::DX9
            RapidXML
            ws2_32
            iphlpapi
            comctl32
            winmm
            d2d1
            bcrypt
)

set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES CXX_STANDARD 11
               CXX_STANDARD_REQUIRED ON
               OUTPUT_NAME "xlive"
)

string(TIMESTAMP DATETIME_YEAR "%Y")
string(TIMESTAMP DATETIME_MONTH "%m")
string(TIMESTAMP DATETIME_DAY "%d")
string(TIMESTAMP DATETIME_HOUR "%H")
string(TIMESTAMP DATETIME_MIN "%M")
string(TIMESTAMP DATETIME_SEC "%S")
target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE UNICODE
            _UNICODE
            "PROJECT_NAME=xlive"
            "BUILD_DATETIME_YEAR=${DATETIME_YEAR}"
            "BUILD_DATETIME_MONTH=${DATETIME_MONTH}"
            "BUILD_DATETIME_DAY=${DATETIME_DAY}"
            "BUILD_DATETIME_HOUR=${DATETIME_HOUR}"
            "BUILD_DATETIME_MINUTE=${DATETIME_MIN}"
            "BUILD_DATETIME_SECOND=${DATETIME_SEC}"
            $<$<CONFIG:Debug>:DEBUG>
            $<$<CONFIG:Debug>:_DEBUG>
)

target_compile_options(
    ${PROJECT_NAME}
    PRIVATE $<$<CONFIG:Debug>:/Od>  # Optimization=Disabled
            $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:/ZI> # DebugInformationFormat=EditAndContinue
            $<$<AND:$<CONFIG:Debug>,$<AND:$<CXX_COMPILER_ID:Clang>,$<BOOL:${MSVC}>>>:/Zi> # use Zi for clang since ZI is not supported
            $<$<CONFIG:Debug>:/hotpatch> # CreateHotpatchableImage=true
            $<$<CONFIG:Release>:/O2> # Optimization=MaxSpeed
            $<$<CONFIG:Release>:/Zi> # generate pdb files for Release builds too
            $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/GL> # FunctionLevelLinking=true
            $<$<CONFIG:Release>:/Oi> # IntrinsicFunctions=true
            $<$<CXX_COMPILER_ID:Clang>:-Wno-c++11-narrowing> # disable c++11 narrowing error
            $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CONFIG:Debug>>:-g> # generate pdb files with clang
            $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CONFIG:Debug>>:-gdwarf> # embed dwarf debug symbols in binary with clang
)

target_link_options(${PROJECT_NAME} PRIVATE
    # SubSystem=Windows like the previous Visual Studio projects. Set explicitly since
    # CMAKE_WIN32_EXECUTABLE only reaches library targets on the Visual Studio generator.
    /SUBSYSTEM:WINDOWS
    $<$<CONFIG:Release>:/DEBUG> # write the pdb file
    $<$<CONFIG:Debug>:/SAFESEH:NO> # ImageHasSafeExceptionHandlers=false
    $<$<CONFIG:Release>:/LTCG> # Required with /GL
    # /DEBUG turns these two linker defaults off so set them back explicitly.
    $<$<CONFIG:Release>:/OPT:REF> # OptimizeReferences=true
    $<$<CONFIG:Release>:/OPT:ICF> # EnableCOMDATFolding=true
)

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<0:>")
set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<0:>")
set_target_properties(${PROJECT_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<0:>")

# Set Visual Studio specific properties.
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES VS_USER_PROPS "${CMAKE_CURRENT_SOURCE_DIR}/bin/TitleBinary.props")
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_COMMAND "$(TitleBinaryPath)$(TitleBinary)")
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "-xllndebug -xllndebuglog $(TitleBinaryArgs)")
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(TitleBinaryPath)")
