/*
 * RageNova - https://github.com/1-RageNova-1/-cs2-external-hub
 */

#include <iostream>

#include "core/engine/Engine.hpp"
#include "gui/renderer/Renderer.hpp"

#include <external/exception.hpp>

int main()
{
    c_exception_handler::setup();

    LogHelper::Init();

    LOGF(INFO, "Compiled {}, Welcome to RageNova!", __TIMESTAMP__);

    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
        LOGF(WARNING, "Could not set application process priority to HIGH");

    if (!Engine::Init()) {
        LOGF(FATAL, "Engine failed to initialize, cannot continue execution");
        goto exit;
    }

    if (!Renderer::Init()) {
        LOGF(FATAL, "Renderer failed to initialize, cannot continue execution");
        goto exit;
    }

    LOGF(INFO, "Everything setup and ready, just... make sure you are not in \"Full Screen\"!");

    Renderer::Thread();

exit:
    LOGF(INFO, "[unload] cleanup complete — process exiting");
    LogHelper::Destroy();
}
