#include <zaber.hpp>
#include <sstream>
#include <stdexcept>
#include <cmath>

Zaber::Zaber(ViSession defaultRM)
    : Instrument(defaultRM) {
}

Zaber::Zaber(ViSession defaultRM, const std::string& resourceName, ViUInt32 timeoutMs)
    : Instrument(defaultRM) {
    open(resourceName, timeoutMs);
}

Zaber::Zaber(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings)
    : Instrument(defaultRM) {
    open(resourceName, settings);
}

ChannelSettings Zaber::getDefaultZaberSettings(ViUInt32 timeoutMs) {
    ChannelSettings settings;
    settings.timeoutMs = timeoutMs;
    settings.baudRate = 115200;
    settings.dataBits = 8;
    settings.parity = VI_ASRL_PAR_NONE;
    settings.stopBits = VI_ASRL_STOP_ONE;
    settings.flowControl = VI_ASRL_FLOW_NONE;
    settings.terminationCharacter = '\n';
    return settings;
}

bool Zaber::open(const std::string& resourceName, ViUInt32 timeoutMs) {
    return open(resourceName, getDefaultZaberSettings(timeoutMs));
}

bool Zaber::open(const std::string& resourceName, const ChannelSettings& settings) {
    return Instrument::open(resourceName, settings);
}

std::string Zaber::formatCommand(uint8_t deviceAddress, uint8_t axis, const std::string& command) const {
    std::ostringstream oss;
    oss << "/" << static_cast<int>(deviceAddress);
    if (axis > 0) { // Formats MCA sub-axis routing automatically
        oss << " " << static_cast<int>(axis);
    }
    oss << " " << command;
    return oss.str();
}

std::string Zaber::sendCommand(uint8_t deviceAddress, const std::string& command, uint8_t axis) {
    std::string formattedCmd = formatCommand(deviceAddress, axis, command);
    return query(formattedCmd);
}

// --- Target-Based API Methods ---

void Zaber::home(const AxisTarget& target) {
    sendCommand(target.device, "home", target.axis);
}

void Zaber::moveAbsolute(int32_t microsteps, const AxisTarget& target) {
    std::string cmd = "move abs " + std::to_string(microsteps);
    sendCommand(target.device, cmd, target.axis);
}

void Zaber::moveRelative(int32_t microsteps, const AxisTarget& target) {
    std::string cmd = "move rel " + std::to_string(microsteps);
    sendCommand(target.device, cmd, target.axis);
}

void Zaber::stop(const AxisTarget& target) {
    sendCommand(target.device, "stop", target.axis);
}

int32_t Zaber::getPosition(const AxisTarget& target) {
    std::string response = sendCommand(target.device, "get pos", target.axis);

    std::istringstream iss(response);
    std::string token, lastToken;
    while (iss >> token) {
        lastToken = token;
    }

    try {
        return std::stol(lastToken);
    }
    catch (...) {
        throw std::runtime_error("Failed to parse Zaber position response: " + response);
    }
}

// --- High-Level 4-Motor API ---

void Zaber::setGantryConfig(const GantryConfig& config) {
    m_cfg = config;
}

void Zaber::homeAll() {
    // Broadcast 'home' command across all MCA channels and DMQ stages simultaneously
    sendCommand(0, "home");
}

void Zaber::moveX(int32_t microsteps) {
    // Move dual-X motors together
    moveAbsolute(microsteps, m_cfg.x1);
    moveAbsolute(microsteps, m_cfg.x2);
}

void Zaber::moveY(int32_t microsteps) {
    moveAbsolute(microsteps, m_cfg.y);
}

void Zaber::moveZ(int32_t microsteps) {
    moveAbsolute(microsteps, m_cfg.z);
}

std::pair<int32_t, int32_t> Zaber::getXPositions() {
    int32_t posX1 = getPosition(m_cfg.x1);
    int32_t posX2 = getPosition(m_cfg.x2);
    return { posX1, posX2 };
}