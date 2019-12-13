#include "./controller/controller_tui.h"

#include <imtui/imtui-impl-ncurses.h>
#include <imtui/imtui.h>

#include "./core/scene.h"
#include "./utils/timer.h"

namespace Splash
{

/*************/
Tui::Tui(RootObject* root)
    : ControllerObject(root)
{
    _type = "tui";
    _renderingPriority = Priority::GUI;

    _scene = dynamic_cast<Scene*>(root);
    if (!_scene)
        return;

    _imGuiContext = ImGui::CreateContext();
    assert(_imGuiContext != nullptr);
    ImGui::SetCurrentContext(_imGuiContext);

    ImTui_ImplNcurses_Init();
    ImTui_ImplText_Init();
}

/*************/
Tui::~Tui()
{
    ImTui_ImplText_Shutdown();
    ImTui_ImplNcurses_Shutdown();
}

/*************/
bool Tui::linkIt(const std::shared_ptr<GraphObject>&)
{
    return false;
}

/*************/
void Tui::unlinkIt(const std::shared_ptr<GraphObject>&) {}

/*************/
void Tui::render()
{
    assert(_imGuiContext != nullptr);
    ImGui::SetCurrentContext(_imGuiContext);
    ImTui_ImplNcurses_NewFrame(_tuiScreen);
    ImTui_ImplText_NewFrame();

    const auto currentTime = static_cast<double>(Timer::getTime()) / 1e6;
    ImGui::GetIO().DeltaTime = static_cast<float>(currentTime - _previousRenderTime);
    _previousRenderTime = currentTime;

    ImGui::NewFrame();

    ImGui::Begin("Splash Control Panel", nullptr, _windowFlags);
    _windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse;

    // if (_showAbout)
    //    renderSplashScreen();

    // if (_showHelp)
    //    renderHelp();

    // Check whether the GUI is alone in its window
    auto objReversedLinks = getObjectReversedLinks();
    auto objLinks = getObjectLinks();
    // if (_fullscreen)
    //{
    //    ImGui::SetWindowPos(ImVec2(0, 0));
    //    ImGui::SetWindowSize(ImVec2(_width, _height));
    //    _windowFlags |= ImGuiWindowFlags_NoMove;
    //}
    // else
    //{
    // ImGui::SetWindowPos(ImVec2(_initialGuiPos[0], _initialGuiPos[1]), ImGuiCond_Once);
    //}

    // drawMenuBar();

    // ImGui::BeginChild("##controlWidgets", ImVec2(0, -256));
    // if (ImGui::BeginTabBar("Tabs", ImGuiTabBarFlags_None))
    //{
    //    // Main tabulation
    //    if (ImGui::BeginTabItem("Main"))
    //    {
    //        drawMainTab();
    //        ImGui::EndTabItem();
    //    }

    //    // Controls tabulations
    //    for (auto& widget : _guiWidgets)
    //    {
    //        widget->update();
    //        if (ImGui::BeginTabItem(widget->getName().c_str()))
    //        {
    //            ImGui::BeginChild(widget->getName().c_str());
    //            widget->render();
    //            ImGui::EndChild();
    //            ImGui::EndTabItem();
    //        }
    //        _windowFlags |= widget->updateWindowFlags();
    //    }

    //    ImGui::EndTabBar();
    //}
    // ImGui::EndChild();

    //// Health information
    // if (!_guiBottomWidgets.empty())
    //{
    //    ImGui::Spacing();
    //    ImGui::Separator();
    //    ImGui::Separator();
    //    ImGui::Spacing();

    //    ImGui::BeginChild("##bottomWidgets", ImVec2(0, -44));
    //    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    //    for (auto& widget : _guiBottomWidgets)
    //    {
    //        widget->update();
    //        ImGui::BeginChild(widget->getName().c_str(), ImVec2(availableSize.x / static_cast<float>(_guiBottomWidgets.size()), 0), true);
    //        widget->render();
    //        ImGui::EndChild();
    //        ImGui::SameLine();
    //    }
    //    ImGui::EndChild();
    //}

    // Master clock
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild("##masterClock", ImVec2(0, 0));
    {

        int year{0}, month{0}, day{0}, hour{0}, minute{0}, second{0}, frame{0};
        bool paused{true};

        auto tree = _root->getTree();
        Value clock;
        if (tree->getValueForLeafAt("/world/attributes/masterClock", clock) && clock.size() == 8)
        {
            year = clock[0].as<int>();
            month = clock[1].as<int>();
            day = clock[2].as<int>();
            hour = clock[3].as<int>();
            minute = clock[4].as<int>();
            second = clock[5].as<int>();
            frame = clock[6].as<int>();
            paused = clock[7].as<bool>();
        }

        ostringstream stream;
        stream << setfill('0') << setw(2) << year;
        stream << "/" << setfill('0') << setw(2) << month;
        stream << "/" << setfill('0') << setw(2) << day;
        stream << " - " << setfill('0') << setw(2) << hour;
        stream << ":" << setfill('0') << setw(2) << minute;
        stream << ":" << setfill('0') << setw(2) << second;
        stream << ":" << setfill('0') << setw(3) << frame;
        stream << (paused ? " - Paused" : " - Playing");

        ImGui::Text("%s", stream.str().c_str());
    }
    ImGui::EndChild();

    ImGui::End();

    ImGui::Render();
    ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), _tuiScreen);
    ImTui_ImplNcurses_DrawScreen(_tuiScreen);
}

} // namespace Splash
