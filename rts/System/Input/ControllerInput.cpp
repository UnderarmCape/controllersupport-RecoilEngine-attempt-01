/* This file is part of the Recoil engine (GPL v2 or later), see LICENSE.html */

#include "ControllerInput.h"

#include "System/Input/InputHandler.h"
#include "System/Log/ILog.h"
#include "System/Misc/SpringTime.h"

#ifndef CONTROLLER_INPUT_LOG_EVENTS
#define CONTROLLER_INPUT_LOG_EVENTS 0
#endif

#ifndef CONTROLLER_INPUT_DIAG_TIMING
#define CONTROLLER_INPUT_DIAG_TIMING 1
#endif

#ifndef HEADLESS
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_error.h>
#include <SDL_gamecontroller.h>
#include <SDL_joystick.h>
#endif

CControllerInput* controllerInput = nullptr;

#if CONTROLLER_INPUT_DIAG_TIMING
namespace {
constexpr float CONTROLLER_INPUT_DIAG_INFO_MS = 2.0f;
constexpr float CONTROLLER_INPUT_DIAG_WARN_MS = 8.0f;

bool ControllerInputDiagShouldLog(const spring_time startTime, spring_time& lastLogTime, float& elapsedMS)
{
	elapsedMS = (spring_gettime() - startTime).toMilliSecsf();
	if (elapsedMS < CONTROLLER_INPUT_DIAG_INFO_MS)
		return false;

	const spring_time now = spring_gettime();
	const spring_time throttle = spring_msecs((elapsedMS >= CONTROLLER_INPUT_DIAG_WARN_MS) ? 250.0f : 1000.0f);

	if (lastLogTime.isTime() && now < (lastLogTime + throttle))
		return false;

	lastLogTime = now;
	return true;
}

void ControllerInputDiagLogSlowCopy(const char* section, const spring_time startTime, size_t returnedCount, size_t trackedCount)
{
	static spring_time lastLogTime = spring_notime;
	float elapsedMS = 0.0f;
	if (!ControllerInputDiagShouldLog(startTime, lastLogTime, elapsedMS))
		return;

	if (elapsedMS >= CONTROLLER_INPUT_DIAG_WARN_MS) {
		LOG_L(L_WARNING,
			"[ControllerInputDiag] %s slow %.3fms returned=%u tracked=%u",
			section, elapsedMS, static_cast<unsigned int>(returnedCount), static_cast<unsigned int>(trackedCount));
	} else {
		LOG_L(L_INFO,
			"[ControllerInputDiag] %s slow %.3fms returned=%u tracked=%u",
			section, elapsedMS, static_cast<unsigned int>(returnedCount), static_cast<unsigned int>(trackedCount));
	}
}

#ifndef HEADLESS
const char* ControllerInputDiagEventName(std::uint32_t eventType)
{
	switch (eventType) {
		case SDL_CONTROLLERDEVICEADDED: return "SDL_CONTROLLERDEVICEADDED";
		case SDL_CONTROLLERDEVICEREMOVED: return "SDL_CONTROLLERDEVICEREMOVED";
		case SDL_CONTROLLERDEVICEREMAPPED: return "SDL_CONTROLLERDEVICEREMAPPED";
		case SDL_CONTROLLERBUTTONDOWN: return "SDL_CONTROLLERBUTTONDOWN";
		case SDL_CONTROLLERBUTTONUP: return "SDL_CONTROLLERBUTTONUP";
		case SDL_CONTROLLERAXISMOTION: return "SDL_CONTROLLERAXISMOTION";
		default: return "non-controller";
	}
}

void ControllerInputDiagLogSlowEvent(const char* section, const spring_time startTime, std::uint32_t eventType, int trackedCount)
{
	static spring_time lastLogTime = spring_notime;
	float elapsedMS = 0.0f;
	if (!ControllerInputDiagShouldLog(startTime, lastLogTime, elapsedMS))
		return;

	if (elapsedMS >= CONTROLLER_INPUT_DIAG_WARN_MS) {
		LOG_L(L_WARNING,
			"[ControllerInputDiag] %s slow %.3fms event=%s tracked=%d",
			section, elapsedMS, ControllerInputDiagEventName(eventType), trackedCount);
	} else {
		LOG_L(L_INFO,
			"[ControllerInputDiag] %s slow %.3fms event=%s tracked=%d",
			section, elapsedMS, ControllerInputDiagEventName(eventType), trackedCount);
	}
}

void ControllerInputDiagLogSlowDeviceOp(const char* section, const spring_time startTime, int id, int trackedCount)
{
	static spring_time lastLogTime = spring_notime;
	float elapsedMS = 0.0f;
	if (!ControllerInputDiagShouldLog(startTime, lastLogTime, elapsedMS))
		return;

	if (elapsedMS >= CONTROLLER_INPUT_DIAG_WARN_MS) {
		LOG_L(L_WARNING,
			"[ControllerInputDiag] %s slow %.3fms id=%d tracked=%d",
			section, elapsedMS, id, trackedCount);
	} else {
		LOG_L(L_INFO,
			"[ControllerInputDiag] %s slow %.3fms id=%d tracked=%d",
			section, elapsedMS, id, trackedCount);
	}
}
#endif
}
#endif

CControllerInput* CControllerInput::GetInstance()
{
	if (controllerInput == nullptr) {
		controllerInput = new CControllerInput();
	}

	return controllerInput;
}

void CControllerInput::FreeInstance()
{
	delete controllerInput;
	controllerInput = nullptr;
}

std::vector<CControllerInput::ControllerState> CControllerInput::GetAvailableControllers() const
{
#if CONTROLLER_INPUT_DIAG_TIMING
	const spring_time controllerInputDiagStart = spring_gettime();
#endif

	std::vector<ControllerState> controllers;
	controllers.reserve(controllersByInstanceID.size());

	for (const auto& controllerIt : controllersByInstanceID) {
		controllers.push_back(controllerIt.second);
	}

#if CONTROLLER_INPUT_DIAG_TIMING
	ControllerInputDiagLogSlowCopy("CControllerInput::GetAvailableControllers", controllerInputDiagStart, controllers.size(), controllersByInstanceID.size());
#endif

	return controllers;
}

std::optional<CControllerInput::ControllerState> CControllerInput::GetControllerState(int instanceID) const
{
#if CONTROLLER_INPUT_DIAG_TIMING
	const spring_time controllerInputDiagStart = spring_gettime();
#endif

	const auto controllerIt = controllersByInstanceID.find(instanceID);
	if (controllerIt == controllersByInstanceID.end()) {
#if CONTROLLER_INPUT_DIAG_TIMING
		ControllerInputDiagLogSlowCopy("CControllerInput::GetControllerState(miss)", controllerInputDiagStart, 0, controllersByInstanceID.size());
#endif
		return std::nullopt;
	}

	const auto controllerState = controllerIt->second;

#if CONTROLLER_INPUT_DIAG_TIMING
	ControllerInputDiagLogSlowCopy("CControllerInput::GetControllerState(hit)", controllerInputDiagStart, 1, controllersByInstanceID.size());
#endif

	return controllerState;
}

#ifndef HEADLESS

CControllerInput::CControllerInput()
{
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
		LOG_L(L_WARNING, "[ControllerInput] Failed to initialize SDL joystick/gamecontroller subsystem: %s", SDL_GetError());
	} else {
		LOG_L(L_INFO, "[ControllerInput] SDL joystick/gamecontroller subsystem initialized");
	}

	SDL_GameControllerEventState(SDL_ENABLE);
	SDL_JoystickEventState(SDL_ENABLE);

	inputCon = input.AddHandler([this](const SDL_Event& event) {
		return this->HandleSDLControllerEvent(event);
	});

	LOG_L(L_INFO, "[ControllerInput] SDL_NumJoysticks at init: %d", SDL_NumJoysticks());
	ScanExistingControllers();

	LOG_L(L_INFO, "[ControllerInput] Initialized SDL controller input handler");
}

CControllerInput::~CControllerInput()
{
	for (auto& controllerIt : controllersByInstanceID) {
		auto& state = controllerIt.second;

		if (state.gameController != nullptr) {
			SDL_GameControllerClose(state.gameController);
			state.gameController = nullptr;
		}
	}

	controllersByInstanceID.clear();

	LOG_L(L_INFO, "[ControllerInput] Shutting down SDL controller input handler");
}

bool CControllerInput::HandleSDLControllerEvent(const SDL_Event& event)
{
#if CONTROLLER_INPUT_DIAG_TIMING
	const spring_time controllerInputDiagStart = spring_gettime();
#endif

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

#if CONTROLLER_INPUT_DIAG_TIMING
	ControllerInputDiagLogSlowEvent("CControllerInput::HandleSDLControllerEvent", controllerInputDiagStart, event.type, static_cast<int>(controllersByInstanceID.size()));
#endif

	return false;
}

void CControllerInput::LogAvailableController(int deviceID) const
{
	if (SDL_IsGameController(deviceID)) {
		const char* name = SDL_GameControllerNameForIndex(deviceID);
		LOG_L(L_INFO, "[ControllerInput] SDL game controller available: deviceID=%d name=%s", deviceID, name != nullptr ? name : "unknown");
		return;
	}

	const char* name = SDL_JoystickNameForIndex(deviceID);
	LOG_L(L_INFO, "[ControllerInput] SDL joystick available but not game controller: deviceID=%d name=%s", deviceID, name != nullptr ? name : "unknown");
}

void CControllerInput::ScanExistingControllers()
{
#if CONTROLLER_INPUT_DIAG_TIMING
	const spring_time controllerInputDiagStart = spring_gettime();
#endif

	const int joystickCount = SDL_NumJoysticks();
	LOG_L(L_INFO, "[ControllerInput] Scanning existing SDL joysticks: count=%d", joystickCount);

	for (int deviceID = 0; deviceID < joystickCount; ++deviceID) {
		LogAvailableController(deviceID);

		if (SDL_IsGameController(deviceID)) {
			HandleDeviceAdded(deviceID);
			continue;
		}

		LOG_L(L_INFO, "[ControllerInput] Skipping existing non-game-controller device: deviceID=%d", deviceID);
	}

#if CONTROLLER_INPUT_DIAG_TIMING
	ControllerInputDiagLogSlowDeviceOp("CControllerInput::ScanExistingControllers", controllerInputDiagStart, joystickCount, static_cast<int>(controllersByInstanceID.size()));
#endif
}

void CControllerInput::HandleDeviceAdded(int deviceID)
{
#if CONTROLLER_INPUT_DIAG_TIMING
	const spring_time controllerInputDiagStart = spring_gettime();
#endif

	LOG_L(L_INFO, "[ControllerInput] Controller device added: deviceID=%d", deviceID);
	LogAvailableController(deviceID);

	if (!SDL_IsGameController(deviceID)) {
		LOG_L(L_INFO, "[ControllerInput] Ignoring non-game-controller device: deviceID=%d", deviceID);
#if CONTROLLER_INPUT_DIAG_TIMING
		ControllerInputDiagLogSlowDeviceOp("CControllerInput::HandleDeviceAdded(non-controller)", controllerInputDiagStart, deviceID, static_cast<int>(controllersByInstanceID.size()));
#endif
		return;
	}

	SDL_GameController* gameController = SDL_GameControllerOpen(deviceID);
	if (gameController == nullptr) {
		LOG_L(L_WARNING, "[ControllerInput] Failed to open SDL game controller: deviceID=%d error=%s", deviceID, SDL_GetError());
#if CONTROLLER_INPUT_DIAG_TIMING
		ControllerInputDiagLogSlowDeviceOp("CControllerInput::HandleDeviceAdded(open-failed)", controllerInputDiagStart, deviceID, static_cast<int>(controllersByInstanceID.size()));
#endif
		return;
	}

	SDL_Joystick* joystick = SDL_GameControllerGetJoystick(gameController);
	const int instanceID = joystick != nullptr ? SDL_JoystickInstanceID(joystick) : -1;

	if (instanceID < 0) {
		LOG_L(L_WARNING, "[ControllerInput] Failed to get joystick instance ID: deviceID=%d", deviceID);
		SDL_GameControllerClose(gameController);
#if CONTROLLER_INPUT_DIAG_TIMING
		ControllerInputDiagLogSlowDeviceOp("CControllerInput::HandleDeviceAdded(instance-failed)", controllerInputDiagStart, deviceID, static_cast<int>(controllersByInstanceID.size()));
#endif
		return;
	}

	TrackedControllerState state;
	state.deviceID = deviceID;
	state.instanceID = instanceID;

	const char* name = SDL_GameControllerName(gameController);
	state.name = name != nullptr ? name : "unknown";
	state.gameController = gameController;

	auto existingIt = controllersByInstanceID.find(instanceID);
	if (existingIt != controllersByInstanceID.end() && existingIt->second.gameController != nullptr) {
		SDL_GameControllerClose(existingIt->second.gameController);
	}

	controllersByInstanceID[instanceID] = state;

	LOG_L(L_INFO, "[ControllerInput] Controller connected: deviceID=%d instanceID=%d name=%s", deviceID, instanceID, state.name.c_str());

#if CONTROLLER_INPUT_DIAG_TIMING
	ControllerInputDiagLogSlowDeviceOp("CControllerInput::HandleDeviceAdded", controllerInputDiagStart, deviceID, static_cast<int>(controllersByInstanceID.size()));
#endif
}

void CControllerInput::HandleDeviceRemoved(int instanceID)
{
#if CONTROLLER_INPUT_DIAG_TIMING
	const spring_time controllerInputDiagStart = spring_gettime();
#endif

	LOG_L(L_INFO, "[ControllerInput] Controller device removed: instanceID=%d", instanceID);

	auto it = controllersByInstanceID.find(instanceID);
	if (it == controllersByInstanceID.end()) {
#if CONTROLLER_INPUT_DIAG_TIMING
		ControllerInputDiagLogSlowDeviceOp("CControllerInput::HandleDeviceRemoved(miss)", controllerInputDiagStart, instanceID, static_cast<int>(controllersByInstanceID.size()));
#endif
		return;
	}

	LOG_L(L_INFO, "[ControllerInput] Removed tracked controller: instanceID=%d name=%s", instanceID, it->second.name.c_str());

	if (it->second.gameController != nullptr) {
		SDL_GameControllerClose(it->second.gameController);
		it->second.gameController = nullptr;
	}

	controllersByInstanceID.erase(it);

#if CONTROLLER_INPUT_DIAG_TIMING
	ControllerInputDiagLogSlowDeviceOp("CControllerInput::HandleDeviceRemoved", controllerInputDiagStart, instanceID, static_cast<int>(controllersByInstanceID.size()));
#endif
}

void CControllerInput::HandleDeviceRemapped(int instanceID)
{
#if CONTROLLER_INPUT_DIAG_TIMING
	const spring_time controllerInputDiagStart = spring_gettime();
#endif

	LOG_L(L_INFO, "[ControllerInput] Controller remapped: instanceID=%d", instanceID);

#if CONTROLLER_INPUT_DIAG_TIMING
	ControllerInputDiagLogSlowDeviceOp("CControllerInput::HandleDeviceRemapped", controllerInputDiagStart, instanceID, static_cast<int>(controllersByInstanceID.size()));
#endif
}

void CControllerInput::HandleButtonDown(int instanceID, int buttonID, std::uint8_t value)
{
	auto controllerIt = controllersByInstanceID.find(instanceID);
	if (controllerIt == controllersByInstanceID.end()) {
		return;
	}

	auto& state = controllerIt->second;

	if (buttonID >= 0 && buttonID < static_cast<int>(state.buttons.size())) {
		state.buttons[buttonID] = value;
	}

#if CONTROLLER_INPUT_LOG_EVENTS
	LOG_L(L_INFO, "[ControllerInput] ButtonDown: instanceID=%d buttonID=%d value=%u", instanceID, buttonID, static_cast<unsigned int>(value));
#endif
}

void CControllerInput::HandleButtonUp(int instanceID, int buttonID, std::uint8_t value)
{
	auto controllerIt = controllersByInstanceID.find(instanceID);
	if (controllerIt == controllersByInstanceID.end()) {
		return;
	}

	auto& state = controllerIt->second;

	if (buttonID >= 0 && buttonID < static_cast<int>(state.buttons.size())) {
		state.buttons[buttonID] = value;
	}

#if CONTROLLER_INPUT_LOG_EVENTS
	LOG_L(L_INFO, "[ControllerInput] ButtonUp: instanceID=%d buttonID=%d value=%u", instanceID, buttonID, static_cast<unsigned int>(value));
#endif
}

void CControllerInput::HandleAxisMotion(int instanceID, int axisID, std::int16_t value)
{
	auto controllerIt = controllersByInstanceID.find(instanceID);
	if (controllerIt == controllersByInstanceID.end()) {
		return;
	}

	auto& state = controllerIt->second;

	if (axisID >= 0 && axisID < static_cast<int>(state.axes.size())) {
		state.axes[axisID] = value;
	}

#if CONTROLLER_INPUT_LOG_EVENTS
	LOG_L(L_INFO, "[ControllerInput] AxisMotion: instanceID=%d axisID=%d value=%d", instanceID, axisID, static_cast<int>(value));
#endif
}

#else

CControllerInput::CControllerInput() = default;
CControllerInput::~CControllerInput() = default;

bool CControllerInput::HandleSDLControllerEvent(const SDL_Event&)
{
	return false;
}

void CControllerInput::LogAvailableController(int) const {}
void CControllerInput::ScanExistingControllers() {}
void CControllerInput::HandleDeviceAdded(int) {}
void CControllerInput::HandleDeviceRemoved(int) {}
void CControllerInput::HandleDeviceRemapped(int) {}
void CControllerInput::HandleButtonDown(int, int, std::uint8_t) {}
void CControllerInput::HandleButtonUp(int, int, std::uint8_t) {}
void CControllerInput::HandleAxisMotion(int, int, std::int16_t) {}

#endif
