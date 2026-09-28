#ifndef IC4_CAMERA_HPP
#define IC4_CAMERA_HPP

#include <ic4/ic4.h>
#include <ic4-interop/interop-Qt.h>
#include <QImage>
#include <memory>
#include <string>

class IC4Camera {
public:
    IC4Camera();
    ~IC4Camera();

    bool openDevice(const std::string& serialOrName);
    void closeDevice();
    bool isConnected() const;

    bool startLiveStream(ic4interop::Qt::DisplayWidget* displayWidget = nullptr);
    void stopLiveStream();
    bool isStreaming() const;

    bool snapImage(QImage& outImage);

    // Property helpers
    bool setPropertyFloat(const std::string& name, double value);
    double getPropertyFloat(const std::string& name) const;

    bool setPropertyInteger(const std::string& name, int64_t value);
    int64_t getPropertyInteger(const std::string& name) const;

private:
    ic4::Grabber m_grabber;
    std::shared_ptr<ic4::QueueSink> m_queueSink;
    bool m_isStreaming;
};

#endif // IC4_CAMERA_HPP