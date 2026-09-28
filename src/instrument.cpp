#include "instrument.hpp"
#include <algorithm>
#include <cctype>
#include <utility>
#include <stdexcept>

Instrument::Instrument(ViSession defaultRM)
    : m_defaultRM(defaultRM), m_session(VI_NULL), m_interface(VisaInterface::Unknown), m_isOpen(false) {
}

Instrument::Instrument(ViSession defaultRM, const std::string& resourceName, ViUInt32 timeoutMs)
    : Instrument(defaultRM) {
    open(resourceName, timeoutMs);
}

Instrument::Instrument(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings)
    : Instrument(defaultRM) {
    open(resourceName, settings);
}

Instrument::~Instrument() {
    close();
}

Instrument::Instrument(Instrument&& other) noexcept
    : m_defaultRM(other.m_defaultRM),
    m_session(other.m_session),
    m_resourceName(std::move(other.m_resourceName)),
    m_interface(other.m_interface),
    m_isOpen(other.m_isOpen) {
    other.m_session = VI_NULL;
    other.m_isOpen = false;
    other.m_interface = VisaInterface::Unknown;
}

Instrument& Instrument::operator=(Instrument&& other) noexcept {
    if (this != &other) {
        close();
        m_defaultRM = other.m_defaultRM;
        m_session = other.m_session;
        m_resourceName = std::move(other.m_resourceName);
        m_interface = other.m_interface;
        m_isOpen = other.m_isOpen;

        other.m_session = VI_NULL;
        other.m_isOpen = false;
        other.m_interface = VisaInterface::Unknown;
    }
    return *this;
}

bool Instrument::open(const std::string& resourceName, ViUInt32 timeoutMs) {
    ChannelSettings settings;
    settings.timeoutMs = timeoutMs;
    return open(resourceName, settings);
}

bool Instrument::open(const std::string& resourceName, const ChannelSettings& settings) {
    if (m_isOpen) {
        close();
    }

    m_resourceName = resourceName;
    m_interface = parseInterface(resourceName);
    ViStatus status = viOpen(m_defaultRM, const_cast<char*>(m_resourceName.c_str()), VI_NULL, VI_NULL, &m_session);

    if (status < VI_SUCCESS) {
        m_isOpen = false;
        m_session = VI_NULL;
        return false;
    }

    m_isOpen = true;
    try {
        applySettings(settings);
    }
    catch (...) {
        close();
        throw;
    }
    return true;
}

void Instrument::close() {
    if (m_isOpen && m_session != VI_NULL) {
        viClose(m_session);
        m_session = VI_NULL;
        m_isOpen = false;
        m_interface = VisaInterface::Unknown;
    }
}

void Instrument::write(const std::string& command) {
    if (!m_isOpen) throw std::runtime_error("Cannot write: VISA session is closed.");

    std::string cmd = command;
    if (cmd.empty() || cmd.back() != '\n') {
        cmd += "\n";
    }

    ViUInt32 bytesWritten = 0;
    ViStatus status = viWrite(
        m_session,
        reinterpret_cast<ViBuf>(const_cast<char*>(cmd.c_str())),
        static_cast<ViUInt32>(cmd.size()),
        &bytesWritten
    );
    checkStatus(status, "Write command failed");
}

std::string Instrument::read(size_t bufferSize) {
    if (!m_isOpen) throw std::runtime_error("Cannot read: VISA session is closed.");

    std::vector<char> buffer(bufferSize);
    ViUInt32 bytesRead = 0;

    ViStatus status = viRead(
        m_session,
        reinterpret_cast<ViPBuf>(buffer.data()),
        static_cast<ViUInt32>(buffer.size() - 1),
        &bytesRead
    );
    checkStatus(status, "Read response failed");

    buffer[bytesRead] = '\0';
    return std::string(buffer.data());
}

std::string Instrument::query(const std::string& command, size_t bufferSize) {
    write(command);
    return read(bufferSize);
}

void Instrument::setTimeout(ViUInt32 timeoutMs) {
    if (!m_isOpen) return;
    setAttribute(VI_ATTR_TMO_VALUE, static_cast<ViAttrState>(timeoutMs), "Set timeout failed");
}

void Instrument::setReadTermination(char character) {
    if (!m_isOpen) {
        throw std::runtime_error("Cannot set termination: VISA session is closed.");
    }

    setAttribute(VI_ATTR_TERMCHAR, static_cast<ViAttrState>(character), "Failed to set VISA termination character");
    setAttribute(VI_ATTR_TERMCHAR_EN, VI_TRUE, "Failed to enable VISA termination character");
}

VisaInterface Instrument::parseInterface(const std::string& resourceName) {
    const auto separator = resourceName.find("::");
    std::string interfaceName = resourceName.substr(0, separator);
    std::transform(interfaceName.begin(), interfaceName.end(), interfaceName.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    const auto startsWith = [&interfaceName](const char* prefix) {
        return interfaceName.rfind(prefix, 0) == 0;
        };
    if (startsWith("ASRL")) return VisaInterface::Serial;
    if (startsWith("GPIB")) return VisaInterface::Gpib;
    if (startsWith("USB")) return VisaInterface::Usb;
    if (startsWith("VXI")) return VisaInterface::Vxi;
    if (startsWith("PXI")) return VisaInterface::Pxi;
    if (startsWith("TCPIP")) {
        std::string upper = resourceName;
        std::transform(upper.begin(), upper.end(), upper.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return upper.size() >= 8 && upper.compare(upper.size() - 8, 8, "::SOCKET") == 0
            ? VisaInterface::TcpipSocket : VisaInterface::TcpipInstrument;
    }
    return VisaInterface::Unknown;
}

std::vector<ChannelAttribute> Instrument::supportedAttributes() const {
    std::vector<ChannelAttribute> attributes{ ChannelAttribute::Timeout, ChannelAttribute::TerminationCharacter };
    if (m_interface == VisaInterface::Serial) {
        attributes.insert(attributes.end(), {
            ChannelAttribute::SerialBaudRate, ChannelAttribute::SerialDataBits,
            ChannelAttribute::SerialParity, ChannelAttribute::SerialStopBits,
            ChannelAttribute::SerialFlowControl });
    }
    if (m_interface == VisaInterface::TcpipSocket) {
        attributes.insert(attributes.end(), { ChannelAttribute::TcpipNoDelay, ChannelAttribute::TcpipKeepAlive });
    }
    return attributes;
}

bool Instrument::supports(ChannelAttribute attribute) const {
    const auto attributes = supportedAttributes();
    return std::find(attributes.begin(), attributes.end(), attribute) != attributes.end();
}

void Instrument::setAttribute(ViAttr attribute, ViAttrState value, const std::string& description) {
    checkStatus(viSetAttribute(m_session, attribute, value), description);
}

void Instrument::applySettings(const ChannelSettings& settings) {
    setTimeout(settings.timeoutMs);

    if (settings.terminationCharacter) {
        setAttribute(VI_ATTR_TERMCHAR, static_cast<ViAttrState>(*settings.terminationCharacter), "Set termination character failed");
        setAttribute(VI_ATTR_TERMCHAR_EN, VI_TRUE, "Enable termination character failed");
    }

    const auto require = [this](ChannelAttribute attribute, const char* setting) {
        if (!supports(attribute)) {
            throw std::invalid_argument(std::string(setting) + " is not supported by this VISA resource string.");
        }
        };
    if (settings.baudRate) {
        require(ChannelAttribute::SerialBaudRate, "baudRate");
        setAttribute(VI_ATTR_ASRL_BAUD, static_cast<ViAttrState>(*settings.baudRate), "Set serial baud rate failed");
    }
    if (settings.dataBits) {
        require(ChannelAttribute::SerialDataBits, "dataBits");
        setAttribute(VI_ATTR_ASRL_DATA_BITS, static_cast<ViAttrState>(*settings.dataBits), "Set serial data bits failed");
    }
    if (settings.parity) {
        require(ChannelAttribute::SerialParity, "parity");
        setAttribute(VI_ATTR_ASRL_PARITY, static_cast<ViAttrState>(*settings.parity), "Set serial parity failed");
    }
    if (settings.stopBits) {
        require(ChannelAttribute::SerialStopBits, "stopBits");
        setAttribute(VI_ATTR_ASRL_STOP_BITS, static_cast<ViAttrState>(*settings.stopBits), "Set serial stop bits failed");
    }
    if (settings.flowControl) {
        require(ChannelAttribute::SerialFlowControl, "flowControl");
        setAttribute(VI_ATTR_ASRL_FLOW_CNTRL, static_cast<ViAttrState>(*settings.flowControl), "Set serial flow control failed");
    }
    if (settings.tcpNoDelay) {
        require(ChannelAttribute::TcpipNoDelay, "tcpNoDelay");
        setAttribute(VI_ATTR_TCPIP_NODELAY, *settings.tcpNoDelay ? VI_TRUE : VI_FALSE, "Set TCP_NODELAY failed");
    }
    if (settings.tcpKeepAlive) {
        require(ChannelAttribute::TcpipKeepAlive, "tcpKeepAlive");
        setAttribute(VI_ATTR_TCPIP_KEEPALIVE, *settings.tcpKeepAlive ? VI_TRUE : VI_FALSE, "Set TCP keep-alive failed");
    }
}

void Instrument::checkStatus(ViStatus status, const std::string& action) const {
    if (status < VI_SUCCESS) {
        throw std::runtime_error(action + " [VISA Error Code: " + std::to_string(status) + "]");
    }
}