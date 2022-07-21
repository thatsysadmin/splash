#include "./controller/window_mover.h"

#include <algorithm>

#include "./graphics/window.h"
#include "./utils/log.h"

namespace Splash
{

/*************/
WindowMover::WindowMover(RootObject* root)
    : ControllerObject(root)
{
    _type = "windowMover";
    _renderingPriority = Priority::POST_WINDOW;
}

/*************/
void WindowMover::setMouseState(const std::vector<UserInput::State>& state)
{
    for (const auto& s : state)
    {
        const auto windowName = s.window;
        const auto object = getObjectPtr(windowName);
        if (object == nullptr)
        {
            Log::get() << Log::DEBUGGING << "WindowMover::" << __FUNCTION__ << " - Input mouse state names a Window which does not exist" << Log::endl;
            continue;
        }

        auto window = std::dynamic_pointer_cast<Window>(object);
        if (window == nullptr)
        {
            Log::get() << Log::DEBUGGING << "WindowMover::" << __FUNCTION__ << " - Input mouse state names an object which seems not to be a Window" << Log::endl;
            continue;
        }
            
        // GUI windows should have decoration, and we don't want to mess
        // with the GUI controls
        if (window->hasGUI())
            continue;

        const auto windowPositionAttr = getObjectAttribute(windowName, "position");

        if (s.action == "mouse_press")
        {
            _isWindowDragged = true;
        }
        else if (s.action == "mouse_release")
        {
            _isWindowDragged = false;
        }
        else if (s.action == "mouse_position")
        {
            _mousePosition[0] = s.value[0].as<int>();
            _mousePosition[1] = s.value[1].as<int>();

            std::cout << _mousePosition[0] << " : " << _mousePosition[1] << "\n";

            if (!_isWindowDragged)
            {
                _previousWindowPosition[0] = windowPositionAttr[0].as<int>();
                _previousWindowPosition[1] = windowPositionAttr[1].as<int>();

                _previousMousePosition[0] = _mousePosition[0];
                _previousMousePosition[1] = _mousePosition[1];
            }
            else
            {
                const auto deltaX = _mousePosition[0] - _previousMousePosition[0];
                const auto deltaY = _mousePosition[1] - _previousMousePosition[1];

                _previousWindowPosition[0] = _previousWindowPosition[0] + deltaX;
                _previousWindowPosition[1] = _previousWindowPosition[1] + deltaY;

                //std::cout << newWindowPositionX << " : " << newWindowPositionY << "\n";
                setObjectAttribute(windowName, "position", {_previousWindowPosition[0], _previousWindowPosition[1]});

                _previousMousePosition[0] = _mousePosition[0];
                _previousMousePosition[1] = _mousePosition[1];
            }
        }
    }
}

}
