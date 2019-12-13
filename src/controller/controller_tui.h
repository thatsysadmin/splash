/*
 * Copyright (C) 2019 Emmanuel Durand
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
 * @controller_tui.h
 * The Gui base class
 */

#ifndef SPLASH_TUI_H
#define SPLASH_TUI_H

#include <imtui/imtui.h>

#include "./config.h"

#include "./controller/controller.h"

namespace Splash
{

/*************/
class Tui : public ControllerObject
{
  public:
    /** Constructor
     * \param root Root scene
     */
    Tui(RootObject* root);

    /**
     * Destructor
     */
    ~Tui() final;

    /**
     * No copy constructor
     */
    Tui(const Tui&) = delete;
    Tui& operator=(const Tui&) = delete;

    /**
     * \brief Render this gui
     */
    void render();

    /**
     * \brief Specify the configuration path (as loaded by World)
     * \param path Configuration path
     */
    void setConfigFilePath(const std::string& path) { _configurationPath = path; }

  protected:
    /**
     * \brief Try to link the given GraphObject to this object
     * \param obj Shared pointer to the (wannabe) child object
     */
    bool linkIt(const std::shared_ptr<GraphObject>& obj) final;

    /**
     * \brief Try to unlink the given GraphObject from this object
     * \param obj Shared pointer to the (supposed) child object
     */
    void unlinkIt(const std::shared_ptr<GraphObject>& obj) final;

  private:
    // ImTui objects
    ImTui::TScreen _tuiScreen{};

    // ImGui objects
    ImGuiContext* _imGuiContext{nullptr};
    ImGuiWindowFlags _windowFlags{0};

    Scene* _scene;
    double _previousRenderTime{0.0};
    std::string _configurationPath;
};

} // namespace Splash

#endif // SPLASH_TUI_H
