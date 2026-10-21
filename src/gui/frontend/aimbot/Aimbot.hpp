#pragma once

#include "core/engine/cache/Cache.hpp"

class Aimbot {
public:
	~Aimbot() = default;
	Aimbot(const Aimbot&) = delete;
	Aimbot(Aimbot&&) = delete;
	Aimbot& operator=(const Aimbot&) = delete;
	Aimbot& operator=(Aimbot&&) = delete;

	static bool Init();
	static void Run();
	static void DrawFov();

private:
	Aimbot() = default;
	static Aimbot& Get() { static Aimbot i; return i; }

	bool InitImpl();
	void RunImpl();
	void DrawFovImpl();

	bool GetViewAngles(Vec3_t& out);
	bool SetViewAngles(const Vec3_t& angles);
	Vec3_t CalcAngle(const Vec3_t& src, const Vec3_t& dst);
	float GetFov(const Vec3_t& view, const Vec3_t& aim);
	Vec3_t SmoothAngle(const Vec3_t& current, const Vec3_t& target, float smooth);
	void NormalizeAngles(Vec3_t& a);
};
