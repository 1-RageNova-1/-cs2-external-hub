#include "Triggerbot.hpp"

#include "core/engine/Engine.hpp"
#include "core/offsets/Offsets.hpp"
#include "config/Current.hpp"

#include <Windows.h>
#include <chrono>
#include <thread>

using namespace std::chrono;

bool Triggerbot::Init() {
    return GetInstance().InitImpl();
}

void Triggerbot::Run() {
    GetInstance().RunImpl();
}

bool Triggerbot::InitImpl() {
    LOGF(INFO, "Triggerbot initialized");
    return true;
}

bool Triggerbot::ResolveCrosshairEntity(int32_t id, int& outTeam, int& outHealth) {
    if (id <= 0)
        return false;

    auto p = Engine::GetProcess();
    if (!p)
        return false;

    auto snap = Cache::CopySnapshot();
    if (!snap.game.entity_list)
        return false;

    // Entity list walk identical to Player::GetPawn style
    const uint32_t handle = static_cast<uint32_t>(id);
    const uintptr_t listEntry = p->read<uintptr_t>(
        snap.game.entity_list + 0x10 + 0x8 * ((handle & 0x7FFF) >> 9)
    );
    if (!listEntry)
        return false;

    const uintptr_t pawn = p->read<uintptr_t>(
        listEntry + 0x70 * (handle & 0x1FF)
    );
    if (!pawn)
        return false;

    outHealth = p->read<int>(pawn + offsets::pawn::m_iHealth);
    outTeam   = p->read<uint8_t>(pawn + offsets::pawn::m_iTeamNum);

    return outHealth > 0 && outHealth <= 100;
}

void Triggerbot::Shoot() {
    // External mouse click – no injection into game process
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    std::this_thread::sleep_for(milliseconds(cfg::triggerbot::shot_duration_ms));
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}

void Triggerbot::RunImpl() {
    if (!cfg::enabled || !cfg::triggerbot::enabled)
        return;

    // Hold key (default: mouse side / XBUTTON2 = VK_XBUTTON2, or configurable)
    const int key = cfg::triggerbot::key;
    const bool keyDown = (key == 0) ? true : (GetAsyncKeyState(key) & 0x8000);

    if (!keyDown) {
        wasShooting = false;
        return;
    }

    auto p = Engine::GetProcess();
    if (!p)
        return;

    auto snap = Cache::CopySnapshot();
    if (snap.local.index < 0 || !snap.local.alive || snap.local.pawn == 0)
        return;

    // Read crosshair entity index from local pawn
    const int32_t crosshairId = p->read<int32_t>(
        snap.local.pawn + offsets::pawn::m_iIDEntIndex
    );

    if (crosshairId == -1 || crosshairId == 0)
        return;

    int team = 0, health = 0;
    if (!ResolveCrosshairEntity(crosshairId, team, health))
        return;

    // Team check
    if (cfg::triggerbot::team_check && team == snap.local.team)
        return;

    // Delay between shots
    const auto now = steady_clock::now();
    if (duration_cast<milliseconds>(now - lastShot).count() < cfg::triggerbot::delay_ms)
        return;

    // Optional: do not fire if already holding LMB
    if (cfg::triggerbot::ignore_already_shooting && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
        return;

    Shoot();
    lastShot = now;
    wasShooting = true;
}
