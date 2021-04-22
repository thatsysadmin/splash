#include "./image/image_v4l2.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#if HAVE_DATAPATH
#include "rgb133control.h"
#include "rgb133v4l2.h"
#endif

#include "./utils/osutils.h"

#define NUMERATOR 1001
#define DIVISOR 10

using Splash::Utils::xioctl;

namespace Splash
{
/*************/
Image_V4L2::Image_V4L2(RootObject* root)
    : Image(root)
{
    init();
}

/*************/
Image_V4L2::~Image_V4L2()
{
    stopCapture();
#if HAVE_DATAPATH
    closeControlDevice();
#endif
}

/*************/
void Image_V4L2::init()
{
    _type = SPLASH_GRAPH_TYPE_IMAGE_V4L2;
    registerAttributes();

    // This is used for getting documentation "offline"
    if (!_root)
        return;

#if HAVE_DATAPATH
    openControlDevice();
#endif
}

/*************/
bool Image_V4L2::doCapture()
{
    std::lock_guard<std::mutex> lockStart(_startStopMutex);

    if (_capturing)
        return true;

    if (_devicePath.empty())
    {
        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - A device path has to be specified" << Log::endl;
        return false;
    }

    _capturing = openCaptureDevice(_devicePath);
    if (_capturing)
        _capturing = initializeIOMethod();
    if (_capturing)
        _capturing = initializeCapture();
    if (_capturing)
    {
        _captureThreadRun = true;
        _captureFuture = async(std::launch::async, [this]() {
            captureThreadFunc();
            _capturing = false;
        });
    }

    return _capturing;
}

/*************/
void Image_V4L2::stopCapture()
{
    std::lock_guard<std::mutex> lockStop(_startStopMutex);

    if (!_capturing)
        return;

    _captureThreadRun = false;
    if (_captureFuture.valid())
        _captureFuture.wait();

    closeCaptureDevice();
}

/*************/
void Image_V4L2::captureThreadFunc()
{
    int result = 0;
    struct v4l2_buffer buffer;
    enum v4l2_buf_type bufferType;

    if (!_hasStreamingIO)
    {
        while (_captureThreadRun)
        {
            if (!_bufferImage || _bufferImage->getSpec() != _imageBuffers[buffer.index]->getSpec())
                _bufferImage = std::make_unique<ImageBuffer>(_spec);

            {
                std::unique_lock<std::shared_mutex> lockWrite(_writeMutex);
                result = ::read(_deviceFd, _bufferImage->data(), _spec.rawSize());
                _imageUpdated = true;
            }

            if (result < 0)
            {
                Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Failed to read from capture device " << _devicePath << Log::endl;
                return;
            }

            updateTimestamp();
            if (!_isConnectedToRemote)
                update();
        }
    }
    else
    {
        assert(_ioMethod == V4L2_MEMORY_MMAP || _ioMethod == V4L2_MEMORY_USERPTR);

        bufferType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        result = xioctl(_deviceFd, VIDIOC_STREAMON, &bufferType);
        if (result < 0)
        {
            Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - VIDIOC_STREAMON failed: " << result << Log::endl;
            return;
        }

        // Run the capture
        while (_captureThreadRun)
        {
            struct pollfd fd;
            fd.fd = _deviceFd;
            fd.events = POLLIN | POLLPRI;
            fd.revents = 0;

            if (poll(&fd, 1, 50) > 0)
            {
                if (fd.revents & (POLLIN | POLLPRI))
                {
                    memset(&buffer, 0, sizeof(buffer));
                    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                    buffer.memory = _ioMethod;

                    result = xioctl(_deviceFd, VIDIOC_DQBUF, &buffer);
                    if (result < 0)
                    {
                        switch (errno)
                        {
                        case EAGAIN:
                            continue;
                        case EIO:
                        default:
                            Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Failed to dequeue buffer " << buffer.index << Log::endl;
                            return;
                        }
                    }

                    assert(buffer.index < _bufferCount);

                    if (!_bufferImage || _bufferImage->getSpec() != _imageBuffers[buffer.index]->getSpec())
                        _bufferImage = std::make_unique<ImageBuffer>(_spec);

                    if (_ioMethod == V4L2_MEMORY_MMAP)
                    {
                        auto& imageBuffer = _imageBuffers[buffer.index];
                        std::unique_lock<std::shared_mutex> lockWrite(_writeMutex);
                        _bufferImage = std::make_unique<ImageBuffer>(imageBuffer->getSpec(), imageBuffer->data());
                        _imageUpdated = true;
                    }
                    else if (_ioMethod == V4L2_MEMORY_USERPTR)
                    {
                        std::unique_lock<std::shared_mutex> lockWrite(_writeMutex);
                        _bufferImage.swap(_imageBuffers[buffer.index]);
                        _imageUpdated = true;
                    }

                    buffer.m.userptr = reinterpret_cast<unsigned long>(_imageBuffers[buffer.index]->data());
                    buffer.length = _spec.rawSize();

                    result = xioctl(_deviceFd, VIDIOC_QBUF, &buffer);
                    if (result < 0)
                    {
                        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Failed to requeue buffer " << buffer.index << Log::endl;
                        return;
                    }

                    updateTimestamp();
                    if (!_isConnectedToRemote)
                        update();
                }
            }

#if HAVE_DATAPATH
            // Get the source video format, for information
            if (_isDatapath)
            {
                memset(&_v4l2SourceFormat, 0, sizeof(_v4l2SourceFormat));
                _v4l2SourceFormat.type = V4L2_BUF_TYPE_CAPTURE_SOURCE;
                if (xioctl(_deviceFd, RGB133_VIDIOC_G_SRC_FMT, &_v4l2SourceFormat) >= 0)
                {
                    auto sourceFormatAsString = std::to_string(_v4l2SourceFormat.fmt.pix.width) + "x" + std::to_string(_v4l2SourceFormat.fmt.pix.height) + std::string("@") +
                                                std::to_string((float)_v4l2SourceFormat.fmt.pix.priv / 1000.f) + "Hz, format " +
                                                std::string(reinterpret_cast<char*>(&_v4l2SourceFormat.fmt.pix.pixelformat), 4);

                    if (!_automaticResizing)
                    {
                        if (_outputWidth != _v4l2SourceFormat.fmt.pix.width || _outputHeight != _v4l2SourceFormat.fmt.pix.height)
                        {
                            _automaticResizing = true;
                            addTask([=]() {
                                stopCapture();
                                _outputWidth = std::max(320u, _v4l2SourceFormat.fmt.pix.width);
                                _outputHeight = std::max(240u, _v4l2SourceFormat.fmt.pix.height);
                                doCapture();
                                _automaticResizing = false;
                            });
                        }
                    }

                    _sourceFormatAsString = sourceFormatAsString;
                }
            }
#endif
        }

        bufferType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        result = xioctl(_deviceFd, VIDIOC_STREAMOFF, &bufferType);
        if (result < 0)
            Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - VIDIOC_STREAMOFF failed: " << result << Log::endl;

        for (uint32_t i = 0; i < _bufferCount; ++i)
        {
            memset(&buffer, 0, sizeof(buffer));
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_USERPTR;
            result = xioctl(_deviceFd, VIDIOC_DQBUF, &buffer);
            if (result < 0)
                Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - VIDIOC_DQBUF failed: " << result << Log::endl;
        }
    }

    // Reset to a default image
    std::unique_lock<std::shared_mutex> lockWrite(_writeMutex);
    _bufferImage = std::make_unique<ImageBuffer>(ImageBufferSpec(512, 512, 4, 32));
    _bufferImage->zero();
    _imageUpdated = true;
    updateTimestamp();
}

/*************/
bool Image_V4L2::initializeIOMethod()
{
    if (initializeUserPtr())
    {
        Log::get() << Log::MESSAGE << "Image_V4L2::" << __FUNCTION__ << " - Initialized device " << _devicePath << " with user pointer io method" << Log::endl;
        _ioMethod = V4L2_MEMORY_USERPTR;
        return true;
    }
    else if (initializeMemoryMap())
    {
        Log::get() << Log::MESSAGE << "Image_V4L2::" << __FUNCTION__ << " - Initialized device " << _devicePath << " with memory map io method" << Log::endl;
        _ioMethod = V4L2_MEMORY_MMAP;
        return true;
    }

    Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Could not initialize device " << _devicePath << Log::endl;
    return false;
}

/*************/
bool Image_V4L2::initializeMemoryMap()
{
    memset(&_v4l2RequestBuffers, 0, sizeof(_v4l2RequestBuffers));
    _v4l2RequestBuffers.count = _bufferCount;
    _v4l2RequestBuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    _v4l2RequestBuffers.memory = V4L2_MEMORY_MMAP;

    if (xioctl(_deviceFd, VIDIOC_REQBUFS, &_v4l2RequestBuffers))
    {
        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Device does not support memory map" << Log::endl;
        return false;
    }

    return true;
}

/*************/
bool Image_V4L2::initializeUserPtr()
{
    memset(&_v4l2RequestBuffers, 0, sizeof(_v4l2RequestBuffers));
    _v4l2RequestBuffers.count = _bufferCount;
    _v4l2RequestBuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    _v4l2RequestBuffers.memory = V4L2_MEMORY_USERPTR;

    int result = xioctl(_deviceFd, VIDIOC_REQBUFS, &_v4l2RequestBuffers);
    if (result < 0)
    {
        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Device does not support userptr IO" << Log::endl;
        return false;
    }

    return true;
}

/*************/
bool Image_V4L2::initializeCapture()
{
    if (!_hasStreamingIO)
        return true;

#if HAVE_DATAPATH
    if (_isDatapath)
    {
        struct v4l2_queryctrl queryCtrl;
        struct v4l2_control control;

        memset(&queryCtrl, 0, sizeof(queryCtrl));
        queryCtrl.id = RGB133_V4L2_CID_LIVESTREAM;

        int result = xioctl(_deviceFd, VIDIOC_QUERYCTRL, &queryCtrl);
        if (result == -1)
        {
            if (errno != EINVAL)
            {
                Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - VIDIOC_QUERYCTRL failed for RGB133_V4L2_CID_LIVESTREAM" << Log::endl;
                return false;
            }
            else
            {
                Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - RGB133_V4L2_CID_LIVESTREAM is not supported" << Log::endl;
            }
        }
        else if (queryCtrl.flags & V4L2_CTRL_FLAG_DISABLED)
        {
            Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - RGB133_V4L2_CID_LIVESTREAM is not supported" << Log::endl;
        }
        else
        {
            memset(&control, 0, sizeof(control));
            control.id = RGB133_V4L2_CID_LIVESTREAM;
            control.value = 1; // enable livestream
            result = xioctl(_deviceFd, VIDIOC_S_CTRL, &control);
            if (result == -1)
            {
                Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - VIDIOC_S_CTRL failed for RGB133_V4L2_CID_LIVESTREAM" << Log::endl;
                return false;
            }
        }

        memset(&control, 0, sizeof(control));
        control.id = RGB133_V4L2_CID_LIVESTREAM;
        result = xioctl(_deviceFd, VIDIOC_G_CTRL, &control);
        if (result == -1)
        {
            Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - VIDIOC_G_CTRL failed for RGB133_V4L2_CID_LIVESTREAM" << Log::endl;
            return false;
        }

        Log::get() << Log::MESSAGE << "Image_V4L2::" << __FUNCTION__ << " - RGB133_V4L2_CID_LIVESTREAM current value: " << control.value << Log::endl;
    }
#endif

    // Initialize the buffers
    _imageBuffers.clear();

    switch (_ioMethod)
    {
    default:
        assert(false);
        return false;
    case V4L2_MEMORY_MMAP:
    {
        int result;
        struct v4l2_buffer buffer;

        for (uint32_t i = 0; i < _bufferCount; ++i)
        {
            memset(&buffer, 0, sizeof(buffer));
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = i;

            result = xioctl(_deviceFd, VIDIOC_QUERYBUF, &buffer);
            if (result < 0)
            {
                Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - VIDIOC_QUERYBUF failed: " << result << Log::endl;
                return false;
            }

            auto mappedMemory = mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, _deviceFd, buffer.m.offset);
            if (mappedMemory == MAP_FAILED)
            {
                Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Error while calling mmap: " << result << Log::endl;
                return false;
            }

            _imageBuffers.push_back(std::make_unique<ImageBuffer>(_spec, static_cast<uint8_t*>(mappedMemory), true));

            result = xioctl(_deviceFd, VIDIOC_QBUF, &buffer);
            if (result < 0)
            {
                Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - VIDIOC_QBUF failed: " << result << Log::endl;
                return false;
            }
        }
        break;
    }
    case V4L2_MEMORY_USERPTR:
    {
        int result;
        struct v4l2_buffer buffer;

        for (uint32_t i = 0; i < _bufferCount; ++i)
        {
            _imageBuffers.push_back(std::make_unique<ImageBuffer>(_spec));

            memset(&buffer, 0, sizeof(buffer));
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_USERPTR;
            buffer.index = i;
            buffer.m.userptr = reinterpret_cast<unsigned long>(_imageBuffers[_imageBuffers.size() - 1]->data());
            buffer.length = _imageBuffers[_imageBuffers.size() - 1]->getSize();

            result = xioctl(_deviceFd, VIDIOC_QBUF, &buffer);
            if (result < 0)
            {
                Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - VIDIOC_QBUF failed: " << result << Log::endl;
                return false;
            }
        }
        break;
    }
    }

    return true;
}

/*************/
#if HAVE_DATAPATH
bool Image_V4L2::openControlDevice()
{
    if ((_controlFd = open(_controlDevicePath.c_str(), O_RDWR | O_NONBLOCK)) < 0)
    {
        _isDatapath = false;
        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Unable to open control device." << Log::endl;
        return false;
    }

    _isDatapath = true;
    return true;
}

/*************/
void Image_V4L2::closeControlDevice()
{
    if (_controlFd >= 0)
    {
        close(_controlFd);
        _controlFd = -1;
    }
}
#endif

/*************/
bool Image_V4L2::openCaptureDevice(const std::string& devicePath)
{
    if (devicePath.empty())
    {
        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - A device path has to be specified" << Log::endl;
        return false;
    }

    if ((_deviceFd = open(devicePath.c_str(), O_RDWR | O_NONBLOCK)) < 0)
    {
        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Unable to open capture device" << Log::endl;
        return false;
    }

    if (xioctl(_deviceFd, VIDIOC_QUERYCAP, &_v4l2Capability) < 0)
    {
        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Unable to query device capabilities" << Log::endl;
        return false;
    }

    if (!_capabilitiesEnumerated)
    {
        if (_v4l2Capability.capabilities & V4L2_CAP_VIDEO_CAPTURE)
        {
            if (!enumerateCaptureDeviceInputs())
                Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Failed to enumerate capture inputs" << Log::endl;

            if (!enumerateVideoStandards())
                Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Failed to enumerate video standards" << Log::endl;

            if (!enumerateCaptureFormats())
                Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Failed to enumerate capture formats" << Log::endl;

            _capabilitiesEnumerated = true;
        }
        else
        {
            Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Only video captures are supported" << Log::endl;
            closeCaptureDevice();
            return false;
        }

        if (_v4l2Capability.capabilities & V4L2_CAP_STREAMING)
        {
            _hasStreamingIO = true;
            Log::get() << Log::MESSAGE << "Image_V4L2::" << __FUNCTION__ << " - Capture device supports streaming I/O" << Log::endl;
        }
        else
        {
            _hasStreamingIO = false;
            Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Capture device does not support streaming I/O" << Log::endl;
        }
    }

    // Choose the input
    v4l2_input v4l2Input;
    memset(&v4l2Input, 0, sizeof(v4l2Input));
    v4l2Input.index = _v4l2Index;
    if (xioctl(_deviceFd, VIDIOC_S_INPUT, &v4l2Input) < 0)
    {
        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Unable to select the input " << _v4l2Index << Log::endl;
        closeCaptureDevice();
        return false;
    }

    // Try setting the video format
    memset(&_v4l2Format, 0, sizeof(_v4l2Format));
    _v4l2Format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    _v4l2Format.fmt.pix.width = _outputWidth;
    _v4l2Format.fmt.pix.height = _outputHeight;
    _v4l2Format.fmt.pix.field = V4L2_FIELD_NONE;
    _v4l2Format.fmt.pix.pixelformat = _outputPixelFormat;

    if (xioctl(_deviceFd, VIDIOC_S_FMT, &_v4l2Format) < 0)
        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Unable to set the desired video format, trying to revert to the original one" << Log::endl;

    Log::get() << Log::MESSAGE << "Image_V4L2::" << __FUNCTION__ << " - Trying to set capture format to: " << _outputWidth << "x" << _outputHeight << ", format "
               << std::string(reinterpret_cast<char*>(&_outputPixelFormat), 4) << Log::endl;

    // Get the real video format
    memset(&_v4l2Format, 0, sizeof(_v4l2Format));
    _v4l2Format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(_deviceFd, VIDIOC_G_FMT, &_v4l2Format) < 0)
    {
        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Failed to get capture video format" << Log::endl;
        closeCaptureDevice();
        return false;
    }

    _outputWidth = _v4l2Format.fmt.pix.width;
    _outputHeight = _v4l2Format.fmt.pix.height;
    _outputPixelFormat = _v4l2Format.fmt.pix.pixelformat;

    Log::get() << Log::MESSAGE << "Image_V4L2::" << __FUNCTION__ << " - Capture format set to: " << _outputWidth << "x" << _outputHeight << " for format "
               << std::string(reinterpret_cast<char*>(&_outputPixelFormat), 4) << Log::endl;

    switch (_outputPixelFormat)
    {
    default:
        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Input format not supported: " << std::string(reinterpret_cast<char*>(&_outputPixelFormat), 4) << Log::endl;
        return false;
    case V4L2_PIX_FMT_RGB24:
        _spec = ImageBufferSpec(_outputWidth, _outputHeight, 3, 24, ImageBufferSpec::Type::UINT8, "RGB");
        break;
    case V4L2_PIX_FMT_BGR24:
        _spec = ImageBufferSpec(_outputWidth, _outputHeight, 3, 24, ImageBufferSpec::Type::UINT8, "BGR");
        break;
    case V4L2_PIX_FMT_YUYV:
        _spec = ImageBufferSpec(_outputWidth, _outputHeight, 3, 16, ImageBufferSpec::Type::UINT8, "YUYV");
        break;
    }

    return true;
}

/*************/
void Image_V4L2::closeCaptureDevice()
{
    _v4l2Inputs.clear();
    _v4l2Standards.clear();
    _v4l2Formats.clear();

    if (_ioMethod == V4L2_MEMORY_MMAP)
    {
        for (auto& buffer : _imageBuffers)
            if (munmap(buffer->data(), buffer->getSize()) == -1)
                Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Failed to unmap buffer for device " << _devicePath << Log::endl;
    }

    if (_deviceFd >= 0)
    {
        close(_deviceFd);
        _deviceFd = -1;
    }
}

/*************/
bool Image_V4L2::enumerateCaptureDeviceInputs()
{
    if (_deviceFd < 0)
    {
        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Invalid device file descriptor" << Log::endl;
        return false;
    }

    _v4l2InputCount = 0;
    struct v4l2_input input;

    memset(&input, 0, sizeof(input));
    while (xioctl(_deviceFd, VIDIOC_ENUMINPUT, &input) >= 0)
    {
        ++_v4l2InputCount;
        input.index = _v4l2InputCount;
    }

    _v4l2Inputs.resize(_v4l2InputCount);
    for (int i = 0; i < _v4l2InputCount; ++i)
    {
        _v4l2Inputs[i].index = i;
        if (xioctl(_deviceFd, VIDIOC_ENUMINPUT, &_v4l2Inputs[i]))
        {
            Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Failed to enumerate input " << i << Log::endl;
            return false;
        }
        Log::get() << Log::MESSAGE << "Image_V4L2::" << __FUNCTION__ << " - Detected input "
                   << std::string(reinterpret_cast<char*>(&_v4l2Inputs[i].name[0]), sizeof(_v4l2Inputs[i].name)) << " of type " << _v4l2Inputs[i].type << Log::endl;
    }

    return true;
}

/*************/
bool Image_V4L2::enumerateCaptureFormats()
{
    if (_deviceFd < 0)
    {
        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Invalid device file descriptor" << Log::endl;
        return false;
    }

    struct v4l2_fmtdesc format;

    memset(&format, 0, sizeof(format));
    format.index = 0;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    _v4l2FormatCount = 0;
    while (xioctl(_deviceFd, VIDIOC_ENUM_FMT, &format) >= 0)
    {
        ++_v4l2FormatCount;
        format.index = _v4l2FormatCount;
    }

    _v4l2Formats.resize(_v4l2FormatCount);
    for (int i = 0; i < _v4l2FormatCount; ++i)
    {
        _v4l2Formats[i].index = i;
        _v4l2Formats[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (xioctl(_deviceFd, VIDIOC_ENUM_FMT, &_v4l2Formats[i]) < 0)
        {
            Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Failed to get format " << i << " description" << Log::endl;
            return false;
        }
        else
        {
            Log::get() << Log::MESSAGE << "Image_V4L2::" << __FUNCTION__ << " - Detected format "
                       << std::string(reinterpret_cast<char*>(&_v4l2Formats[i].description), sizeof(_v4l2Formats[i].description)) << Log::endl;
        }
    }

    return true;
}

/*************/
bool Image_V4L2::enumerateVideoStandards()
{
    if (_deviceFd < 0)
    {
        Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Invalid device file descriptor" << Log::endl;
        return false;
    }

    _v4l2StandardCount = 0;
    struct v4l2_standard standards;
    standards.index = 0;

    while (xioctl(_deviceFd, VIDIOC_ENUMSTD, &standards) >= 0)
    {
        ++_v4l2StandardCount;
        standards.index = _v4l2StandardCount;
    }

    _v4l2Standards.resize(_v4l2StandardCount);
    for (int i = 0; i < _v4l2StandardCount; ++i)
    {
        _v4l2Standards[i].index = i;
        if (xioctl(_deviceFd, VIDIOC_ENUMSTD, &_v4l2Standards[i]))
        {
            Log::get() << Log::WARNING << "Image_V4L2::" << __FUNCTION__ << " - Failed to enumerate video standard " << i << Log::endl;
            return false;
        }
        Log::get() << Log::MESSAGE << "Image_V4L2::" << __FUNCTION__
                   << " - Detected video standard: " << std::string(reinterpret_cast<char*>(&_v4l2Standards[i].name[0]), sizeof(_v4l2Standards[i].name)) << Log::endl;
    }

    return true;
}

/*************/
void Image_V4L2::updateMoreMediaInfo(Values& mediaInfo)
{
    mediaInfo.push_back(Value(_devicePath, "devicePath"));
    mediaInfo.push_back(Value(_v4l2Index, "v4l2Index"));
}

/*************/
void Image_V4L2::scheduleCapture()
{
    _shouldCapture = true;
    addTask([=]() {
        if (_shouldCapture and !_capturing)
        {
            doCapture();
            _shouldCapture = false;
        }
    });
}

/*************/
void Image_V4L2::registerAttributes()
{
    Image::registerAttributes();

    addAttribute("doCapture",
        [&](const Values& args) {
            if (args[0].as<bool>() and !_capturing)
                scheduleCapture();
            else if (_capturing)
                stopCapture();

            return true;
        },
        [&]() -> Values { return {_capturing}; },
        {'b'});

    addAttribute("captureSize",
        [&](const Values& args) {
            auto isCapturing = _capturing;
            stopCapture();
            _outputWidth = std::max(320, args[0].as<int>());
            _outputHeight = std::max(240, args[1].as<int>());
            if (isCapturing)
                scheduleCapture();

            return true;
        },
        [&]() -> Values {
            return {_outputWidth, _outputHeight};
        },
        {'i', 'i'});

    addAttribute("device",
        [&](const Values& args) {
            auto path = args[0].as<std::string>();
            auto index = -1;

            if (path.find("/dev/video") == std::string::npos)
                Log::get() << Log::WARNING << "Image_V4L2~~device"
                           << " - V4L2 device path should start with /dev/video" << path << Log::endl;

            try
            {
                index = stoi(path.substr(10));
            }
            catch (...)
            {
                Log::get() << Log::WARNING << "Image_V4L2~~device"
                           << " - Invalid V4L2 device path: " << path << Log::endl;
                return false;
            }

            auto isCapturing = _capturing;
            stopCapture();

            if (index != -1) // Only the index is specified
                _devicePath = "/dev/video" + std::to_string(index);
            else if (path.empty() || path[0] != '/') // No path specified, or an invalid one
                _devicePath = "";
            else
                _devicePath = path;

            _capabilitiesEnumerated = false;
            if (isCapturing)
                scheduleCapture();

            return true;
        },
        [&]() -> Values { return {_devicePath}; },
        {'s'});

    addAttribute("index",
        [&](const Values& args) {
            auto isCapturing = _capturing;
            stopCapture();
            _v4l2Index = std::max(args[0].as<int>(), 0);
            if (isCapturing)
                scheduleCapture();

            return true;
        },
        [&]() -> Values { return {_v4l2Index}; },
        {'i'});
    setAttributeDescription("index", "Set the input index for the selected V4L2 capture device");

    addAttribute("sourceFormat", [&](const Values&) { return true; }, [&]() -> Values { return {_sourceFormatAsString}; }, {});

    addAttribute("pixelFormat",
        [&](const Values& args) {
            auto isCapturing = _capturing;
            stopCapture();

            auto format = args[0].as<std::string>();
            if (format.find("RGB") != std::string::npos)
                _outputPixelFormat = V4L2_PIX_FMT_RGB24;
            else if (format.find("BGR") != std::string::npos)
                _outputPixelFormat = V4L2_PIX_FMT_BGR24;
            else if (format.find("YUYV") != std::string::npos)
                _outputPixelFormat = V4L2_PIX_FMT_YUYV;
            else
                _outputPixelFormat = V4L2_PIX_FMT_RGB24;

            if (isCapturing)
                scheduleCapture();

            return true;
        },
        [&]() -> Values {
            std::string format;
            switch (_outputPixelFormat)
            {
            default:
                format = "RGB";
                break;
            case V4L2_PIX_FMT_RGB24:
                format = "RGB";
                break;
            case V4L2_PIX_FMT_BGR24:
                format = "BGR";
                break;
            case V4L2_PIX_FMT_YUYV:
                format = "YUYV";
                break;
            }
            return {format};
        },
        {'s'});
    setAttributeDescription("pixelFormat", "Set the desired output format, either RGB or YUYV");
}

} // namespace Splash
