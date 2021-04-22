#include "./image/image_opencv.h"

#include <chrono>
#include <regex>

#include <opencv2/opencv.hpp>

#include "./utils/cgutils.h"
#include "./utils/log.h"
#include "./utils/timer.h"

namespace Splash
{

/*************/
Image_OpenCV::Image_OpenCV(RootObject* root)
    : Image(root)
{
    init();
}

/*************/
Image_OpenCV::~Image_OpenCV()
{
    _continueReading = false;
    if (_readLoopThread.joinable())
        _readLoopThread.join();
}

/*************/
bool Image_OpenCV::read(const std::string& filename)
{
    try
    {
        _inputIndex = stoi(filename);
    }
    catch (...)
    {
        _inputIndex = -1;
    }

    // This releases any previous input
    _continueReading = false;
    if (_readLoopThread.joinable())
        _readLoopThread.join();

    _continueReading = true;
    _readLoopThread = std::thread([&]() { readLoop(); });

    return true;
}

/*************/
void Image_OpenCV::init()
{
    _type = SPLASH_GRAPH_TYPE_IMAGE_OPENCV;
    registerAttributes();

    // This is used for getting documentation "offline"
    if (!_root)
        return;
}

/*************/
void Image_OpenCV::readLoop()
{
    std::unique_ptr<cv::VideoCapture> videoCapture{nullptr};
    if (_inputIndex >= 0)
        videoCapture = std::make_unique<cv::VideoCapture>(_inputIndex);
    else
        videoCapture = std::make_unique<cv::VideoCapture>(_filepath);

    if (!videoCapture->isOpened())
    {
        Log::get() << Log::WARNING << "Image_OpenCV::" << __FUNCTION__ << " - Unable to open video capture input " << _filepath << Log::endl;
        return;
    }

    uint32_t width = videoCapture->get(cv::CAP_PROP_FRAME_WIDTH);
    uint32_t height = videoCapture->get(cv::CAP_PROP_FRAME_HEIGHT);
    uint32_t framerate = videoCapture->get(cv::CAP_PROP_FPS);
    float exposure = videoCapture->get(cv::CAP_PROP_AUTO_EXPOSURE);

    Log::get() << Log::MESSAGE << "Image_OpenCV::" << __FUNCTION__ << " - Successfully initialized VideoCapture " << _filepath << Log::endl;

    _capturing = true;
    while (_continueReading)
    {
        if (_cvOptionsUpdated)
        {
            _cvOptionsUpdated = false;
            auto options = parseCVOptions(_cvOptions);
            for (const auto& [prop, value] : options)
                videoCapture->set(prop, value);
        }

        // Update capture parameters
        if (width != _width)
            videoCapture->set(cv::CAP_PROP_FRAME_WIDTH, _width);
        if (height != _height)
            videoCapture->set(cv::CAP_PROP_FRAME_HEIGHT, _height);
        if (framerate != _framerate)
            videoCapture->set(cv::CAP_PROP_FPS, _framerate);
        if (exposure != _exposure)
            videoCapture->set(cv::CAP_PROP_EXPOSURE, _exposure);

        _width = width = videoCapture->get(cv::CAP_PROP_FRAME_WIDTH);
        _height = height = videoCapture->get(cv::CAP_PROP_FRAME_HEIGHT);
        _framerate = framerate = videoCapture->get(cv::CAP_PROP_FPS);
        _exposure = exposure = videoCapture->get(cv::CAP_PROP_EXPOSURE);

        // Capture
        auto capture = cv::Mat();
        if (!videoCapture->read(capture))
        {
            Log::get() << Log::WARNING << "Image_OpenCV::" << __FUNCTION__ << " - An error occurred while reading the VideoCapture" << Log::endl;
            return;
        }

        if (capture.channels() == 1)
        {
            auto rgba = cv::Mat();
            cv::cvtColor(capture, rgba, cv::COLOR_GRAY2RGBA);
            capture = rgba;
        }

        auto spec = _readBuffer.getSpec();
        if (static_cast<int>(spec.width) != capture.rows || static_cast<int>(spec.height) != capture.cols || static_cast<int>(spec.channels) != capture.channels())
        {
            ImageBufferSpec newSpec(capture.cols, capture.rows, capture.channels(), 8 * capture.channels(), ImageBufferSpec::Type::UINT8);
            newSpec.format = "BGRA";
            _readBuffer = ImageBuffer(newSpec);
        }
        unsigned char* pixels = reinterpret_cast<unsigned char*>(_readBuffer.data());

        unsigned int imageSize = capture.rows * capture.cols * capture.channels();
        std::copy(capture.data, capture.data + imageSize, pixels);

        {
            std::lock_guard<std::shared_mutex> lockWrite(_writeMutex);
            if (!_bufferImage)
                _bufferImage = std::make_unique<ImageBuffer>();
            std::swap(*_bufferImage, _readBuffer);
            _imageUpdated = true;
        }
        updateTimestamp();
        if (!_isConnectedToRemote)
            update();

        if (Timer::get().isDebug())
            Timer::get() >> ("read " + _name);
    }
    _capturing = false;
}

/*************/
std::map<int, double> Image_OpenCV::parseCVOptions(const std::string& options)
{
    std::regex re("(([^=]+)=([^ ,]+))+");
    auto sre_begin = std::sregex_iterator(options.begin(), options.end(), re);
    auto sre_end = std::sregex_iterator();

    std::map<int, double> result;
    for (auto i = sre_begin; i != sre_end; ++i)
    {
        std::smatch match = *i;
        try
        {
            auto key = std::stoi(match[2]);
            auto value = std::stod(match[3]);
            result[key] = value;
        }
        catch (...)
        {
            Log::get() << Log::WARNING << "Image_OpenCV::" << __FUNCTION__ << "Key " << std::string(match[2]) << " is not an integer, or value " << std::string(match[3])
                       << " is not a floating point value" << Log::endl;
            continue;
        }
    }
    return result;
}

/*************/
void Image_OpenCV::registerAttributes()
{
    Image::registerAttributes();

    addAttribute("size",
        [&](const Values& args) {
            _width = args[0].as<int>();
            _height = args[1].as<int>();

            _width = (_width == 0) ? 640 : _width;
            _height = (_height == 0) ? 480 : _height;

            return true;
        },
        [&]() -> Values {
            return {(int)_width, (int)_height};
        },
        {'i', 'i'});
    setAttributeDescription("size", "Set the desired capture resolution");

    addAttribute("framerate",
        [&](const Values& args) {
            _framerate = (args[0].as<float>() == 0) ? 60 : args[0].as<float>();
            return true;
        },
        [&]() -> Values { return {_framerate}; },
        {'r'});
    setAttributeDescription("framerate", "Set the desired capture framerate");

    addAttribute("exposure",
        [&](const Values& args) {
            _exposure = args[0].as<float>();
            return true;
        },
        [&]() -> Values { return {_exposure}; },
        {'r'});
    setAttributeDescription("exposure", "Camera exposure");

    addAttribute("capturing", [&]() -> Values { return {_capturing}; });
    setAttributeDescription("capturing", "Ask whether the camera is grabbing images");

    addAttribute("cvOptions",
        [&](const Values& args) {
            _cvOptions = args[0].as<std::string>();
            _cvOptionsUpdated = true;
            return true;
        },
        [&]() -> Values { return {_cvOptions}; },
        {'s'});
    setAttributeDescription("cvOptions",
        R"(OpenCV attributes set as a string following the format: "key1=value1, key2=value2, ...".
        Keys must be indices from cv::CAP_PROPs, and values must be doubles.
        Attributes can be found in OpenCV documentation: https://docs.opencv.org.)");
}

} // namespace Splash
