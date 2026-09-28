#pragma once 

#include <visa.h>
#include <optional>
#include <string>
#include <vector>

enum class VisaInterface {
    Unknown,
    Gpib,
    Serial,
    Usb,
    TcpipInstrument,
    TcpipSocket,
    Vxi,
    Pxi
};

enum class ChannelAttribute {
    Timeout,
    TerminationCharacter,
    SerialBaudRate,
    SerialDataBits,
    SerialParity,
    SerialStopBits,
    SerialFlowControl,
    TcpipNoDelay,
    TcpipKeepAlive
};

struct ChannelSettings {
    ViUInt32 timeoutMs = 5000;

    std::optional<ViUInt8> terminationCharacter;

    // ASRL (serial) resources
    std::optional<ViUInt32> baudRate;
    std::optional<ViUInt16> dataBits;
    std::optional<ViUInt16> parity;       // VI_ASRL_PAR_*
    std::optional<ViUInt16> stopBits;     // VI_ASRL_STOP_*
    std::optional<ViUInt16> flowControl;  // VI_ASRL_FLOW_*

    // TCPIP...::SOCKET resources
    std::optional<bool> tcpNoDelay;
    std::optional<bool> tcpKeepAlive;
};

class Instrument {
public:
    explicit Instrument(ViSession defaultRM);
    Instrument(ViSession defaultRM, const std::string& resourceName, ViUInt32 timeoutMs = 5000);
    Instrument(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings);

    virtual ~Instrument();

    Instrument(const Instrument&) = delete;
    Instrument& operator=(const Instrument&) = delete;

    Instrument(Instrument&& other) noexcept;
    Instrument& operator=(Instrument&& other) noexcept;

    // Session Management
    virtual bool open(const std::string& resourceName, ViUInt32 timeoutMs = 5000);
    virtual bool open(const std::string& resourceName, const ChannelSettings& settings);
    virtual void close();
    bool isOpen() const { return m_isOpen; }

    VisaInterface interfaceType() const { return m_interface; }
    std::vector<ChannelAttribute> supportedAttributes() const;
    static VisaInterface parseInterface(const std::string& resourceName);

    // Communication
    virtual void write(const std::string& command);
    virtual std::string read(size_t bufferSize = 1024);
    virtual std::string query(const std::string& command, size_t bufferSize = 1024);

    virtual void setTimeout(ViUInt32 timeoutMs);
    virtual void setReadTermination(char character);

protected:
    ViSession m_defaultRM = VI_NULL;
    ViSession m_session = VI_NULL;
    std::string m_resourceName;
    VisaInterface m_interface = VisaInterface::Unknown;
    bool m_isOpen = false;

    void applySettings(const ChannelSettings& settings);
    void setAttribute(ViAttr attribute, ViAttrState value, const std::string& description);
    bool supports(ChannelAttribute attribute) const;
    void checkStatus(ViStatus status, const std::string& action) const;
}; 
