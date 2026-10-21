#include "Splash.hpp"

#include "assets/fonts/SplashBanner.h"
#include "common.hpp"

#include <imgui/imgui.h>
#include <wincodec.h>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")

static bool LoadPngWIC(
	ID3D11Device* device,
	const unsigned char* data,
	size_t size,
	ID3D11ShaderResourceView** out_srv,
	int* out_w,
	int* out_h)
{
	*out_srv = nullptr;
	*out_w = *out_h = 0;

	IWICImagingFactory* factory = nullptr;
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) || !factory)
		return false;

	IWICStream* stream = nullptr;
	if (FAILED(factory->CreateStream(&stream))) {
		factory->Release();
		return false;
	}

	if (FAILED(stream->InitializeFromMemory(const_cast<unsigned char*>(data), static_cast<DWORD>(size)))) {
		stream->Release();
		factory->Release();
		return false;
	}

	IWICBitmapDecoder* decoder = nullptr;
	if (FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder))) {
		stream->Release();
		factory->Release();
		return false;
	}

	IWICBitmapFrameDecode* frame = nullptr;
	if (FAILED(decoder->GetFrame(0, &frame))) {
		decoder->Release();
		stream->Release();
		factory->Release();
		return false;
	}

	IWICFormatConverter* converter = nullptr;
	if (FAILED(factory->CreateFormatConverter(&converter))) {
		frame->Release();
		decoder->Release();
		stream->Release();
		factory->Release();
		return false;
	}

	if (FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
		converter->Release();
		frame->Release();
		decoder->Release();
		stream->Release();
		factory->Release();
		return false;
	}

	UINT w = 0, h = 0;
	converter->GetSize(&w, &h);
	const UINT stride = w * 4;
	const UINT buf_size = stride * h;
	std::vector<unsigned char> pixels(buf_size);
	if (FAILED(converter->CopyPixels(nullptr, stride, buf_size, pixels.data()))) {
		converter->Release();
		frame->Release();
		decoder->Release();
		stream->Release();
		factory->Release();
		return false;
	}

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = w;
	desc.Height = h;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA sub{};
	sub.pSysMem = pixels.data();
	sub.SysMemPitch = stride;

	ID3D11Texture2D* tex = nullptr;
	HRESULT hr = device->CreateTexture2D(&desc, &sub, &tex);
	converter->Release();
	frame->Release();
	decoder->Release();
	stream->Release();
	factory->Release();
	if (FAILED(hr) || !tex)
		return false;

	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
	srv_desc.Format = desc.Format;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = 1;

	hr = device->CreateShaderResourceView(tex, &srv_desc, out_srv);
	tex->Release();
	if (FAILED(hr))
		return false;

	*out_w = static_cast<int>(w);
	*out_h = static_cast<int>(h);
	return true;
}

bool Splash::Init(ID3D11Device* device) {
	return Get().InitImpl(device);
}

void Splash::Shutdown() {
	Get().ShutdownImpl();
}

bool Splash::Render() {
	return Get().RenderImpl();
}

bool Splash::IsActive() {
	return Get().active;
}

bool Splash::InitImpl(ID3D11Device* device) {
	if (!device)
		return false;

	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	texture_ok = LoadPngWIC(device, splash_banner_png, splash_banner_len, &srv, &tex_w, &tex_h);
	if (!texture_ok) {
		active = false;
		return false;
	}

	start = std::chrono::steady_clock::now();
	active = true;
	return true;
}

void Splash::ShutdownImpl() {
	if (srv) {
		srv->Release();
		srv = nullptr;
	}
	active = false;
	texture_ok = false;
}

bool Splash::RenderImpl() {
	if (!active)
		return false;

	using namespace std::chrono;
	const float t = duration_cast<duration<float>>(steady_clock::now() - start).count();

	float alpha = 1.f;
	if (t < 0.4f)
		alpha = t / 0.4f;
	else if (t > duration_sec - 0.8f)
		alpha = (duration_sec - t) / 0.8f;
	if (alpha < 0.f) alpha = 0.f;
	if (alpha > 1.f) alpha = 1.f;

	if (t >= duration_sec) {
		active = false;
		ShutdownImpl();
		return false;
	}

	const ImGuiIO& io = ImGui::GetIO();
	const ImVec2 screen = io.DisplaySize;
	ImDrawList* dl = ImGui::GetForegroundDrawList();

	dl->AddRectFilled(ImVec2(0, 0), screen, IM_COL32(4, 0, 12, static_cast<int>(255 * alpha)));

	if (texture_ok && srv) {
		const float max_w = screen.x * 0.82f;
		const float max_h = screen.y * 0.72f;
		float scale = max_w / static_cast<float>(tex_w);
		if (tex_h * scale > max_h)
			scale = max_h / static_cast<float>(tex_h);

		const float draw_w = tex_w * scale;
		const float draw_h = tex_h * scale;
		const ImVec2 p0((screen.x - draw_w) * 0.5f, (screen.y - draw_h) * 0.5f - screen.y * 0.02f);
		const ImVec2 p1(p0.x + draw_w, p0.y + draw_h);

		const float glow = 28.f;
		dl->AddRectFilled(
			ImVec2(p0.x - glow, p0.y - glow),
			ImVec2(p1.x + glow, p1.y + glow),
			IM_COL32(120, 40, 255, static_cast<int>(40 * alpha)),
			16.f);

		dl->AddImage(
			reinterpret_cast<ImTextureID>(srv),
			p0, p1,
			ImVec2(0, 0), ImVec2(1, 1),
			IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)));
	}

	const float bar_w = screen.x * 0.28f;
	const float bar_h = 4.f;
	const ImVec2 bar_pos((screen.x - bar_w) * 0.5f, screen.y * 0.88f);
	const float progress = t / duration_sec < 1.f ? t / duration_sec : 1.f;

	dl->AddRectFilled(bar_pos, ImVec2(bar_pos.x + bar_w, bar_pos.y + bar_h), IM_COL32(40, 20, 70, static_cast<int>(200 * alpha)), 2.f);
	dl->AddRectFilled(bar_pos, ImVec2(bar_pos.x + bar_w * progress, bar_pos.y + bar_h), IM_COL32(180, 80, 255, static_cast<int>(255 * alpha)), 2.f);

	const char* status = "INITIALIZING";
	if (progress > 0.35f) status = "LOADING MODULES";
	if (progress > 0.65f) status = "READY";
	if (progress > 0.90f) status = "LAUNCHING";

	const ImVec2 ts = ImGui::CalcTextSize(status);
	dl->AddText(
		ImVec2((screen.x - ts.x) * 0.5f, bar_pos.y + 14.f),
		IM_COL32(200, 160, 255, static_cast<int>(220 * alpha)),
		status);

	return true;
}
