/*
 * Copyright (C) 2015 Emmanuel Durand
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
 * @texture_texture.h
 * The Texture_Syphon base class
 */

#ifndef SPLASH_TEXTURESYPHON_H
#define SPLASH_TEXTURESYPHON_H

#include <string>
#include <vector>

#include "config.h"
#include "coretypes.h"
#include "basetypes.h"
#include "texture.h"
#include "texture_syphon_client.h"

namespace Splash
{

/**************/
class Texture_Syphon : public Texture
{
    public:
        /**
         * \brief Constructor
         * \param root Root object
         */
        Texture_Syphon(std::weak_ptr<RootObject> root);

        /**
         * \brief Destructor
         */
        ~Texture_Syphon();

        /**
         * No copy constructor, but a move one
         */
        Texture_Syphon(const Texture_Syphon&) = delete;
        Texture_Syphon& operator=(const Texture_Syphon&) = delete;

        /**
         * \brief Bind this texture
         */
        void bind();

        /**
         * \brief Unbind this texture
         */
        void unbind();

        /**
         * \brief Get the shader parameters related to this texture. Texture should be locked first.
         * \return Return the shader uniforms
         */
        std::unordered_map<std::string, Values> getShaderUniforms() const {return _shaderUniforms;}

        /**
         * \brief Get spec of the texture
         * \return Return the texture spec
         */
        ImageBufferSpec getSpec() const {return ImageBufferSpec();}

        /**
         * \brief Try to link the given BaseObject to this object
         * \param obj Shared pointer to the (wannabe) child object
         */
        bool linkTo(std::shared_ptr<BaseObject> obj);

        /**
         * \brief Update the texture according to the owned Image
         */
        void update() {};

    private:
        SyphonReceiver _syphonReceiver;
        std::string _serverName {""};
        std::string _appName {""};

        GLint _activeTexture;

        // Parameters to send to the shader
        std::unordered_map<std::string, Values> _shaderUniforms;

        /**
         * \brief Initialization
         */
        void init();

        /**
         * \brief Register new functors to modify attributes
         */
        void registerAttributes();
};

} // end of namespace

#endif
