#include "./graphics/filter_isf.h"

#include <array>
#include <iostream>
#include <memory>
#include <regex>

#include "./graphics/texture_image.h"
#include "./utils/jsonutils.h"
#include "./utils/osutils.h"

using namespace std;

namespace Splash
{

namespace ISF
{

const char* DESCRIPTION = "DESCRIPTION";
const char* CREDIT = "CREDIT";
const char* ISFVSN = "ISFVSN";
const char* VSN = "VSN";
const char* CATEGORIES = "CATEGORIES";
const char* INPUTS = "INPUTS";
const char* PASSES = "PASSES";
const char* NAME = "NAME";
const char* TYPE = "TYPE";
const char* LABEL = "LABEL";
const char* DEFAULT = "DEFAULT";
const char* MIN = "MIN";
const char* MAX = "MAX";
const char* TARGET = "TARGET";
const char* PERSISTENT = "PERSISTENT";
const char* WIDTH = "WIDTH";
const char* HEIGHT = "HEIGHT";

enum class InputType : uint8_t
{
    Bool,
    Color,
    Float,
    Image,
    Integer,
    Long,
    Point2D,
    Undefined
};

struct Input
{
    std::string name{""};
    InputType type{InputType::undefined};
    std::string label{""};
    Values def{};
    Values min{};
    Values max{};
};

struct Pass
{
    std::string target;
    uint32_t width;
    uint32_t height;
    bool persistent;
};

InputType getTypeFromField(const Json::Value& typeField)
{
    auto fieldAsStr = typeField.asString();
    if (fieldAsStr == "bool")
        return InputType::Bool;
    else if (fieldAsStr == "color")
        return InputType::Color;
    else if (fieldAsStr == "float")
        return InputType::Float;
    else if (fieldAsStr == "image")
        return InputType::Image;
    else if (fieldAsStr == "integer")
        return InputType::Integer;
    else if (fieldAsStr == "long")
        return InputType::Long;
    else if (fieldAsStr == "point2d")
        return InputType::Point2D;
    else
        return InputType::Undefined;
}

/*************/
std::string isfShaderToGLSL(const std::string& isfSource)
{
    std::string glsl = isfSource;

    //
    // Replace ISF functions with GLSL code
    //
    struct Conversion
    {
        std::string regex;
        std::string format;
    };

    std::array<Conversion> conversions{{R"((.*)IMG_PIXEL\((.*),(.*)\)(.*))", R"(\1 texture(\2, \3 * \2_size)\4)"},
        {R"((.*)IMG_NORM_PIXEL\((.*),(.*)\)(.*))", R"(\1 texture(\2, \3)\4)"},
        {R"((.*)IMG_THIS_PIXEL\((.*),(.*)\)(.*))", R"(\1 texture(\2, gl_FragCoord.xy * \2_size)\3)"},
        {R"((.*)IMG_THIS_NORM_PIXEL\((.*),(.*)\)(.*))", R"(\1 texture(\2, gl_FragCoord.xy)\3)"},
        {R"((.*)IMG_SIZE\((.*)\)(.*))", R"(\1 \2_size \3)"}};

    std::arra

        return isfSource;
}

} // namespace ISF

/*************/
FilterISF::FilterISF(RootObject* root)
    : Filter(root)
{
    _type = "filter_isf";
    registerAttributes();
}

/*************/
void FilterISF::render() {}

/*************/
optional<string> FilterISF::getJSONHeader(const string& source)
{
    auto headerStart = source.find("/*{");
    auto headerEnd = source.find("}*/");
    if (headerStart == string::npos || headerEnd == string::npos)
        return {};

    string header{source, headerStart + 2, headerEnd - headerStart - 1};
    return header;
}

/*************/
bool FilterISF::setFilterSourceFile(const string& file)
{
    //
    // Extract the ISF header
    //
    auto textFile = Utils::readTextFile(file);
    if (!textFile)
    {
        Log::get() << Log::ERROR << " - Unable to load file " << file << "\n";
        return false;
    }
    auto source = textFile.value();

    auto header = getJSONHeader(source);
    if (!header)
    {
        Log::get() << Log::ERROR << " - Could not find the description header in file " << file << "\n";
        return false;
    }

    auto description = Utils::stringToJson(header.value());
    if (!description)
    {
        Log::get() << Log::ERROR << " - Could not extract description from header for file " << file << "\n";
        return false;
    }

    auto jsonDesc = description.value();

    //
    // Extract data from the ISF header
    //
    std::string fileDescription = jsonDesc.get(ISF::DESCRIPTION, {""}).asString();
    std::string isfVersion = jsonDesc.get(ISF::ISFVSN, {""}).asString();
    std::string fileVersion = jsonDesc.get(ISF::VSN, {""}).asString();
    Values categories = Utils::jsonToValues(jsonDesc.get(ISF::CATEGORIES, {""}));

    std::vector<ISF::Input> inputs;
    if (jsonDesc.isMember(ISF::INPUTS))
    {
        for (const auto& inputField : jsonDesc[ISF::INPUTS])
        {
            ISF::Input input = {
                .name = inputField.get(ISF::NAME, {""}).asString(),
                .type = ISF::getTypeFromField(inputField.get(ISF::TYPE, {""})),
                .label = inputField.get(ISF::LABEL, {""}).asString(),
                .def = Utils::jsonToValues(inputField.get(ISF::DEFAULT, {""})),
                .min = Utils::jsonToValues(inputField.get(ISF::MIN, {""})),
                .max = Utils::jsonToValues(inputField.get(ISF::MAX, {""})),
            };

            if (input.name.empty())
            {
                Log::get() << Log::WARNING << "FilterISF::" << __FUNCTION__ << " - An input without a name has been found in file " << file << "\n";
                continue;
            }

            if (input.type == ISF::InputType::Undefined)
            {
                Log::get() << Log::WARNING << "FilterISF::" << __FUNCTION__ << " - An input without a type has been found in file " << file << "\n";
                continue;
            }

            inputs.push_back(input);
        }
    }

    std::vector<ISF::Pass> passes;
    if (jsonDesc.isMember(ISF::PASSES))
    {
        for (const auto& passField : jsonDesc[ISF::PASSES])
        {
            ISF::Pass pass = {
                .target = passField.get(ISF::TARGET, {""}).asString(),
                .width = passField.get(ISF::WIDTH, {"0"}).asUInt(),
                .height = passField.get(ISF::HEIGHT, {"0"}).asUInt(),
                .persistent = passField.get(ISF::PERSISTENT, {"false"}).asBool()
            };

            if (pass.target.empty())
            {
                Log::get() << Log::WARNING << "FilterISF::" << __FUNCTION__ << " - A pass without a target has been found in file " << file << "\n";
                continue;
            }

            passes.push_back(pass);
        }
    }

    //
    // Generate the filters for each pass
    //
    for (const auto& pass : passes)
    {
        auto filter = std::make_shared<FilterCustom>();
    }
    
    //
    // Connect the filters together, as well as the inputs and output
    //

    return true;
}

/*************/
void FilterISF::registerAttributes()
{
    Filter::registerAttributes();

    addAttribute(
        "filterSourceFile",
        [&](const Values& args) {
            auto srcFile = args[0].as<string>();
            if (srcFile.empty())
                return true; // No shader specified

            if (filesystem::exists(srcFile))
            {
                _isfSourceFile = srcFile;
                addTask([=]() { setFilterSourceFile(srcFile); });
                return true;
            }
            else
            {
                Log::get() << Log::WARNING << __FUNCTION__ << " - Unable to load file " << srcFile << Log::endl;
                return false;
            }
        },
        [&]() -> Values { return {_isfSourceFile}; },
        {'s'});
    setAttributeDescription("filterSourceFile", "Set the filter ISF source file");

    addAttribute(
        "watchShaderFile",
        [&](const Values& args) {
            _watchFile = args[0].as<bool>();

            if (_watchFile)
            {
                addPeriodicTask(
                    "watchShader",
                    [=]() {
                        if (_isfSourceFile.empty())
                            return;

                        std::filesystem::path sourcePath(_isfSourceFile);
                        try
                        {
                            auto lastWriteTime = std::filesystem::last_write_time(sourcePath);
                            if (lastWriteTime != _lastSourceFileWrite)
                            {
                                _lastSourceFileWrite = lastWriteTime;
                                addTask([=]() { setFilterSourceFile(_isfSourceFile); });
                            }
                        }
                        catch (...)
                        {
                        }
                    },
                    500);
            }
            else
            {
                removePeriodicTask("watchShader");
            }

            return true;
        },
        [&]() -> Values { return {_watchFile}; },
        {'n'});
    setAttributeDescription("watchShaderFile", "If true, automatically updates the shader from the source file");
}

} // namespace Splash
