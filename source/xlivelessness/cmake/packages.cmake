CPMAddPackage(
    NAME opus
    GIT_REPOSITORY https://gitlab.xiph.org/xiph/opus.git
    GIT_TAG v1.5.2
    OPTIONS
        "OPUS_RUNTIME_STATIC ON"
        "OPUS_X86_PRESUME_AVX2 ON"
)

CPMAddPackage(
    NAME RapidXML
    URL https://sourceforge.net/projects/rapidxml/files/rapidxml/rapidxml%201.13/rapidxml-1.13.zip/download
    DOWNLOAD_ONLY TRUE
    URL_HASH SHA256=c3f0b886374981bb20fabcf323d755db4be6dba42064599481da64a85f5b3571
)
if(RapidXML_ADDED)
    add_library(RapidXML INTERFACE)
    target_include_directories(RapidXML SYSTEM INTERFACE ${RapidXML_SOURCE_DIR})
    message(STATUS "RapidXML: ${RapidXML_SOURCE_DIR}")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/FindDirectXSDK.cmake")
