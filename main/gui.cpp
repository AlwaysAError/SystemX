#include "gui.h"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"
#include <Windows.h>
#include <iphlpapi.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <string>
#include <vector>
#include <codecvt>
#include <filesystem>
#include <tlhelp32.h>
#include <iostream>
#include <shlobj.h>
#include <cstdlib> // For system()
#include <setupapi.h>
#include <cfgmgr32.h>
#include <iomanip>
#include <random>
#include <sstream>
#include <psapi.h>
#include <powrprof.h> // For battery information
#include <timezoneapi.h> // For GetTimeZoneInformation
#include <oleauto.h> // For SafeArray functions
#include <rpc.h> // For RPC_S_ALREADY_INITIALIZED



#define _WIN32_WINNT 0x0600 // Ensure Windows Vista or later APIs



// Link necessary libraries
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "advapi32.lib") // For registry functions
#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "powrprof.lib")



//-------------------------------------------------------------------------------------------------------------------------------------------
//IP Config And DNS Changer/Fixer
//-------------------------------------------------------------------------------------------------------------------------------------------



// Function to execute network reset commands
void RunNetworkReset() {
	// Array of commands (Windows-specific)
	const char* commands[] = {
		"netsh winsock reset",
		"netsh int ip reset",
		"netsh advfirewall reset",
		"ipconfig /flushdns",
		"ipconfig /release",
		"ipconfig /renew"
	};

	// Execute each command
	for (const auto& cmd : commands) {
		// system() returns 0 on success, non-zero on failure
		int result = system(cmd);
		if (result != 0) {
			// Handle error (e.g., log to console or show in UI)
			// For simplicity, we continue with other commands
		}
	}
}



//-------------------------------------------------------------------------------------------------------------------------------------------
//MachineGUID Changer
//-------------------------------------------------------------------------------------------------------------------------------------------



// Function to generate a random GUID
std::wstring GenerateRandomGUID() {
	char guid[40];
	sprintf_s(guid, "{%08X-%04X-%04X-%04X-%012X}",
		rand() % 0xFFFFFFFF, rand() % 0xFFFF, rand() % 0xFFFF,
		rand() % 0xFFFF, rand() % 0xFFFFFFFFFFF);
	return std::wstring(guid, guid + strlen(guid));
}

// Function to get the current MachineGuid
std::wstring GetMachineGuid() {
	HKEY hKey;
	wchar_t buffer[256];
	DWORD size = sizeof(buffer);
	std::wstring result = L"Unknown";

	LONG res = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &hKey);
	if (res == ERROR_SUCCESS) {
		res = RegQueryValueExW(hKey, L"MachineGuid", nullptr, nullptr, (LPBYTE)buffer, &size);
		if (res == ERROR_SUCCESS) {
			result = buffer;
		}
		RegCloseKey(hKey);
	}
	return result;
}

// Function to set a new MachineGuid
std::wstring SetMachineGuid(const std::wstring& newGuid) {
	HKEY hKey;
	LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &hKey);
	if (result != ERROR_SUCCESS) {
		return L"Failed to open registry key. Error: " + std::to_wstring(result) + L". Run as Administrator.";
	}

	result = RegSetValueExW(hKey, L"MachineGuid", 0, REG_SZ, (const BYTE*)newGuid.c_str(), (newGuid.size() + 1) * sizeof(wchar_t));
	RegCloseKey(hKey);
	if (result != ERROR_SUCCESS) {
		return L"Failed to set MachineGuid. Error: " + std::to_wstring(result);
	}
	return L"Successfully set MachineGuid to: " + newGuid;
}

std::wstring currentGuid = GetMachineGuid();
std::wstring statusMessage = L"Ready";



//------------------------------------------------------------------------------------------------------------------------------------------
//GPU HWID Changer
//------------------------------------------------------------------------------------------------------------------------------------------



// Function to generate a random HWID in PCI format
std::string GenerateRandomHWID() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> hexDist(0, 15); // For hexadecimal digits (0-F)

	auto hexChar = [](int n) -> char {
		return n < 10 ? '0' + n : 'A' + (n - 10);
		};

	std::stringstream ss;
	ss << "PCI\\VEN_";

	// Generate 4-digit Vendor ID
	for (int i = 0; i < 4; ++i) {
		ss << hexChar(hexDist(gen));
	}
	ss << "&DEV_";

	// Generate 4-digit Device ID
	for (int i = 0; i < 4; ++i) {
		ss << hexChar(hexDist(gen));
	}
	ss << "&SUBSYS_";

	// Generate 8-digit Subsystem ID
	for (int i = 0; i < 8; ++i) {
		ss << hexChar(hexDist(gen));
	}
	ss << "&REV_";

	// Generate 2-digit Revision ID
	for (int i = 0; i < 2; ++i) {
		ss << hexChar(hexDist(gen));
	}

	std::string hwid = ss.str();
	// Cap length at 50 characters to prevent registry issues
	if (hwid.length() > 50) {
		hwid = hwid.substr(0, 50);
	}
	return hwid;
}

// Function to find the registry path for the first display adapter
bool FindGPURegistryPath(std::wstring& registryPath, std::string& deviceID) {
	// GUID for display adapters
	const GUID displayGUID = { 0x4d36e968, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };

	// Get device information set for display adapters
	HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&displayGUID, NULL, NULL, DIGCF_PRESENT);
	if (deviceInfoSet == INVALID_HANDLE_VALUE) {
		return false;
	}

	SP_DEVINFO_DATA deviceInfoData;
	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	// Enumerate devices
	for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); ++i) {
		// Get device ID
		wchar_t deviceIDBuffer[256];
		if (CM_Get_Device_IDW(deviceInfoData.DevInst, deviceIDBuffer, sizeof(deviceIDBuffer) / sizeof(wchar_t), 0) == CR_SUCCESS) {
			// Convert device ID to string
			deviceID = std::string(deviceIDBuffer, deviceIDBuffer + wcslen(deviceIDBuffer));

			// Construct registry path
			registryPath = L"SYSTEM\\CurrentControlSet\\Enum\\" + std::wstring(deviceIDBuffer);
			SetupDiDestroyDeviceInfoList(deviceInfoSet);
			return true; // Return first valid GPU
		}
	}

	SetupDiDestroyDeviceInfoList(deviceInfoSet);
	return false; // No GPU found
}

// Function to spoof GPU HWID in the registry
bool SpoofGPU(const std::wstring& registryPath, const std::string& newHardwareID) {
	HKEY hKey;

	// Open registry key with write access
	LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, registryPath.c_str(), 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);
	if (result != ERROR_SUCCESS) {
		return false; // Failed to open key
	}

	// Prepare values
	const wchar_t* classGUID = L"{4d36e968-e325-11ce-bfc1-08002be10318}";
	const wchar_t* className = L"Display";
	const wchar_t* driver = L"pci.sys";
	DWORD configFlags = 0x00000000;
	std::wstring wideNewHardwareID(newHardwareID.begin(), newHardwareID.end());

	// Set HardwareID
	result = RegSetValueExW(hKey, L"HardwareID", 0, REG_SZ,
		(const BYTE*)wideNewHardwareID.c_str(),
		(wideNewHardwareID.size() + 1) * sizeof(wchar_t));
	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	// Set CompatibleIDs (as multi-string)
	result = RegSetValueExW(hKey, L"CompatibleIDs", 0, REG_MULTI_SZ,
		(const BYTE*)wideNewHardwareID.c_str(),
		(wideNewHardwareID.size() + 2) * sizeof(wchar_t)); // +2 for double null-termination
	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	// Set Driver
	result = RegSetValueExW(hKey, L"Driver", 0, REG_SZ,
		(const BYTE*)driver,
		(wcslen(driver) + 1) * sizeof(wchar_t));
	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	// Set ConfigFlags
	result = RegSetValueExW(hKey, L"ConfigFlags", 0, REG_DWORD,
		(const BYTE*)&configFlags, sizeof(DWORD));
	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	// Set ClassGUID
	result = RegSetValueExW(hKey, L"ClassGUID", 0, REG_SZ,
		(const BYTE*)classGUID,
		(wcslen(classGUID) + 1) * sizeof(wchar_t));
	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	// Set Class
	result = RegSetValueExW(hKey, L"Class", 0, REG_SZ,
		(const BYTE*)className,
		(wcslen(className) + 1) * sizeof(wchar_t));
	if (result != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return false;
	}

	RegCloseKey(hKey);
	return true;
}

// Function to get current HWID from registry
std::string GetCurrentHWID(const std::wstring& registryPath) {
	HKEY hKey;
	std::string hwid = "Unknown";

	LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, registryPath.c_str(), 0, KEY_QUERY_VALUE, &hKey);
	if (result == ERROR_SUCCESS) {
		wchar_t buffer[256];
		DWORD bufferSize = sizeof(buffer);
		DWORD type;
		result = RegQueryValueExW(hKey, L"HardwareID", 0, &type, (BYTE*)buffer, &bufferSize);
		if (result == ERROR_SUCCESS && type == REG_SZ) {
			hwid = std::string(buffer, buffer + bufferSize / sizeof(wchar_t) - 1);
		}
		RegCloseKey(hKey);
	}
	return hwid;
}

// Variables for ImGui
std::wstring registryPath;
std::string deviceID;
bool gpuFound = FindGPURegistryPath(registryPath, deviceID);
std::string currentHWID = gpuFound ? GetCurrentHWID(registryPath) : "No GPU found";
std::string newHWID = GenerateRandomHWID(); // Auto-generate HWID on startup
std::string appliedHWID = ""; // Store HWID after applying
bool showMessage = false;
std::string message;



//-------------------------------------------------------------------------------------------------------------------------------------------
//ImGui Handling And Functions
//-------------------------------------------------------------------------------------------------------------------------------------------



extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);

long __stdcall WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: {
		if (gui::device && wideParameter != SIZE_MINIMIZED)
		{
			gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
			gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
			gui::ResetDevice();
		}
	}return 0;

	case WM_SYSCOMMAND: {
		if ((wideParameter & 0xfff0) == SC_KEYMENU)
			return 0;
	}break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN: {
		gui::position = MAKEPOINTS(longParameter);
	}return 0;

	case WM_MOUSEMOVE: {
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{ };

			GetWindowRect(gui::window, &rect);

			rect.left += points.x - gui::position.x;
			rect.top += points.y - gui::position.y;

			if (gui::position.x >= 0 &&
				gui::position.x <= gui::WIDTH &&
				gui::position.y >= 0 && gui::position.y <= 19)
				SetWindowPos(
					gui::window,
					HWND_TOPMOST,
					rect.left,
					rect.top,
					0, 0,
					SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
				);
		}

	}return 0;

	}

	return DefWindowProc(window, message, wideParameter, longParameter);
}

void gui::CreateHWindow(const char* windowName) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = "class001";
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		0,
		"class001",
		windowName,
		WS_POPUP,
		100,
		100,
		WIDTH,
		HEIGHT,
		0,
		0,
		windowClass.hInstance,
		0
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}

void gui::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}

void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void gui::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}

void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}

void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void gui::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			isRunning = !isRunning;
			return;
		}
	}

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}

void gui::Render() noexcept
{
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize({ WIDTH, HEIGHT });
	ImGui::Begin(
		"System X https://github.com/AlwaysAError/SystemX",
		&isRunning,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoMove
	);



	//-------------------------------------------------------------------------------------------------------------------------------------------
	//ImGui Popup Function On Start Up
	//-------------------------------------------------------------------------------------------------------------------------------------------



	// ImGui pop-up logic
	static bool show_popuprizz = true; // Show on startup
	if (show_popuprizz) {
		ImGui::OpenPopup("WARNING");
	}

	if (ImGui::BeginPopupModal("WARNING", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("This application can change Windows settings, info and more.");
		ImGui::Text("We are not responsible for any damages you may obtain using this software");
		ImGui::Separator();
		if (ImGui::Button("Continue", ImVec2(120, 0))) {
			show_popuprizz = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}



	//-------------------------------------------------------------------------------------------------------------------------------------------
	//Tab 1 ImGui Elemnts
	//-------------------------------------------------------------------------------------------------------------------------------------------



	// Rest of your tabbed interface (unchanged)
	if (ImGui::BeginTabBar("MainTabBar")) {
		// ... (your existing tab code remains unchanged)
	}


	//Credits Tab
	if (ImGui::BeginTabItem("Credits")) {
		ImGui::Text("   ");
		ImGui::Text("Hey there, thanks for using System X. System X was created by https://github.com/AlwaysAError to help manage your system");
		ImGui::Text("If you encounter any bugs or have any ideas please leave them on the GitHub page");

		ImGui::EndTabItem();
	}

	//Spoofing Tab
	if (ImGui::BeginTabItem("Spoofing")) {
		ImGui::Text("   ");


		if (ImGui::Button("Spoof MachineGuid")) {
			std::wstring newGuid = GenerateRandomGUID();
			statusMessage = SetMachineGuid(newGuid);
			currentGuid = GetMachineGuid(); // Refresh displayed GUID
		}
		ImGui::Text("Current MachineGuid: %ls", currentGuid.c_str());
		ImGui::Separator();

		// Button to trigger network reset
		if (ImGui::Button("Clean IP and DNS")) {
			RunNetworkReset();
		}
		ImGui::Text("After pressing the Clean IP and DNS button please wait 10 seconds and ignore any CMD pop ups");
		ImGui::Separator();


		ImGui::Text("Current GPU HWID: %s", currentHWID.c_str());

		// Button to generate new HWID
		if (ImGui::Button("Generate New HWID")) {
			newHWID = GenerateRandomHWID(); // Generate new random HWID
		}
		ImGui::Text("Generated HWID: %s", newHWID.c_str());

		// Button to spoof HWID
		if (ImGui::Button("Spoof HWID")) {
			if (!gpuFound) {
				message = "No GPU found. Cannot spoof HWID.";
				showMessage = true;
				appliedHWID = "";
			}
			else if (SpoofGPU(registryPath, newHWID)) {
				message = "Registry HWID spoofed!";
				currentHWID = GetCurrentHWID(registryPath); // Refresh current HWID
				appliedHWID = currentHWID; // Store applied HWID
			}
			else {
				message = "Failed to spoof HWID. Ensure admin privileges.";
				appliedHWID = "";
			}
			showMessage = true;
		}

		ImGui::Separator();

		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
		ImGui::EndTabItem();
	}

	ImGui::End();
}

//-------------------------------------------------------------------------------------------------------------------------------------------