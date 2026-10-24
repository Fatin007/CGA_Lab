// shader.cpp -- Implementation of the Shader helper.

#include "shader.hpp"

// IMPORTANT: glew.h must be included BEFORE glfw3.h anywhere in the project.
// GLEW includes the legacy <GL/gl.h>, which clashes with the GL header that
// GLFW includes. Putting GLEW first avoids the conflict.
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#include <fstream>
#include <sstream>
#include <iostream>

// ---------------------------------------------------------------------------
// Reading a text file into a string.
// ---------------------------------------------------------------------------
static bool read_text_file(const std::string& path, std::string& out_source)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "[Shader] Could not open file: " << path << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    out_source = buffer.str();
    return true;
}

// ---------------------------------------------------------------------------
// Compiling one shader stage and printing any errors.
// ---------------------------------------------------------------------------
static unsigned int compile_stage(GLenum stage, const std::string& source, const std::string& label)
{
    unsigned int id = glCreateShader(stage);
    const char* source_ptr = source.c_str();
    glShaderSource(id, 1, &source_ptr, nullptr);
    glCompileShader(id);

    GLint compiled = GL_FALSE;
    glGetShaderiv(id, GL_COMPILE_STATUS, &compiled);

    if (compiled != GL_TRUE)
    {
        GLint log_length = 0;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &log_length);

        std::string log(static_cast<size_t>(log_length > 0 ? log_length : 1), '\0');
        glGetShaderInfoLog(id, log_length, nullptr, &log[0]);

        std::cerr << "[Shader] Compilation failed for " << label << ":\n" << log << std::endl;

        glDeleteShader(id);
        return 0;
    }

    return id;
}

Shader::~Shader()
{
    if (program_id_ != 0)
        glDeleteProgram(program_id_);
}

bool Shader::load_from_files(const std::string& vertex_path, const std::string& fragment_path)
{
    std::string vertex_source;
    std::string fragment_source;

    if (!read_text_file(vertex_path, vertex_source))   return false;
    if (!read_text_file(fragment_path, fragment_source)) return false;

    unsigned int vertex_shader   = compile_stage(GL_VERTEX_SHADER,   vertex_source,   vertex_path);
    unsigned int fragment_shader = compile_stage(GL_FRAGMENT_SHADER, fragment_source, fragment_path);

    if (vertex_shader == 0 || fragment_shader == 0)
    {
        if (vertex_shader)   glDeleteShader(vertex_shader);
        if (fragment_shader) glDeleteShader(fragment_shader);
        return false;
    }

    program_id_ = glCreateProgram();
    glAttachShader(program_id_, vertex_shader);
    glAttachShader(program_id_, fragment_shader);
    glLinkProgram(program_id_);

    // The shader objects are no longer needed once the program is linked.
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program_id_, GL_LINK_STATUS, &linked);

    if (linked != GL_TRUE)
    {
        GLint log_length = 0;
        glGetProgramiv(program_id_, GL_INFO_LOG_LENGTH, &log_length);

        std::string log(static_cast<size_t>(log_length > 0 ? log_length : 1), '\0');
        glGetProgramInfoLog(program_id_, log_length, nullptr, &log[0]);

        std::cerr << "[Shader] Link failed:\n" << log << std::endl;

        glDeleteProgram(program_id_);
        program_id_ = 0;
        return false;
    }

    return true;
}

void Shader::use() const
{
    glUseProgram(program_id_);
}

// ---------------------------------------------------------------------------
// Uniform setters.
//
// If a uniform name does not exist in the shader (location == -1) the driver
// silently ignores the call. We warn once so such a typo does not become a
// mystery later.
// ---------------------------------------------------------------------------
static void warn_if_missing(GLint location, const char* name)
{
    if (location == -1)
        std::cerr << "[Shader] Warning: uniform '" << name << "' not found (typo?)" << std::endl;
}

void Shader::set_mat4(const char* name, const math3d::Mat4& value) const
{
    GLint location = glGetUniformLocation(program_id_, name);
    warn_if_missing(location, name);
    if (location != -1)
        glUniformMatrix4fv(location, 1, GL_FALSE, value.m);
}

void Shader::set_vec3(const char* name, const math3d::Vec3& value) const
{
    GLint location = glGetUniformLocation(program_id_, name);
    warn_if_missing(location, name);
    if (location != -1)
        glUniform3f(location, value.x, value.y, value.z);
}

void Shader::set_float(const char* name, float value) const
{
    GLint location = glGetUniformLocation(program_id_, name);
    warn_if_missing(location, name);
    if (location != -1)
        glUniform1f(location, value);
}

void Shader::set_int(const char* name, int value) const
{
    GLint location = glGetUniformLocation(program_id_, name);
    warn_if_missing(location, name);
    if (location != -1)
        glUniform1i(location, value);
}

// ---------------------------------------------------------------------------
// Locating the folder that contains the running executable.
// ---------------------------------------------------------------------------
std::string find_asset_path(const std::string& relative_path)
{
#ifdef _WIN32
    char buffer[MAX_PATH] = { 0 };
    DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        std::string exe_path(buffer, length);

        // Cut off the file name, keeping the directory (with trailing slash).
        size_t last_separator = exe_path.find_last_of("\\/");
        if (last_separator != std::string::npos)
        {
            std::string exe_folder = exe_path.substr(0, last_separator + 1);
            return exe_folder + relative_path;
        }
    }
#endif

    // Fallback: assume we are running from the project folder.
    return relative_path;
}
