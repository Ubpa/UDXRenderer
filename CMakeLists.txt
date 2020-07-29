cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(UDXRenderer VERSION 0.0.3)
message(STATUS "[Project] ${PROJECT_NAME}")

include(cmake/InitUCMake.cmake)
Ubpa_InitUCMake()

Ubpa_InitProject()

Ubpa_AddDep(UDX12 0.0.7)

Ubpa_AddSubDirsRec(src)

Ubpa_Export(DIRECTORIES "include")
