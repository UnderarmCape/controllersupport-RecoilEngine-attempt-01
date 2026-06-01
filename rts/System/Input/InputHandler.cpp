/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "InputHandler.h"

#include "System/Log/ILog.h"
#include "System/Misc/SpringTime.h"
#include "System/TimeProfiler.h"

InputHandler input;

#ifndef CONTROLLER_INPUT_DIAG_TIMING
#define CONTROLLER_INPUT_DIAG_TIMING 1
#endif

#if CONTROLLER_INPUT_DIAG_TIMING
namespace {
constexpr float CONTROLLER_INPUT_DIAG_INFO_MS = 2.0f;
constexpr float CONTROLLER_INPUT_DIAG_WARN_MS = 8.0f;

struct ControllerInputDiagEventCounts {
	int total = 0;
	int controller = 0;
	int device = 0;
	int button = 0;
	int axis = 0;
};

void ControllerInputDiagCountEvent(const SDL_Event& event, ControllerInputDiagEventCounts& counts)
{
	++counts.total;

	switch (event.type) {
		case SDL_CONTROLLERDEVICEADDED:
		case SDL_CONTROLLERDEVICEREMOVED:
		case SDL_CONTROLLERDEVICEREMAPPED: {
			++counts.controller;
			++counts.device;
		} break;

		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP: {
			++counts.controller;
			++counts.button;
		} break;

		case SDL_CONTROLLERAXISMOTION: {
			++counts.controller;
			++counts.axis;
		} break;

		default:
			break;
	}
}

void ControllerInputDiagLogSlowPushEvents(const spring_time startTime, const ControllerInputDiagEventCounts& counts)
{
	const float elapsedMS = (spring_gettime() - startTime).toMilliSecsf();
	if (elapsedMS < CONTROLLER_INPUT_DIAG_INFO_MS)
		return;

	static spring_time lastLogTime = spring_notime;
	const spring_time now = spring_gettime();
	const spring_time throttle = spring_msecs((elapsedMS >= CONTROLLER_INPUT_DIAG_WARN_MS) ? 250.0f : 1000.0f);

	if (lastLogTime.isTime() && now < (lastLogTime + throttle))
		return;

	lastLogTime = now;

	if (elapsedMS >= CONTROLLER_INPUT_DIAG_WARN_MS) {
		LOG_L(L_WARNING,
			"[ControllerInputDiag] InputHandler::PushEvents slow %.3fms events=%d controller=%d device=%d button=%d axis=%d",
			elapsedMS, counts.total, counts.controller, counts.device, counts.button, counts.axis);
	} else {
		LOG_L(L_INFO,
			"[ControllerInputDiag] InputHandler::PushEvents slow %.3fms events=%d controller=%d device=%d button=%d axis=%d",
			elapsedMS, counts.total, counts.controller, counts.device, counts.button, counts.axis);
	}
}
}
#endif

InputHandler::InputHandler() = default;

void InputHandler::PushEvent(const SDL_Event& ev)
{
	for (const auto& eventHandler : eventHandlers) {
		if (eventHandler) {
			if (eventHandler(ev))
				break;
		}
	}
}

void InputHandler::PushEvents()
{
	SCOPED_TIMER("Misc::InputHandler::PushEvents");

#if CONTROLLER_INPUT_DIAG_TIMING
	const spring_time controllerInputDiagStart = spring_gettime();
	ControllerInputDiagEventCounts controllerInputDiagCounts;
#endif

	SDL_Event event;

	while (SDL_PollEvent(&event)) {
#if CONTROLLER_INPUT_DIAG_TIMING
		ControllerInputDiagCountEvent(event, controllerInputDiagCounts);
#endif

		// SDL_PollEvent may modify FPU flags
		streflop::streflop_init<streflop::Simple>();
		PushEvent(event);
	}

#if CONTROLLER_INPUT_DIAG_TIMING
	ControllerInputDiagLogSlowPushEvents(controllerInputDiagStart, controllerInputDiagCounts);
#endif
}

InputHandler::HandlerTokenT InputHandler::AddHandler(InputHandler::HandlerFuncT func)
{
	for (size_t i = 0; i < eventHandlers.size(); ++i) {
		if (eventHandlers[i] == nullptr) {
			eventHandlers[i] = func;
			return InputHandler::HandlerTokenT{ *this, i};
		}
	}
	eventHandlers.emplace_back(func);
	return InputHandler::HandlerTokenT{ *this, eventHandlers.size() - 1 };
}

