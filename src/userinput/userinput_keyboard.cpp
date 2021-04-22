#include "./userinput/userinput_keyboard.h"

namespace Splash
{
/*************/
Keyboard::Keyboard(RootObject* root)
    : UserInput(root)
{
    _type = SPLASH_GRAPH_TYPE_KEYBOARD;
}

/*************/
Keyboard::~Keyboard()
{
}

/*************/
void Keyboard::updateMethod()
{
    while (true)
    {
        GLFWwindow* win;
        int key, action, mods;
        if (!Window::getKey(win, key, action, mods))
            break;

        State substate;
        switch (action)
        {
        case GLFW_PRESS:
            substate.action = "keyboard_press";
            break;
        case GLFW_RELEASE:
            substate.action = "keyboard_release";
            break;
        case GLFW_REPEAT:
            substate.action = "keyboard_repeat";
            break;
        }

        substate.value = Values({Value(key)});
        substate.modifiers = mods;
        substate.window = getWindowName(win);

        _state.emplace_back(std::move(substate));
    }

    while (true)
    {
        GLFWwindow* win;
        unsigned int unicodeChar;
        if (!Window::getChar(win, unicodeChar))
            break;

        State substate;
        substate.action = "keyboard_unicodeChar";
        substate.value = Values({Value(unicodeChar)});
        substate.window = getWindowName(win);
        _state.emplace_back(std::move(substate));
    }
}

} // end of namespace
