cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(UDXRenderer VERSION 0.0.2)
message(STATUS "[Project] ${PROJECT_NAME}")

include(cmake/InitUCMake.cmake)
Ubpa_InitUCMake()

Ubpa_InitProject()

Ubpa_AddDep(UFG 0.3.0)

Ubpa_AddSubDirsRec(src)

Ubpa_Export(DIRECTORIES "include")
