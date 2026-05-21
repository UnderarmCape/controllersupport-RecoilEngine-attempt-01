/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "ControllerInput.h"

#include "System/Input/InputHandler.h"
#include "System/Log/ILog.h"

#include <SDL_events.h>
#include <SDL_joystick.h>

#if SDL_VERSION_ATLEAST(2, 0, 0)
	#include <SDL_gamecontroller.h>
#endif

CControllerInput* controllerInput = nullptr;

CControllerInput* CControllerInput::GetInstance()
{
	if (controllerInput == nullptr) {
		controllerInput = new CControllerInput();
	}

	return controllerInput;
}

void CControllerInput::FreeInstance(CControllerInput* controllerInputPtr)
{
	if (controllerInputPtr == controllerInput) {
		delete controllerInput;
		controllerInput = nullptr;
	}
}
CControllerInput::CControllerInput()
{
	inputCon = input.AddHandler([this](const SDL_Event& event) {
		return this->HandleSDLControllerEvent(event);
	});

	LOG_L(L_INFO, "[ControllerInput] Initialized SDL controller input handler");
}

CControllerInput::~CControllerInput()
{
	for (auto& [instanceId, state] : controllersByInstanceId) {
		if (state.gameController != nullptr) {
			SDL_GameControllerClose(state.gameController);
			state.gameController = nullptr;
		}
	}

	controllersByInstanceId.clear();

	LOG_L(L_INFO, "[ControllerInput] Shutting down SDL controller input handler");
}

bool CControllerInput::HandleSDLControllerEvent(const SDL_Event& event)
{
	switch (event.type) {
		case SDL_CONTROLLERDEVICEADDED: {
			HandleDeviceAdded(event.cdevice.which);
		} break;

		case SDL_CONTROLLERDEVICEREMOVED: {
			HandleDeviceRemoved(event.cdevice.which);
		} break;

		case SDL_CONTROLLERDEVICEREMAPPED: {
			HandleDeviceRemapped(event.cdevice.which);
		} break;

		case SDL_CONTROLLERBUTTONDOWN: {
			HandleButtonDown(event.cbutton.which, event.cbutton.button, event.cbutton.state);
		} break;

		case SDL_CONTROLLERBUTTONUP: {
			HandleButtonUp(event.cbutton.which, event.cbutton.button, event.cbutton.state);
		} break;

		case SDL_CONTROLLERAXISMOTION: {
			HandleAxisMotion(event.caxis.which, event.caxis.axis, event.caxis.value);
		} break;

		default:
			break;
	}

	return false;
}

void CControllerInput::LogAvailableController(int deviceId) const
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (SDL_IsGameController(deviceId)) {
		const char* name = SDL_GameControllerNameForIndex(deviceId);
		LOG_L(L_INFO, "[ControllerInput] SDL game controller available: deviceId=%d name=%s", deviceId, name != nullptr ? name : "unknown");
	} else {
		const char* name = SDL_JoystickNameForIndex(deviceId);
		LOG_L(L_INFO, "[ControllerInput] SDL joystick available but not game controller: deviceId=%d name=%s", deviceId, name != nullptr ? name : "unknown");
	}
#else
	LOG_L(L_INFO, "[ControllerInput] SDL2 game controller API unavailable at compile time: deviceId=%d", deviceId);
#endif
}

void CControllerInput::HandleDeviceAdded(int deviceId)
{
	LOG_L(L_INFO, "[ControllerInput] Controller device added: deviceId=%d", deviceId);
	LogAvailableController(deviceId);

#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (!SDL_IsGameController(deviceId)) {
		LOG_L(L_INFO, "[ControllerInput] Ignoring non-game-controller device: deviceId=%d", deviceId);
		return;
	}

	SDL_GameController* gameController = SDL_GameControllerOpen(deviceId);
	if (gameController == nullptr) {
		LOG_L(L_WARNING, "[ControllerInput] Failed to open SDL game controller: deviceId=%d error=%s", deviceId, SDL_GetError());
		return;
	}

	SDL_Joystick* joystick = SDL_GameControllerGetJoystick(gameController);
	const int instanceId = joystick != nullptr ? SDL_JoystickInstanceID(joystick) : -1;

	ControllerState state;
	state.deviceId = deviceId;
	state.instanceId = instanceId;

	const char* name = SDL_GameControllerName(gameController);
	state.name = name != nullptr ? name : "unknown";
	state.gameController = gameController;

	controllersByInstanceId[instanceId] = state;

	LOG_L(L_INFO, "[ControllerInput] Controller connected: deviceId=%d instanceId=%d name=%s", deviceId, instanceId, state.name.c_str());
#endif
}

void CControllerInput::HandleDeviceRemoved(int instanceId)
{
	LOG_L(L_INFO, "[ControllerInput] Controller device removed: instanceId=%d", instanceId);

	auto it = controllersByInstanceId.find(instanceId);
	if (it != controllersByInstanceId.end()) {
	LOG_L(L_INFO, "[ControllerInput] Removed tracked controller: instanceId=%d name=%s", instanceId, it->second.name.c_str());

	if (it->second.gameController != nullptr) {
		SDL_GameControllerClose(it->second.gameController);
		it->second.gameController = nullptr;
	}

	controllersByInstanceId.erase(it);
}

void CControllerInput::HandleDeviceRemapped(int instanceId)
{
	LOG_L(L_INFO, "[ControllerInput] Controller remapped: instanceId=%d", instanceId);
}

void CControllerInput::HandleButtonDown(int instanceId, int buttonId, std::uint8_t value)
{
	auto& state = controllersByInstanceId[instanceId];

	if (buttonId >= 0 && buttonId < static_cast<int>(state.buttons.size())) {
		state.buttons[buttonId] = value;
	}

	LOG_L(L_INFO, "[ControllerInput] ButtonDown: instanceId=%d buttonId=%d value=%u", instanceId, buttonId, value);
}

void CControllerInput::HandleButtonUp(int instanceId, int buttonId, std::uint8_t value)
{
	auto& state = controllersByInstanceId[instanceId];

	if (buttonId >= 0 && buttonId < static_cast<int>(state.buttons.size())) {
		state.buttons[buttonId] = value;
	}

	LOG_L(L_INFO, "[ControllerInput] ButtonUp: instanceId=%d buttonId=%d value=%u", instanceId, buttonId, value);
}

void CControllerInput::HandleAxisMotion(int instanceId, int axisId, std::int16_t value)
{
	auto& state = controllersByInstanceId[instanceId];

	if (axisId >= 0 && axisId < static_cast<int>(state.axes.size())) {
		state.axes[axisId] = value;
	}

	LOG_L(L_INFO, "[ControllerInput] AxisMotion: instanceId=%d axisId=%d value=%d", instanceId, axisId, value);
}