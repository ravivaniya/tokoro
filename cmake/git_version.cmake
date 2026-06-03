set(GIT_EXECUTABLE "git")
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
    WORKING_DIRECTORY ${SRC_DIR}
    RESULT_VARIABLE GIT_RESULT
    OUTPUT_VARIABLE GIT_DESCRIBE
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT GIT_RESULT EQUAL 0)
    set(GIT_DESCRIBE "${DEFAULT_VERSION}")
endif()

set(VERSION_HEADER "${BIN_DIR}/generated/version.hpp")

set(FILE_CONTENT "#pragma once\n\n")
string(APPEND FILE_CONTENT "namespace tokoro {\n")
string(APPEND FILE_CONTENT "    inline constexpr const char* VERSION = \"${GIT_DESCRIBE}\";\n")
string(APPEND FILE_CONTENT "}\n")

# Only write the file if it has changed, to avoid unnecessary recompilations
if(EXISTS ${VERSION_HEADER})
    file(READ ${VERSION_HEADER} OLD_CONTENT)
    if("${OLD_CONTENT}" STREQUAL "${FILE_CONTENT}")
        return()
    endif()
endif()

file(MAKE_DIRECTORY "${BIN_DIR}/generated")
file(WRITE ${VERSION_HEADER} "${FILE_CONTENT}")
