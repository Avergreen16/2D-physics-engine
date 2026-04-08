#include "wrapper.hpp"
#include "stb_image.h"
#include "stb_image_write.h"

#include <windows.h>

std::ostream& operator<<(std::ostream& c, glm::vec3 v) {
    c << v.x << " " << v.y << " " << v.z;
    return c;
}

std::string get_text_from_file(std::string path) {
    std::ifstream file;
    file.open(path);

    if(file.is_open()) {
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();

    } else {
        std::cout << "failed to open file " << path << std::endl;
    }

    file.close();
}

std::vector<uint8_t> get_bytes_from_file(std::string path) {
    std::ifstream file;
    file.open(path, std::ios::in | std::ios::binary);

    if(file.is_open()) {
        std::vector<uint8_t> ret;

        uint8_t byte;
        while(file.read((char*)&byte, 1)) {
            ret.push_back(byte);
        }

        file.close();
        return ret;
    } else {
        std::cout << "failed to open file " << path << std::endl;
        file.close();

        return {};
    }
}

void copy_to_clipboard(std::string str) {
    size_t len = str.size() + 1;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    memcpy(GlobalLock(hMem), str.c_str(), len);
    GlobalUnlock(hMem);

    OpenClipboard(NULL);
    EmptyClipboard();
    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();
}

std::string paste_from_clipboard() {
    if (!OpenClipboard(nullptr)) return "";

    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData == nullptr)
    {
        CloseClipboard();
        return "";
    }

    char* pszText = static_cast<char*>(GlobalLock(hData));
    if (pszText == nullptr)
    {
        CloseClipboard();
        return "";
    }

    std::string text(pszText);

    GlobalUnlock(hData);
    CloseClipboard();

    return text;
}

void Vertices::init() {
    glGenBuffers(1, &vertex_buffer);
    glGenVertexArrays(1, &vertex_array);
    glBindVertexArray(vertex_array);

    glGenBuffers(1, &index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);

    initialized = true;
}   

void Vertices::vertex_buffer_data(void* ptr, uint32_t num_vertices_, uint32_t vertex_size, uint32_t usage) {
    bind();
    num_vertices = num_vertices_;
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, num_vertices * vertex_size, ptr, usage);
}

void Vertices::index_buffer_data(void* ptr, uint32_t num_indices_, GLenum index_type_, uint32_t index_size, uint32_t usage) {
    bind();
    num_indices = num_indices_;
    index_type = index_type_;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer); 
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_indices * index_size, ptr, usage);
}

void Vertices::add_vertex_attribute(uint32_t index, uint32_t size, GLenum type, GLenum normalized, uint32_t stride, uint32_t offset) {
    glBindVertexArray(vertex_array);
    if(type == GL_INT || type == GL_UNSIGNED_INT) glVertexAttribIPointer(index, size, type, stride, (void*)offset);
    else glVertexAttribPointer(index, size, type, normalized, stride, (void*)offset);
    glEnableVertexAttribArray(index);  
}

void Vertices::bind() {
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBindVertexArray(vertex_array);
}

void Vertices::draw_vertices(GLenum mode) {
    bind();
    glDrawArrays(mode, 0, num_vertices);
}

void Vertices::draw_indices(GLenum mode) {
    bind();
    glDrawElements(mode, num_indices, index_type, (void*)0);
}

Vertices::Vertices(Vertices&& a) {
    vertex_buffer = a.vertex_buffer;
    a.vertex_buffer = 0;

    vertex_array = a.vertex_array;
    a.vertex_array = 0;

    index_buffer = a.index_buffer;
    a.index_buffer = 0;

    num_indices = a.num_indices;
    a.num_indices = 0;

    num_vertices = a.num_vertices;
    a.num_vertices = 0;

    initialized = a.initialized;
    a.initialized = false;

    index_type = a.index_type;
}

Vertices& Vertices::operator=(Vertices&& a) {
    vertex_buffer = a.vertex_buffer;
    a.vertex_buffer = 0;

    vertex_array = a.vertex_array;
    a.vertex_array = 0;

    index_buffer = a.index_buffer;
    a.index_buffer = 0;

    num_indices = a.num_indices;
    a.num_indices = 0;

    num_vertices = a.num_vertices;
    a.num_vertices = 0;

    initialized = a.initialized;
    a.initialized = false;

    index_type = a.index_type;

    return *this;
}

Vertices::~Vertices() {
    if(initialized) {
        glDeleteBuffers(1, &vertex_buffer);
        glDeleteBuffers(1, &index_buffer);
        glDeleteVertexArrays(1, &vertex_array);
    }
}

void Storage_buffer::init() {
    if(!initialized) {
        glGenBuffers(1, &id);
        initialized = true;
    }
}

void Storage_buffer::buffer_data(void* data, uint32_t size_bytes, GLenum usage) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size_bytes, data, usage);
}

void Storage_buffer::buffer_subdata(void* data, uint32_t size_bytes, uint32_t offset) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size_bytes, data);
}

void Storage_buffer::bind(uint32_t binding) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, id);
}

void Storage_buffer::delete_buffer() {
    glDeleteBuffers(1, &id);
}

Storage_buffer::~Storage_buffer() {
    delete_buffer();
}

void Uniform_buffer::init() {
    if(!initialized) {
        glGenBuffers(1, &id);
        initialized = true;
    }
}

void Uniform_buffer::buffer_data(void* data, uint32_t size_bytes, GLenum usage) {
    glBindBuffer(GL_UNIFORM_BUFFER, id);
    glBufferData(GL_UNIFORM_BUFFER, size_bytes, data, usage);
}

void Uniform_buffer::buffer_subdata(void* data, uint32_t size_bytes, uint32_t offset) {
    glBindBuffer(GL_UNIFORM_BUFFER, id);
    glBufferSubData(GL_UNIFORM_BUFFER, offset, size_bytes, data);
}

void Uniform_buffer::bind(uint32_t binding) {
    glBindBuffer(GL_UNIFORM_BUFFER, id);
    glBindBufferBase(GL_UNIFORM_BUFFER, binding, id);
}

void Uniform_buffer::delete_buffer() {
    glDeleteBuffers(1, &id);
}

Uniform_buffer::~Uniform_buffer() {
    delete_buffer();
}

Uniform_buffer::Uniform_buffer(Uniform_buffer&& a) {
    id = a.id;
    a.id = 0;
}

Uniform_buffer& Uniform_buffer::operator=(Uniform_buffer&& a) {
    id = a.id;
    a.id = 0;

    return *this;
}

Shader::Shader(std::string vspath, std::string fspath) {
    compile(vspath, fspath);
}

Shader::Shader(std::string vspath, std::string gspath, std::string fspath) {
    compile(vspath, gspath, fspath);
}

Shader::Shader(std::string vspath, std::string tcspath, std::string tespath, std::string gspath, std::string fspath) {
    compile(vspath, tcspath, tespath, gspath, fspath);
}

Shader::Shader(std::string cspath) {
    compile(cspath);
}

Shader::Shader(Shader&& s) {
    id = s.id;
    s.id = 0;
}

Shader::~Shader() {
    glDeleteProgram(id);
}

bool Shader::compile(std::string vspath, std::string fspath) {
    std::string vertex_shader_src = get_text_from_file(vspath);
    std::string fragment_shader_src = get_text_from_file(fspath);
    const char* v_ptr = vertex_shader_src.data();
    const char* f_ptr = fragment_shader_src.data();
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &v_ptr, 0);
    glCompileShader(vertex_shader);
    
    // check if vertex shader compiled correctly
    GLint compile_check;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_check);
    if(compile_check == GL_FALSE) {
        int log_size = 0;
        glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &log_size);

        std::vector<char> error_log(log_size);
        glGetShaderInfoLog(vertex_shader, log_size, &log_size, &error_log[0]);
        std::cout << "ERROR: Vertex shader failed to compile:\n" << error_log.data() << "\n";
        std::cout << "Filepath: " << vspath << "\n";

        return false;
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &f_ptr, 0);
    glCompileShader(fragment_shader);
    
    // check if fragment shader compiled correctly
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_check);
    if(compile_check == GL_FALSE) {
        int log_size = 0;
        glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &log_size);

        std::vector<char> error_log(log_size);
        glGetShaderInfoLog(fragment_shader, log_size, &log_size, &error_log[0]);
        std::cout << "ERROR: Fragment shader failed to compile:\n" << error_log.data() << "\n";
        std::cout << "Filepath: " << fspath << "\n";

        glDeleteShader(vertex_shader);
        return false;
    }
    
    // attach and link vertex/fragment shaders
    id = glCreateProgram();
    glAttachShader(id, vertex_shader);
    glAttachShader(id, fragment_shader);
    glLinkProgram(id);

    glGetShaderiv(id, GL_LINK_STATUS, &compile_check);
    if(compile_check == GL_FALSE) {
        int log_size = 0;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &log_size);

        std::vector<char> error_log(log_size);
        glGetShaderInfoLog(id, log_size, &log_size, &error_log[0]);
        std::cout << "ERROR: Program failed to link:\n" << error_log.data() << "\n";
        std::cout << "Filepath: " << fspath << "\n";
        std::cout << "Filepath: " << vspath << "\n";

        glDeleteShader(id);
        return false;
    }

    // delete vertex/fragment shaders after compilation
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return true;
}

bool Shader::compile(std::string cspath) {
    std::string compute_shader_src = get_text_from_file(cspath);
    const char* c_ptr = compute_shader_src.data();
    GLuint compute_shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(compute_shader, 1, &c_ptr, 0);
    glCompileShader(compute_shader);
    
    // check if vertex shader compiled correctly
    GLint compile_check;
    glGetShaderiv(compute_shader, GL_COMPILE_STATUS, &compile_check);
    if(compile_check == GL_FALSE) {
        int log_size = 0;
        glGetShaderiv(compute_shader, GL_INFO_LOG_LENGTH, &log_size);

        std::vector<char> error_log(log_size);
        glGetShaderInfoLog(compute_shader, log_size, &log_size, &error_log[0]);
        std::cout << "ERROR: Compute shader failed to compile:\n" << error_log.data() << "\n";
        std::cout << "Filepath: " << cspath << "\n";

        return false;
    }
    
    // attach and link vertex/fragment shaders
    id = glCreateProgram();
    glAttachShader(id, compute_shader);
    glLinkProgram(id);

    glGetShaderiv(id, GL_LINK_STATUS, &compile_check);
    if(compile_check == GL_FALSE) {
        int log_size = 0;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &log_size);

        std::vector<char> error_log(log_size);
        glGetShaderInfoLog(id, log_size, &log_size, &error_log[0]);
        std::cout << "ERROR: Program failed to link:\n" << error_log.data() << "\n";
        std::cout << "Filepath: " << cspath << "\n";

        glDeleteShader(id);
        return false;
    }

    // delete vertex/fragment shaders after compilation
    glDeleteShader(compute_shader);

    return true;
}

bool Shader::compile(std::string vspath, std::string gspath, std::string fspath) {
    std::string vertex_shader_src = get_text_from_file(vspath);
    std::string fragment_shader_src = get_text_from_file(fspath);
    std::string geometry_shader_src = get_text_from_file(gspath);

    const char* v_ptr = vertex_shader_src.data();
    const char* f_ptr = fragment_shader_src.data();
    const char* g_ptr = geometry_shader_src.data();

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &v_ptr, 0);
    glCompileShader(vertex_shader);
    
    // check if vertex shader compiled correctly
    GLint compile_check;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_check);
    if(compile_check == GL_FALSE) {
        int log_size = 0;
        glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &log_size);

        std::vector<char> error_log(log_size);
        glGetShaderInfoLog(vertex_shader, log_size, &log_size, &error_log[0]);
        std::cout << "ERROR: Vertex shader failed to compile:\n" << error_log.data() << "\n";
        std::cout << "Filepath: " << vspath << "\n";

        return false;
    }

    GLuint geometry_shader = glCreateShader(GL_GEOMETRY_SHADER);
    glShaderSource(geometry_shader, 1, &g_ptr, 0);
    glCompileShader(geometry_shader);
    
    // check if geometry shader compiled correctly
    glGetShaderiv(geometry_shader, GL_COMPILE_STATUS, &compile_check);
    if(compile_check == GL_FALSE) {
        int log_size = 0;
        glGetShaderiv(geometry_shader, GL_INFO_LOG_LENGTH, &log_size);

        std::vector<char> error_log(log_size);
        glGetShaderInfoLog(geometry_shader, log_size, &log_size, &error_log[0]);
        std::cout << "ERROR: Geometry shader failed to compile:\n" << error_log.data() << "\n";
        std::cout << "Filepath: " << gspath << "\n";

        glDeleteShader(geometry_shader);
        return false;
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &f_ptr, 0);
    glCompileShader(fragment_shader);
    
    // check if fragment shader compiled correctly
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_check);
    if(compile_check == GL_FALSE) {
        int log_size = 0;
        glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &log_size);

        std::vector<char> error_log(log_size);
        glGetShaderInfoLog(fragment_shader, log_size, &log_size, &error_log[0]);
        std::cout << "ERROR: Fragment shader failed to compile:\n" << error_log.data() << "\n";
        std::cout << "Filepath: " << fspath << "\n";

        glDeleteShader(vertex_shader);
        return false;
    }
    
    // attach and link vertex/fragment shaders
    id = glCreateProgram();
    glAttachShader(id, vertex_shader);
    glAttachShader(id, geometry_shader);
    glAttachShader(id, fragment_shader);
    glLinkProgram(id);

    // delete vertex/fragment shaders after compilation
    glDeleteShader(vertex_shader);
    glDeleteShader(geometry_shader);
    glDeleteShader(fragment_shader);

    return true;
}

bool Shader::compile(std::string vspath, std::string tcspath, std::string tespath, std::string gspath, std::string fspath) {
    std::string vertex_shader_src = get_text_from_file(vspath);
    std::string tess_control_shader_src = get_text_from_file(tcspath);
    std::string tess_eval_shader_src = get_text_from_file(tespath);
    std::string geometry_shader_src = get_text_from_file(gspath);
    std::string fragment_shader_src = get_text_from_file(fspath);

    const char* v_ptr = vertex_shader_src.data();
    const char* tc_ptr = tess_control_shader_src.data();
    const char* te_ptr = tess_eval_shader_src.data();
    const char* g_ptr = geometry_shader_src.data();
    const char* f_ptr = fragment_shader_src.data();

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &v_ptr, 0);
    glCompileShader(vertex_shader);
    
    // check if vertex shader compiled correctly
    GLint compile_check;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_check);
    if(compile_check == GL_FALSE) {
        int log_size = 0;
        glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &log_size);

        std::vector<char> error_log(log_size);
        glGetShaderInfoLog(vertex_shader, log_size, &log_size, &error_log[0]);
        std::cout << "ERROR: Vertex shader failed to compile:\n" << error_log.data() << "\n";
        std::cout << "Filepath: " << vspath << "\n";

        return false;
    }

    GLuint tess_ctrl_shader = glCreateShader(GL_TESS_CONTROL_SHADER);
    glShaderSource(tess_ctrl_shader, 1, &tc_ptr, 0);
    glCompileShader(tess_ctrl_shader);
    
    // check if tess ctrl shader compiled correctly
    glGetShaderiv(tess_ctrl_shader, GL_COMPILE_STATUS, &compile_check);
    if(compile_check == GL_FALSE) {
        int log_size = 0;
        glGetShaderiv(tess_ctrl_shader, GL_INFO_LOG_LENGTH, &log_size);

        std::vector<char> error_log(log_size);
        glGetShaderInfoLog(tess_ctrl_shader, log_size, &log_size, &error_log[0]);
        std::cout << "ERROR: Tesselation control shader failed to compile:\n" << error_log.data() << "\n";
        std::cout << "Filepath: " << tcspath << "\n";

        glDeleteShader(tess_ctrl_shader);
        return false;
    }
    
    GLuint tess_eval_shader = glCreateShader(GL_TESS_EVALUATION_SHADER);
    glShaderSource(tess_eval_shader, 1, &te_ptr, 0);
    glCompileShader(tess_eval_shader);
    
    // check if tess  shader compiled correctly
    glGetShaderiv(tess_eval_shader, GL_COMPILE_STATUS, &compile_check);
    if(compile_check == GL_FALSE) {
        int log_size = 0;
        glGetShaderiv(tess_eval_shader, GL_INFO_LOG_LENGTH, &log_size);

        std::vector<char> error_log(log_size);
        glGetShaderInfoLog(tess_eval_shader, log_size, &log_size, &error_log[0]);
        std::cout << "ERROR: Tesselation evaluation shader failed to compile:\n" << error_log.data() << "\n";
        std::cout << "Filepath: " << tespath << "\n";

        glDeleteShader(tess_eval_shader);
        return false;
    }

    
    GLuint geometry_shader = glCreateShader(GL_GEOMETRY_SHADER);
    glShaderSource(geometry_shader, 1, &g_ptr, 0);
    glCompileShader(geometry_shader);
    
    // check if geometry shader compiled correctly
    glGetShaderiv(geometry_shader, GL_COMPILE_STATUS, &compile_check);
    if(compile_check == GL_FALSE) {
        int log_size = 0;
        glGetShaderiv(geometry_shader, GL_INFO_LOG_LENGTH, &log_size);

        std::vector<char> error_log(log_size);
        glGetShaderInfoLog(geometry_shader, log_size, &log_size, &error_log[0]);
        std::cout << "ERROR: Geometry shader failed to compile:\n" << error_log.data() << "\n";
        std::cout << "Filepath: " << gspath << "\n";

        glDeleteShader(geometry_shader);
        return false;
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &f_ptr, 0);
    glCompileShader(fragment_shader);
    
    // check if fragment shader compiled correctly
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_check);
    if(compile_check == GL_FALSE) {
        int log_size = 0;
        glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &log_size);

        std::vector<char> error_log(log_size);
        glGetShaderInfoLog(fragment_shader, log_size, &log_size, &error_log[0]);
        std::cout << "ERROR: Fragment shader failed to compile:\n" << error_log.data() << "\n";
        std::cout << "Filepath: " << fspath << "\n";

        glDeleteShader(vertex_shader);
        return false;
    }
    
    // attach and link vertex/fragment shaders
    id = glCreateProgram();
    glAttachShader(id, vertex_shader);
    glAttachShader(id, tess_ctrl_shader);
    glAttachShader(id, tess_eval_shader);
    glAttachShader(id, geometry_shader);
    glAttachShader(id, fragment_shader);
    glLinkProgram(id);

    // delete vertex/fragment shaders after compilation
    glDeleteShader(vertex_shader);
    glDeleteShader(tess_ctrl_shader);
    glDeleteShader(tess_eval_shader);
    glDeleteShader(geometry_shader);
    glDeleteShader(fragment_shader);

    return true;
}

void Shader::use() {
    glUseProgram(id);
}

void Shader::dispatch_compute(glm::uvec3 work_groups) {
    glDispatchCompute(work_groups.x, work_groups.y, work_groups.z);
}

Texture::Texture(std::string path, Format format, int mip_levels) {
    type = GL_TEXTURE_2D;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    this->format = format;

    uint8_t* data = stbi_load(path.data(), &size.x, &size.y, &num_channels, 0);

    if(!data) {
        std::cout << "ERROR: Texture failed to load.\n";
        std::cout << path << "\n";
        return;
    } else {
        glTexStorage2D(GL_TEXTURE_2D, mip_levels + 1, format.format_bits, size.x, size.y);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.x, size.y, format.format, format.bits, data);
        stbi_image_free(data);


        if(mip_levels > 0) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, mip_levels);
            glGenerateMipmap(GL_TEXTURE_2D);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        return;
    }
}

bool Texture::load(std::string path, Format format, int mip_levels) {
    type = GL_TEXTURE_2D;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    this->format = format;

    uint8_t* data = stbi_load(path.data(), &size.x, &size.y, &num_channels, 4);

    if(!data) {
        std::cout << "ERROR: Texture failed to load.\n";
        return false;
    } else {
        glTexStorage2D(GL_TEXTURE_2D, mip_levels + 1, format.format_bits, size.x, size.y);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.x, size.y, format.format, format.bits, data);
        stbi_image_free(data);


        if(mip_levels > 0) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, mip_levels);
            glGenerateMipmap(GL_TEXTURE_2D);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        return true;
    }
}

bool Texture::load(glm::uvec2 size, Format format) {
    type = GL_TEXTURE_2D;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    this->format = format;

    glTexStorage2D(GL_TEXTURE_2D, 1, format.format_bits, size.x, size.y);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return true;
}

bool Texture::load(glm::uvec3 size, Format format) {
    type = GL_TEXTURE_3D;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_3D, id);
    this->format = format;

    glTexStorage3D(GL_TEXTURE_3D, 1, format.format_bits, size.x, size.y, size.z);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return true;
}

Texture::Texture(uint8_t* data, glm::uvec3 size_, GLenum type_, Format format, int mip_levels) {
    type = type_;
    size = size_;
    this->format = format;

    glGenTextures(1, &id);
    glBindTexture(type, id);

    if(!data) {
        std::cout << "ERROR: Texture failed to load. (raw data texture)\n";
        return;
    } else {

        if(this->type == GL_TEXTURE_2D) {
            glTexStorage2D(GL_TEXTURE_2D, mip_levels + 1, format.format_bits, size.x, size.y);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.x, size.y, format.format, format.bits, data);

            if(mip_levels > 0) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, mip_levels);
                glGenerateMipmap(GL_TEXTURE_2D);
            } else {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            }
        } else if(this->type == GL_TEXTURE_3D) {

            if(mip_levels == 0) {
                glTexStorage3D(GL_TEXTURE_3D, mip_levels + 1, format.format_bits, size.x, size.y, size.z);
                glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, size.x, size.y, size.z, format.format, format.bits, data);

                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
            } else {
                glTexStorage3D(GL_TEXTURE_3D, mip_levels, format.format_bits, size.x, size.y, size.z);
                glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, size.x, size.y, size.z, format.format, format.bits, data);

                if(mip_levels == 1) glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                else glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
            }
        }

        return;
    }
}

Texture::Texture(glm::uvec3 size, GLenum type, Format format) {
    this->type = type;
    this->size = size;
    this->format = format;

    glGenTextures(1, &id);
    glBindTexture(type, id);

    if(type == GL_TEXTURE_2D) {
        glTexStorage2D(GL_TEXTURE_2D, 1, format.format_bits, size.x, size.y);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    } else if(type == GL_TEXTURE_3D) {
        glTexStorage3D(GL_TEXTURE_3D, 4, format.format_bits, size.x, size.y, size.z);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    } else if(this->type == GL_TEXTURE_2D_ARRAY) {
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, format.format_bits, size.x, size.y, size.z);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
}

void Texture::bind(int binding) {
    glActiveTexture(GL_TEXTURE0 + binding);
    glBindTexture(type, id);
}

void Texture::bind() {
    glBindTexture(type, id);
}

void Texture::bind_image(unsigned int binding, unsigned int level, GLenum format, unsigned int layer) {
    glBindImageTexture(binding, id, level, GL_TRUE, layer, GL_READ_WRITE, format);
}

Texture::Texture(Texture&& t) noexcept {
    id = t.id;
    size = t.size;
    num_channels = t.num_channels;
    format = t.format;
    t.id = 0;
    type = t.type;
}

Texture& Texture::operator=(Texture&& t) noexcept {
    id = t.id;
    size = t.size;
    num_channels = t.num_channels;
    format = t.format;
    t.id = 0;
    type = t.type;

    return *this;
}

void Texture::delete_texture() {
    glDeleteTextures(1, &id);
}

Texture::~Texture() {
    glDeleteTextures(1, &id);
}

Framebuffer::Framebuffer(glm::ivec2 size_, std::vector<Fb_tex_params>&& tp, GLenum filter) {
    size = size_;
    tex_params = std::move(tp);
    this->filter = filter;

    initialized = true;
    glGenFramebuffers(1, &id);
    glBindFramebuffer(GL_FRAMEBUFFER, id);

    for(int i = 0; i < tex_params.size(); ++i) {
        Fb_tex_params& p = tex_params[i];

        textures.emplace_back(Texture());
        Texture& t = textures[i];

        glm::ivec2 s = size;

        if(p.layers > 1) {
            glGenTextures(1, &t.id);
            glBindTexture(GL_TEXTURE_2D_ARRAY, t.id);
            glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, p.format.format_bits, size.x, size.y, p.layers);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, filter);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, filter);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            t.size = glm::ivec3(size, p.layers);
            t.format = p.format;
            t.type = GL_TEXTURE_2D_ARRAY;
        } else {
            glGenTextures(1, &t.id);
            glBindTexture(GL_TEXTURE_2D, t.id);
            glTexStorage2D(GL_TEXTURE_2D, 1, p.format.format_bits, size.x, size.y);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            t.size = glm::ivec3(size, 1);
            t.format = p.format;
            t.type = GL_TEXTURE_2D;
        }

        glFramebufferTexture(GL_FRAMEBUFFER, p.attachment, t.id, 0);

        if(p.binding != -1) {
            int buffers_size = draw_buffers.size();

            if(p.binding >= buffers_size) {
                for(int j = 0; j < p.binding - buffers_size + 1; ++j) {
                    draw_buffers.push_back(GL_NONE);
                }
            }

            draw_buffers[p.binding] = p.attachment;
        }
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE) std::cerr << "FBO incomplete: " << status << std::endl;
}

Framebuffer::Framebuffer(Framebuffer&& a) noexcept {
    id = a.id;
    a.id = 0;
    textures = std::move(a.textures);
    tex_params = std::move(a.tex_params);
    size = a.size;

    draw_buffers = a.draw_buffers;
}

void Framebuffer::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, id);
    
    glDrawBuffers(draw_buffers.size(), draw_buffers.data());

    glViewport(0, 0, size.x, size.y);
}

void Framebuffer::resize(glm::ivec2 new_size) {
    size = new_size;
    glBindFramebuffer(GL_FRAMEBUFFER, id);

    for(int i = 0; i < tex_params.size(); i++) {
        Fb_tex_params& p = tex_params[i];
        Texture& t = textures[i];
        t.bind();
        t.delete_texture();

        glGenTextures(1, &t.id);
        glBindTexture(t.type, t.id);
        if(t.type == GL_TEXTURE_2D_ARRAY) glTexStorage3D(t.type, 1, p.format.format_bits, size.x, size.y, t.size.z);
        else glTexStorage2D(t.type, 1, p.format.format_bits, size.x, size.y);
        glTexParameteri(t.type, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(t.type, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(t.type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(t.type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture(GL_FRAMEBUFFER, p.attachment, t.id, 0);

        t.size = glm::ivec3(size, 1);
    }
}

Framebuffer::~Framebuffer() {
    if(initialized) {
        glDeleteFramebuffers(1, &id);
    }
}

void Framebuffer::clear() {
    for(int i = 0; i < textures.size(); ++i) {
        Texture& t = textures[i];
        Fb_tex_params& p = tex_params[i];
    }
}

void Framebuffer::bind_texture(std::shared_ptr<Texture> texture, GLenum attachment, int32_t binding) {
    glBindFramebuffer(GL_FRAMEBUFFER, id);

    glFramebufferTexture(GL_FRAMEBUFFER, attachment, texture->id, 0);

    std::vector<GLenum> new_draw_buffers;
    bool inserted = false;
    for(int i = 0; i < draw_buffers.size(); ++i) {
        if(!inserted) {
            if(i == binding) {
                inserted = true;
                new_draw_buffers.push_back(attachment);
            } else {
                new_draw_buffers.push_back(draw_buffers[i]);
            }
        } else {
            new_draw_buffers.push_back(draw_buffers[i]);
        }
    }

    if(!inserted) {
        int buffers_size = draw_buffers.size();
        for(int i = 0; i < binding - buffers_size + 1; ++i) {
            new_draw_buffers.push_back(GL_NONE);
        }

        new_draw_buffers[binding] = attachment;
    }

    draw_buffers = new_draw_buffers;
}

std::vector<uint8_t> get_image(Texture& texture, glm::uvec3 size, unsigned int channels) {
    std::vector<uint8_t> vector(size.x * size.y * size.z * channels);
    texture.bind();
    glGetTexImage(texture.type, 0, texture.format.format, texture.format.bits, vector.data());

    return vector;
}

void get_image(void* ptr, unsigned int buf_size, Texture& texture) {
    texture.bind();
    glGetnTexImage(GL_TEXTURE_3D, 0, texture.format.format, texture.format.bits, buf_size, ptr);
}

void save_texture(Texture& texture, const char* save_path, uvec2 size, int num_channels) {
    std::vector<uint8_t> vector(size.x * size.y * num_channels);
    texture.bind();
    glGetTexImage(GL_TEXTURE_2D, 0, texture.format.format, texture.format.bits, vector.data());

    stbi_flip_vertically_on_write(true);
    stbi_write_png(save_path, size.x, size.y, num_channels, vector.data(), size.x * num_channels);
}