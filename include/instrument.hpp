#pragma once 

#include <visa.h>
#include <optional>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <stdexcept>

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

    std::optional<ViUInt8> terminationCharacter; // Read termination character
    std::string writeTermination = "\n";         // String appended to every write command

    // ASRL (serial) resources
    std::optional<ViUInt32> baudRate;
    std::optional<ViUInt16> dataBits;
    std::optional<ViUInt16> parity;       // VI_ASRL_PAR_*
    std::optional<ViUInt16> stopBits;     // VI_ASRL_STOP_*
    std::optional<ViUInt16> flowControl;  // VI_ASRL_FLOW_*

    // TCPIP...::SOCKET resources
    std::optional<bool> tcpNoDelay;
    std::optional<bool> tcpKeepAlive;

    ChannelSettings() = default;
    ChannelSettings(ViUInt32 timeout) : timeoutMs(timeout) {}
};

class Instrument {
public:
    // Unopened session constructor
    explicit Instrument(ViSession defaultRM);

    // Primary constructor: safely accepts concrete settings evaluated BEFORE base construction
    Instrument(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings = {});

    virtual ~Instrument();

    Instrument(const Instrument&) = delete;
    Instrument& operator=(const Instrument&) = delete;

    Instrument(Instrument&& other) noexcept;
    Instrument& operator=(Instrument&& other) noexcept;

    // Session Management
    virtual bool open(const std::string& resourceName, const ChannelSettings& settings);
    virtual bool open(const std::string& resourceName, ViUInt32 timeoutMs);
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

    // Virtual hook for runtime queries after the object is fully constructed
    virtual ChannelSettings defaultSettings() const {
        return ChannelSettings{};
    }

    // String cleanup helper
    static std::string trim(std::string str) {
        auto start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        auto end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    // Generic typed query helper
    template <typename T>
    T queryAs(const std::string& command) {
        std::string raw = trim(query(command));
        std::istringstream iss(raw);
        T val;
        if (!(iss >> val)) {
            throw std::runtime_error("Failed to parse response '" + raw + "' for command: " + command);
        }
        return val;
    }

protected:
    ViSession m_defaultRM = VI_NULL;
    ViSession m_session = VI_NULL;
    std::string m_resourceName;
    VisaInterface m_interface = VisaInterface::Unknown;
    bool m_isOpen = false;
    std::string m_writeTermination = "\n";

    void applySettings(const ChannelSettings& settings);
    void setAttribute(ViAttr attribute, ViAttrState value, const std::string& description);
    bool supports(ChannelAttribute attribute) const;
    void checkStatus(ViStatus status, const std::string& action) const;
};