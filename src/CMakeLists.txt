add_library(matching_engine STATIC
    matching_engine.cpp
)

target_include_directories(matching_engine PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

add_executable(matching_engine_bin
    main.cpp
)

target_link_libraries(matching_engine_bin PRIVATE matching_engine)

# Install
install(TARGETS matching_engine matching_engine_bin
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

set_target_properties(matching_engine PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
