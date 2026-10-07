#include <velmex.hpp>
#include <stdexcept>
#include <algorithm>

ChannelSettings VelmexVXM::defaultVelmexSettings() {
    ChannelSettings settings;
    settings.timeoutMs = 5000;
    settings.terminationCharacter = '\r';
    settings.writeTermination = "\r";
    settings.baudRate = 9600;
    settings.dataBits = 8;
    settings.parity = VI_ASRL_PAR_NONE;
    settings.stopBits = VI_ASRL_STOP_ONE;
    settings.flowControl = VI_ASRL_FLOW_NONE;
    return settings;
}

VelmexVXM::VelmexVXM(ViSession defaultRM)
    : Instrument(defaultRM) {}

VelmexVXM::VelmexVXM(ViSession defaultRM, const std::string& resourceName, const ChannelSettings& settings)
    : Instrument(defaultRM, resourceName, settings) {}

bool VelmexVXM::open(const std::string& resourceName, const ChannelSettings& settings) {
    ChannelSettings effectiveSettings = settings;
    if (!effectiveSettings.baudRate.has_value()) {
        effectiveSettings = defaultVelmexSettings();
        effectiveSettings.timeoutMs = settings.timeoutMs;
    }
    return Instrument::open(resourceName, effectiveSettings);
}

void VelmexVXM::writeCommand(const std::string& cmd) {
    write(cmd);
}

std::string VelmexVXM::readResponse() {
    return read();
}

void VelmexVXM::clearMemory() {
    writeCommand("C");
}

std::string VelmexVXM::run() {
    writeCommand("R");
    return readResponse();
}

void VelmexVXM::setSpeed(int motor, int speed, bool fullPower) {
    std::string prefix = fullPower ? "SA" : "S";
    std::string cmd = prefix + std::to_string(motor) + "M" + std::to_string(speed);
    writeCommand(cmd);
}

void VelmexVXM::indexIncremental(int motor, int steps) {
    std::string cmd = "I" + std::to_string(motor) + "M" + std::to_string(steps);
    writeCommand(cmd);
}

int32_t VelmexVXM::getPosition(int motor) {
    if (!isOpen()) return 0;

    std::string cmd = "X" + std::to_string(motor);
    writeCommand(cmd);
    std::string response = readResponse();

    try {
        std::string trimmed = trim(response);
        return std::stol(trimmed);
    } catch (...) {
        return 0;
    }
}

void VelmexVXM::write(const std::string& command) {
    if (!isOpen()) throw std::runtime_error("Cannot write: VISA session is closed.");

    std::string cmd = command;
    if (!cmd.empty() && cmd.back() == '\n') {
        cmd.pop_back();
    }
    if (cmd.empty() || cmd.back() != '\r') {
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

// --- Velmex Adapter Implementation ---
VelmexAdapter::VelmexAdapter(std::shared_ptr<VelmexVXM> velmex) : m_velmex(std::move(velmex)) {}

void VelmexAdapter::moveRelative(int axis, int distance) {
    if (m_velmex && m_velmex->isOpen()) {
        m_velmex->clearMemory();
        m_velmex->indexIncremental(axis, distance);
        m_velmex->run();
    }
}

void VelmexAdapter::moveAbsolute(int axis, int position) {
    if (m_velmex && m_velmex->isOpen()) {
        m_velmex->clearMemory();
        std::string cmd = "IA" + std::to_string(axis) + "M" + std::to_string(position);
        m_velmex->write(cmd);
        m_velmex->run();
    }
}

void VelmexAdapter::stop(int /*axis*/) {
    if (m_velmex && m_velmex->isOpen()) {
        m_velmex->clearMemory();
    }
}

void VelmexAdapter::setSpeed(int axis, int speed) {
    if (m_velmex && m_velmex->isOpen()) {
        m_velmex->setSpeed(axis, speed, false);
    }
}

void VelmexAdapter::indexIncremental(int axis, int steps) {
    if (m_velmex && m_velmex->isOpen()) {
        m_velmex->indexIncremental(axis, steps);
    }
}

int32_t VelmexAdapter::getPosition(int axis) {
    if (m_velmex && m_velmex->isOpen()) {
        return m_velmex->getPosition(axis);
    }
    return 0;
}