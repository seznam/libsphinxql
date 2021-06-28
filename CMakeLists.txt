cmake_minimum_required(VERSION 3.0)
cmake_policy(VERSION 3.0)

project(libsphinxql)
find_package(PkgConfig REQUIRED)

# Guess whether to use mysqlclient or mariadb library
pkg_check_modules(LIBMYSQL QUIET mysqlclient)
if (${LIBMYSQL_FOUND})
    set(mysql_library "mysqlclient")
else()
    set(mysql_library "mariadb")
endif()
message(STATUS "MySQL client library to be used: ${mysql_library}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2 -std=c++14 -Wall -Wextra -Wformat-security -Wuninitialized -Wfloat-equal")

macro(include_lib target name)
    set(version "")
    set(extra_macro_args ${ARGN})

    list(LENGTH extra_macro_args num_extra_args)
    if (${num_extra_args} GREATER 0)
        list(GET extra_macro_args 0 version)
    endif()
    pkg_check_modules(LIBS${name} REQUIRED ${name}${version})
    target_link_libraries(${target} ${LIBS${name}_LIBRARIES})
    message(STATUS "\tLink library '${LIBS${name}_LIBRARIES}'")
    target_include_directories(${target} PUBLIC ${LIBS${name}_INCLUDE_DIRS})
    message(STATUS "\tInclude directory '${LIBS${name}_INCLUDE_DIRS}'")
    target_compile_options(${target} PUBLIC ${LIBS${name}_CFLAGS})
    message(STATUS "\tAdd compile options '${LIBS${name}_CFLAGS}'")
endmacro(include_lib)

add_library(libsphinxql SHARED src/sphinxql.cc src/querybuilder.cc)
set_target_properties(libsphinxql PROPERTIES VERSION 0 SOVERSION 0)

target_include_directories(libsphinxql PUBLIC "${CMAKE_SOURCE_DIR}/include/sphinxql")
include_lib(libsphinxql ${mysql_library})

install(TARGETS libsphinxql LIBRARY DESTINATION lib)
install(DIRECTORY include/sphinxql DESTINATION include)


# ---
enable_testing()

add_executable(test-build tests/test_build.cc)
target_include_directories(test-build PUBLIC "${CMAKE_SOURCE_DIR}/include/sphinxql")
target_link_libraries(test-build libsphinxql)

add_executable(test-query tests/test_query.cc)
target_include_directories(test-query PUBLIC "${CMAKE_SOURCE_DIR}/include/sphinxql")
target_link_libraries(test-query libsphinxql -lgtest -lpthread)

find_program(BASH_BINARY bash)
if (BASH_BINARY)
    add_test(test-query-runner ${BASH_BINARY} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-tests.sh)
else()
    message(WARNING "Tests are currently unix only...")
endif()
