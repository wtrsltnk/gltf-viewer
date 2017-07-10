cmake_minimum_required(VERSION 2.8)

project(gltf-viewer)

include(win-cpp-deps.cmake/win-cpp-deps.cmake)

install_dep("https://bitbucket.org/wincppdeps/glm.git")
install_dep("https://bitbucket.org/wincppdeps/glfw.git")
install_dep("https://bitbucket.org/wincppdeps/glextloader.git")

find_package(OPENGL REQUIRED)
find_package(GLM REQUIRED)
find_package(GLFW REQUIRED)

add_executable(gltf-viewer
    glview.cc
    impl.cc
    picojson.h
    shader.frag
    shader.vert
    stb_image.h
    tiny_gltf.h
    trackball.cc
    trackball.h
    )

target_include_directories(gltf-viewer
    PRIVATE ${GLM_INCLUDE_DIR}
    PRIVATE ${GLFW3_INCLUDE_DIR}
    )

target_compile_features(gltf-viewer
    PRIVATE cxx_auto_type
    PRIVATE cxx_nullptr
    PRIVATE cxx_lambdas
    )

target_link_libraries(gltf-viewer
    ${GLFW3_LIBRARY}
    ${OPENGL_LIBRARIES}
    )

add_executable(loader_example
    loader_example.cc
    picojson.h
    stb_image.h
    tiny_gltf.h
    )
