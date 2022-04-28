/*
 * Copyright (C) 2022 Emmanuel Durand
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
 * @xrcamera.h
 * OpenXR camera
 */

#ifndef SPLASH_XRCAMERA_H
#define SPLASH_XRCAMERA_H

#include <memory>

#include "./core/graph_object.h"
#include "./graphics/object.h"
#include "./graphics/texture_image.h"

namespace Splash
{

class XRCamera : public GraphObject
{
  public:
    /**
     * Constructor
     * \param root Root object
     */
    explicit XRCamera(RootObject* root);

    /**
     * Destructor
     */
    ~XRCamera() override;

    /**
     * No move or copy constructors
     */
    XRCamera(const XRCamera&) = delete;
    XRCamera& operator=(const XRCamera&) = delete;
    XRCamera(XRCamera&&) = delete;
    XRCamera& operator=(XRCamera&&) = delete;

    /**
     * Get the output texture for this camera
     * \return Return a pointer to the output texture
     */
    std::shared_ptr<Texture_Image> getTexture() const;

    /**
     * Get the timestamp
     * \return Return the timestamp in us
     */
    int64_t getTimestamp() const final;

    /**
     * Render this camera into its textures
     */
    void render() override;

  protected:
    /**
     * Try to link the given GraphObject to this object
     * \param obj Shared pointer to the (wannabe) child object
     */
    bool linkIt(const std::shared_ptr<GraphObject>& obj) final;

    /**
     * Try to unlink the given GraphObject from this object
     * \param obj Shared pointer to the (supposed) child object
     */
    void unlinkIt(const std::shared_ptr<GraphObject>& obj) final;

  private:
    std::vector<std::weak_ptr<Object>> _objects;

    /**
     * Register new functors to modify attributes
     */
    void registerAttributes();
};

} // namespace Splash

#endif // SPLASH_XRCAMERA_H
