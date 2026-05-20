\# Recoil Controller API Investigation



Date: 2026-05-21  

Branch: `research/controller-api-investigation`



\## Context



A BAR LuaUI diagnostics widget was added in the BAR game repo to test whether controller APIs are available through `Spring.\*`.



BAR-side diagnostic result:



```text

\[ControllerDiag], Controller API incomplete or unavailable.

\[ControllerDiag], GetAvailableControllers:, no

\[ControllerDiag], ConnectController:, no

\[ControllerDiag], DisconnectController:, no

\[ControllerDiag], GetControllerState:, no


## Input/Lua event pipeline findings

Additional grep results show the expected engine integration path.

### SDL event pump

`InputHandler::PushEvents()` polls SDL events:

```text
rts/System/Input/InputHandler.cpp:26
while (SDL_PollEvent(&event)) {