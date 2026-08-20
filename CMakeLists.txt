cmake_minimum_required(VERSION 3.20)
project(CAD_STEP_to_GLB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCASCADE REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

add_executable(cad-step-to-glb
    main.cpp
    core/step_parser.cpp
    core/geometry_processor.cpp
    core/mesh_converter.cpp
    core/step_header_parser.cpp
    core/step_entity_extractor.cpp
    export/glb_exporter.cpp
    export/json_exporter.cpp
    utils/logger.cpp
    third_party/tinygltf/tiny_gltf_impl.cpp
)

target_include_directories(cad-step-to-glb PRIVATE
    ${OpenCASCADE_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinygltf
)

target_link_libraries(cad-step-to-glb PRIVATE
    ${OpenCASCADE_LIBRARIES}
    nlohmann_json::nlohmann_json
    Threads::Threads
)

# Copy OpenCASCADE DLLs next to the exe on Windows
if(WIN32)
    foreach(LIB ${OpenCASCADE_LIBRARIES})
        get_target_property(DLL_PATH ${LIB} IMPORTED_LOCATION_RELEASE)
        if(DLL_PATH)
            add_custom_command(TARGET cad-step-to-glb POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${DLL_PATH}"
                    "$<TARGET_FILE_DIR:cad-step-to-glb>"
            )
        endif()
    endforeach()
endif()

set_target_properties(cad-step-to-glb PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)