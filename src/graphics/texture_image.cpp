#include "./graphics/texture_image.h"

#include <string>

#include "./image/image.h"
#include "./utils/log.h"
#include "./utils/timer.h"

namespace Splash
{

constexpr int Texture_Image::_texLevels;

/*************/
Texture_Image::Texture_Image(RootObject* root)
    : Texture(root)
{
    init();
}

/*************/
Texture_Image::Texture_Image(RootObject* root, GLsizei width, GLsizei height, const std::string& pixelFormat, const GLvoid* data, int multisample, bool cubemap)
    : Texture(root)
{
    init();
    reset(width, height, pixelFormat, data, multisample, cubemap);
}

/*************/
Texture_Image::~Texture_Image()
{
    if (!_root)
        return;

#ifdef DEBUG
    Log::get() << Log::DEBUGGING << "Texture_Image::~Texture_Image - Destructor" << Log::endl;
#endif

    std::lock_guard<std::mutex> lock(_mutex);
    glDeleteTextures(1, &_glTex);
    glDeleteBuffers(2, _pbos);
}

/*************/
Texture_Image& Texture_Image::operator=(const std::shared_ptr<Image>& img)
{
    _img = std::weak_ptr<Image>(img);
    return *this;
}

/*************/
void Texture_Image::bind()
{
    glGetIntegerv(GL_ACTIVE_TEXTURE, &_activeTexture);
    _activeTexture = _activeTexture - GL_TEXTURE0;
    glBindTextureUnit(_activeTexture, _glTex);
}

/*************/
void Texture_Image::generateMipmap() const
{
    glGenerateTextureMipmap(_glTex);
}

/*************/
RgbValue Texture_Image::getMeanValue() const
{
    int level = _texLevels - 1;
    int width, height;
    glGetTextureLevelParameteriv(_glTex, level, GL_TEXTURE_WIDTH, &width);
    glGetTextureLevelParameteriv(_glTex, level, GL_TEXTURE_HEIGHT, &height);
    auto size = width * height * 4;
    ResizableArray<uint8_t> buffer(size);
    glGetTextureImage(_glTex, level, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer.size(), buffer.data());

    RgbValue meanColor;
    for (int y = 0; y < height; ++y)
    {
        RgbValue rowMeanColor;
        for (int x = 0; x < width; ++x)
        {
            auto index = (x + y * width) * 4;
            RgbValue color(buffer[index], buffer[index + 1], buffer[index + 2]);
            rowMeanColor += color;
        }
        rowMeanColor /= static_cast<float>(width);
        meanColor += rowMeanColor;
    }
    meanColor /= height;

    return meanColor;
}

/*************/
std::unordered_map<std::string, Values> Texture_Image::getShaderUniforms() const
{
    auto uniforms = _shaderUniforms;
    uniforms["size"] = {static_cast<float>(_spec.width), static_cast<float>(_spec.height)};
    return uniforms;
}

/*************/
ImageBuffer Texture_Image::grabMipmap(unsigned int level) const
{
    int mipmapLevel = std::min<int>(level, _texLevels);
    GLint width, height;
    glGetTextureLevelParameteriv(_glTex, level, GL_TEXTURE_WIDTH, &width);
    glGetTextureLevelParameteriv(_glTex, level, GL_TEXTURE_HEIGHT, &height);

    auto spec = _spec;
    spec.width = width;
    spec.height = height;

    auto image = ImageBuffer(spec);
    glGetTextureImage(_glTex, mipmapLevel, _texFormat, _texType, image.getSize(), image.data());
    return image;
}

/*************/
bool Texture_Image::linkIt(const std::shared_ptr<GraphObject>& obj)
{
    if (std::dynamic_pointer_cast<Image>(obj))
    {
        auto img = std::dynamic_pointer_cast<Image>(obj);
        img->setDirty();
        _img = std::weak_ptr<Image>(img);
        return true;
    }

    return false;
}

/*************/
void Texture_Image::unlinkIt(const std::shared_ptr<GraphObject>& obj)
{
    if (std::dynamic_pointer_cast<Image>(obj))
    {
        auto img = std::dynamic_pointer_cast<Image>(obj);
        if (img == _img.lock())
            _img.reset();
    }
}

/*************/
std::shared_ptr<Image> Texture_Image::read()
{
    auto img = std::make_shared<Image>(_root, _spec);
    glGetTextureImage(_glTex, 0, _texFormat, _texType, img->getSpec().rawSize(), (GLvoid*)img->data());
    return img;
}

/*************/
void Texture_Image::reset(int width, int height, const std::string& pixelFormat, const GLvoid* data, int multisample, bool cubemap)
{
    if (width == 0 || height == 0)
    {
#ifdef DEBUG
        Log::get() << Log::DEBUGGING << "Texture_Image::" << __FUNCTION__ << " - Texture size is null" << Log::endl;
#endif
        return;
    }

    // Fill texture parameters
    _spec.width = width;
    _spec.height = height;
    _pixelFormat = pixelFormat.empty() ? "RGBA" : pixelFormat;
    _multisample = multisample;
    _cubemap = multisample == 0 ? cubemap : false;

    if (_pixelFormat == "RGBA")
    {
        _spec = ImageBufferSpec(width, height, 4, 32, ImageBufferSpec::Type::UINT8, "RGBA");
        _texInternalFormat = GL_RGBA8;
        _texFormat = GL_RGBA;
        _texType = GL_UNSIGNED_INT_8_8_8_8_REV;
    }
    else if (_pixelFormat == "sRGBA")
    {
        _spec = ImageBufferSpec(width, height, 4, 32, ImageBufferSpec::Type::UINT8, "RGBA");
        _texInternalFormat = GL_SRGB8_ALPHA8;
        _texFormat = GL_RGBA;
        _texType = GL_UNSIGNED_INT_8_8_8_8_REV;
    }
    else if (_pixelFormat == "RGBA16")
    {
        _spec = ImageBufferSpec(width, height, 4, 64, ImageBufferSpec::Type::UINT16, "RGBA");
        _texInternalFormat = GL_RGBA16;
        _texFormat = GL_RGBA;
        _texType = GL_UNSIGNED_SHORT;
    }
    else if (_pixelFormat == "RGB")
    {
        _spec = ImageBufferSpec(width, height, 3, 24, ImageBufferSpec::Type::UINT8, "RGB");
        _texInternalFormat = GL_RGBA8;
        _texFormat = GL_RGB;
        _texType = GL_UNSIGNED_BYTE;
    }
    else if (_pixelFormat == "R16")
    {
        _spec = ImageBufferSpec(width, height, 1, 16, ImageBufferSpec::Type::UINT16, "R");
        _texInternalFormat = GL_R16;
        _texFormat = GL_RED;
        _texType = GL_UNSIGNED_SHORT;
    }
    else if (_pixelFormat == "YUYV" || _pixelFormat == "UYVY")
    {
        _spec = ImageBufferSpec(width, height, 3, 16, ImageBufferSpec::Type::UINT8, _pixelFormat);
        _texInternalFormat = GL_RGBA8;
        _texFormat = GL_RGBA;
        _texType = GL_UNSIGNED_INT_8_8_8_8_REV;
    }
    else if (_pixelFormat == "D")
    {
        _spec = ImageBufferSpec(width, height, 1, 24, ImageBufferSpec::Type::FLOAT, "R");
        _texInternalFormat = GL_DEPTH_COMPONENT24;
        _texFormat = GL_DEPTH_COMPONENT;
        _texType = GL_FLOAT;
    }

    // Create and initialize the texture
    if (glIsTexture(_glTex))
        glDeleteTextures(1, &_glTex);

    if (_multisample > 1)
        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &_glTex);
    else if (_cubemap)
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &_glTex);
    else
        glCreateTextures(GL_TEXTURE_2D, 1, &_glTex);

    if (_texInternalFormat == GL_DEPTH_COMPONENT)
    {
        glTextureParameteri(_glTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_glTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_glTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(_glTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    {
        glTextureParameteri(_glTex, GL_TEXTURE_WRAP_S, _glTextureWrap);
        glTextureParameteri(_glTex, GL_TEXTURE_WRAP_T, _glTextureWrap);

        // Anisotropic filtering. Not in core, but available on any GPU capable of running Splash
        // See https://www.khronos.org/opengl/wiki/Sampler_Object#Anisotropic_filtering
        float maxAnisoFiltering;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisoFiltering);
        glTextureParameterf(_glTex, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisoFiltering);

        if (_filtering)
        {
            glTextureParameteri(_glTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTextureParameteri(_glTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else
        {
            glTextureParameteri(_glTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(_glTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
    }

    if (_multisample > 1)
    {
        glTextureStorage2DMultisample(_glTex, _multisample, _texInternalFormat, width, height, false);
    }
    else if (_cubemap == true)
    {
        glTextureStorage2D(_glTex, _texLevels, _texInternalFormat, width, height);
    }
    else
    {
        glTextureStorage2D(_glTex, _texLevels, _texInternalFormat, width, height);
        if (data)
            glTextureSubImage2D(_glTex, 0, 0, 0, width, height, _texFormat, _texType, data);
    }

#ifdef DEBUG
    Log::get() << Log::DEBUGGING << "Texture_Image::" << __FUNCTION__ << " - Reset the texture to size " << width << "x" << height << Log::endl;
#endif
}

/*************/
void Texture_Image::resize(int width, int height)
{
    if (!_resizable)
        return;
    if (static_cast<uint32_t>(width) != _spec.width || static_cast<uint32_t>(height) != _spec.height)
        reset(width, height, _pixelFormat, 0, _multisample, _cubemap);
}

/*************/
void Texture_Image::unbind()
{
#ifdef DEBUG
    glBindTextureUnit(_activeTexture, 0);
#endif
    _lastDrawnTimestamp = Timer::getTime();
}

/*************/
GLenum Texture_Image::getChannelOrder(const ImageBufferSpec& spec)
{
    GLenum glChannelOrder = GL_RGB;

    // We don't want to let the driver convert from BGR to RGB, as this can lead in
    // some cases to mediocre performances.
    if (spec.format == "RGB" || spec.format == "BGR" || spec.format == "RGB_DXT1")
        glChannelOrder = GL_RGB;
    else if (spec.format == "RGBA" || spec.format == "BGRA" || spec.format == "RGBA_DXT5")
        glChannelOrder = GL_RGBA;
    else if (spec.format == "YUYV" || spec.format == "UYVY")
        glChannelOrder = GL_RG;
    else if (spec.channels == 1)
        glChannelOrder = GL_RED;
    else if (spec.channels == 3)
        glChannelOrder = GL_RGB;
    else if (spec.channels == 4)
        glChannelOrder = GL_RGBA;

    return glChannelOrder;
}

/*************/
void Texture_Image::update()
{
    std::lock_guard<std::mutex> lock(_mutex);

    // If _img is nullptr, this texture is not set from an Image
    if (_img.expired())
        return;
    const auto img = _img.lock();

    if (img->getTimestamp() == _spec.timestamp)
        return;

    if (_multisample > 1)
    {
        Log::get() << Log::ERROR << "Texture_Image::" << __FUNCTION__ << " - Texture " << _name << " is multisampled, and can not be set from an image" << Log::endl;
        return;
    }

    img->update();
    const auto imgLock = img->getReadLock();

    auto spec = img->getSpec();
    Values srgb, flip, flop;
    img->getAttribute("srgb", srgb);
    img->getAttribute("flip", flip);
    img->getAttribute("flop", flop);

    // Store the image data size
    const int imageDataSize = spec.rawSize();
    GLenum glChannelOrder = getChannelOrder(spec);

    // If the texture is compressed, we need to modify a few values
    bool isCompressed = false;
    if (spec.format == "RGB_DXT1")
    {
        isCompressed = true;
        spec.height *= 2;
        spec.channels = 3;
    }
    else if (spec.format == "RGBA_DXT5")
    {
        isCompressed = true;
        spec.channels = 4;
    }
    else if (spec.format == "YCoCg_DXT5")
    {
        isCompressed = true;
    }

    // Get GL parameters
    GLenum internalFormat;
    GLenum dataFormat = GL_UNSIGNED_BYTE;
    if (!isCompressed)
    {
        if (spec.channels == 4 && spec.type == ImageBufferSpec::Type::UINT8)
        {
            dataFormat = GL_UNSIGNED_INT_8_8_8_8_REV;
            if (srgb[0].as<bool>())
                internalFormat = GL_SRGB8_ALPHA8;
            else
                internalFormat = GL_RGBA;
        }
        else if (spec.channels == 3 && spec.type == ImageBufferSpec::Type::UINT8)
        {
            dataFormat = GL_UNSIGNED_BYTE;
            if (srgb[0].as<bool>())
                internalFormat = GL_SRGB8_ALPHA8;
            else
                internalFormat = GL_RGBA;
        }
        else if (spec.channels == 1 && spec.type == ImageBufferSpec::Type::UINT16)
        {
            dataFormat = GL_UNSIGNED_SHORT;
            internalFormat = GL_R16;
        }
        else if (spec.channels == 2 && spec.type == ImageBufferSpec::Type::UINT8)
        {
            dataFormat = GL_UNSIGNED_SHORT;
            internalFormat = GL_RG;
        }
        else
        {
            Log::get() << Log::WARNING << "Texture_Image::" << __FUNCTION__ << " - Unknown uncompressed format" << Log::endl;
            return;
        }
    }
    else if (isCompressed)
    {
        if (spec.format == "RGB_DXT1")
        {
            if (srgb[0].as<bool>())
                internalFormat = GL_COMPRESSED_SRGB_S3TC_DXT1_EXT;
            else
                internalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        }
        else if (spec.format == "RGBA_DXT5")
        {
            if (srgb[0].as<bool>())
                internalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
            else
                internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        }
        else if (spec.format == "YCoCg_DXT5")
        {
            internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        }
        else
        {
            Log::get() << Log::WARNING << "Texture_Image::" << __FUNCTION__ << " - Unknown compressed format" << Log::endl;
            return;
        }
    }

    // Update the textures if the format changed
    if (spec != _spec || !spec.videoFrame)
    {
        // glTexStorage2D is immutable, so we have to delete the texture first
        glDeleteTextures(1, &_glTex);
        glCreateTextures(GL_TEXTURE_2D, 1, &_glTex);

        glTextureParameteri(_glTex, GL_TEXTURE_WRAP_S, _glTextureWrap);
        glTextureParameteri(_glTex, GL_TEXTURE_WRAP_T, _glTextureWrap);

        if (_filtering)
        {
            if (isCompressed)
                glTextureParameteri(_glTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            else
                glTextureParameteri(_glTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTextureParameteri(_glTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else
        {
            glTextureParameteri(_glTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(_glTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }

        // Create or update the texture parameters
        if (!isCompressed)
        {
#ifdef DEBUG
            Log::get() << Log::DEBUGGING << "Texture_Image::" << __FUNCTION__ << " - Creating a new texture" << Log::endl;
#endif
            glTextureStorage2D(_glTex, _texLevels, internalFormat, spec.width, spec.height);
            glTextureSubImage2D(_glTex, 0, 0, 0, spec.width, spec.height, glChannelOrder, dataFormat, img->data());
        }
        else if (isCompressed)
        {
#ifdef DEBUG
            Log::get() << Log::DEBUGGING << "Texture_Image::" << __FUNCTION__ << " - Creating a new compressed texture" << Log::endl;
#endif

            glTextureStorage2D(_glTex, _texLevels, internalFormat, spec.width, spec.height);
            glCompressedTextureSubImage2D(_glTex, 0, 0, 0, spec.width, spec.height, internalFormat, imageDataSize, img->data());
        }

        if (!updatePbos(spec.width, spec.height, spec.pixelBytes()))
            return;

        // Fill one of the PBOs right now
        auto pixels = _pbosPixels[0];
        if (pixels != nullptr)
        {
            memcpy((void*)pixels, img->data(), imageDataSize);
        }

        // And copy it to the second PBO
        glCopyNamedBufferSubData(_pbos[0], _pbos[1], 0, 0, imageDataSize);
        _spec = spec;
    }
    // Update the content of the texture, i.e the image
    else
    {
        // Copy the pixels from the current PBO to the texture
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _pbos[_pboUploadIndex]);
        if (!isCompressed)
            glTextureSubImage2D(_glTex, 0, 0, 0, spec.width, spec.height, glChannelOrder, dataFormat, 0);
        else
            glCompressedTextureSubImage2D(_glTex, 0, 0, 0, spec.width, spec.height, internalFormat, imageDataSize, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        _pboUploadIndex = (_pboUploadIndex + 1) % 2;

        // Fill the next PBO with the image pixels
        auto pixels = _pbosPixels[_pboUploadIndex];
        if (pixels != nullptr)
            memcpy(pixels, img->data(), imageDataSize);
    }

    _spec.timestamp = spec.timestamp;

    // If needed, specify some uniforms for the shader which will use this texture
    _shaderUniforms.clear();

    // Presentation parameters
    _shaderUniforms["flip"] = flip;
    _shaderUniforms["flop"] = flop;

    // Specify the color encoding
    if (spec.format.find("RGB") != std::string::npos)
        _shaderUniforms["encoding"] = {ColorEncoding::RGB};
    else if (spec.format.find("BGR") != std::string::npos)
        _shaderUniforms["encoding"] = {ColorEncoding::BGR};
    else if (spec.format == "UYVY")
        _shaderUniforms["encoding"] = {ColorEncoding::UYVY};
    else if (spec.format == "YUYV")
        _shaderUniforms["encoding"] = {ColorEncoding::YUYV};
    else if (spec.format == "YCoCg_DXT5")
        _shaderUniforms["encoding"] = {ColorEncoding::YCoCg};
    else
        _shaderUniforms["encoding"] = {ColorEncoding::RGB}; // Default case: RGB

    if (_filtering && !isCompressed)
        generateMipmap();
}

/*************/
void Texture_Image::init()
{
    _type = "texture_image";
    registerAttributes();

    // This is used for getting documentation "offline"
    if (!_root)
        return;
}

/*************/
bool Texture_Image::updatePbos(int width, int height, int bytes)
{
    glDeleteBuffers(2, _pbos);

    auto flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    auto imageDataSize = width * height * bytes;

    glCreateBuffers(2, _pbos);
    glNamedBufferStorage(_pbos[0], imageDataSize, 0, flags);
    glNamedBufferStorage(_pbos[1], imageDataSize, 0, flags);

    _pbosPixels[0] = (GLubyte*)glMapNamedBufferRange(_pbos[0], 0, imageDataSize, flags);
    _pbosPixels[1] = (GLubyte*)glMapNamedBufferRange(_pbos[1], 0, imageDataSize, flags);

    if (!_pbosPixels[0] || !_pbosPixels[1])
    {
        Log::get() << Log::ERROR << "Texture_Image::" << __FUNCTION__ << " - Unable to initialize upload PBOs" << Log::endl;
        return false;
    }

    return true;
}

/*************/
void Texture_Image::registerAttributes()
{
    Texture::registerAttributes();

    addAttribute("filtering",
        [&](const Values& args) {
            _filtering = args[0].as<bool>();
            return true;
        },
        [&]() -> Values { return {_filtering}; },
        {'b'});
    setAttributeDescription("filtering", "Activate the mipmaps for this texture");

    addAttribute("clampToEdge",
        [&](const Values& args) {
            _glTextureWrap = args[0].as<bool>() ? GL_CLAMP_TO_EDGE : GL_REPEAT;
            return true;
        },
        {'b'});
    setAttributeDescription("clampToEdge", "If true, clamp the texture to the edge");

    addAttribute("size",
        [&](const Values& args) {
            resize(args[0].as<int>(), args[1].as<int>());
            return true;
        },
        [&]() -> Values {
            return {_spec.width, _spec.height};
        },
        {'i', 'i'});
    setAttributeDescription("size", "Change the texture size");
}

} // namespace Splash
