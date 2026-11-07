cmake_minimum_required(VERSION 3.19)
project(ZTLWorkspace LANGUAGES CXX)
include(CTest)
add_subdirectory(desktop)
# The maintenance tool is configured independently with cmake -S deploy.
