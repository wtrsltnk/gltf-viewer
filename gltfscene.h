/* gltfscene - v0.1 - public domain gltf 2.0 model renderer for opengl

    Do this:
        #define GLSCENE_IMPLEMENTATION
    before you include this file in *one* C or C++ file to create the implementation.

    Release notes:
        v0.1    (2017-07-13)    initial version based on tiny_gltf glview.cc

LICENSE

This software is in the public domain. Where that dedication is not
recognized, you are granted a perpetual, irrevocable license to copy,
distribute, and modify this file as you see fit.

*/
#ifndef GLSCENE_H
#define GLSCENE_H

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <GL/gl.h>

#include "tiny_gltf.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

class GLScene
{
    typedef struct { GLuint vb; } GLBufferState;

    typedef struct {
        std::vector<GLuint> diffuseTex;  // for each primitive in mesh
    } GLMeshState;

    tinygltf::Model _model;
    std::map<int, GLBufferState> _buffers;
    std::map<std::string, GLMeshState> _meshStates;
    std::map<std::string, GLint> _attribs;

public:
    GLScene();
    virtual ~GLScene();

    bool Load(const std::string& filename);
    void Setup(GLuint prog);
    void DrawMesh(int index);
    void DrawNode(int index);
    void Draw();
    void Cleanup();
};

#endif // GLSCENE_H

#ifdef GLSCENE_IMPLEMENTATION

std::string GetFilePathExtension(const std::string &FileName)
{
    if (FileName.find_last_of(".") != std::string::npos)
        return FileName.substr(FileName.find_last_of(".") + 1);
    return "";
}

GLScene::GLScene() { }

GLScene::~GLScene() { }

bool GLScene::Load(const std::string& filename)
{
    std::string err;
    std::string ext = GetFilePathExtension(filename);
    bool ret = false;
    if (ext.compare("glb") == 0) // assume binary glTF.
    {
        ret = tinygltf::TinyGLTF().LoadBinaryFromFile(&this->_model, &err, filename.c_str());
    }
    else // assume ascii glTF.
    {
        ret = tinygltf::TinyGLTF().LoadASCIIFromFile(&this->_model, &err, filename.c_str());
    }

    if (!err.empty()) std::cerr << "ERR: " << err << std::endl;

    return ret;
}

void GLScene::Setup(GLuint prog)
{
    glUseProgram(prog);

    this->_attribs["POSITION"] = glGetAttribLocation(prog, "in_vertex");
    this->_attribs["NORMAL"] = glGetAttribLocation(prog, "in_normal");
    this->_attribs["TEXCOORD_0"] = glGetAttribLocation(prog, "in_texcoord");

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

void GLScene::DrawMesh(int index)
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

            int count = 1;
            if (accessor.type == TINYGLTF_TYPE_SCALAR) count = 1;
            else if (accessor.type == TINYGLTF_TYPE_VEC2) count = 2;
            else if (accessor.type == TINYGLTF_TYPE_VEC3) count = 3;
            else if (accessor.type == TINYGLTF_TYPE_VEC4) count = 4;
            else assert(0);

            if ((it.first.compare("POSITION") == 0) || (it.first.compare("NORMAL") == 0) || (it.first.compare("TEXCOORD_0") == 0))
            {
                auto attr = this->_attribs[it.first];
                if (attr >= 0)
                {
                    glVertexAttribPointer(attr, count, accessor.componentType, GL_FALSE, 0, BUFFER_OFFSET(accessor.byteOffset));
                    glEnableVertexAttribArray(attr);
                }
            }
        }

        auto indexAccessor = this->_model.accessors[primitive.indices];
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->_buffers[indexAccessor.bufferView].vb);

        int mode = -1;
        if (primitive.mode == TINYGLTF_MODE_TRIANGLES) mode = GL_TRIANGLES;
        else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP) mode = GL_TRIANGLE_STRIP;
        else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_FAN) mode = GL_TRIANGLE_FAN;
        else if (primitive.mode == TINYGLTF_MODE_POINTS) mode = GL_POINTS;
        else if (primitive.mode == TINYGLTF_MODE_LINE) mode = GL_LINES;
        else if (primitive.mode == TINYGLTF_MODE_LINE_LOOP)  mode = GL_LINE_LOOP;

        glDrawElements(mode, indexAccessor.count, indexAccessor.componentType, BUFFER_OFFSET(indexAccessor.byteOffset));

        for (auto it : primitive.attributes)
        {
            if ((it.first.compare("POSITION") == 0) || (it.first.compare("NORMAL") == 0) || (it.first.compare("TEXCOORD_0") == 0))
            {
                auto attr = this->_attribs[it.first];
                if (attr >= 0) glDisableVertexAttribArray(attr);
            }
        }
    }
}

void GLScene::DrawNode(int index)
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

void GLScene::Draw()
{
    if (this->_model.defaultScene < 0) return;

    auto scene = this->_model.scenes[this->_model.defaultScene];
    for (auto node : scene.nodes)
    {
        DrawNode(node);
    }
}

void GLScene::Cleanup()
{

}

#endif // GLSCENE_IMPLEMENTATION
