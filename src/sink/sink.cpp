#include "./sink/sink.h"

#include <fstream>

#include "./core/constants.h"
#include "./core/scene.h"
#include "./graphics/texture.h"
#include "./utils/timer.h"

namespace Splash
{

/*************/
Sink::Sink(RootObject* root)
    : GraphObject(root)
{
    _type = "sink";
    _renderingPriority = Priority::POST_CAMERA;
    registerAttributes();

    if (!_root)
        return;
}

/*************/
Sink::~Sink()
{
    if (!_root)
        return;

    if (_mappedPixels)
    {
        glUnmapNamedBuffer(_pbos[_pboWriteIndex]);
        _mappedPixels = nullptr;
    }

    glDeleteBuffers(_pbos.size(), _pbos.data());
}

/*************/
std::string Sink::getCaps() const
{
    return "video/x-raw,format=(string)" + _spec.format + ",width=(int)" + std::to_string(_spec.width) + ",height=(int)" + std::to_string(_spec.height) + ",framerate=(fraction)" +
           std::to_string(_framerate) + "/1,pixel-aspect-ratio=(fraction)1/1";
}

/*************/
bool Sink::linkIt(const std::shared_ptr<GraphObject>& obj)
{
    if (auto objAsFilter = std::dynamic_pointer_cast<Filter>(obj); objAsFilter)
    {
        _inputFilter = std::dynamic_pointer_cast<Filter>(obj);
        return true;
    }
    else if (auto objAsTexture = std::dynamic_pointer_cast<Texture>(obj); objAsTexture)
    {
        auto filter = std::dynamic_pointer_cast<Filter>(_root->createObject("filter", getName() + "_" + obj->getName() + "_filter").lock());
        filter->setSavable(_savable); // We always save the filters as they hold user-specified values, if this is savable

        if (filter->linkTo(obj))
            return linkTo(filter);
        else
            return false;
    }

    return false;
}

/*************/
void Sink::unlinkIt(const std::shared_ptr<GraphObject>& obj)
{
    if (auto objAsFilter = std::dynamic_pointer_cast<Filter>(obj); objAsFilter)
    {
        objAsFilter->setSixteenBpc(true);
        _inputFilter.reset();
    }
    else if (auto objAsTexture = std::dynamic_pointer_cast<Texture>(obj); objAsTexture)
    {
        auto filterName = getName() + "_" + obj->getName() + "_filter";

        if (auto filter = _root->getObject(filterName))
        {
            filter->unlinkFrom(obj);
            unlinkFrom(filter);
        }

        _root->disposeObject(filterName);
    }
}

/*************/
void Sink::render()
{
    if (!_inputFilter || !_mappedPixels)
        return;

    handlePixels(reinterpret_cast<char*>(_mappedPixels), _spec);
}

/*************/
void Sink::update()
{
    if (!_inputFilter)
        return;

    auto textureSpec = _inputFilter->getSpec();
    if (textureSpec.rawSize() == 0)
        return;

    if (!_pbos.empty() && _mappedPixels)
    {
        glUnmapNamedBuffer(_pbos[_pboWriteIndex]);
        _mappedPixels = nullptr;
    }

    if (_spec.type != ImageBufferSpec::Type::UINT16 && _sixteenBpc)
        _inputFilter->setSixteenBpc(true);
    else if (_spec.type == ImageBufferSpec::Type::UINT16 && !_sixteenBpc)
        _inputFilter->setSixteenBpc(false);

    if (std::tuple<int, int>(_captureSize[0], _captureSize[0]) != _inputFilter->getSizeOverride())
    {
        _inputFilter->setAttribute("sizeOverride", {_captureSize[0], _captureSize[1]});
        if (_keepRatio)
        {
            int width = std::get<0>(_inputFilter->getSizeOverride());
            int height = std::get<1>(_inputFilter->getSizeOverride());
            if (width > 0 && height > 0)
                setAttribute("captureSize", {width, height});
        }
    }

    if (_keepRatio != _inputFilter->getKeepRatio())
    {
        _inputFilter->setAttribute("keepRatio", {_keepRatio});
        int width = std::get<0>(_inputFilter->getSizeOverride());
        int height = std::get<1>(_inputFilter->getSizeOverride());
        if (width > 0 && height > 0)
            setAttribute("captureSize", {width, height});
    }

    if (!_opened)
        return;

    uint64_t currentTime = Timer::get().getTime();
    uint64_t period = static_cast<uint64_t>(1e6 / (double)_framerate);
    if (period != 0 && _lastFrameTiming != 0 && currentTime - _lastFrameTiming < period)
        return;
    _lastFrameTiming = currentTime;

    if (_spec != textureSpec || _pbos.size() != _pboCount)
    {
        updatePbos(textureSpec.width, textureSpec.height, textureSpec.pixelBytes());
        _spec = textureSpec;
        _image = ImageBuffer(_spec);
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, _pbos[_pboWriteIndex]);

    auto scene = dynamic_cast<Scene*>(_root);
    // Nvidia hardware and/or drivers do not like much copying to a PBO
    // from a named texture, it prefers the old way for some unknown reason.
    // Hence the special treatment, even though it's not modern-ish.
    // For reference, the behavior with Nvidia hardware is that the grabbed
    // image is always completely black. It works correctly with AMD and Intel
    // hardware using Mesa driver.
    if (scene != nullptr && scene->getGLVendor() == Constants::GL_VENDOR_NVIDIA)
    {
        _inputFilter->bind();
        if (_spec.bpp == 64)
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_SHORT, 0);
        else if (_spec.bpp == 32)
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, 0);
        else if (_spec.bpp == 24)
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        else if (_spec.bpp == 16 && _spec.channels != 1)
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_UNSIGNED_SHORT, 0);
        else if (_spec.bpp == 16 && _spec.channels == 1)
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_SHORT, 0);
        else if (_spec.bpp == 8)
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
        _inputFilter->unbind();
    }
    else
    {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, _pbos[_pboWriteIndex]);
        if (_spec.bpp == 64)
            glGetTextureImage(_inputFilter->getTexId(), 0, GL_RGBA, GL_UNSIGNED_SHORT, 0, 0);
        else if (_spec.bpp == 32)
            glGetTextureImage(_inputFilter->getTexId(), 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, 0, 0);
        else if (_spec.bpp == 24)
            glGetTextureImage(_inputFilter->getTexId(), 0, GL_RGB, GL_UNSIGNED_BYTE, 0, 0);
        else if (_spec.bpp == 16 && _spec.channels != 1)
            glGetTextureImage(_inputFilter->getTexId(), 0, GL_RG, GL_UNSIGNED_SHORT, 0, 0);
        else if (_spec.bpp == 16 && _spec.channels == 1)
            glGetTextureImage(_inputFilter->getTexId(), 0, GL_RED, GL_UNSIGNED_SHORT, 0, 0);
        else if (_spec.bpp == 8)
            glGetTextureImage(_inputFilter->getTexId(), 0, GL_RED, GL_UNSIGNED_BYTE, 0, 0);
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    _pboWriteIndex = (_pboWriteIndex + 1) % _pbos.size();

    _mappedPixels = (GLubyte*)glMapNamedBufferRange(_pbos[_pboWriteIndex], 0, _spec.rawSize(), GL_MAP_READ_BIT);
}

/*************/
void Sink::handlePixels(const char* pixels, const ImageBufferSpec& spec)
{
    uint32_t size = spec.rawSize();
    if (size != _buffer.size())
        _buffer.resize(size);

    memcpy(_buffer.data(), pixels, size);
}

/*************/
void Sink::updatePbos(int width, int height, int bytes)
{
    if (!_pbos.empty())
        glDeleteBuffers(_pbos.size(), _pbos.data());

    _pbos.resize(_pboCount);
    glCreateBuffers(_pbos.size(), _pbos.data());

    for (uint32_t i = 0; i < _pbos.size(); ++i)
        glNamedBufferData(_pbos[i], width * height * bytes, 0, GL_STREAM_READ);

    _pboWriteIndex = 0;
}

/*************/
void Sink::registerAttributes()
{
    GraphObject::registerAttributes();

    addAttribute(
        "bufferCount",
        [&](const Values& args) {
            _pboCount = std::max(args[0].as<int>(), 2);
            return true;
        },
        [&]() -> Values { return {_pboCount}; },
        {'i'});
    setAttributeDescription("bufferCount", "Number of GPU buffers to use for data download to CPU memory");

    addAttribute(
        "framerate",
        [&](const Values& args) {
            _framerate = std::max(1, args[0].as<int>());
            return true;
        },
        [&]() -> Values { return {_framerate}; },
        {'i'});
    setAttributeDescription("framerate", "Maximum framerate, additional frames are dropped");

    addAttribute(
        "opened",
        [&](const Values& args) {
            _opened = args[0].as<bool>();
            return true;
        },
        [&]() -> Values { return {_opened}; },
        {'b'});
    setAttributeDescription("opened", "If true, the sink lets frames through");

    addAttribute(
        "16bits",
        [&](const Values& args) {
            _sixteenBpc = args[0].as<bool>();
            return true;
        },
        [&]() -> Values { return {_sixteenBpc}; },
        {'b'});
    setAttributeDescription("16bits", "Set to true for the sink to render in 16 bits per components (otherwise 8bpc)");

    addAttribute(
        "captureSize",
        [&](const Values& args) {
            _captureSize[0] = args[0].as<int>();
            _captureSize[1] = args[1].as<int>();
            return true;
        },
        [&]() -> Values {
            return {_captureSize[0], _captureSize[1]};
        },
        {'i', 'i'});
    setAttributeDescription("captureSize", "Sets the sink output size");

    addAttribute(
        "keepRatio",
        [&](const Values& args) {
            _keepRatio = args[0].as<bool>();
            return true;
        },
        [&]() -> Values { return {_keepRatio}; },
        {'b'});
    setAttributeDescription("keepRatio", "If true, keeps the ratio of the input image");
}

} // namespace Splash
