/* glprogram - v0.1 - public domain shader class for opengl

    Do this:
        #define GLPROGRAM_IMPLEMENTATION
    before you include this file in *one* C or C++ file to create the implementation.

    Release notes:
        v0.1    (2017-07-13)    initial version based on tiny_gltf glview.cc

LICENSE

This software is in the public domain. Where that dedication is not
recognized, you are granted a perpetual, irrevocable license to copy,
distribute, and modify this file as you see fit.

*/
#ifndef GLPROGRAM_H
#define GLPROGRAM_H

#include <GL/gl.h>
#include <map>

class GLProgram
{
    std::map<GLenum, GLuint> _shaders;
    GLuint _prog;
public:
    GLProgram();
    virtual ~GLProgram();

    GLuint ProgId() const;

    bool LoadShader(GLenum shaderType, const char *shaderSourceFilename);
    bool LinkProgram();
    bool Setup(const std::map<GLenum, const char*>& shaders);
    void Cleanup();
};

#endif // GLPROGRAM_H

#ifdef GLPROGRAM_IMPLEMENTATION

GLProgram::GLProgram() : _prog(0) { }

GLProgram::~GLProgram() { }

GLuint GLProgram::ProgId() const { return _prog; }

bool GLProgram::LoadShader(GLenum shaderType, const char *shaderSourceFilename)
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

bool GLProgram::LinkProgram()
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

bool GLProgram::Setup(const std::map<GLenum, const char*>& shaders)
{
//    if (false == LoadShader(GL_VERTEX_SHADER, "shader.vert")) return false;
//    CheckErrors("load vert shader");

//    if (false == LoadShader(GL_FRAGMENT_SHADER, "shader.frag")) return false;
//    CheckErrors("load frag shader");

    for (auto pair : shaders)
    {
        if (false == LoadShader(pair.first, pair.second)) return false;
    }

    if (false == LinkProgram()) return false;

    // At least `in_vertex` should be used in the shader.
    GLint vtxLoc = glGetAttribLocation(_prog, "in_vertex");
    if (vtxLoc < 0)
    {
        std::cerr << "vertex loc not found." << std::endl;
        return false;
    }

    return true;
}

void GLProgram::Cleanup()
{
    glDeleteProgram(_prog);
    _prog = 0;
}
#endif // GLPROGRAM_IMPLEMENTATION
