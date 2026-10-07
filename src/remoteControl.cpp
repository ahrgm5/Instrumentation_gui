#include <remoteControl.hpp>
#include <iostream>
#include <cstdlib>

RemoteControl::RemoteControl() : m_controller(nullptr), m_connected(false) {}

RemoteControl::~RemoteControl() {
    if (m_controller) {
        SDL_CloseGamepad(m_controller);
        m_controller = nullptr;
    }
    SDL_Quit();
}

bool RemoteControl::initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        return false;
    }

    int numJoysticks = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&numJoysticks);
    if (joysticks) {
        for (int i = 0; i < numJoysticks; ++i) {
            SDL_JoystickID jid = joysticks[i];
            if (SDL_IsGamepad(jid)) {
                m_controller = SDL_OpenGamepad(jid);
                if (m_controller) {
                    m_connected = true;
                    SDL_free(joysticks);
                    return true;
                }
            }
        }
        SDL_free(joysticks);
    }

    return false;
}

void RemoteControl::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            m_connected = false;
            break;

        case SDL_EVENT_GAMEPAD_REMOVED:
            if (m_controller) {
                SDL_CloseGamepad(m_controller);
                m_controller = nullptr;
            }
            m_connected = false;
            break;
        }
    }
}

bool RemoteControl::isConnected() const {
    return m_connected;
}

std::string RemoteControl::getControllerName() const {
    if (m_controller) {
        const char* name = SDL_GetGamepadName(m_controller);
        return name ? name : "Unknown";
    }
    return "None";
}

bool RemoteControl::isButtonPressed(SDL_GamepadButton button) const {
    if (m_controller && m_connected) {
        return SDL_GetGamepadButton(m_controller, button);
    }
    return false;
}

Sint16 RemoteControl::getAxisValue(SDL_GamepadAxis axis) const {
    if (m_controller && m_connected) {
        return SDL_GetGamepadAxis(m_controller, axis);
    }
    return 0;
}

// --- Target Motor Bindings ---
void RemoteControl::setTargetMotor(std::shared_ptr<IMotorAxis> motor) {
    m_activeMotor = motor;
}

void RemoteControl::bindZaber(std::shared_ptr<Zaber> zaber) {
    if (zaber) {
        m_activeMotor = std::make_shared<ZaberAdapter>(zaber);
    } else {
        m_activeMotor.reset();
    }
}

void RemoteControl::bindVelmex(std::shared_ptr<VelmexVXM> velmex) {
    if (velmex) {
        m_activeMotor = std::make_shared<VelmexAdapter>(velmex);
    } else {
        m_activeMotor.reset();
    }
}

void RemoteControl::clearTargetMotor() {
    m_activeMotor.reset();
}

// --- Direct Delegation to Active Adapter ---
void RemoteControl::moveRelative(int axis, int distance) {
    if (m_activeMotor) m_activeMotor->moveRelative(axis, distance);
}

void RemoteControl::moveAbsolute(int axis, int position) {
    if (m_activeMotor) m_activeMotor->moveAbsolute(axis, position);
}

void RemoteControl::setSpeed(int axis, int speed) {
    if (m_activeMotor) m_activeMotor->setSpeed(axis, speed);
}

void RemoteControl::indexIncremental(int axis, int steps) {
    if (m_activeMotor) m_activeMotor->indexIncremental(axis, steps);
}

void RemoteControl::stop(int axis) {
    if (m_activeMotor) m_activeMotor->stop(axis);
}

// --- Dynamic Pad Motion Command Processing ---
void RemoteControl::processMotionCommands(int stepDistance) {
    if (!m_connected || !m_activeMotor) return;

    if (isButtonPressed(SDL_GAMEPAD_BUTTON_SOUTH)) {
    moveRelative(1, stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_EAST)) {
    moveRelative(1, -stepDistance);

    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) {
    moveRelative(2, stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
    moveRelative(2, -stepDistance);

    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        moveRelative(3, stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        moveRelative(3, -stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        moveRelative(4, -stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        moveRelative(4, stepDistance);
    }

}