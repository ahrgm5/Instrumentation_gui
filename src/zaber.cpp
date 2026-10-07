#include <zaber.hpp>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <chrono>

ChannelSettings Zaber::defaultZaberSettings() {
    ChannelSettings settings;
    settings.timeoutMs = 500;
    settings.terminationCharacter = '\n';
    settings.writeTermination = "\n";
    settings.baudRate = 115200;
    settings.dataBits = 8;
    settings.parity = VI_ASRL_PAR_NONE;
    settings.stopBits = VI_ASRL_STOP_ONE;
    settings.flowControl = VI_ASRL_FLOW_NONE;
    return settings;
}

Zaber::Zaber(ViSession defaultRM)
    : Instrument(defaultRM) {}

Zaber::Zaber(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings)
    : Instrument(defaultRM, resourceName, settings) {}

bool Zaber::open(const std::string& resourceName, const ChannelSettings& settings) {
    ChannelSettings effectiveSettings = settings;
    if (!effectiveSettings.baudRate.has_value()) {
        effectiveSettings = defaultZaberSettings();
        effectiveSettings.timeoutMs = settings.timeoutMs;
    }
    return Instrument::open(resourceName, effectiveSettings);
}

std::string Zaber::sendCommand(int deviceAddress, const std::string& command) {
    std::string cmd = "/" + std::to_string(deviceAddress) + " " + command;
    std::string response = query(cmd);

    if (response.find(" RJ ") != std::string::npos) {
        throw std::runtime_error("Zaber command rejected (" + cmd + "): " + response);
    }

    return response;
}

void Zaber::writeCommand(int deviceAddress, const std::string& command) {
    std::string cmd = "/" + std::to_string(deviceAddress) + " " + command;
    write(cmd);
}

int32_t Zaber::parseSettingResponse(const std::string& response) {
    std::string trimmed = trim(response);
    std::istringstream iss(trimmed);

    std::string token;
    int32_t parsedValue = 0;
    bool foundNumber = false;

    while (iss >> token) {
        try {
            parsedValue = std::stol(token);
            foundNumber = true;
        } catch (...) {}
    }

    if (!foundNumber) {
        throw std::runtime_error("Failed to parse numeric value from Zaber response: " + response);
    }

    return parsedValue;
}

void Zaber::home(int deviceAddress, int axisNumber) {
    sendCommand(deviceAddress, std::to_string(axisNumber) + " home");
}

void Zaber::moveAbsolute(int deviceAddress, int axisNumber, int32_t microsteps) {
    std::string cmd = std::to_string(axisNumber) + " move abs " + std::to_string(microsteps);
    sendCommand(deviceAddress, cmd);
}

void Zaber::moveRelative(int deviceAddress, int axisNumber, int32_t microsteps) {
    // Correct Zaber ASCII Syntax for Daisy-Chained Multi-Axis Devices:
    // Format: "/<device> <axis> move rel <microsteps>" -> e.g. "/1 1 move rel 5000"
    std::string cmd = std::to_string(axisNumber) + " move rel " + std::to_string(microsteps);
    sendCommand(deviceAddress, cmd);
}

void Zaber::stop(int deviceAddress, int axisNumber) {
    writeCommand(deviceAddress, std::to_string(axisNumber) + " stop");
}

int32_t Zaber::getPosition(int numDevices) {
    std::cout << "--- Device Positions (All Devices) ---\n";
    int32_t firstDevicePos = 0;

    for (int devIndex = 1; devIndex <= numDevices; ++devIndex) {
        try {
            int32_t pos = getPositionForAxis(devIndex, 1);
            std::cout << "Device " << devIndex << " Position: " << pos << " microsteps\n";
            if (devIndex == 1) {
                firstDevicePos = pos;
            }
        } catch (const std::exception& ex) {
            std::cout << "Device " << devIndex << " Position: [Error: " << ex.what() << "]\n";
        }
    }
    std::cout << "-------------------------------------\n";
    return firstDevicePos;
}

int32_t Zaber::getPositionForDevice(int index) {
    if (index == 0) {
        return getPosition();
    }
    return getPositionForAxis(index, 1);
}

int32_t Zaber::getPositionForAxis(int deviceAddress, int axisNumber) {
    if (!isOpen()) return 0;
    std::string cmd = std::to_string(axisNumber) + " get pos";
    std::string response = sendCommand(deviceAddress, cmd);
    return parseSettingResponse(response);
}

bool Zaber::isIdle(int deviceAddress, int axisNumber) {
    std::string response = sendCommand(deviceAddress, std::to_string(axisNumber) + " get status");
    return (response.find("idle") != std::string::npos || response.find("IDLE") != std::string::npos);
}

void Zaber::waitUntilIdle(int deviceAddress, int axisNumber, int pollIntervalMs, int timeoutSeconds) {
    auto start = std::chrono::steady_clock::now();

    while (!isIdle(deviceAddress, axisNumber)) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::steady_clock::now() - start
                           ).count();

        if (elapsed >= timeoutSeconds) {
            throw std::runtime_error("Timeout waiting for Zaber device " + std::to_string(deviceAddress) +
                                     " axis " + std::to_string(axisNumber) + " to become idle.");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
    }
}

int32_t Zaber::getLimitMax(int deviceAddress, int axisNumber) {
    return parseSettingResponse(sendCommand(deviceAddress, std::to_string(axisNumber) + " get limit.max"));
}

int32_t Zaber::getLimitMin(int deviceAddress, int axisNumber) {
    return parseSettingResponse(sendCommand(deviceAddress, std::to_string(axisNumber) + " get limit.min"));
}

int32_t Zaber::getResolution(int deviceAddress, int axisNumber) {
    return parseSettingResponse(sendCommand(deviceAddress, std::to_string(axisNumber) + " get resolution"));
}

// --- Zaber Adapter Implementation ---
ZaberAdapter::ZaberAdapter(std::shared_ptr<Zaber> zaber) : m_zaber(std::move(zaber)) {}

void ZaberAdapter::moveRelative(int axis, int distance) {
    if (!m_zaber || !m_zaber->isOpen()) return;

    // Daisy-chain routing map:
    // Axis 1 = Device /1 (X-MCA)
    // Axis 2 = Device /2 (LSQ)
    // Axis 3 = Device /3 (DMQ-1)
    // Axis 4 = Device /4 (DMQ-2)
    int deviceAddress = axis;
    int axisNumber = 1;

    m_zaber->moveRelative(deviceAddress, axisNumber, distance);
}

void ZaberAdapter::moveAbsolute(int axis, int position) {
    if (!m_zaber || !m_zaber->isOpen()) return;

    int deviceAddress = axis;
    int axisNumber = 1;

    m_zaber->moveAbsolute(deviceAddress, axisNumber, position);
}

void ZaberAdapter::stop(int axis) {
    if (m_zaber && m_zaber->isOpen()) {
        m_zaber->stop(axis, 1);
    }
}

void ZaberAdapter::setSpeed(int axis, int speed) {
    if (m_zaber && m_zaber->isOpen()) {
        m_zaber->query("/" + std::to_string(axis) + " 1 set maxspeed " + std::to_string(speed));
    }
}

void ZaberAdapter::indexIncremental(int axis, int steps) {
    moveRelative(axis, steps);
}

int32_t ZaberAdapter::getPosition(int axis) {
    if (!m_zaber || !m_zaber->isOpen()) return 0;
    return m_zaber->getPositionForAxis(axis, 1);
}