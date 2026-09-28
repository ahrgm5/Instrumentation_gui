#include "ic4_camera.hpp"
#include <stdexcept>

IC4Camera::IC4Camera() : m_isStreaming(false) {}

IC4Camera::~IC4Camera() {
    stopLiveStream();
    closeDevice();
}

bool IC4Camera::openDevice(const std::string& serialOrName) {
    ic4::Error err;
    auto devices = ic4::DeviceEnum::enumDevices(err);
    if (!err.isSuccess()) {
        return false;
    }

    std::shared_ptr<ic4::DeviceInfo> targetDevice = nullptr;
    for (const auto& dev : devices) {
        if (dev->serial() == serialOrName || dev->modelName() == serialOrName) {
            targetDevice = dev;
            break;
        }
    }

    if (!targetDevice && !devices.empty()) {
        targetDevice = devices[0];
    }

    if (!targetDevice) {
        return false;
    }

    bool success = m_grabber.deviceOpen(*targetDevice, err);
    return success && err.isSuccess();
}

void IC4Camera::closeDevice() {
    stopLiveStream();
    ic4::Error err;
    m_grabber.deviceClose(err);
}

bool IC4Camera::isConnected() const {
    return m_grabber.isDeviceValid();
}

bool IC4Camera::startLiveStream(ic4interop::Qt::DisplayWidget* displayWidget) {
    if (m_isStreaming) return true;

    ic4::Error err;

    // Disambiguate QueueSink::create using a callback lambda
    m_queueSink = ic4::QueueSink::create([](ic4::QueueSink&) {}, err);
    if (!err.isSuccess() || !m_queueSink) {
        return false;
    }

    bool setupSuccess = false;
    if (displayWidget) {
        // Obtain std::shared_ptr<ic4::Display> via asDisplay() from interop-Qt.h
        auto display = displayWidget->asDisplay();
        setupSuccess = m_grabber.streamSetup(m_queueSink, display, err);
    } else {
        setupSuccess = m_grabber.streamSetup(m_queueSink, err);
    }

    if (!setupSuccess || !err.isSuccess()) {
        return false;
    }

    m_isStreaming = true;
    return true;
}

void IC4Camera::stopLiveStream() {
    if (!m_isStreaming) return;

    ic4::Error err;
    m_grabber.streamStop(err);
    m_queueSink.reset();
    m_isStreaming = false;
}

bool IC4Camera::isStreaming() const {
    return m_isStreaming;
}

bool IC4Camera::snapImage(QImage& outImage) {
    if (!m_isStreaming || !m_queueSink) {
        return false;
    }

    ic4::Error err;
    // popOutputBuffer takes ic4::Error& as its argument
    auto buffer = m_queueSink->popOutputBuffer(err);
    if (!err.isSuccess() || !buffer) {
        return false;
    }

    auto imgType = buffer->imageType(err);
    if (!err.isSuccess()) {
        return false;
    }

    uint32_t width = imgType.width();
    uint32_t height = imgType.height();
    auto pixelFormat = imgType.pixel_format();

    void* dataPtr = buffer->ptr(err);
    if (!err.isSuccess() || !dataPtr) {
        return false;
    }

    ptrdiff_t pitch = buffer->pitch(err);

    QImage::Format qFormat = QImage::Format_Invalid;
    if (pixelFormat == ic4::PixelFormat::BGR8 || pixelFormat == ic4::PixelFormat::RGB8) {
        qFormat = QImage::Format_RGB888;
    } else if (pixelFormat == ic4::PixelFormat::Mono8) {
        qFormat = QImage::Format_Grayscale8;
    } else if (pixelFormat == ic4::PixelFormat::BGRa8 || pixelFormat == ic4::PixelFormat::BGRA8) {
        qFormat = QImage::Format_ARGB32;
    }

    if (qFormat == QImage::Format_Invalid) {
        return false;
    }

    QImage temp(static_cast<const uchar*>(dataPtr), width, height, static_cast<int>(pitch), qFormat);
    if (pixelFormat == ic4::PixelFormat::BGR8) {
        outImage = temp.rgbSwapped();
    } else {
        outImage = temp.copy();
    }

    return true;
}

bool IC4Camera::setPropertyFloat(const std::string& name, double value) {
    ic4::Error err;
    auto prop = m_grabber.propertyMap().find(name, err);
    if (!prop.is_valid()) return false;
    auto floatProp = prop.asFloat(err);
    if (!err.isSuccess()) return false;
    return floatProp.setValue(value, err);
}

double IC4Camera::getPropertyFloat(const std::string& name) const {
    ic4::Error err;
    auto prop = m_grabber.propertyMap().find(name, err);
    if (!prop.is_valid()) return 0.0;
    auto floatProp = prop.asFloat(err);
    if (!err.isSuccess()) return 0.0;
    return floatProp.getValue(err);
}

bool IC4Camera::setPropertyInteger(const std::string& name, int64_t value) {
    ic4::Error err;
    auto prop = m_grabber.propertyMap().find(name, err);
    if (!prop.is_valid()) return false;
    auto intProp = prop.asInteger(err);
    if (!err.isSuccess()) return false;
    return intProp.setValue(value, err);
}

int64_t IC4Camera::getPropertyInteger(const std::string& name) const {
    ic4::Error err;
    auto prop = m_grabber.propertyMap().find(name, err);
    if (!prop.is_valid()) return 0;
    auto intProp = prop.asInteger(err);
    if (!err.isSuccess()) return 0;
    return intProp.getValue(err);
}