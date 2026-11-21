cmake_minimum_required(VERSION 3.20)
project(Mp3DecodeCheck LANGUAGES CXX)

add_executable(Mp3DecodeCheck src/main.cpp)
target_compile_features(Mp3DecodeCheck PRIVATE cxx_std_17)
target_compile_definitions(Mp3DecodeCheck PRIVATE UNICODE _UNICODE)
target_link_libraries(Mp3DecodeCheck PRIVATE mfplat mfreadwrite mfuuid ole32)
