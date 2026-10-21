#pragma once

#include <chrono>
#include <d3d11.h>

class Splash {
public:
	static bool Init(ID3D11Device* device);
	static void Shutdown();
	static bool Render();
	static bool IsActive();

private:
	Splash() = default;
	static Splash& Get() { static Splash s; return s; }

	bool InitImpl(ID3D11Device* device);
	bool RenderImpl();
	void ShutdownImpl();

	ID3D11ShaderResourceView* srv = nullptr;
	int tex_w = 0;
	int tex_h = 0;
	bool active = false;
	bool texture_ok = false;
	std::chrono::steady_clock::time_point start{};
	float duration_sec = 3.2f;
};
