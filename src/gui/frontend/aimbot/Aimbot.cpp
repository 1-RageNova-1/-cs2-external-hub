#include "Aimbot.hpp"

#include "core/engine/Engine.hpp"
#include "core/offsets/Offsets.hpp"
#include "core/engine/classes/Bones.hpp"
#include "config/Current.hpp"

#include <Windows.h>
#include <cmath>
#include <chrono>
#include <imgui/imgui.h>

using namespace std::chrono;

bool Aimbot::Init() {
	return Get().InitImpl();
}

void Aimbot::Run() {
	Get().RunImpl();
}

void Aimbot::DrawFov() {
	Get().DrawFovImpl();
}

bool Aimbot::InitImpl() {
	LOGF(INFO, "[aimbot] initialized");
	LOGF(INFO, "[aimbot] viewAngles offset = 0x{:X} ({})",
		offsets::viewAngles,
		offsets::viewAngles ? "OK" : "MISSING — aimbot will not work");
	LOGF(INFO, "[aimbot] defaults: fov={:.1f} smooth={:.1f} bone={} key=0x{:X} wallcheck={} team_check={}",
		cfg::aimbot::fov,
		cfg::aimbot::smooth,
		cfg::aimbot::bone,
		cfg::aimbot::key,
		cfg::aimbot::visible_only,
		cfg::aimbot::team_check);
	return true;
}

void Aimbot::NormalizeAngles(Vec3_t& a) {
	if (a.x > 89.f) a.x = 89.f;
	if (a.x < -89.f) a.x = -89.f;
	while (a.y > 180.f) a.y -= 360.f;
	while (a.y < -180.f) a.y += 360.f;
	a.z = 0.f;
}

Vec3_t Aimbot::CalcAngle(const Vec3_t& src, const Vec3_t& dst) {
	Vec3_t delta = { dst.x - src.x, dst.y - src.y, dst.z - src.z };
	return delta.ToAngle();
}

float Aimbot::GetFov(const Vec3_t& view, const Vec3_t& aim) {
	Vec3_t delta = { aim.x - view.x, aim.y - view.y, 0.f };
	NormalizeAngles(delta);
	return std::sqrt(delta.x * delta.x + delta.y * delta.y);
}

Vec3_t Aimbot::SmoothAngle(const Vec3_t& current, const Vec3_t& target, float smooth) {
	if (smooth <= 1.f)
		return target;

	Vec3_t delta = { target.x - current.x, target.y - current.y, 0.f };
	NormalizeAngles(delta);
	return {
		current.x + delta.x / smooth,
		current.y + delta.y / smooth,
		0.f
	};
}

bool Aimbot::GetViewAngles(Vec3_t& out) {
	if (!offsets::viewAngles)
		return false;
	auto p = Engine::GetProcess();
	auto client = Engine::GetClient();
	if (!p)
		return false;
	// CS2 stores viewangles as pitch,yaw only (Vec2) — same as Georg-S/CS2-AI
	float v[2]{};
	if (!p->read_raw(client.base + offsets::viewAngles, v, sizeof(v)))
		return false;
	out.x = v[0];
	out.y = v[1];
	out.z = 0.f;
	if (out.x < -89.f || out.x > 89.f || out.y < -180.f || out.y > 180.f)
		return false;
	return true;
}

bool Aimbot::SetViewAngles(const Vec3_t& angles) {
	if (!offsets::viewAngles)
		return false;
	auto p = Engine::GetProcess();
	auto client = Engine::GetClient();
	if (!p)
		return false;
	// pitch + yaw only (Georg-S/CS2-AI style)
	p->write<float>(client.base + offsets::viewAngles, angles.x);
	p->write<float>(client.base + offsets::viewAngles + sizeof(float), angles.y);
	return true;
}

static void AimDebug(const std::string& msg) {
	if (!cfg::aimbot::debug)
		return;
	static steady_clock::time_point last{};
	static std::string last_msg;
	const auto now = steady_clock::now();
	if (msg == last_msg && duration_cast<milliseconds>(now - last).count() < 500)
		return;
	last = now;
	last_msg = msg;
	LOGF(INFO, "{}", msg);
}


static bool SpottedByLocal(const Player& target, const Player& local) {
	// controller entity index slots are typically 1..64
	int slot = local.index + 1; // entity list index often 0-based controller slot
	if (slot < 0) slot = 0;
	// try both local.index and local.index+1 — CS2 mask uses player slot
	auto bit_set = [&](int s) -> bool {
		if (s < 0 || s >= 64) return false;
		return (target.spotted_mask[s / 32] & (1u << (s % 32))) != 0;
	};
	if (bit_set(local.index) || bit_set(local.index + 1))
		return true;
	// fallback: m_bSpotted only if mask empty (loading edge case)
	return false;
}

void Aimbot::RunImpl() {
	// silent when master or module off — no console spam
	if (!cfg::enabled || !cfg::aimbot::enabled)
		return;

	if (!offsets::viewAngles) {
		AimDebug("[aimbot] skip: viewAngles offset is 0 (signature failed)");
		return;
	}

	const int key = cfg::aimbot::key;
	const bool keyDown = (key == 0) ? true : (GetAsyncKeyState(key) & 0x8000);
	if (!keyDown)
		return;

	auto snap = Cache::CopySnapshot();
	auto& local = snap.local;
	auto& players = snap.players;

	if (local.index < 0 || !local.alive || local.pawn == 0)
		return;

	if (local.bone_list.empty()) {
		AimDebug("[aimbot] skip: local bone_list empty");
		return;
	}

	Vec3_t view{};
	if (!GetViewAngles(view)) {
		AimDebug("[aimbot] skip: failed to read view angles");
		return;
	}

	if (cfg::aimbot::debug) {
		static steady_clock::time_point last_view{};
		if (duration_cast<milliseconds>(steady_clock::now() - last_view).count() > 1000) {
			last_view = steady_clock::now();
			LOGF(INFO, "[aimbot] view=({:.1f}, {:.1f}) players={} local_bones={}",
				view.x, view.y, players.size(), local.bone_list.size());
		}
	}

	Vec3_t eye = local.bone_list[bone_index::head].pos;
	if (eye.zero())
		eye = local.pos;
	eye.z += 8.f;

	float best_fov = cfg::aimbot::fov;
	Vec3_t best_angle{};
	bool found = false;
	int scanned = 0;
	int skipped_team = 0;
	int skipped_wall = 0;
	int skipped_bones = 0;
	int skipped_fov = 0;
	const char* best_name = "?";

	const int bone_id = cfg::aimbot::bone;
	for (auto& player : players) {
		if (!player.alive || player.localplayer)
			continue;

		scanned++;

		if (cfg::aimbot::team_check && player.team == local.team) {
			skipped_team++;
			continue;
		}

		if (cfg::aimbot::visible_only && !SpottedByLocal(player, local)) {
			skipped_wall++;
			continue;
		}

		if (player.bone_list.empty()) {
			skipped_bones++;
			continue;
		}

		int idx = bone_id;
		if (idx < 0 || idx >= static_cast<int>(player.bone_list.size()))
			idx = bone_index::head;

		Vec3_t target = player.bone_list[idx].pos;
		if (target.zero()) {
			skipped_bones++;
			continue;
		}

		Vec3_t aim = CalcAngle(eye, target);
		NormalizeAngles(aim);

		float fov = GetFov(view, aim);
		if (fov < best_fov) {
			best_fov = fov;
			best_angle = aim;
			found = true;
			best_name = player.name;
		}
		else {
			skipped_fov++;
		}
	}

	if (!found) {
		if (cfg::aimbot::debug) {
			static steady_clock::time_point last_miss{};
			if (duration_cast<milliseconds>(steady_clock::now() - last_miss).count() > 1000) {
				last_miss = steady_clock::now();
				LOGF(INFO,
					"[aimbot] no target | scanned={} team_skip={} wall_skip={} bone_skip={} fov_skip={} fov_limit={:.1f}",
					scanned, skipped_team, skipped_wall, skipped_bones, skipped_fov, cfg::aimbot::fov);
			}
		}
		return;
	}

	if (cfg::aimbot::rcs && local.pawn) {
		auto p = Engine::GetProcess();
		if (p) {
			Vec3_t punch = p->read<Vec3_t>(local.pawn + offsets::pawn::m_aimPunchAngle);
			best_angle.x -= punch.x * cfg::aimbot::rcs_scale;
			best_angle.y -= punch.y * cfg::aimbot::rcs_scale;
			NormalizeAngles(best_angle);
			if (cfg::aimbot::debug) {
				static steady_clock::time_point last_rcs{};
				if (duration_cast<milliseconds>(steady_clock::now() - last_rcs).count() > 1000) {
					last_rcs = steady_clock::now();
					LOGF(INFO, "[aimbot] rcs punch=({:.2f}, {:.2f})", punch.x, punch.y);
				}
			}
		}
	}

	Vec3_t delta = { best_angle.x - view.x, best_angle.y - view.y, 0.f };
	NormalizeAngles(delta);

	if (cfg::aimbot::mouse_aim) {
		// convert angle delta to mouse pixels (sensitivity-ish scale)
		float smooth = cfg::aimbot::smooth < 1.f ? 1.f : cfg::aimbot::smooth;
		float sens = 0.022f; // matches CS default m_yaw-ish feel
		float dx = -delta.y / (sens * smooth);
		float dy = delta.x / (sens * smooth);

		// clamp one step so it doesn't teleport
		if (dx > 80.f) dx = 80.f;
		if (dx < -80.f) dx = -80.f;
		if (dy > 80.f) dy = 80.f;
		if (dy < -80.f) dy = -80.f;

		if (static_cast<int>(dx) != 0 || static_cast<int>(dy) != 0)
			mouse_event(MOUSEEVENTF_MOVE, static_cast<DWORD>(dx), static_cast<DWORD>(dy), 0, 0);

		if (cfg::aimbot::debug) {
			static steady_clock::time_point last_hit{};
			if (duration_cast<milliseconds>(steady_clock::now() - last_hit).count() > 250) {
				last_hit = steady_clock::now();
				LOGF(INFO,
					"[aimbot] LOCK '{}' fov={:.2f} mouse dx={:.1f} dy={:.1f} delta=({:.1f},{:.1f})",
					best_name, best_fov, dx, dy, delta.x, delta.y);
			}
		}
		return;
	}

	Vec3_t final_ang = SmoothAngle(view, best_angle, cfg::aimbot::smooth);
	NormalizeAngles(final_ang);

	if (!SetViewAngles(final_ang)) {
		AimDebug("[aimbot] FAILED to write view angles");
		return;
	}

	if (cfg::aimbot::debug) {
		static steady_clock::time_point last_hit{};
		if (duration_cast<milliseconds>(steady_clock::now() - last_hit).count() > 250) {
			last_hit = steady_clock::now();
			LOGF(INFO,
				"[aimbot] LOCK '{}' fov={:.2f} aim=({:.1f},{:.1f}) -> write=({:.1f},{:.1f}) smooth={:.1f}",
				best_name, best_fov, best_angle.x, best_angle.y, final_ang.x, final_ang.y, cfg::aimbot::smooth);
		}
	}
}

void Aimbot::DrawFovImpl() {
	if (!cfg::enabled || !cfg::aimbot::enabled || !cfg::aimbot::draw_fov)
		return;

	const ImGuiIO& io = ImGui::GetIO();
	const ImVec2 c(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
	const float radius = (cfg::aimbot::fov / 90.f) * (io.DisplaySize.y * 0.5f);

	ImGui::GetBackgroundDrawList()->AddCircle(
		c,
		radius,
		IM_COL32(180, 80, 255, 120),
		64,
		1.5f
	);
}
