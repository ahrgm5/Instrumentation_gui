#pragma once

#include <SDL3/SDL.h>
#include <string>

class RemoteControl {
public:
    RemoteControl();
    ~RemoteControl();

    bool initialize();
    void pollEvents();
    bool isConnected() const;
    std::string getControllerName() const;

    bool isButtonPressed(SDL_GamepadButton button) const;
    Sint16 getAxisValue(SDL_GamepadAxis axis) const;

private:
    SDL_Gamepad* controller;
    bool connected;
};