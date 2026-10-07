#include <ipg.hpp>
#include <stdexcept>

ChannelSettings ipgYLR::defaultSettings() {
    ChannelSettings settings;
    settings.timeoutMs = 5000;
    settings.terminationCharacter = '\r'; // YLR uses Carriage Return
    settings.tcpNoDelay = true;           // Recommended for TCP/IP socket control
    settings.tcpKeepAlive = true;
    return settings;
}

ipgYLR::ipgYLR(ViSession defaultRM)
    : Instrument(defaultRM) {
}

ipgYLR::ipgYLR(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings)
    : Instrument(defaultRM, resourceName, settings) {
}

void ipgYLR::write(const std::string& command) {
    if (!m_isOpen) throw std::runtime_error("Cannot write: VISA session is closed.");

    std::string cmd = command;
    // Ensure command ends with a Carriage Return (\r)
    if (cmd.empty() || cmd.back() != '\r') {
        if (!cmd.empty() && cmd.back() == '\n') {
            cmd.pop_back(); // Remove accidental newline if present
        }
        cmd += "\r";
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

std::string ipgYLR::getIdentity() {
    return query("*IDN?");
}

std::string ipgYLR::setIpAddress(const std::string& newIp) {
    // Remotely configure laser IP address via socket command
    return query("SIP " + newIp);
}

void ipgYLR::setEmission(bool enable) {
    write(enable ? "ENA 1" : "ENA 0");
}

bool ipgYLR::isEmissionEnabled() {
    std::string response = query("ENA?");
    return response.find("1") != std::string::npos;
}

std::string ipgYLR::getStatus() {
    return query("STA");
}