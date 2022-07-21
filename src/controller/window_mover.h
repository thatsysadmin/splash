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
 * @window_mover.h
 * The WindowMover class, which allows for moving projection windows with
 * the mouse easily
 */

#include <string>
#include <vector>

#include "./controller/controller.h"
#include "./userinput/userinput.h"

namespace Splash
{

class WindowMover final : public ControllerObject
{
  public:
    /**
     * Constructor
     * \param root Root object
     */
    explicit WindowMover(RootObject* root);

    /**
     * Set the mouse state
     * \param state Mouse state
     */
    void setMouseState(const std::vector<UserInput::State>& state);

  private:
    int _mousePosition[2];
    int _previousMousePosition[2];
    bool _isWindowDragged{false};
    std::string _draggedWindowName;
};

} // namespace Splash
