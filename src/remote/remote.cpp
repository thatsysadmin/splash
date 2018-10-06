#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>

#include <emscripten/emscripten.h>
#include <imgui.h>

#include <GLES3/gl3.h>
#define GLFW_INCLUDE_ES3
#include <GLFW/glfw3.h>

#include "./core/coretypes.h"
#include "./core/resizable_array.h"
#include "./core/tree.h"
#include "./network/socket_client.h"
#include "./network/socket_messages.h"

// TODO: clean things up on quitting
// TODO: add support for keyboard
// TODO: improve support for mouse, especially supported multiple mouse buttons at at time

using namespace std;

static GLint _imGuiShaderHandle, _imGuiVertHandle, _imGuiFragHandle;
static GLint _imGuiTextureLocation;
static GLint _imGuiPositionLocation;
static GLint _imGuiProjMatrixLocation;
static GLint _imGuiUVLocation;
static GLint _imGuiColorLocation;
static GLuint _imGuiVaoHandle;
static GLuint _imGuiVboHandle;
static GLuint _imGuiElementsHandle;
static GLuint _imFontTextureId;

Splash::SocketClient _websocketClient;
Splash::Tree::Root _tree;
bool _isConnected{false};

GLFWwindow* _window{nullptr};
double _mouseX, _mouseY;
int _mouseButton, _mouseAction, _mouseMods;

/*************/
void mouseBtnCallback(GLFWwindow* win, int button, int action, int mods)
{
    _mouseButton = button;
    _mouseAction = action;
    _mouseMods = mods;
}

/*************/
void mousePosCallback(GLFWwindow* win, double xpos, double ypos)
{
    _mouseX = xpos;
    _mouseY = ypos;
}

/*************/
void imguiRenderDrawLists(ImDrawData* draw_data)
{
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);

    const float width = ImGui::GetIO().DisplaySize.x;
    const float height = ImGui::GetIO().DisplaySize.y;
    const float orthoProjection[4][4] = {
        {2.0f / width, 0.0f, 0.0f, 0.0f},
        {0.0f, 2.0f / -height, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f, 1.0f},
    };

    glUseProgram(_imGuiShaderHandle);
    glUniform1i(_imGuiTextureLocation, 0);
    glUniformMatrix4fv(_imGuiProjMatrixLocation, 1, GL_FALSE, (float*)orthoProjection);
    glBindVertexArray(_imGuiVaoHandle);

    for (int n = 0; n < draw_data->CmdListsCount; ++n)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        const ImDrawIdx* idx_buffer_offset = 0;

        glBindBuffer(GL_ARRAY_BUFFER, _imGuiVboHandle);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.size() * sizeof(ImDrawVert), (GLvoid*)&cmd_list->VtxBuffer.front(), GL_STREAM_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _imGuiElementsHandle);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx), (GLvoid*)&cmd_list->IdxBuffer.front(), GL_STREAM_DRAW);

        for (const ImDrawCmd* pcmd = cmd_list->CmdBuffer.begin(); pcmd != cmd_list->CmdBuffer.end(); ++pcmd)
        {
            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
                glScissor((int)pcmd->ClipRect.x, (int)(height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
                glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset);
            }
            idx_buffer_offset += pcmd->ElemCount;
        }
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
}

/*************/
bool imguiInit()
{
    using namespace ImGui;

    // Initialize GL stuff for ImGui
    const GLchar* vertexShader = R"(
        uniform mat4 ProjMtx;
        attribute vec2 Position;
        attribute vec2 UV;
        attribute vec4 Color;
        varying vec2 Frag_UV;
        varying vec4 Frag_Color;

        void main()
        {
            Frag_UV = UV;
            Frag_Color = Color;
            gl_Position = ProjMtx * vec4(Position.xy, 0, 1);
        }
    )";

    const GLchar* fragmentShader = R"(
        precision mediump float;

        uniform sampler2D Texture;
        varying vec2 Frag_UV;
        varying vec4 Frag_Color;

        void main()
        {
            gl_FragColor = Frag_Color * texture2D(Texture, Frag_UV);
        }
    )";

    _imGuiShaderHandle = glCreateProgram();
    _imGuiVertHandle = glCreateShader(GL_VERTEX_SHADER);
    _imGuiFragHandle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(_imGuiVertHandle, 1, &vertexShader, 0);
    glShaderSource(_imGuiFragHandle, 1, &fragmentShader, 0);

    glCompileShader(_imGuiVertHandle);
    glCompileShader(_imGuiFragHandle);
    glAttachShader(_imGuiShaderHandle, _imGuiVertHandle);
    glAttachShader(_imGuiShaderHandle, _imGuiFragHandle);
    glLinkProgram(_imGuiShaderHandle);

    GLint status;
    glGetProgramiv(_imGuiShaderHandle, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        cout << "Error while linking the shader program" << endl;

        GLint length;
        glGetProgramiv(_imGuiShaderHandle, GL_INFO_LOG_LENGTH, &length);
        char* log = (char*)malloc(length);
        glGetProgramInfoLog(_imGuiShaderHandle, length, &length, log);
        cout << "Error log: \n" << (const char*)log << endl;
        free(log);
        return false;
    }

    _imGuiTextureLocation = glGetUniformLocation(_imGuiShaderHandle, "Texture");
    _imGuiProjMatrixLocation = glGetUniformLocation(_imGuiShaderHandle, "ProjMtx");
    _imGuiPositionLocation = glGetAttribLocation(_imGuiShaderHandle, "Position");
    _imGuiUVLocation = glGetAttribLocation(_imGuiShaderHandle, "UV");
    _imGuiColorLocation = glGetAttribLocation(_imGuiShaderHandle, "Color");

    glGenBuffers(1, &_imGuiVboHandle);
    glGenBuffers(1, &_imGuiElementsHandle);

    glGenVertexArrays(1, &_imGuiVaoHandle);
    glBindVertexArray(_imGuiVaoHandle);
    glBindBuffer(GL_ARRAY_BUFFER, _imGuiVboHandle);
    glEnableVertexAttribArray(_imGuiPositionLocation);
    glEnableVertexAttribArray(_imGuiUVLocation);
    glEnableVertexAttribArray(_imGuiColorLocation);

    glVertexAttribPointer(_imGuiPositionLocation, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)(size_t) & (((ImDrawVert*)0)->pos));
    glVertexAttribPointer(_imGuiUVLocation, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)(size_t) & (((ImDrawVert*)0)->uv));
    glVertexAttribPointer(_imGuiColorLocation, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)(size_t) & (((ImDrawVert*)0)->col));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Initialize ImGui
    ImGuiIO& io = GetIO();

    // Set style
    ImGuiStyle& style = ImGui::GetStyle();
    style.AntiAliasedLines = true;
    style.ChildRounding = 2.f;
    style.ChildBorderSize = 1.f;
    style.FrameRounding = 2.f;
    style.PopupBorderSize = 0.f;
    style.ScrollbarSize = 12.f;
    style.WindowBorderSize = 0.f;
    style.Colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_Border] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.90f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.45f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.79f, 0.45f, 0.17f, 0.80f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.79f, 0.45f, 0.17f, 0.80f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.98f, 0.58f, 0.12, 0.74f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.98f, 0.58f, 0.12, 0.74f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.98f, 0.58f, 0.12, 0.74f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.58f, 0.12, 0.74f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.81f, 0.40f, 0.25f, 0.27f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.81f, 0.40f, 0.24f, 0.40f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.80f, 0.50f, 0.50f, 0.40f);
    style.Colors[ImGuiCol_ColumnHovered] = ImVec4(0.60f, 0.40f, 0.40f, 0.45f);
    style.Colors[ImGuiCol_ColumnActive] = ImVec4(0.65f, 0.50f, 0.50f, 0.55f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 0.50f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.80f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.67f, 0.40f, 0.40f, 0.60f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.67f, 0.40f, 0.40f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.50f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.81f, 0.40f, 0.24f, 0.45f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.79f, 0.45f, 0.17f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.79f, 0.53f, 0.21f, 0.80f);
    style.Colors[ImGuiCol_Column] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_ColumnHovered] = ImVec4(0.60f, 0.40f, 0.40f, 1.00f);
    style.Colors[ImGuiCol_ColumnActive] = ImVec4(0.80f, 0.47f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.00f, 1.00f, 0.35f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.10f, 0.90f);

    unsigned char* pixels;
    int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);

    // Set GL texture for font
    glGenTextures(1, &_imFontTextureId);
    glBindTexture(GL_TEXTURE_2D, _imFontTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    io.Fonts->TexID = (void*)(intptr_t)_imFontTextureId;

    return true;
}

/**************/
void main_loop()
{
    _tree.processQueue();

    // GUI rendering
    auto& io = ImGui::GetIO();

    io.MousePos = ImVec2(static_cast<float>(_mouseX), static_cast<float>(_mouseY));
    bool isButtonPressed = _mouseAction == GLFW_PRESS ? true : false;
    switch (_mouseButton)
    {
    default:
        break;
    case GLFW_MOUSE_BUTTON_LEFT:
        io.MouseDown[0] = isButtonPressed;
        break;
    case GLFW_MOUSE_BUTTON_RIGHT:
        io.MouseDown[1] = isButtonPressed;
        break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        io.MouseDown[2] = isButtonPressed;
        break;
    }
    io.KeyCtrl = (_mouseMods & GLFW_MOD_CONTROL) != 0;
    io.KeyShift = (_mouseMods & GLFW_MOD_SHIFT) != 0;

    static double time = 0.0;
    auto currentTime = glfwGetTime();
    io.DeltaTime = static_cast<float>(currentTime - time);
    time = currentTime;

    int displayWidth, displayHeight;
    glfwGetFramebufferSize(_window, &displayWidth, &displayHeight);
    io.DisplaySize = ImVec2(static_cast<float>(displayWidth), static_cast<float>(displayHeight));
    io.DisplayFramebufferScale = ImVec2(1.0, 1.0);

    glViewport(0, 0, displayWidth, displayHeight);
    glClearColor(0.1, 0.1, 0.1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui::NewFrame();
    ImGui::ShowDemoWindow();
    ImGui::Render();
    imguiRenderDrawLists(ImGui::GetDrawData());
    ImGui::EndFrame();

    glfwSwapBuffers(_window);

    _websocketClient.sendUpdatesToServer(_tree);
}

/*************/
int main()
{
    if (!_websocketClient.connect("127.0.0.1", 9090))
    {
        cout << "Error while initializing network connection" << endl;
        return 1;
    }

    if (!glfwInit())
    {
        cout << "Error while initializing GLFW" << endl;
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 0);

    glfwWindowHint(GLFW_SRGB_CAPABLE, GL_TRUE);
    glfwWindowHint(GLFW_VISIBLE, true);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    _window = glfwCreateWindow(1280, 720, "splash-remote", nullptr, nullptr);

    if (!_window)
    {
        cout << "Unable to create a GLFW window" << endl;
        return 1;
    }
    glfwSetMouseButtonCallback(_window, mouseBtnCallback);
    glfwSetCursorPosCallback(_window, mousePosCallback);

    glfwMakeContextCurrent(_window);
    ImGui::CreateContext();
    imguiInit();

    emscripten_set_main_loop(main_loop, 0, 1);

    glfwMakeContextCurrent(nullptr);
    glfwDestroyWindow(_window);

    return 0;
}
