#pragma once

#include "core/engine/cache/Cache.hpp"

class Triggerbot {
public:
    ~Triggerbot() = default;
    Triggerbot(const Triggerbot&) = delete;
    Triggerbot(Triggerbot&&) = delete;
    Triggerbot& operator=(const Triggerbot&) = delete;
    Triggerbot& operator=(Triggerbot&&) = delete;

    static bool Init();
    static void Run();

private:
    Triggerbot() {};

    static Triggerbot& GetInstance()
    {
        static Triggerbot i{};
        return i;
    }

    bool InitImpl();
    void RunImpl();

    bool ResolveCrosshairEntity(int32_t id, int& outTeam, int& outHealth);
    void Shoot();

private:
    bool wasShooting = false;
    std::chrono::steady_clock::time_point lastShot{};
    std::chrono::steady_clock::time_point holdStart{};
};
