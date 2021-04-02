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
    assert(getObjectPtr("geometricCalibrator") != nullptr);

    // Header
    ImGui::Text("Geometric Calibration");
    ImGui::Text("Positions captured: %i (recommended: 3+)", getObjectAttribute("geometricCalibrator", "positionCount")[0].as<int>());

    // Settings
    auto computeTexCoord = getObjectAttribute("geometricCalibrator", "computeTexCoord")[0].as<bool>();
    if (ImGui::Checkbox("Compute Texture Coordinates", &computeTexCoord))
        setObjectAttribute("geometricCalibrator", "computeTexCoord", {computeTexCoord});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Compute texture coordinates during mesh generation");

    // Calibration actions
    if (ImGui::Button("Start calibration", ImVec2(availableSize.x * 0.5f - 12.f, 32.f)))
        setObjectAttribute("geometricCalibrator", "calibrate", {1});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Enter the calibration mode");

    ImGui::SameLine();
    if (ImGui::Button("Capture new position", ImVec2(availableSize.x * 0.5f - 12.f, 32.f)))
        setObjectAttribute("geometricCalibrator", "nextPosition", {1});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Capture the patterns for the current camera position");

    if (ImGui::Button("Finalize calibration", ImVec2(availableSize.x * 0.5f - 12.f, 32.f)))
        setObjectAttribute("geometricCalibrator", "finalizeCalibration", {1});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Compute calibration from the captured patterns");

    ImGui::SameLine();
    if (ImGui::Button("Abort calibration", ImVec2(availableSize.x * 0.5f - 12.f, 32.f)))
        setObjectAttribute("geometricCalibrator", "abortCalibration", {1});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Abort the calibration process");
}

/*************/
void GuiCalibration::renderTexCoordCalibration(ImVec2& availableSize)
{
    assert(getObjectPtr("texCoordGenerator") != nullptr);

    // Header
    ImGui::Text("Automatic texture coordinates generation");

    // Settings
    ImGui::Text("Projection type: ");
    ImGui::SameLine();
    auto tmpCurrentMethod = getObjectAttribute("texCoordGenerator", "method")[0].as<std::string>();
    if (ImGui::BeginCombo("##TexCoord method", tmpCurrentMethod.c_str()))
    {
        for (const auto& method : _texCoordMethodsList)
        {
            bool isSelected = (tmpCurrentMethod == method);
            if (ImGui::Selectable(method.c_str(), isSelected))
            {
                tmpCurrentMethod = method;
                setObjectAttribute("texCoordGenerator", "method", {tmpCurrentMethod});
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::TextWrapped("%s: %s",
        calimiro::TexCoordUtils::getTexCoordMethodTitle(_stringToMethod[tmpCurrentMethod]).c_str(),
        calimiro::TexCoordUtils::getTexCoordMethodDescription(_stringToMethod[tmpCurrentMethod]).c_str());

    auto tmpCameraPositionValue = getObjectAttribute("texCoordGenerator", "eyePosition");
    std::array<float, 3> tmpCameraPosition{tmpCameraPositionValue[0].as<float>(), tmpCameraPositionValue[1].as<float>(), tmpCameraPositionValue[2].as<float>()};
    const std::string position_name = "Eye position";
    if (ImGui::InputFloat3(position_name.c_str(), tmpCameraPosition.data(), 5, ImGuiInputTextFlags_None))
        setObjectAttribute("texCoordGenerator", "eyePosition", {tmpCameraPosition[0], tmpCameraPosition[1], tmpCameraPosition[2]});

    auto tmpCameraOrientationValue = getObjectAttribute("texCoordGenerator", "eyeOrientation");
    std::array<float, 3> tmpCameraOrientation{tmpCameraOrientationValue[0].as<float>(), tmpCameraOrientationValue[1].as<float>(), tmpCameraOrientationValue[2].as<float>()};
    const std::string orientation_name = "Eye orientation";
    if (ImGui::InputFloat3(orientation_name.c_str(), tmpCameraOrientation.data(), 5, ImGuiInputTextFlags_None))
        setObjectAttribute("texCoordGenerator", "eyeOrientation", {tmpCameraOrientation[0], tmpCameraOrientation[1], tmpCameraOrientation[2]});

    auto replaceMesh = getObjectAttribute("texCoordGenerator", "replaceMesh")[0].as<bool>();
    if (ImGui::Checkbox("Replace mesh", &replaceMesh))
        setObjectAttribute("texCoordGenerator", "replaceMesh", {replaceMesh});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Replace the selected mesh with the new one with computed texture coordinates");

    auto tmpHorizonRotation = getObjectAttribute("texCoordGenerator", "horizonRotation")[0].as<float>();
    const std::string horizonRotationName = "Horizon (degrees)";
    if (ImGui::InputFloat(horizonRotationName.c_str(), &tmpHorizonRotation, 5, ImGuiInputTextFlags_None))
        setObjectAttribute("texCoordGenerator", "horizonRotation", {tmpHorizonRotation});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Additional rotation to align texture coordinates with horizon");

    auto flipHorizontal = getObjectAttribute("texCoordGenerator", "flipHorizontal")[0].as<bool>();
    if (ImGui::Checkbox("Flip horizontal", &flipHorizontal))
        setObjectAttribute("texCoordGenerator", "flipHorizontal", {flipHorizontal});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Mirror the texture coordinates on the horizontal axis");

    ImGui::SameLine();
    auto flipVertical = getObjectAttribute("texCoordGenerator", "flipVertical")[0].as<bool>();
    if (ImGui::Checkbox("Flip vertical", &flipVertical))
        setObjectAttribute("texCoordGenerator", "flipVertical", {flipVertical});
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Mirror the texture coordinates on the vertical axis");

    // Spheric options
    if (_stringToMethod[tmpCurrentMethod] == calimiro::TexCoordUtils::texCoordMethod::SPHERIC)
    {
        auto tmpSphericFov = getObjectAttribute("texCoordGenerator", "sphericFov")[0].as<float>();
        const std::string spheric_fov_name = "Field of view (degrees)";
        if (ImGui::InputFloat(spheric_fov_name.c_str(), &tmpSphericFov, 5, ImGuiInputTextFlags_None))
            setObjectAttribute("texCoordGenerator", "sphericFov", {tmpSphericFov});
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Range of modelisation of a sphere for Spheric method, in degrees");
    }

    // Actions
    const auto meshList = getSceneMeshes();
    for (const auto& mesh : meshList)
    {
        std::string meshBtnLabel = "Generate texture coordinates on mesh: " + mesh->getName();
        if (ImGui::Button(meshBtnLabel.c_str(), ImVec2(availableSize.x * 0.5f - 12.f, 32.f)))
        {
            setObjectAttribute("texCoordGenerator", "meshName", {mesh->getName()});
            setObjectAttribute("texCoordGenerator", "generate", {1});
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
