#include "OffsetSync.hpp"
#include "Offsets.hpp"

#include "common.hpp"
#include "core/logger/LogHelper.hpp"

#include <Windows.h>
#include <winhttp.h>
#include <string>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace {
	bool HttpGet(const std::string& url, std::string& out) {
		out.clear();

		std::wstring wurl(url.begin(), url.end());

		URL_COMPONENTS uc{};
		uc.dwStructSize = sizeof(uc);
		wchar_t host[256]{};
		wchar_t path[1024]{};
		uc.lpszHostName = host;
		uc.dwHostNameLength = _countof(host);
		uc.lpszUrlPath = path;
		uc.dwUrlPathLength = _countof(path);

		if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc))
			return false;

		HINTERNET session = WinHttpOpen(L"RageNova/1.0",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session) return false;

		HINTERNET conn = WinHttpConnect(session, host, uc.nPort, 0);
		if (!conn) {
			WinHttpCloseHandle(session);
			return false;
		}

		DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
		HINTERNET req = WinHttpOpenRequest(conn, L"GET", path,
			nullptr, WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
		if (!req) {
			WinHttpCloseHandle(conn);
			WinHttpCloseHandle(session);
			return false;
		}

		bool ok = false;
		if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
			&& WinHttpReceiveResponse(req, nullptr)) {

			DWORD status = 0, sz = sizeof(status);
			WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);

			if (status == 200) {
				std::string body;
				for (;;) {
					DWORD avail = 0;
					if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0)
						break;
					std::string chunk(avail, '\0');
					DWORD read = 0;
					if (!WinHttpReadData(req, chunk.data(), avail, &read))
						break;
					chunk.resize(read);
					body += chunk;
				}
				out = std::move(body);
				ok = !out.empty();
			}
		}

		WinHttpCloseHandle(req);
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);
		return ok;
	}
}

bool OffsetSync::ApplyJson(const std::string& body) {
	try {
		auto j = nlohmann::json::parse(body);

		if (j.contains("build"))
			last_build = j["build"].is_number()
				? std::to_string(j["build"].get<int>())
				: j["build"].get<std::string>();

		if (j.contains("dwViewAngles")) {
			auto v = j["dwViewAngles"];
			offsets::viewAngles = v.is_string()
				? static_cast<DWORD>(std::stoul(v.get<std::string>(), nullptr, 0))
				: static_cast<DWORD>(v.get<uint64_t>());
		}

		if (j.contains("m_iIDEntIndex")) {
			auto v = j["m_iIDEntIndex"];
			offsets::pawn::m_iIDEntIndex = v.is_string()
				? static_cast<std::ptrdiff_t>(std::stoll(v.get<std::string>(), nullptr, 0))
				: static_cast<std::ptrdiff_t>(v.get<int64_t>());
		}

		if (j.contains("m_aimPunchAngle")) {
			auto v = j["m_aimPunchAngle"];
			offsets::pawn::m_aimPunchAngle = v.is_string()
				? static_cast<std::ptrdiff_t>(std::stoll(v.get<std::string>(), nullptr, 0))
				: static_cast<std::ptrdiff_t>(v.get<int64_t>());
		}

		LOGF(INFO, "[offsets] applied build={} viewAngles=0x{:X} m_iIDEntIndex=0x{:X}",
			last_build, offsets::viewAngles, offsets::pawn::m_iIDEntIndex);

		last_status = "ok " + last_build;
		last_error.clear();
		return true;
	}
	catch (const std::exception& e) {
		last_error = e.what();
		last_status = "parse error";
		LOGF(WARNING, "[offsets] parse failed: {}", e.what());
		return false;
	}
}

bool OffsetSync::FetchAndApply() {
	last_status = "fetching...";
	std::string body;
	if (!HttpGet(url, body)) {
		last_error = "http failed";
		last_status = "fetch failed";
		LOGF(WARNING, "[offsets] fetch failed from {}", url);
		return false;
	}
	return ApplyJson(body);
}
