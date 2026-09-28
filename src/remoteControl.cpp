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