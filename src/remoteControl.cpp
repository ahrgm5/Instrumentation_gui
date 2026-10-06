#include "remoteControl.hpp"
#include <iostream>
#include <cstdlib>

RemoteControl::RemoteControl() : controller(nullptr), connected(false) {}

RemoteControl::~RemoteControl() {
    if (controller) {
        SDL_CloseGamepad(controller);
        controller = nullptr;
    }
    SDL_Quit();
}

bool RemoteControl::initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    int numJoysticks = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&numJoysticks);
    if (joysticks) {
        for (int i = 0; i < numJoysticks; ++i) {
            SDL_JoystickID jid = joysticks[i];
            if (SDL_IsGamepad(jid)) {
                controller = SDL_OpenGamepad(jid);
                if (controller) {
                    connected = true;
                    std::cout << "Connected to: " << SDL_GetGamepadName(controller) << std::endl;
                    SDL_free(joysticks);
                    return true;
                }
            }
        }
        SDL_free(joysticks);
    }

    std::cerr << "No compatible Xbox/game controller found!" << std::endl;
    return false;
}

void RemoteControl::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            connected = false;
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            std::cout << "Button Pressed: " << static_cast<int>(event.gbutton.button) << std::endl;
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            std::cout << "Button Released: " << static_cast<int>(event.gbutton.button) << std::endl;
            break;

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            if (std::abs(event.gaxis.value) > 8000) {
                std::cout << "Axis " << static_cast<int>(event.gaxis.axis)
                << " moved to: " << event.gaxis.value << std::endl;
            }
            break;

        case SDL_EVENT_GAMEPAD_REMOVED:
            std::cout << "Controller disconnected." << std::endl;
            if (controller) {
                SDL_CloseGamepad(controller);
                controller = nullptr;
            }
            connected = false;
            break;
        }
    }
}

bool RemoteControl::isConnected() const {
    return connected;
}

std::string RemoteControl::getControllerName() const {
    if (controller) {
        const char* name = SDL_GetGamepadName(controller);
        return name ? name : "Unknown";
    }
    return "None";
}

bool RemoteControl::isButtonPressed(SDL_GamepadButton button) const {
    if (controller && connected) {
        return SDL_GetGamepadButton(controller, button);
    }
    return false;
}

Sint16 RemoteControl::getAxisValue(SDL_GamepadAxis axis) const {
    if (controller && connected) {
        return SDL_GetGamepadAxis(controller, axis);
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
    if (!connected || !m_activeMotor) return;

    if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        moveRelative(1, stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        moveRelative(1, -stepDistance);
    }

    if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        moveRelative(2, stepDistance);
    } else if (isButtonPressed(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        moveRelative(2, -stepDistance);
    }

    if (isButtonPressed(SDL_GAMEPAD_BUTTON_EAST)) {
        stop(0);
    }
}