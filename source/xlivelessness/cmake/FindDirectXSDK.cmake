# allow passing a custom DirectX SDK path using -DDXSDK_DIR=...
set(DXSDK_DIR "" CACHE PATH "Path to the DirectX SDK")

# use the DXSDK_DIR env var if available (is set by the installer)
if(NOT DXSDK_DIR AND DEFINED ENV{DXSDK_DIR})
    set(DXSDK_DIR $ENV{DXSDK_DIR})
endif()

# determine target arch
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DXSDK_ARCH "x64")
else()
    set(DXSDK_ARCH "x86")
endif()

# check install locations for header
find_path(DXSDK_INSTALL_DIR "Include/d3dx9.h"
    PATHS
        "${DXSDK_DIR}"
        "C:/Program Files/Microsoft DirectX SDK (June 2010)"
        "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)"
        "${WINE_PREFIX_DIR}/drive_c/Program Files/Microsoft DirectX SDK (June 2010)"
        "${WINE_PREFIX_DIR}/drive_c/Program Files (x86)/Microsoft DirectX SDK (June 2010)"
)

if(DXSDK_INSTALL_DIR)
    set(DXSDK_DIR "${DXSDK_INSTALL_DIR}")
    set(DXSDK_INCLUDE_DIR "${DXSDK_INSTALL_DIR}/Include")
    set(DXSDK_LIB_DIR "${DXSDK_INSTALL_DIR}/Lib/${DXSDK_ARCH}")
    if(NOT EXISTS "${DXSDK_INCLUDE_DIR}/d3d9.h" OR NOT EXISTS "${DXSDK_LIB_DIR}/d3d9.lib")
        message(FATAL_ERROR "The DirectX SDK install at ${DXSDK_INSTALL_DIR} appears to be corrupt.")
    endif()
    message(STATUS "Found DirectX SDK at ${DXSDK_INSTALL_DIR}")
else()
    message(FATAL_ERROR "DirectX SDK was not found on the system! Please install it.")
endif()

# setup header include list
file(GLOB dxsdk_headers "${DXSDK_DIR}/Include/*.*")
# remove D2D headers to avoid type redeclaration (those headers are already available from the WinSDK)
list(REMOVE_ITEM dxsdk_headers 
    "${DXSDK_DIR}/Include/D2D1.h"
    "${DXSDK_DIR}/Include/D2D1Helper.h"
    "${DXSDK_DIR}/Include/D2DBaseTypes.h"
    "${DXSDK_DIR}/Include/D2Derr.h"
)

function(add_directx_lib_targets libnames)
    foreach(lib IN LISTS libnames)
        set(target_name ${lib})
        set(lib_file "${lib}.lib")

        add_library(${target_name} STATIC IMPORTED GLOBAL)
        set_target_properties(${target_name} PROPERTIES
            IMPORTED_LOCATION "${DXSDK_LIB_DIR}/${lib_file}"
        )
    endforeach()
endfunction()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(DEBUG_SUFFIX "d")
endif()

# add all the DirectX SDK libraries as targets
add_directx_lib_targets("d2d1;d3d10;d3d10_1;d3d11;d3d9;d3dcompiler;D3DCSX;D3DCSXd;d3dx10;d3dx10d;d3dx11;d3dx11d;d3dx9;d3dx9d;d3dxof;dinput8;dsetup;dsound;dwrite;DxErr;dxgi;dxguid;X3DAudio;xapobase;xapobased;XAPOFX;XInput")

# define DX9 interface target
add_library(DirectX9SDK INTERFACE)
target_include_directories(DirectX9SDK INTERFACE "${dxsdk_headers}")
target_link_libraries(DirectX9SDK INTERFACE d3d9 "d3dx9${DEBUG_SUFFIX}" XInput dxguid)
add_library(DXSDK::DX9 ALIAS DirectX9SDK)

# define DX10 interface target
add_library(DirectX10SDK INTERFACE)
target_include_directories(DirectX10SDK INTERFACE "${dxsdk_headers}")
target_link_libraries(DirectX10SDK INTERFACE d3d10 "d3dx10${DEBUG_SUFFIX}" XInput dxguid)
add_library(DXSDK::DX10 ALIAS DirectX10SDK)

# define DX11 interface target
add_library(DirectX11SDK INTERFACE)
target_include_directories(DirectX11SDK INTERFACE "${dxsdk_headers}")
target_link_libraries(DirectX11SDK INTERFACE d3d11 "d3dx11${DEBUG_SUFFIX}" XInput dxguid)
add_library(DXSDK::DX11 ALIAS DirectX11SDK)

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    # silence warnings from DXSDK when using clang
    target_compile_options(DirectX9SDK INTERFACE -Wno-comment -Wno-new-returns-null)
    target_compile_options(DirectX10SDK INTERFACE -Wno-comment -Wno-new-returns-null)
    target_compile_options(DirectX11SDK INTERFACE -Wno-comment -Wno-new-returns-null)
endif()
