/* glfwcamera - v0.1 - public domain trackball camera class integrated into GLFW3

    Do this:
        #define GLFWCAMERA_IMPLEMENTATION
    before you include this file in *one* C or C++ file to create the implementation.

    Release notes:
        v0.1    (2017-07-13)    initial version based on tiny_gltf glview.cc

LICENSE

This software is in the public domain. Where that dedication is not
recognized, you are granted a perpetual, irrevocable license to copy,
distribute, and modify this file as you see fit.

*/
#ifndef GLFWCAMERA_H
#define GLFWCAMERA_H

#include <GL/gl.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>

class GLFWCamera
{
    double prevMouseX, prevMouseY;
    bool mouseLeftPressed;
    bool mouseMiddlePressed;
    bool mouseRightPressed;
    float curr_quat[4];
    float prev_quat[4];
    float eye[3], lookat[3], up[3];
    int width;
    int height;
    float scale;

    static void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow *window, double mouse_x, double mouse_y);
    static void setWindowSizeCallback(GLFWwindow *window, int w, int h);
public:
    GLFWCamera();
    virtual ~GLFWCamera();

    void SetScale(float scale);
    void Setup(GLFWwindow *window);
    void Build();
};

#endif // GLFWCAMERA_H

#ifdef GLFWCAMERA_IMPLEMENTATION

#include "trackball.h"

void GLFWCamera::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    auto thiz = reinterpret_cast<GLFWCamera*>(glfwGetWindowUserPointer(window));
    double x, y;
    glfwGetCursorPos(window, &x, &y);

    bool shiftPressed = (mods & GLFW_MOD_SHIFT);
    bool ctrlPressed = (mods & GLFW_MOD_CONTROL);

    if ((button == GLFW_MOUSE_BUTTON_LEFT) && (!shiftPressed) && (!ctrlPressed))
    {
        thiz->mouseLeftPressed = true;
        thiz->mouseMiddlePressed = false;
        thiz->mouseRightPressed = false;
        if (action == GLFW_PRESS)
        {
            int id = -1;
            if (id < 0) trackball(thiz->prev_quat, 0.0, 0.0, 0.0, 0.0);
        }
        else if (action == GLFW_RELEASE)
        {
            thiz->mouseLeftPressed = false;
        }
    }
    if ((button == GLFW_MOUSE_BUTTON_RIGHT) || ((button == GLFW_MOUSE_BUTTON_LEFT) && ctrlPressed))
    {
        if (action == GLFW_PRESS)
        {
            thiz->mouseRightPressed = true;
            thiz->mouseLeftPressed = false;
            thiz->mouseMiddlePressed = false;
        }
        else if (action == GLFW_RELEASE)
        {
            thiz->mouseRightPressed = false;
        }
    }
    if ((button == GLFW_MOUSE_BUTTON_MIDDLE) || ((button == GLFW_MOUSE_BUTTON_LEFT) && shiftPressed))
    {
        if (action == GLFW_PRESS)
        {
            thiz->mouseMiddlePressed = true;
            thiz->mouseLeftPressed = false;
            thiz->mouseRightPressed = false;
        }
        else if (action == GLFW_RELEASE)
        {
            thiz->mouseMiddlePressed = false;
        }
    }
}

void GLFWCamera::cursorPosCallback(GLFWwindow *window, double mouse_x, double mouse_y)
{
    auto thiz = reinterpret_cast<GLFWCamera*>(glfwGetWindowUserPointer(window));
    float rotScale = 1.0f;
    float transScale = 2.0f;

    if (thiz->mouseLeftPressed)
    {
        trackball(thiz->prev_quat, rotScale * (2.0f * thiz->prevMouseX - thiz->width) / (float)thiz->width,
                  rotScale * (thiz->height - 2.0f * thiz->prevMouseY) / (float)thiz->height,
                  rotScale * (2.0f * mouse_x - thiz->width) / (float)thiz->width,
                  rotScale * (thiz->height - 2.0f * mouse_y) / (float)thiz->height);

        add_quats(thiz->prev_quat, thiz->curr_quat, thiz->curr_quat);
    }
    else if (thiz->mouseMiddlePressed)
    {
        thiz->eye[0] += -transScale * (mouse_x - thiz->prevMouseX) / (float)thiz->width;
        thiz->lookat[0] += -transScale * (mouse_x - thiz->prevMouseX) / (float)thiz->width;
        thiz->eye[1] += transScale * (mouse_y - thiz->prevMouseY) / (float)thiz->height;
        thiz->lookat[1] += transScale * (mouse_y - thiz->prevMouseY) / (float)thiz->height;
    }
    else if (thiz->mouseRightPressed)
    {
        thiz->eye[2] += transScale * (mouse_y - thiz->prevMouseY) / (float)thiz->height;
        thiz->lookat[2] += transScale * (mouse_y - thiz->prevMouseY) / (float)thiz->height;
    }

    // Update mouse point
    thiz->prevMouseX = mouse_x;
    thiz->prevMouseY = mouse_y;
}

void GLFWCamera::setWindowSizeCallback(GLFWwindow *window, int w, int h)
{
    auto thiz = reinterpret_cast<GLFWCamera*>(glfwGetWindowUserPointer(window));

    glfwGetFramebufferSize(window, &thiz->width, &thiz->height);
    glViewport(0, 0, thiz->width, thiz->height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)thiz->width / (float)thiz->height, 0.1f, 1000.0f);
}

GLFWCamera::GLFWCamera() { }

GLFWCamera::~GLFWCamera() { }

void GLFWCamera::SetScale(float scale) { this->scale = scale; }

void GLFWCamera::Setup(GLFWwindow *window)
{
    glfwGetFramebufferSize(window, &width, &height);

    trackball(curr_quat, 0, 0, 0, 0);
    mouseLeftPressed = false;
    mouseMiddlePressed = false;
    mouseRightPressed = false;

    eye[0] = 0.0f;
    eye[1] = 0.0f;
    eye[2] = 2.0f;

    lookat[0] = 0.0f;
    lookat[1] = 0.0f;
    lookat[2] = 0.0f;

    up[0] = 0.0f;
    up[1] = 1.0f;
    up[2] = 0.0f;

    glfwSetWindowUserPointer(window, this);

    setWindowSizeCallback(window, this->width, this->height);

    glfwSetWindowSizeCallback(window, GLFWCamera::setWindowSizeCallback);
    glfwSetMouseButtonCallback(window, GLFWCamera::mouseButtonCallback);
    glfwSetCursorPosCallback(window, GLFWCamera::cursorPosCallback);
}

void GLFWCamera::Build()
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(eye[0], eye[1], eye[2], lookat[0], lookat[1], lookat[2], up[0], up[1], up[2]);

    GLfloat mat[4][4];
    build_rotmatrix(mat, curr_quat);
    glMultMatrixf(&mat[0][0]);

    glScalef(scale, scale, scale);
}

#endif // GLFWCAMERA_IMPLEMENTATION
