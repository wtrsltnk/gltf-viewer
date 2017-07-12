#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>

#include <GL/glextl.h>
#include <GL/glu.h>

#include <GLFW/glfw3.h>

#include "trackball.h"

#include "tiny_gltf.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

#define CAM_Z (3.0f)
int width = 768;
int height = 768;

double prevMouseX, prevMouseY;
bool mouseLeftPressed;
bool mouseMiddlePressed;
bool mouseRightPressed;
float curr_quat[4];
float prev_quat[4];
float eye[3], lookat[3], up[3];

void CheckErrors(const char* desc)
{
    GLenum e = glGetError();
    if (e != GL_NO_ERROR)
    {
        std::cerr << "OpenGL error in \"" << desc << "\": " << e << std::endl;
        exit(20);
    }
}

static std::string GetFilePathExtension(const std::string &FileName)
{
    if (FileName.find_last_of(".") != std::string::npos)
        return FileName.substr(FileName.find_last_of(".") + 1);
    return "";
}

bool LoadShader(GLenum shaderType, GLuint &shader, const char *shaderSourceFilename)
{
    GLint val = 0;

    // free old shader/program
    if (shader != 0)
    {
        glDeleteShader(shader);
    }

    FILE *fp = fopen(shaderSourceFilename, "rb");
    if (!fp)
    {
        std::cerr << "failed to load shader: " << shaderSourceFilename << std::endl;
        return false;
    }

    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    rewind(fp);
    std::vector<GLchar> srcbuf;
    srcbuf.resize(len + 1);
    len = fread(&srcbuf.at(0), 1, len, fp);
    srcbuf[len] = 0;
    fclose(fp);

    const GLchar *srcs[1];
    srcs[0] = &srcbuf.at(0);

    shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, srcs, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &val);
    if (val != GL_TRUE)
    {
        char log[4096];
        GLsizei msglen;
        glGetShaderInfoLog(shader, 4096, &msglen, log);
        std::cerr << log << std::endl;

        std::cerr << "ERR: Failed to load or compile shader [ " << shaderSourceFilename << " ]" << std::endl;

        return false;
    }

    std::cout << "Load shader [ " << shaderSourceFilename << " ] OK" << std::endl;

    return true;
}

bool LinkShader(GLuint &prog, GLuint &vertShader, GLuint &fragShader) {
    GLint val = 0;

    if (prog != 0) glDeleteProgram(prog);

    prog = glCreateProgram();

    glAttachShader(prog, vertShader);
    glAttachShader(prog, fragShader);
    glLinkProgram(prog);

    glGetProgramiv(prog, GL_LINK_STATUS, &val);
    assert(val == GL_TRUE && "failed to link shader");

    std::cout << "Link shader OK" << std::endl;

    return true;
}

static GLuint SetupShader()
{
    GLuint vertId = 0, fragId = 0, progId = 0;
    if (false == LoadShader(GL_VERTEX_SHADER, vertId, "shader.vert")) return 0;
    CheckErrors("load vert shader");

    if (false == LoadShader(GL_FRAGMENT_SHADER, fragId, "shader.frag")) return 0;
    CheckErrors("load frag shader");

    if (false == LinkShader(progId, vertId, fragId)) return 0;
    CheckErrors("link");

    // At least `in_vertex` should be used in the shader.
    GLint vtxLoc = glGetAttribLocation(progId, "in_vertex");
    if (vtxLoc < 0)
    {
        std::cerr << "vertex loc not found." << std::endl;
        return 0;
    }

    return progId;
}

void CleanupShader(GLuint progId)
{
    glDeleteProgram(progId);
    CheckErrors("CleanupShader");
}

void reshapeFunc(GLFWwindow *window, int w, int h)
{
    (void)window;
    int fb_w, fb_h;
    glfwGetFramebufferSize(window, &fb_w, &fb_h);
    glViewport(0, 0, fb_w, fb_h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)w / (float)h, 0.1f, 1000.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    width = w;
    height = h;
}

void keyboardFunc(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    if ((action == GLFW_PRESS || action == GLFW_REPEAT) && (key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE))
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
}

void clickFunc(GLFWwindow *window, int button, int action, int mods)
{
    double x, y;
    glfwGetCursorPos(window, &x, &y);

    bool shiftPressed = (mods & GLFW_MOD_SHIFT);
    bool ctrlPressed = (mods & GLFW_MOD_CONTROL);

    if ((button == GLFW_MOUSE_BUTTON_LEFT) && (!shiftPressed) && (!ctrlPressed))
    {
        mouseLeftPressed = true;
        mouseMiddlePressed = false;
        mouseRightPressed = false;
        if (action == GLFW_PRESS)
        {
            int id = -1;
            if (id < 0) trackball(prev_quat, 0.0, 0.0, 0.0, 0.0);
        }
        else if (action == GLFW_RELEASE)
        {
            mouseLeftPressed = false;
        }
    }
    if ((button == GLFW_MOUSE_BUTTON_RIGHT) || ((button == GLFW_MOUSE_BUTTON_LEFT) && ctrlPressed))
    {
        if (action == GLFW_PRESS)
        {
            mouseRightPressed = true;
            mouseLeftPressed = false;
            mouseMiddlePressed = false;
        }
        else if (action == GLFW_RELEASE)
        {
            mouseRightPressed = false;
        }
    }
    if ((button == GLFW_MOUSE_BUTTON_MIDDLE) || ((button == GLFW_MOUSE_BUTTON_LEFT) && shiftPressed))
    {
        if (action == GLFW_PRESS)
        {
            mouseMiddlePressed = true;
            mouseLeftPressed = false;
            mouseRightPressed = false;
        }
        else if (action == GLFW_RELEASE)
        {
            mouseMiddlePressed = false;
        }
    }
}

void motionFunc(GLFWwindow *window, double mouse_x, double mouse_y)
{
    (void)window;
    float rotScale = 1.0f;
    float transScale = 2.0f;

    if (mouseLeftPressed)
    {
        trackball(prev_quat, rotScale * (2.0f * prevMouseX - width) / (float)width,
                  rotScale * (height - 2.0f * prevMouseY) / (float)height,
                  rotScale * (2.0f * mouse_x - width) / (float)width,
                  rotScale * (height - 2.0f * mouse_y) / (float)height);

        add_quats(prev_quat, curr_quat, curr_quat);
    }
    else if (mouseMiddlePressed)
    {
        eye[0] += -transScale * (mouse_x - prevMouseX) / (float)width;
        lookat[0] += -transScale * (mouse_x - prevMouseX) / (float)width;
        eye[1] += transScale * (mouse_y - prevMouseY) / (float)height;
        lookat[1] += transScale * (mouse_y - prevMouseY) / (float)height;
    }
    else if (mouseRightPressed)
    {
        eye[2] += transScale * (mouse_y - prevMouseY) / (float)height;
        lookat[2] += transScale * (mouse_y - prevMouseY) / (float)height;
    }

    // Update mouse point
    prevMouseX = mouse_x;
    prevMouseY = mouse_y;
}

typedef struct { GLuint vb; } GLBufferState;

typedef struct {
    std::vector<GLuint> diffuseTex;  // for each primitive in mesh
} GLMeshState;

typedef struct {
    std::map<std::string, GLint> attribs;
    std::map<std::string, GLint> uniforms;
} GLProgramState;

class GLScene
{
    tinygltf::Model _model;
    std::map<int, GLBufferState> _buffers;
    std::map<std::string, GLMeshState> _meshStates;
    GLProgramState _programState;

public:
    GLScene() { }
    virtual ~GLScene() { }

    bool Load(const std::string& filename)
    {
        std::string err;
        std::string ext = GetFilePathExtension(filename);
        bool ret = false;
        if (ext.compare("glb") == 0)
        {
            // assume binary glTF.
            ret = tinygltf::TinyGLTF().LoadBinaryFromFile(&this->_model, &err, filename.c_str());
        }
        else
        {
            // assume ascii glTF.
            ret = tinygltf::TinyGLTF().LoadASCIIFromFile(&this->_model, &err, filename.c_str());
        }

        if (!err.empty()) std::cerr << "ERR: " << err << std::endl;

        return ret;
    }

    void Setup(GLuint progId)
    {
        glUseProgram(progId);
        CheckErrors("useProgram");

        this->_programState.attribs["POSITION"] = glGetAttribLocation(progId, "in_vertex");
        this->_programState.attribs["NORMAL"] = glGetAttribLocation(progId, "in_normal");
        this->_programState.attribs["TEXCOORD_0"] = glGetAttribLocation(progId, "in_texcoord");

        for (size_t i = 0; i < this->_model.bufferViews.size(); i++)
        {
            auto bufferView = this->_model.bufferViews[i];
            if (bufferView.target == 0)
            {
                std::cout << "WARN: bufferView.target is zero" << std::endl;
                continue;  // Unsupported bufferView.
            }

            auto buffer = this->_model.buffers[bufferView.buffer];
            GLBufferState state;

            glGenBuffers(1, &state.vb);
            glBindBuffer(bufferView.target, state.vb);
            glBufferData(bufferView.target, bufferView.byteLength, &buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);
            glBindBuffer(bufferView.target, 0);

            this->_buffers[i] = state;
        }
    }

    void DrawMesh(int index)
    {
        auto mesh = this->_model.meshes[index];
        for (auto primitive : mesh.primitives)
        {
            if (primitive.indices < 0) return;

            for (auto it : primitive.attributes)
            {
                if (it.second < 0) continue;

                auto accessor = this->_model.accessors[it.second];
                glBindBuffer(GL_ARRAY_BUFFER, this->_buffers[accessor.bufferView].vb);
                CheckErrors("bind buffer");

                int count = 1;
                if (accessor.type == TINYGLTF_TYPE_SCALAR) count = 1;
                else if (accessor.type == TINYGLTF_TYPE_VEC2) count = 2;
                else if (accessor.type == TINYGLTF_TYPE_VEC3) count = 3;
                else if (accessor.type == TINYGLTF_TYPE_VEC4) count = 4;
                else assert(0);

                if ((it.first.compare("POSITION") == 0) || (it.first.compare("NORMAL") == 0) || (it.first.compare("TEXCOORD_0") == 0))
                {
                    auto attr = this->_programState.attribs[it.first];
                    if (attr >= 0)
                    {
                        glVertexAttribPointer(attr, count, accessor.componentType, GL_FALSE, 0, BUFFER_OFFSET(accessor.byteOffset));
                        CheckErrors("vertex attrib pointer");

                        glEnableVertexAttribArray(attr);
                        CheckErrors("enable vertex attrib array");
                    }
                }
            }

            auto indexAccessor = this->_model.accessors[primitive.indices];
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->_buffers[indexAccessor.bufferView].vb);
            CheckErrors("bind buffer");

            int mode = -1;
            if (primitive.mode == TINYGLTF_MODE_TRIANGLES) mode = GL_TRIANGLES;
            else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP) mode = GL_TRIANGLE_STRIP;
            else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_FAN) mode = GL_TRIANGLE_FAN;
            else if (primitive.mode == TINYGLTF_MODE_POINTS) mode = GL_POINTS;
            else if (primitive.mode == TINYGLTF_MODE_LINE) mode = GL_LINES;
            else if (primitive.mode == TINYGLTF_MODE_LINE_LOOP)  mode = GL_LINE_LOOP;
            else assert(0);

            glDrawElements(mode, indexAccessor.count, indexAccessor.componentType, BUFFER_OFFSET(indexAccessor.byteOffset));
            CheckErrors("draw elements");

            for (auto it : primitive.attributes)
            {
                if ((it.first.compare("POSITION") == 0) || (it.first.compare("NORMAL") == 0) || (it.first.compare("TEXCOORD_0") == 0))
                {
                    auto attr = this->_programState.attribs[it.first];
                    if (attr >= 0) glDisableVertexAttribArray(attr);
                }
            }
        }
    }

    void DrawNode(int index)
    {
        auto node = this->_model.nodes[index];
        glPushMatrix();
        if (node.matrix.size() == 16)
        {
            // Use `matrix' attribute
            glMultMatrixd(node.matrix.data());
        }
        else
        {
            if (node.translation.size() == 3)
            {
                glTranslated(node.translation[0], node.translation[1], node.translation[2]);
            }

            if (node.rotation.size() == 4)
            {
                glRotated(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
            }

            if (node.scale.size() == 3)
            {
                glScaled(node.scale[0], node.scale[1], node.scale[2]);
            }
        }

        if (node.mesh >= 0) DrawMesh(node.mesh);

        // Draw child nodes.
        for (auto child : node.children) DrawNode(child);

        glPopMatrix();
    }

    void Draw()
    {
        if (this->_model.defaultScene < 0) return;

        auto scene = this->_model.scenes[this->_model.defaultScene];
        for (auto node : scene.nodes)
        {
            DrawNode(node);
        }
    }

    void Cleanup()
    {

    }

};

static void Init()
{
    trackball(curr_quat, 0, 0, 0, 0);

    eye[0] = 0.0f;
    eye[1] = 0.0f;
    eye[2] = CAM_Z;

    lookat[0] = 0.0f;
    lookat[1] = 0.0f;
    lookat[2] = 0.0f;

    up[0] = 0.0f;
    up[1] = 1.0f;
    up[2] = 0.0f;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "glview input.gltf <scale>\n" << std::endl;
        return 0;
    }

    float scale = argc > 2 ? std::stof(argv[2]) : 1.0f;

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW." << std::endl;
        return -1;
    }

    std::stringstream title;
    title << "Simple glTF viewer: " << argv[1];

    auto window = glfwCreateWindow(width, height, title.str().c_str(), NULL, NULL);
    if (window == NULL)
    {
        std::cerr << "Failed to open GLFW window. " << std::endl;
        glfwTerminate();
        return 1;
    }

    glfwGetWindowSize(window, &width, &height);

    glfwMakeContextCurrent(window);

    if (!glExtLoadAll((PFNGLGETPROC*)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLEXTL." << std::endl;
        return -1;
    }

    // Callback
    glfwSetWindowSizeCallback(window, reshapeFunc);
    glfwSetKeyCallback(window, keyboardFunc);
    glfwSetMouseButtonCallback(window, clickFunc);
    glfwSetCursorPosCallback(window, motionFunc);

    Init();

    reshapeFunc(window, width, height);

    GLuint progId = SetupShader();
    if (progId == 0) return -1;

    GLScene scene;
    if (!scene.Load(argv[1])) return -1;
    scene.Setup(progId);

    while (glfwWindowShouldClose(window) == GL_FALSE)
    {
        glfwPollEvents();
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        GLfloat mat[4][4];
        build_rotmatrix(mat, curr_quat);

        // camera(define it in projection matrix)
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        gluLookAt(eye[0], eye[1], eye[2], lookat[0], lookat[1], lookat[2], up[0], up[1], up[2]);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glMultMatrixf(&mat[0][0]);

        glScalef(scale, scale, scale);

        scene.Draw();

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        glFlush();

        glfwSwapBuffers(window);
    }

    scene.Cleanup();

    CleanupShader(progId);

    glfwTerminate();
}
