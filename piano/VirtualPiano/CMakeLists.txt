cmake_minimum_required(VERSION 3.16)
project(VirtualPiano)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenCV REQUIRED)
find_package(SFML REQUIRED COMPONENTS audio graphics window system)

# Include directories
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${SFML_INCLUDE_DIR})

# Source files
set(SOURCES
    src/main.cpp
    src/OpenCVPiano.cpp
    src/HandDetector.cpp
    src/AudioManager.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    ${OpenCV_LIBS}
    sfml-audio
    sfml-graphics
    sfml-window
    sfml-system
)

# Copy audio files to build directory
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
