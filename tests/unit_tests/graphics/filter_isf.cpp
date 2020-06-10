#include "./graphics/filter_isf.h"

#include <doctest.h>

#include "./core/root_object.h"
#include "./utils/jsonutils.h"
#include "./utils/osutils.h"

using namespace Splash;

/*************/
class FilterISFMock : public FilterISF
{
  public:
    FilterISFMock(RootObject* root)
        : FilterISF(root)
    {
    }

    std::optional<std::string> testJSONHeader(const std::string& source) { return getJSONHeader(source); }
};

/*************/
TEST_CASE("Testing JSON header extraction")
{
    RootObject root;
    FilterISFMock filter(&root);

    auto sourceFile = Utils::readTextFile(Utils::getCurrentWorkingDirectory() + "/data/isf_sample.fs");
    CHECK(sourceFile);
    auto source = sourceFile.value();

    auto headerFile = Utils::readTextFile(Utils::getCurrentWorkingDirectory() + "/data/isf_sample_header.json");
    CHECK(headerFile);
    auto header = headerFile.value();

    auto headerFromSource = filter.testJSONHeader(source);
    CHECK(headerFromSource);

    auto groundTruthJson = Utils::stringToJson(header);
    auto sourceHeaderJson = Utils::stringToJson(headerFromSource.value());
    CHECK(groundTruthJson);
    CHECK(sourceHeaderJson);
    CHECK_EQ(groundTruthJson, sourceHeaderJson);
}
