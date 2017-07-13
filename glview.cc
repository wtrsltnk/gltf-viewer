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

#define CAM_Z (2.0f)

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

class GLProgram
{
    std::map<GLenum, GLuint> _shaders;
    GLuint _prog;
public:
    GLProgram() : _prog(0) { }
    virtual ~GLProgram() { }

    GLuint ProgId() const { return _prog; }

    bool LoadShader(GLenum shaderType, const char *shaderSourceFilename)
    {
        if (_shaders.find(shaderType) != _shaders.end())
        {
            glDeleteShader(_shaders[shaderType]);
            _shaders.erase(shaderType);
        }

        GLint val = 0;

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

        GLuint shader = glCreateShader(shaderType);
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

        _shaders.insert(std::make_pair(shaderType, shader));

        return true;
    }

    bool LinkShader()
    {
        GLint val = 0;

        if (_prog != 0) glDeleteProgram(_prog);

        _prog = glCreateProgram();

        for (auto pair : _shaders)
            glAttachShader(_prog, pair.second);

        glLinkProgram(_prog);

        glGetProgramiv(_prog, GL_LINK_STATUS, &val);
        assert(val == GL_TRUE && "failed to link shader");

        std::cout << "Link shader OK" << std::endl;

        return true;
    }

    bool Setup()
    {
        if (false == LoadShader(GL_VERTEX_SHADER, "shader.vert")) return false;
        CheckErrors("load vert shader");

        if (false == LoadShader(GL_FRAGMENT_SHADER, "shader.frag")) return false;
        CheckErrors("load frag shader");

        if (false == LinkShader()) return false;
        CheckErrors("link");

        // At least `in_vertex` should be used in the shader.
        GLint vtxLoc = glGetAttribLocation(_prog, "in_vertex");
        if (vtxLoc < 0)
        {
            std::cerr << "vertex loc not found." << std::endl;
            return false;
        }

        return true;
    }

    void Cleanup()
    {
        glDeleteProgram(_prog);
        _prog = 0;
        CheckErrors("CleanupShader");
    }
};

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

    void Setup(GLProgram& prog)
    {
        glUseProgram(prog.ProgId());
        CheckErrors("useProgram");

        this->_programState.attribs["POSITION"] = glGetAttribLocation(prog.ProgId(), "in_vertex");
        this->_programState.attribs["NORMAL"] = glGetAttribLocation(prog.ProgId(), "in_normal");
        this->_programState.attribs["TEXCOORD_0"] = glGetAttribLocation(prog.ProgId(), "in_texcoord");

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

class GLView
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

    static void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
    {
        auto thiz = reinterpret_cast<GLView*>(glfwGetWindowUserPointer(window));
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

    static void cursorPosCallback(GLFWwindow *window, double mouse_x, double mouse_y)
    {
        auto thiz = reinterpret_cast<GLView*>(glfwGetWindowUserPointer(window));
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

    static void setWindowSizeCallback(GLFWwindow *window, int w, int h)
    {
        auto thiz = reinterpret_cast<GLView*>(glfwGetWindowUserPointer(window));

        glfwGetFramebufferSize(window, &thiz->width, &thiz->height);
        glViewport(0, 0, thiz->width, thiz->height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, (float)thiz->width / (float)thiz->height, 0.1f, 1000.0f);
    }
public:
    GLView() { }

    virtual ~GLView() { }

    void SetScale(float scale) { this->scale = scale; }

    void Setup(GLFWwindow *window)
    {
        glfwGetFramebufferSize(window, &width, &height);

        trackball(curr_quat, 0, 0, 0, 0);
        mouseLeftPressed = false;
        mouseMiddlePressed = false;
        mouseRightPressed = false;

        eye[0] = 0.0f;
        eye[1] = 0.0f;
        eye[2] = CAM_Z;

        lookat[0] = 0.0f;
        lookat[1] = 0.0f;
        lookat[2] = 0.0f;

        up[0] = 0.0f;
        up[1] = 1.0f;
        up[2] = 0.0f;

        glfwSetWindowUserPointer(window, this);

        setWindowSizeCallback(window, this->width, this->height);

        glfwSetWindowSizeCallback(window, GLView::setWindowSizeCallback);
        glfwSetMouseButtonCallback(window, GLView::mouseButtonCallback);
        glfwSetCursorPosCallback(window, GLView::cursorPosCallback);
    }

    void Build()
    {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(eye[0], eye[1], eye[2], lookat[0], lookat[1], lookat[2], up[0], up[1], up[2]);

        GLfloat mat[4][4];
        build_rotmatrix(mat, curr_quat);
        glMultMatrixf(&mat[0][0]);

        glScalef(scale, scale, scale);
    }
};

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

    GLView view;
    view.Setup(window);
    view.SetScale(argc > 2 ? std::stof(argv[2]) : 1.0f);

    GLProgram program;
    if (!program.Setup())
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

    scene.Setup(program);

    while (glfwWindowShouldClose(window) == GL_FALSE)
    {
        glfwPollEvents();
        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        view.Build();

        scene.Draw();

        glFlush();

        glfwSwapBuffers(window);
    }

    scene.Cleanup();

    program.Cleanup();

    glfwTerminate();

    return 0;
}
