# Test component CMakeLists.txt
# This builds the unit tests for the firmware

idf_component_register(
    SRCS
        "wifi_csi_test.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        unity
)

# Add the main component to access the code under test
# We need to link against the main component's header files
target_include_directories(${COMPONENT_LIB} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../main
)
