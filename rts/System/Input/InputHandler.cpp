/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "InputHandler.h"

#include "System/Log/ILog.h"
#include "System/TimeProfiler.h"

#include <SDL_error.h>

InputHandler input;

#ifndef CONTROLLER_INPUT_SINGLE_PUMP_EVENT_POLL
#define CONTROLLER_INPUT_SINGLE_PUMP_EVENT_POLL 1
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

	SDL_Event event;

#if CONTROLLER_INPUT_SINGLE_PUMP_EVENT_POLL
	static bool loggedSinglePumpPath = false;
	if (!loggedSinglePumpPath) {
		loggedSinglePumpPath = true;
		LOG_L(L_INFO, "[ControllerStutterFix01] Single-pump SDL event path active for Stutter Fix attempt-02");
	}

	SDL_PumpEvents();
	// SDL_PumpEvents may modify FPU flags even when no events are returned.
	streflop::streflop_init<streflop::Simple>();

	while (true) {
		const int eventCount = SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
		if (eventCount <= 0) {
			if (eventCount < 0) {
				static bool loggedPeepError = false;
				if (!loggedPeepError) {
					loggedPeepError = true;
					LOG_L(L_WARNING, "[ControllerStutterFix01] SDL_PeepEvents failed while polling input events: %s", SDL_GetError());
				}
			}
			break;
		}

		// SDL_PumpEvents may modify FPU flags
		streflop::streflop_init<streflop::Simple>();
		PushEvent(event);
	}
#else
	while (SDL_PollEvent(&event)) {
		// SDL_PollEvent may modify FPU flags
		streflop::streflop_init<streflop::Simple>();
		PushEvent(event);
	}
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
