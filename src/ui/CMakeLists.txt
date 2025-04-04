cmake_minimum_required(VERSION 3.16)

# Find Qt packages
find_package(Qt5 COMPONENTS Widgets Core Network REQUIRED)

# Find CUDA
find_package(CUDA REQUIRED)

# Include Qt headers
include_directories(${Qt5Widgets_INCLUDE_DIRS} ${Qt5Core_INCLUDE_DIRS} ${Qt5Network_INCLUDE_DIRS} ${CUDA_INCLUDE_DIRS})

# Define UI source files
set(UI_SOURCES
    main.cpp
    main_window.cpp
    settings_dialog.cpp
    history_dialog.cpp
    mining_task.cpp
    cuda_miner.cpp
)

set(UI_HEADERS
    main_window.h
    settings_dialog.h
    history_dialog.h
    mining_task.h
    cuda_miner.h
)

# Add executable
add_executable(miner_ui ${UI_SOURCES} ${UI_HEADERS})

# Link with Qt libraries
target_link_libraries(miner_ui 
    Qt5::Widgets
    Qt5::Core
    Qt5::Network
    miner_lib
    proto_lib
    ${CUDA_LIBRARIES}
    ${CUDA_CUDART_LIBRARY}
)

# Add include directories for the miner source files and generated protobuf files
target_include_directories(miner_ui PRIVATE 
    ..
    ${CMAKE_BINARY_DIR}/generated
    ${CUDA_INCLUDE_DIRS}
)

# Add Qt-specific MOC processing
set_target_properties(miner_ui PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
