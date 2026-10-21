#include "Updater.hpp"

bool Updater::Init() {
	return GetInstance().InitImpl();
}

bool Updater::Process() {
	return GetInstance().ProcessImpl();
}

Status Updater::GetStatus() {
	return GetInstance().status;
}

bool Updater::InitImpl() {
	json response;
	auto http_status = HttpHelper::Get(this->status_url, response);

	// Soft-fail: missing status.json on a fork must never block startup
	if (http_status != 200) {
		LOGF(WARNING, "Updater status unavailable (code {}), continuing without remote status", http_status);
		isSetup = true;
		status.unsafe = false;
		status.notice = "";
		status.version_current = current_version;
		status.version_minimum = current_version;
		LOGF(INFO, "Successfully initialized updater (offline)...");
		return true;
	}

	isSetup = true;

	try {
		status.unsafe = response.value("unsafe", false);
		status.notice = response.value("notice", "");

		status.version_current = response["version"].value("current", 0);
		status.version_minimum = response["version"].value("minimum", 0);
	}
	catch (std::exception& e) {
		LOGF(VERBOSE, e.what());
		LOGF(WARNING, "Failed to parse status json, continuing with defaults");
		status.unsafe = false;
		status.notice = "";
		status.version_current = current_version;
		status.version_minimum = current_version;
		LOGF(INFO, "Successfully initialized updater (offline)...");
		return true;
	}

	if (status.unsafe)
		LOGF(WARNING, "This application has been marked as \"Unsafe\" to use, its not recommended to proceed");
	else if (current_version < status.version_minimum)
		LOGF(WARNING, "This application is \"Out Of Date\", and might not work as expected");
	else if (status.version_current > current_version)
		LOGF(WARNING, "There is a newer version available, current version is {} and github version is {}", current_version, status.version_current);
	else if (status.version_current != current_version)
		LOGF(WARNING, "Version missmatch detected, current version is {} and github version is {}", current_version, status.version_current);
	else if (!status.notice.empty())
		LOGF(WARNING, "Developer notice: {}", status.notice);

	LOGF(INFO, "Successfully initialized updater...");
	return true;
}

bool Updater::ProcessImpl() {
	if (!isSetup) {
		LOGF(FATAL, "Please, call Init() function before calling Process() and make sure it returns ok");
		return false;
	}

	if (status.unsafe) {
		auto result = MessageBox(
			NULL, 
			"This application has been marked as \"Unsafe\" to use, its not recommended to proceed.\nDo you want to continue?"
			"\n\nMore information available at: https://github.com/1-RageNova-1/-cs2-external-hub"
			"\n\nCtrl+C To copy this message",
			"Unsafe | Warning", MB_ICONWARNING | MB_YESNO
		);

		if (result != IDYES) {
			LOGF(VERBOSE, "User has opted to close application after it was warned about it been unsafe");
			return false;
		}
		
		LOGF(WARNING, "User has opted to continue, even after been warned that the application is not safe to use");
		return true;
	}
	
	if (current_version < status.version_minimum) {
		auto result = MessageBox(
			NULL,
			"This application is \"out-of-date\", and might not work as expected.\nDo you want to continue?"
			"\n\nMore information available at: https://github.com/1-RageNova-1/-cs2-external-hub"
			"\n\nCtrl+C To copy this message",
			"Out Of Date | Warning", MB_ICONWARNING | MB_YESNO
		);

		if (result != IDYES) {
			LOGF(VERBOSE, "User has opted to close application after it was warned about it been \"out-of-date\"");
			return false;
		}

		LOGF(WARNING, "User has opted to continue, even after been warned that the application is \"out-of-date\"");
		return true;
	}

	return true;
}