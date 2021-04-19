#include "./controller/widget/widget_calibration.h"

#if HAVE_CALIMIRO
#include "./controller/texcoordgenerator.h"
#endif

namespace Splash
{

/*************/
GuiCalibration::GuiCalibration(Scene* scene, const std::string& name)
    : GuiWidget(scene, name)
{
#if HAVE_CALIMIRO
    std::string methodToString;
    for (auto const& method : calimiro::TexCoordUtils::getTexCoordMethodList())
    {
        methodToString = calimiro::TexCoordUtils::getTexCoordMethodTitle(method);
        _texCoordMethodsList.push_back(methodToString);
        _stringToMethod[methodToString] = method;
    }
#endif
}

/*************/
void GuiCalibration::render()
{
    ImVec2 availableSize = ImGui::GetContentRegionAvail();

    ImGui::BeginChild("##calibration_wrapper", ImVec2(availableSize.x, availableSize.y), true);

#if !HAVE_CALIMIRO
    ImGui::Text("Install Calimiro for calibration options");
#else
    GuiCalibration::renderGeometricCalibration(availableSize);
    ImGui::Separator();
    GuiCalibration::renderTexCoordCalibration(availableSize);
#endif

    ImGui::EndChild();
}

#if HAVE_CALIMIRO
/*************/
void GuiCalibration::renderGeometricCalibration(ImVec2& availableSize)
{
    assert(getObjectPtr(SPLASH_SCENE_OBJ_GEOMETRICCALIBRATOR) != nullptr);

    // Header
    ImGui::Text("Geometric Calibration");
    ImGui::Text("Positions captured: %i (recommended: 3+)", getObjectAttribute(SPLASH_SCENE_OBJ_GEOMETRICCALIBRATOR, "positionCount")[0].as<int>());

    // Settings
    auto computeTexCoord = getObjectAttribute(SPLASH_SCENE_OBJ_GEOMETRICCALIBRATOR, "computeTexCoord")[0].as<bool>();
    if (ImGui::Checkbox("Compute Texture Coordinates", &computeTexCoord))
        setObjectAttribute(SPLASH_SCENE_OBJ_GEOMETRICCALIBRATOR, "computeTexCoord", {computeTexCoord});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Compute texture coordinates during mesh generation");

    // Calibration actions
    if (ImGui::Button("Start calibration", ImVec2(availableSize.x * 0.5f - 12.f, 32.f)))
        setObjectAttribute(SPLASH_SCENE_OBJ_GEOMETRICCALIBRATOR, "calibrate", {});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Enter the calibration mode");

    ImGui::SameLine();
    if (ImGui::Button("Capture new position", ImVec2(availableSize.x * 0.5f - 12.f, 32.f)))
        setObjectAttribute(SPLASH_SCENE_OBJ_GEOMETRICCALIBRATOR, "nextPosition", {});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Capture the patterns for the current camera position");

    if (ImGui::Button("Finalize calibration", ImVec2(availableSize.x * 0.5f - 12.f, 32.f)))
        setObjectAttribute(SPLASH_SCENE_OBJ_GEOMETRICCALIBRATOR, "finalizeCalibration", {});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Compute calibration from the captured patterns");

    ImGui::SameLine();
    if (ImGui::Button("Abort calibration", ImVec2(availableSize.x * 0.5f - 12.f, 32.f)))
        setObjectAttribute(SPLASH_SCENE_OBJ_GEOMETRICCALIBRATOR, "abortCalibration", {});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Abort the calibration process");
}

/*************/
void GuiCalibration::renderTexCoordCalibration(ImVec2& availableSize)
{
    assert(getObjectPtr(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR) != nullptr);

    // Header
    ImGui::Text("Automatic texture coordinates generation");

    // Settings
    ImGui::Text("Projection type: ");
    ImGui::SameLine();
    auto tmpCurrentMethod = getObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "method")[0].as<std::string>();
    if (ImGui::BeginCombo("##TexCoord method", tmpCurrentMethod.c_str()))
    {
        for (const auto& method : _texCoordMethodsList)
        {
            bool isSelected = (tmpCurrentMethod == method);
            if (ImGui::Selectable(method.c_str(), isSelected))
            {
                tmpCurrentMethod = method;
                setObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "method", {tmpCurrentMethod});
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::TextWrapped("%s: %s",
        calimiro::TexCoordUtils::getTexCoordMethodTitle(_stringToMethod[tmpCurrentMethod]).c_str(),
        calimiro::TexCoordUtils::getTexCoordMethodDescription(_stringToMethod[tmpCurrentMethod]).c_str());

    auto tmpCameraPositionValue = getObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "eyePosition");
    std::array<float, 3> tmpCameraPosition{tmpCameraPositionValue[0].as<float>(), tmpCameraPositionValue[1].as<float>(), tmpCameraPositionValue[2].as<float>()};
    if (ImGui::InputFloat3("Eye position", tmpCameraPosition.data(), 5, ImGuiInputTextFlags_None))
        setObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "eyePosition", {tmpCameraPosition[0], tmpCameraPosition[1], tmpCameraPosition[2]});

    auto tmpCameraOrientationValue = getObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "eyeOrientation");
    std::array<float, 3> tmpCameraOrientation{tmpCameraOrientationValue[0].as<float>(), tmpCameraOrientationValue[1].as<float>(), tmpCameraOrientationValue[2].as<float>()};
    if (ImGui::InputFloat3("Eye orientation", tmpCameraOrientation.data(), 5, ImGuiInputTextFlags_None))
        setObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "eyeOrientation", {tmpCameraOrientation[0], tmpCameraOrientation[1], tmpCameraOrientation[2]});

    if (ImGui::Button("Normalize orientation", ImVec2(availableSize.x * 0.5f - 12.f, 24.f)))
        setObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "normalizeEyeOrientation", {});

    auto replaceMesh = getObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "replaceMesh")[0].as<bool>();
    if (ImGui::Checkbox("Replace mesh", &replaceMesh))
        setObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "replaceMesh", {replaceMesh});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Replace the selected mesh with the new one with computed texture coordinates");

    auto tmpHorizonRotation = getObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "horizonRotation")[0].as<float>();
    if (ImGui::InputFloat("Horizon (degrees)", &tmpHorizonRotation, 5, ImGuiInputTextFlags_None))
        setObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "horizonRotation", {tmpHorizonRotation});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Additional rotation to align texture coordinates with horizon");

    auto flipHorizontal = getObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "flipHorizontal")[0].as<bool>();
    if (ImGui::Checkbox("Flip horizontal", &flipHorizontal))
        setObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "flipHorizontal", {flipHorizontal});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Mirror the texture coordinates on the horizontal axis");

    ImGui::SameLine();
    auto flipVertical = getObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "flipVertical")[0].as<bool>();
    if (ImGui::Checkbox("Flip vertical", &flipVertical))
        setObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "flipVertical", {flipVertical});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Mirror the texture coordinates on the vertical axis");

    // Conditionnal options
    if (_stringToMethod[tmpCurrentMethod] == calimiro::TexCoordUtils::texCoordMethod::EQUIRECTANGULAR ||
        _stringToMethod[tmpCurrentMethod] == calimiro::TexCoordUtils::texCoordMethod::DOMEMASTER)
    {
        auto tmpFov = getObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "fov")[0].as<float>();
        if (ImGui::InputFloat("Field of view (degrees)", &tmpFov, 5, ImGuiInputTextFlags_None))
            setObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "fov", {tmpFov});
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Range of modelisation of a spheric object, in degrees");
    }

    // Actions
    const auto meshList = getSceneMeshes();
    for (const auto& mesh : meshList)
    {
        if (ImGui::Button(("Generate texture coordinates on mesh: " + mesh->getName()).c_str(), ImVec2(availableSize.x * 0.5f - 12.f, 32.f)))
        {
            setObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "meshName", {mesh->getName()});
            setObjectAttribute(SPLASH_SCENE_OBJ_TEXCOORDGENERATOR, "generate", {});
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Generate the texture coordinates for the mesh: %s ", mesh->getName().c_str());
    }
}
#endif

/*************/
const std::list<std::shared_ptr<Mesh>> GuiCalibration::getSceneMeshes()
{
    auto meshList = std::list<std::shared_ptr<Mesh>>();
    const auto meshTypes = getTypesFromCategory(GraphObject::Category::MESH);

    for (const auto& type : meshTypes)
    {
        const auto objects = getObjectsPtr(getObjectsOfType(type));
        for (const auto& object : objects)
            if (object->getSavable() && object->getType() != "mesh_shmdata")
                meshList.push_back(std::dynamic_pointer_cast<Mesh>(object));
    }

    return meshList;
}

/*************/
int GuiCalibration::updateWindowFlags()
{
    ImGuiWindowFlags flags = 0;
    return flags;
}

} // namespace Splash
