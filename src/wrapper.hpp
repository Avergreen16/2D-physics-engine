#pragma once
#define _USE_MATH_DEFINES

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include <array>
#include <map>
#include <unordered_map>
#include <queue>
#include <fstream>
#include <sstream>
#include <memory>
#include <chrono>
#include <set>
#include <unordered_set>
#include <deque>
#include <variant>
#include <functional>
#include <iomanip>
#include <thread>
#include <mutex>
#include <fstream>
#include <any>
#include <list>
#include <string>
#include <shared_mutex>

#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_RADIANS
#include "glm\glm.hpp"
#include "glm\gtx\matrix_transform_2d.hpp"
#include "glm\gtx\transform.hpp"
#include "glm\gtx\quaternion.hpp"
#include "glm\gtx\orthonormalize.hpp"

using namespace glm;

using lvec3 = vec<3, int64_t>;
using ulvec3 = vec<3, uint64_t>;

std::ostream& operator<<(std::ostream& c, glm::vec3 v);

std::string get_text_from_file(std::string path);

struct Vertices {
    uint32_t vertex_buffer;
    uint32_t vertex_array;
    uint32_t index_buffer;

    uint32_t num_vertices;
    uint32_t num_indices;
    GLenum index_type;

    bool initialized = false;

    Vertices() = default;

    Vertices(const Vertices& a) = delete;

    Vertices& operator=(const Vertices& a) = delete;

    Vertices(Vertices&& a);

    ~Vertices();

    Vertices& operator=(Vertices&& a);

    void init();

    void vertex_buffer_data(void* ptr, uint32_t num_vertices_, uint32_t vertex_size, uint32_t usage);

    void index_buffer_data(void* ptr, uint32_t num_indices_, GLenum index_type_, uint32_t index_size, uint32_t usage);

    void add_vertex_attribute(uint32_t index, uint32_t size, GLenum type, GLenum normalized, uint32_t stride, uint32_t offset);

    void bind();

    void draw_vertices(GLenum mode);

    void draw_indices(GLenum mode);
};

struct Storage_buffer {
    uint32_t id;
    bool initialized = false;

    void init();

    void buffer_data(void* data, uint32_t size_bytes, GLenum usage);

    void buffer_subdata(void* data, uint32_t size_bytes, uint32_t offset);

    void bind(uint32_t binding);

    void delete_buffer();

    ~Storage_buffer();
};

struct Uniform_buffer {
    uint32_t id;
    bool initialized = false;

    void init();

    void buffer_data(void* data, uint32_t size_bytes, GLenum usage);

    void buffer_subdata(void* data, uint32_t size_bytes, uint32_t offset);

    void bind(uint32_t binding);

    void delete_buffer();

    Uniform_buffer() = default;
    

    Uniform_buffer(const Uniform_buffer& a) = delete;

    Uniform_buffer& operator=(const Uniform_buffer& a) = delete;

    Uniform_buffer(Uniform_buffer&& a);

    Uniform_buffer& operator=(Uniform_buffer&& a);

    ~Uniform_buffer();
};

struct Shader {
    uint32_t id;

    Shader() = default;

    bool compile(std::string vspath, std::string fspath);

    bool compile(std::string vspath, std::string gspath, std::string fspath);

    bool compile(std::string vspath, std::string tcspath, std::string tespath, std::string gspath, std::string fspath);

    bool compile(std::string cspath);

    Shader(std::string vspath, std::string fspath);

    Shader(std::string vspath, std::string gspath, std::string fspath);

    Shader(std::string vspath, std::string tcspath, std::string tespath, std::string gspath, std::string fspath);

    Shader(std::string cspath);

    void use();

    static void dispatch_compute(glm::uvec3 work_groups);

    Shader(Shader&& s);

    ~Shader();
};

struct Format {
    GLenum format_bits;
    GLenum format;
    GLenum bits;
};

struct Texture {
    GLuint id;
    GLenum type;
    glm::ivec3 size;
    int num_channels;
    Format format;

    Texture(std::string path, Format format, int mip_levels = 0);

    Texture(uint8_t* data, glm::uvec3 size, GLenum type, Format format, int mip_levels = 0);
    
    Texture(glm::uvec3 size, GLenum type, Format format);

    bool load(std::string path, Format format, int mip_levels = 0);

    bool load(glm::uvec2 size, Format format);

    bool load(glm::uvec3 size, Format format);

    void bind(int binding);

    void bind();

    void bind_image(unsigned int binding, unsigned int level, GLenum format, unsigned int layer = 0);
    
    Texture() = default;

    Texture(const Texture& t) = delete;

    Texture(Texture&& t) noexcept;

    Texture& operator=(Texture&& t) noexcept;

    void delete_texture();

    ~Texture();
};

struct Fb_tex_params {
    Format format;
    GLenum attachment;
    int32_t binding = -1;
    int32_t layers = 1;
};

struct Framebuffer {
    GLuint id;
    std::vector<Texture> textures;
    std::vector<Fb_tex_params> tex_params;
    std::vector<GLenum> draw_buffers;
    bool initialized = false;
    glm::ivec2 size;
    GLenum filter = GL_NEAREST;
    
    Framebuffer() = default;
    Framebuffer(Framebuffer&& a) noexcept;
    Framebuffer& operator=(Framebuffer&& a) noexcept = default;

    Framebuffer(glm::ivec2 size_, std::vector<Fb_tex_params>&& tp, GLenum filter = GL_NEAREST);

    void bind_texture(std::shared_ptr<Texture> texture, GLenum attachment, int32_t binding);

    void bind();

    void resize(glm::ivec2 new_size);

    void clear();

    ~Framebuffer();
};

/*struct Framebuffer_depth {
    GLuint id;
    Texture depth_tex;
    bool initialized = false;

    void init(int width, int height) {
        initialized = true;
        glGenFramebuffers(1, &id);
        glBindFramebuffer(GL_FRAMEBUFFER, id);

        // allocate and bind depth texture
        glGenTextures(1, &depth_tex.id);
        glBindTexture(GL_TEXTURE_2D, depth_tex.id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        glm::vec4 border_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &border_color.x);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_tex.id, 0);

        depth_tex.size = {width, height};

        if(glCheckFramebufferStatus(id) == GL_FRAMEBUFFER_COMPLETE) std::cout << "framebuffer complete\n";
        else std::cout << "Framebuffer NOT complete\n";
    }
    
    void bind() {
        glBindFramebuffer(GL_FRAMEBUFFER, id);
    }

    void bind(int width, int height) {
        glBindFramebuffer(GL_FRAMEBUFFER, id);
        glViewport(0, 0, width, height);
    }

    void resize(glm::ivec2 new_size) {
        bind();

        depth_tex.bind();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, new_size.x, new_size.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
        depth_tex.size = new_size;
    }

    ~Framebuffer_depth() {
        if(initialized) {
            glDeleteFramebuffers(1, &id);
        }
    }
};*/

std::vector<uint8_t> get_image(Texture& texture, glm::uvec3 size, unsigned int channels);

void get_image(void* ptr, unsigned int buf_size, Texture& texture);

void save_texture(Texture& texture, const char* save_path, uvec2 size, int num_channels);