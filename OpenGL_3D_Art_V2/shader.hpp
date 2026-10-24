// shader.hpp -- Loads, compiles and links GLSL programs.
//
// Keeping this in one place means the rest of the project never has to touch
// raw glCreateShader / glCompileShader calls.

#pragma once

#include "math3d.hpp"

#include <string>

class Shader
{
public:
    Shader() = default;
    ~Shader();

    // Non-copyable: the GL program name must not be duplicated.
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    // Loads GLSL source from the given files and builds the program.
    // Returns false and prints the compiler log on failure.
    bool load_from_files(const std::string& vertex_path, const std::string& fragment_path);

    // Makes this shader the active one for subsequent draw calls.
    void use() const;

    void set_mat4(const char* name, const math3d::Mat4& value) const;
    void set_vec3(const char* name, const math3d::Vec3& value) const;
    void set_float(const char* name, float value) const;
    void set_int(const char* name, int value) const;

private:
    unsigned int program_id_ = 0;
};

// Resolves a path relative to the folder containing the .exe file.
//
// Why? If the user double-clicks the .exe in Windows Explorer, the working
// directory is System32 and a path like "assets/shaders/planet.vert" would
// not be found. Anchoring to the executable makes the program run from
// anywhere.
std::string find_asset_path(const std::string& relative_path);
