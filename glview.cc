#include <GL/glextl.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>

#include "tiny_gltf.h"
#include "gltfscene.h"
#include "glfwcamera.h"
#include "glprogram.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "glview input.gltf <scale>\n" << std::endl;
        return 0;
    }

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW." << std::endl;
        return -1;
    }

    std::stringstream title;
    title << "Simple glTF viewer: " << argv[1];

    auto window = glfwCreateWindow(1024, 768, title.str().c_str(), NULL, NULL);
    if (window == NULL)
    {
        std::cerr << "Failed to open GLFW window. " << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    if (!glExtLoadAll((PFNGLGETPROC*)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLEXTL." << std::endl;
        glfwTerminate();
        return -1;
    }

    // Callback
    glfwSetKeyCallback(window, [] (GLFWwindow *window, int key, int scancode, int action, int mods)
    {
        (void)scancode;
        (void)mods;
        if ((action == GLFW_PRESS || action == GLFW_REPEAT) && (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE))
        {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }
    });

    GLFWCamera camera;
    camera.Setup(window);
    camera.SetScale(argc > 2 ? std::stof(argv[2]) : 1.0f);

    GLProgram program;
    std::map<GLenum, const char*> shaders = {
        { GL_VERTEX_SHADER, "shader.vert" },
        { GL_FRAGMENT_SHADER, "shader.frag" }
    };
    if (!program.Setup(shaders))
    {
        glfwTerminate();
        return -1;
    }

    GLScene scene;
    if (!scene.Load(argv[1]))
    {
        glfwTerminate();
        return -1;
    }

    scene.Setup(program.ProgId());

    while (glfwWindowShouldClose(window) == GL_FALSE)
    {
        glfwPollEvents();
        glClearColor(0.3f, 0.4f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        camera.Build();

        scene.Draw();

        glFlush();

        glfwSwapBuffers(window);
    }

    scene.Cleanup();

    program.Cleanup();

    glfwTerminate();

    return 0;
}
