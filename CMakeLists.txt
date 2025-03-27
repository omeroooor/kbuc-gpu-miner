cmake_minimum_required(VERSION 3.18)

# Set vcpkg toolchain file before project()
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Force Ninja generator
set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "" FORCE)

# Set CUDA flags before project()
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -allow-unsupported-compiler")

# Start project with both CXX and CUDA languages
project(bitcoin_miner LANGUAGES CXX CUDA)

# Set CUDA compiler settings
set(CMAKE_CUDA_COMPILER "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.3/bin/nvcc.exe")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -arch=sm_50")
set(CMAKE_CUDA_FLAGS_DEBUG "${CMAKE_CUDA_FLAGS_DEBUG} -G")

# Find CUDA
find_package(CUDAToolkit REQUIRED)

# Set C++ and CUDA standards
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Set OpenSSL paths explicitly
set(OPENSSL_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/packages/openssl_x64-windows")
set(OPENSSL_USE_STATIC_LIBS FALSE)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")

# Find gRPC and protobuf
find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Generate protobuf and gRPC files
find_program(PROTOC protoc REQUIRED)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)

# Get proto files
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto")

# Custom command to generate gRPC code
foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    set(PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY ${PROTO_GEN_DIR})
    
    add_custom_command(
        OUTPUT "${PROTO_GEN_DIR}/${PROTO_NAME}.pb.cc"
               "${PROTO_GEN_DIR}/${PROTO_NAME}.pb.h"
               "${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.cc"
               "${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.h"
        COMMAND ${PROTOC}
        ARGS --grpc_out=${PROTO_GEN_DIR}
             --cpp_out=${PROTO_GEN_DIR}
             --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
             -I ${CMAKE_CURRENT_SOURCE_DIR}/proto
             ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
    )
endforeach()

# Create library for generated protobuf code
add_library(proto_lib
    ${CMAKE_CURRENT_BINARY_DIR}/generated/miner.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/generated/miner.pb.h
    ${CMAKE_CURRENT_BINARY_DIR}/generated/miner.grpc.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/generated/miner.grpc.pb.h
)

target_link_libraries(proto_lib
    PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc++_reflection
)

target_include_directories(proto_lib
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CUDAToolkit_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${CURL_INCLUDE_DIRS})

# Set source file properties
set_source_files_properties(src/miner.cu PROPERTIES LANGUAGE CUDA)

# Add executable
add_executable(miner
    src/main.cpp
    src/miner.cu
    src/miner_service.cpp
    src/hash_writer.cpp
)

target_link_libraries(miner
    PRIVATE
    gRPC::gpr
    gRPC::grpc
    gRPC::grpc++
    gRPC::grpc++_alts
    protobuf::libprotoc
    protobuf::libprotobuf
    protobuf::libprotobuf-lite
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    nlohmann_json::nlohmann_json
    CUDA::cudart
    proto_lib
)

# Set CUDA specific properties
set_target_properties(miner PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_ARCHITECTURES native
)

# Set compiler options for MSVC
if(MSVC)
    set(MSVC_COMPILE_OPTIONS "/W4")
    target_compile_options(miner PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${MSVC_COMPILE_OPTIONS}>"
        "$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=${MSVC_COMPILE_OPTIONS}>"
    )
endif()
