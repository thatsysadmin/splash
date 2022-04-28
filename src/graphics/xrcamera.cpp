#include "./graphics/xrcamera.h"

namespace Splash
{

/*************/
XRCamera::XRCamera(RootObject* root)
    : GraphObject(root)
{
    _type = "xrcamera";
    _renderingPriority = Priority::CAMERA;
    registerAttributes();

    // This is used for getting documentation "offline"
    if (!_root)
        return;
}

/*************/
XRCamera::~XRCamera() {}

/*************/
bool XRCamera::linkIt(const std::shared_ptr<GraphObject>& obj)
{
    if (auto obj3D = std::dynamic_pointer_cast<Object>(obj); obj)
    {
        _objects.push_back(obj3D);
        return true;
    }

    return false;
}

/*************/
void XRCamera::unlinkIt(const std::shared_ptr<GraphObject>& obj)
{
    auto objIterator = find_if(_objects.begin(), _objects.end(), [&](const std::weak_ptr<Object>& o) {
        if (o.expired())
            return false;
        auto object = o.lock();
        if (object == obj)
            return true;
        return false;
    });

    if (objIterator != _objects.end())
        _objects.erase(objIterator);
}

/*************/
std::shared_ptr<Texture_Image> XRCamera::getTexture() const {}

/*************/
int64_t XRCamera::getTimestamp() const {}

/*************/
void XRCamera::render() {}

/*************/
void XRCamera::registerAttributes() {}

} // namespace Splash
