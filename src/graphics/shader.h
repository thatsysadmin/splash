/*
 * Copyright (C) 2013 Emmanuel Durand
 *
 * This file is part of Splash.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Splash is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Splash.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * @shader.h
 * The Shader class
 */

#ifndef SPLASH_SHADER_H
#define SPLASH_SHADER_H

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "./core/constants.h"

#include "./core/attribute.h"
#include "./core/graph_object.h"
#include "./graphics/texture.h"

namespace Splash
{

class Shader final : public GraphObject
{
  public:
    enum ProgramType
    {
        prgGraphic = 0,
        prgCompute,
        prgFeedback
    };

    enum ShaderType
    {
        vertex = 0,
        tess_ctrl,
        tess_eval,
        geometry,
        fragment,
        compute
    };

    enum Sideness
    {
        doubleSided = 0,
        singleSided,
        inverted
    };

    enum Fill
    {
        texture = 0,
        texture_rect,
        object_cubemap,
        cubemap_projection,
        color,
        image_filter,
        blacklevel_filter,
        color_curves_filter,
        primitiveId,
        uv,
        userDefined,
        warp,
        warpControl,
        wireframe,
        window
    };

    /**
     * Constructor
     * \param type Shader type
     */
    explicit Shader(ProgramType type = prgGraphic);

    /**
     * Destructor
     */
    ~Shader() final;

    /**
     * Constructors/operators
     */
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) = delete;
    Shader& operator=(Shader&&) = delete;

    /**
     * Activate this shader
     */
    void activate();

    /**
     * Deactivate this shader
     */
    void deactivate();

    /**
     * Launch the compute shader, if present
     * \param numGroupsX Compute group count along X
     * \param numGroupsY Compute group count along Y
     */
    void doCompute(GLuint numGroupsX = 1, GLuint numGroupsY = 1);

    /**
     * Set the sideness of the object
     * \param side Sideness
     */
    void setSideness(const Sideness side);

    /**
     * Get the sideness of the object
     * \return Return the sideness
     */
    Sideness getSideness() const { return _sideness; }

    /**
     * Get the list of uniforms in the shader program
     * \return Return a map of uniforms and their values
     */
    std::map<std::string, Values> getUniforms() const;

    /**
     * Get the documentation for the uniforms based on the comments in GLSL code
     * \return Return a map of uniforms and their documentation
     */
    std::unordered_map<std::string, std::string> getUniformsDocumentation() const { return _uniformsDocumentation; }

    /**
     * Set a shader source
     * \param src Shader string
     * \param type Shader type
     * \return Return true if the shader was compiled successfully
     */
    bool setSource(const std::string& src, const ShaderType type);

    /**
     * Set multiple shaders at once
     * \param sources Map of shader sources
     * \return Return true if all shader could be compiled
     */
    bool setSource(const std::map<ShaderType, std::string>& sources);

    /**
     * Set a shader source from file
     * \param filename Shader file
     * \param type Shader type
     * \return Return true if the shader was compiled successfully
     */
    bool setSourceFromFile(const std::string& filename, const ShaderType type);

    /**
     * Add a new texture to use
     * \param texture Texture
     * \param textureUnit GL texture unit
     * \param name Texture name
     */
    void setTexture(const std::shared_ptr<Texture>& texture, const GLuint textureUnit, const std::string& name);

    /**
     * Set the model view and projection matrices
     * \param mv View matrix
     * \param mp Projection matrix
     */
    void setModelViewProjectionMatrix(const glm::dmat4& mv, const glm::dmat4& mp);

    /**
     * Set the currently queued uniforms updates
     */
    void updateUniforms();

  private:
    mutable std::mutex _mutex;
    std::atomic_bool _activated{false};
    ProgramType _programType{prgGraphic};

    std::unordered_map<int, GLuint> _shaders;
    std::unordered_map<int, std::string> _shadersSource;
    GLuint _program{0};
    bool _isLinked = {false};

    struct Uniform
    {
        std::string type{""};
        uint32_t elementSize{1};
        uint32_t arraySize{0};
        Values values{};
        GLint glIndex{-1};
        GLuint glBuffer{0};
        bool glBufferReady{false};
    };
    std::map<std::string, Uniform> _uniforms;
    std::unordered_map<std::string, std::string> _uniformsDocumentation;
    std::vector<std::string> _uniformsToUpdate;
    std::vector<std::shared_ptr<Texture>> _textures; // Currently used textures
    std::string _currentProgramName{};

    // Rendering parameters
    Fill _fill{texture};
    std::string _shaderOptions{""};
    Sideness _sideness{doubleSided};

    /**
     * Compile the shader program
     */
    void compileProgram();

    /**
     * Link the shader program
     */
    bool linkProgram();

    /**
     * Parses the shader to replace includes by the corresponding sources
     * \param src Shader source
     */
    void parseIncludes(std::string& src);

    /**
     * Parses the shader to find uniforms
     */
    void parseUniforms(const std::string& src);

    /**
     * Get a string expression of the shader type, used for logging
     * \param type Shader type
     * \return Return the shader type as a string
     */
    std::string stringFromShaderType(int type);

    /**
     * Replace a shader with an empty one
     * \param type Shader type
     */
    void resetShader(ShaderType type);

    /**
     * Register new functors to modify attributes
     */
    void registerAttributes();
    void registerGraphicAttributes();
    void registerComputeAttributes();
    void registerFeedbackAttributes();
};

} // namespace Splash

#endif // SPLASH_SHADER_H
