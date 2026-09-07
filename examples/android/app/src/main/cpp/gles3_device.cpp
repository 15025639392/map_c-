#include "gles3_device.h"

#include <android/log.h>
#include <cstring>

#define GL_LOG_TAG "map_cplus_gles"
#define GLOG(...) __android_log_print(ANDROID_LOG_INFO, GL_LOG_TAG, __VA_ARGS__)
#define GLOGE(...) __android_log_print(ANDROID_LOG_ERROR, GL_LOG_TAG, __VA_ARGS__)

namespace demoscene {

namespace {

GLuint compileShader(GLenum type, const char* src) {
    const GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        GLOGE("shader compile failed (type=%d): %s", static_cast<int>(type), log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

} // namespace

Gles3RenderDevice::~Gles3RenderDevice() {
    // 上下文销毁由 demo 的 destroyGlObjects 先调用 release；这里兜底清空。
    for (auto& [h, m] : meshes_) {
        (void)h;
        if (m.vao) glDeleteVertexArrays(1, &m.vao);
        if (m.vboPos) glDeleteBuffers(1, &m.vboPos);
        if (m.vboNor) glDeleteBuffers(1, &m.vboNor);
        if (m.vboHei) glDeleteBuffers(1, &m.vboHei);
        if (m.ebo) glDeleteBuffers(1, &m.ebo);
    }
    meshes_.clear();
    for (auto& [h, t] : textures_) {
        (void)h;
        if (t.id) glDeleteTextures(1, &t.id);
    }
    textures_.clear();
    for (auto& [h, p] : programs_) {
        (void)h;
        if (p.id) glDeleteProgram(p.id);
    }
    programs_.clear();
}

uint32_t Gles3RenderDevice::uploadMesh(const earth_engine::render::MeshUploadData& mesh) {
    if (!mesh.valid()) {
        GLOGE("uploadMesh: invalid mesh");
        return 0;
    }
    Mesh m;
    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);

    glGenBuffers(1, &m.vboPos);
    glBindBuffer(GL_ARRAY_BUFFER, m.vboPos);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.positions.size() * sizeof(float)),
                 mesh.positions.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glGenBuffers(1, &m.vboNor);
    glBindBuffer(GL_ARRAY_BUFFER, m.vboNor);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.normals.size() * sizeof(float)),
                 mesh.normals.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    if (mesh.heights.empty()) {
        // 无高度通道：attribute 2 给常量 0（避免 shader 读到垃圾）。
        glVertexAttrib2f(2, 0.0f, 0.0f);
    } else {
        glGenBuffers(1, &m.vboHei);
        glBindBuffer(GL_ARRAY_BUFFER, m.vboHei);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(mesh.heights.size() * sizeof(float)),
                     mesh.heights.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    if (mesh.uvs.empty()) {
        glVertexAttrib2f(3, 0.0f, 0.0f);
    } else {
        glGenBuffers(1, &m.vboUv);
        glBindBuffer(GL_ARRAY_BUFFER, m.vboUv);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(mesh.uvs.size() * sizeof(float)),
                     mesh.uvs.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    if (mesh.displacements.empty()) {
        glVertexAttrib3f(4, 0.0f, 0.0f, 0.0f);
    } else {
        glGenBuffers(1, &m.vboDisp);
        glBindBuffer(GL_ARRAY_BUFFER, m.vboDisp);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(mesh.displacements.size() * sizeof(float)),
                     mesh.displacements.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    glGenBuffers(1, &m.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint32_t)),
                 mesh.indices.data(), GL_STATIC_DRAW);

    m.indexCount = static_cast<GLsizei>(mesh.indices.size());
    glBindVertexArray(0);

    const uint32_t handle = nextMesh_++;
    meshes_.emplace(handle, m);
    stats_.uploadedBytes += mesh.positions.size() * sizeof(float) +
                            mesh.normals.size() * sizeof(float) +
                            mesh.heights.size() * sizeof(float) +
                            mesh.uvs.size() * sizeof(float) +
                            mesh.indices.size() * sizeof(uint32_t);
    stats_.liveMeshes = static_cast<uint32_t>(meshes_.size());
    return handle;
}

void Gles3RenderDevice::releaseMesh(uint32_t handle) {
    const auto it = meshes_.find(handle);
    if (it == meshes_.end()) {
        return; // 幂等
    }
    const Mesh& m = it->second;
    if (m.vao) glDeleteVertexArrays(1, &m.vao);
    if (m.vboPos) glDeleteBuffers(1, &m.vboPos);
    if (m.vboNor) glDeleteBuffers(1, &m.vboNor);
    if (m.vboHei) glDeleteBuffers(1, &m.vboHei);
    if (m.vboUv) glDeleteBuffers(1, &m.vboUv);
    if (m.vboDisp) glDeleteBuffers(1, &m.vboDisp);
    if (m.ebo) glDeleteBuffers(1, &m.ebo);
    meshes_.erase(it);
    stats_.liveMeshes = static_cast<uint32_t>(meshes_.size());
}

uint32_t Gles3RenderDevice::createProgram(const earth_engine::render::ProgramSource& source) {
    if (!source.valid()) {
        GLOGE("createProgram: empty source");
        return 0;
    }
    const GLuint vs = compileShader(GL_VERTEX_SHADER, source.vertexShader.c_str());
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, source.fragmentShader.c_str());
    if (vs == 0 || fs == 0) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    const GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        GLOGE("program link failed: %s", log);
        glDeleteProgram(prog);
        return 0;
    }
    Program p;
    p.id = prog;
    const uint32_t handle = nextProgram_++;
    programs_.emplace(handle, p);
    return handle;
}

void Gles3RenderDevice::releaseProgram(uint32_t handle) {
    const auto it = programs_.find(handle);
    if (it == programs_.end()) {
        return; // 幂等
    }
    if (it->second.id) {
        glDeleteProgram(it->second.id);
    }
    if (boundHandle_ == handle) {
        boundHandle_ = 0;
    }
    programs_.erase(it);
}

void Gles3RenderDevice::useProgram(uint32_t handle) {
    const auto it = programs_.find(handle);
    if (it == programs_.end()) {
        GLOGE("useProgram: unknown handle %u", handle);
        boundHandle_ = 0;
        return;
    }
    boundHandle_ = handle;
    glUseProgram(it->second.id);
}

GLint Gles3RenderDevice::uniformLocation(Program& prog, const char* name) {
    const std::string key = name ? name : "";
    const auto it = prog.uniformLocs.find(key);
    if (it != prog.uniformLocs.end()) {
        return it->second;
    }
    const GLint loc = glGetUniformLocation(prog.id, name);
    prog.uniformLocs.emplace(key, loc); // -1 = 未使用，缓存防重复查询
    return loc;
}

void Gles3RenderDevice::setUniformMat4(const char* name, const float* mat4x4) {
    const auto it = programs_.find(boundHandle_);
    if (it == programs_.end()) {
        return;
    }
    glUniformMatrix4fv(uniformLocation(it->second, name), 1, GL_FALSE, mat4x4);
}

void Gles3RenderDevice::setUniformMat3(const char* name, const float* mat3x3) {
    const auto it = programs_.find(boundHandle_);
    if (it == programs_.end()) {
        return;
    }
    glUniformMatrix3fv(uniformLocation(it->second, name), 1, GL_FALSE, mat3x3);
}

void Gles3RenderDevice::setUniformVec3(const char* name, float x, float y, float z) {
    const auto it = programs_.find(boundHandle_);
    if (it == programs_.end()) {
        return;
    }
    glUniform3f(uniformLocation(it->second, name), x, y, z);
}

uint32_t Gles3RenderDevice::createTexture2D(const earth_engine::render::Texture2DData& data) {
    if (!data.valid()) {
        GLOGE("createTexture2D: invalid data");
        return 0;
    }
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, data.width, data.height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, data.rgba8.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    const uint32_t handle = nextTexture_++;
    textures_.emplace(handle, Texture{id});
    stats_.uploadedBytes += static_cast<uint64_t>(data.width) * data.height * 4u;
    stats_.liveTextures = static_cast<uint32_t>(textures_.size());
    return handle;
}

void Gles3RenderDevice::releaseTexture(uint32_t handle) {
    const auto it = textures_.find(handle);
    if (it == textures_.end()) {
        return;
    }
    glDeleteTextures(1, &it->second.id);
    textures_.erase(it);
    stats_.liveTextures = static_cast<uint32_t>(textures_.size());
}

void Gles3RenderDevice::bindTexture2D(uint32_t unit, uint32_t handle) {
    if (handle == 0) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, 0);
        return;
    }
    const auto it = textures_.find(handle);
    if (it == textures_.end()) {
        GLOGE("bindTexture2D: dead handle %u", handle);
        return;
    }
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, it->second.id);
}

void Gles3RenderDevice::setUniformInt(const char* name, int value) {
    const auto it = programs_.find(boundHandle_);
    if (it == programs_.end()) {
        return;
    }
    glUniform1i(uniformLocation(it->second, name), value);
}

void Gles3RenderDevice::setViewport(int widthPx, int heightPx) {
    glViewport(0, 0, widthPx, heightPx);
}

void Gles3RenderDevice::clearColor(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
}

void Gles3RenderDevice::drawMesh(uint32_t handle) {
    const auto itMesh = meshes_.find(handle);
    const auto itProg = programs_.find(boundHandle_);
    if (itMesh == meshes_.end() || itProg == programs_.end()) {
        GLOGE("drawMesh: dead mesh %u or no bound program", handle);
        return; // 使用错误防御，不崩溃
    }
    glBindVertexArray(itMesh->second.vao);
    glDrawElements(GL_TRIANGLES, itMesh->second.indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    stats_.drawCalls += 1;
    stats_.trianglesDrawn += static_cast<uint64_t>(itMesh->second.indexCount) / 3u;
}

} // namespace demoscene
