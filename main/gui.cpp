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
#include <cstdlib>
#include <setupapi.h>
#include <array>
#include <cfgmgr32.h>
#include <iomanip>
#include <random>
#include <sstream>
#include <psapi.h>
#include <powrprof.h>
#include <timezoneapi.h>
#include <oleauto.h>
#include <rpc.h>
#include <intrin.h>
#include <fstream>



#define _WIN32_WINNT 0x0600 // Ensure Windows Vista or later APIs


// Link necessary libraries
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "kernel32.lib")




//-------------------------------------------------------------------------------------------------------------------------------------------
//PC Name Spoofer
//-------------------------------------------------------------------------------------------------------------------------------------------



// Function to generate a random 10-character string (letters and numbers)
std::string GenerateRandomString() {
	const std::string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, characters.length() - 1);

	std::string result;
	for (int i = 0; i < 10; ++i) {
		result += characters[dis(gen)];
	}
	return result;
}

// Function to set registry values
bool SetRegistryValues(const std::string& randomName) {
	HKEY hKey;
	LONG result;
	const wchar_t* wRandomName = std::wstring(randomName.begin(), randomName.end()).c_str();

	// Open and set SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ComputerName
	result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ComputerName", 0, KEY_SET_VALUE, &hKey);
	if (result != ERROR_SUCCESS) {
		return false;
	}
	RegSetValueExW(hKey, L"ComputerName", 0, REG_SZ, (const BYTE*)wRandomName, (wcslen(wRandomName) + 1) * sizeof(wchar_t));
	RegSetValueExW(hKey, L"ActiveComputerName", 0, REG_SZ, (const BYTE*)wRandomName, (wcslen(wRandomName) + 1) * sizeof(wchar_t));
	RegSetValueExW(hKey, L"ComputerNamePhysicalDnsDomain", 0, REG_SZ, (const BYTE*)L"", sizeof(L""));
	RegCloseKey(hKey);

	// Open and set SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ActiveComputerName
	result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ActiveComputerName", 0, KEY_SET_VALUE, &hKey);
	if (result != ERROR_SUCCESS) {
		return false;
	}
	RegSetValueExW(hKey, L"ComputerName", 0, REG_SZ, (const BYTE*)wRandomName, (wcslen(wRandomName) + 1) * sizeof(wchar_t));
	RegSetValueExW(hKey, L"ActiveComputerName", 0, REG_SZ, (const BYTE*)wRandomName, (wcslen(wRandomName) + 1) * sizeof(wchar_t));
	RegSetValueExW(hKey, L"ComputerNamePhysicalDnsDomain", 0, REG_SZ, (const BYTE*)L"", sizeof(L""));
	RegCloseKey(hKey);

	// Open and set SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters
	result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters", 0, KEY_SET_VALUE, &hKey);
	if (result != ERROR_SUCCESS) {
		return false;
	}
	RegSetValueExW(hKey, L"Hostname", 0, REG_SZ, (const BYTE*)wRandomName, (wcslen(wRandomName) + 1) * sizeof(wchar_t));
	RegSetValueExW(hKey, L"NV Hostname", 0, REG_SZ, (const BYTE*)wRandomName, (wcslen(wRandomName) + 1) * sizeof(wchar_t));
	RegCloseKey(hKey);

	// Open SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces
	result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces", 0, KEY_ALL_ACCESS, &hKey);
	if (result != ERROR_SUCCESS) {
		return false;
	}

	// Enumerate subkeys
	DWORD index = 0;
	wchar_t subKeyName[256];
	DWORD subKeyNameSize = 256;
	while (RegEnumKeyExW(hKey, index, subKeyName, &subKeyNameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
		HKEY hSubKey;
		result = RegOpenKeyExW(hKey, subKeyName, 0, KEY_SET_VALUE, &hSubKey);
		if (result == ERROR_SUCCESS) {
			RegSetValueExW(hSubKey, L"Hostname", 0, REG_SZ, (const BYTE*)wRandomName, (wcslen(wRandomName) + 1) * sizeof(wchar_t));
			RegSetValueExW(hSubKey, L"NV Hostname", 0, REG_SZ, (const BYTE*)wRandomName, (wcslen(wRandomName) + 1) * sizeof(wchar_t));
			RegCloseKey(hSubKey);
		}
		subKeyNameSize = 256;
		index++;
	}
	RegCloseKey(hKey);

	return true;
}



//-------------------------------------------------------------------------------------------------------------------------------------------
//Function to take file permissions
//-------------------------------------------------------------------------------------------------------------------------------------------



// Function to execute a command in CMD and return the output
std::string ExecuteCommand(const std::string& command) {
	std::string result;
	std::string cmd = "cmd.exe /C " + command;

	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi = { 0 };
	SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };

	HANDLE read_pipe, write_pipe;
	if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
		return "Error: Failed to create pipe.";
	}

	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = write_pipe;
	si.hStdError = write_pipe;

	if (CreateProcessA(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
		CloseHandle(write_pipe);

		std::vector<char> buffer(1024);
		DWORD bytes_read;
		while (ReadFile(read_pipe, buffer.data(), buffer.size() - 1, &bytes_read, nullptr) && bytes_read > 0) {
			buffer[bytes_read] = '\0';
			result += buffer.data();
		}

		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	else {
		result = "Error: Failed to execute command.";
	}

	CloseHandle(read_pipe);
	return result;
}

// Function to grant full permissions to the current user for a given path
std::string GrantPermissions(const std::string& path) {
	char username[256];
	DWORD username_len = sizeof(username);
	if (!GetUserNameA(username, &username_len)) {
		return "Error: Failed to get username.";
	}

	std::string takeown_command = "takeown /F \"" + path + "\" /R /D Y";
	std::string icacls_command = "icacls \"" + path + "\" /grant \"" + username + ":F\" /T";

	std::string takeown_result = ExecuteCommand(takeown_command);
	std::string icacls_result = ExecuteCommand(icacls_command);

	return "Takeown Result:\n" + takeown_result + "\nICACLS Result:\n" + icacls_result;
}

static char path_buffer[512] = "";
static std::string result_message = "";



//-------------------------------------------------------------------------------------------------------------------------------------------
//Network tab functions
//-------------------------------------------------------------------------------------------------------------------------------------------



// Function to execute network reset commands
void IPRESET() {
	const char* commands[] = {
		"netsh int ip reset",
		"ipconfig /release",
		"ipconfig /renew"

	};

	for (const auto& cmd : commands) {
		int result = system(cmd);
		if (result != 0) {
		}
	}
}

// Function to execute network reset commands
void cleandns() {
	const char* commands[] = {
		 "ipconfig /flushdns",

	};

	for (const auto& cmd : commands) {
		int result = system(cmd);
		if (result != 0) {
		}
	}
}

// Function to execute network reset commands
void Firewallfix() {
	const char* commands[] = {
		 "netsh winsock reset",
		 "netsh advfirewall reset",

	};

	for (const auto& cmd : commands) {
		int result = system(cmd);
		if (result != 0) {
		}
	}
}

// Function to execute network reset commands
void windowsiptracker() {
	const char* commands[] = {
		 "netsh winsock reset",
		 "netsh advfirewall reset",
		 "net stop winmgmt /y",
		 "net start winmgmt /y",
		 "sc stop winmgmt",
		 "sc start winmgmt",

	};

	for (const auto& cmd : commands) {
		int result = system(cmd);
		if (result != 0) {
		}
	}
}

// Function to execute network reset commands
void fullnetworkspoof() {
	const char* commands[] = {
		 "sc stop winmgmt",
		 "net stop winmgmt /y",
		 "netsh winsock reset",
		 "netsh advfirewall reset",
		 "netsh winsock reset",
		 "netsh advfirewall reset",
		 "netsh int ip reset",
	     "ipconfig /flushdns",
		 "ipconfig /release",
		 "ipconfig /renew"
		 "netsh interface ip delete arpcache"
	     "nbtstat -R"
		 "net start winmgmt /y",
         "sc start winmgmt",
		 
	};

	for (const auto& cmd : commands) {
		int result = system(cmd);
		if (result != 0) {
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
	static bool show_popuprizz = true;
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



	if (ImGui::BeginTabBar("MainTabBar")) {

	}


	//Credits Tab
	if (ImGui::BeginTabItem("Credits")) {
		ImGui::Text("   ");
		ImGui::Text("Hey there, thanks for using System X. System X was created by https://github.com/AlwaysAError to help manage your system");
		ImGui::Text("If you encounter any bugs or have any ideas please leave them on the GitHub page");



		ImGui::Separator();
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
		ImGui::EndTabItem();
	}

	//HWID CHanging
	if (ImGui::BeginTabItem("HWID Changing")) {
		ImGui::Text("   ");


		if (ImGui::Button("Spoof MachineGuid")) {
			std::wstring newGuid = GenerateRandomGUID();
			statusMessage = SetMachineGuid(newGuid);
			currentGuid = GetMachineGuid();
		}
		ImGui::Text("Current MachineGuid: %ls", currentGuid.c_str());

		ImGui::Separator();
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
		ImGui::EndTabItem();
	}

	//Registry Data Spoofing
	if (ImGui::BeginTabItem("Registry Data Spoofing")) {
		ImGui::Text("   ");

		// PC Name spoofer stuff
		static std::string randomString;
		static char displayText[11] = "";
		static bool showMessage = false;
		static bool success = false;

		if (ImGui::Button("Generate Random Name")) {
			randomString = GenerateRandomString();
			strncpy_s(displayText, randomString.c_str(), sizeof(displayText));
			showMessage = false;
		}

		ImGui::InputText("Generated Name", displayText, sizeof(displayText), ImGuiInputTextFlags_ReadOnly);

		if (ImGui::Button("Spoof PC Name")) {
			if (!randomString.empty()) {
				success = SetRegistryValues(randomString);
				showMessage = true;
			}
		}

		ImGui::Separator();
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
		ImGui::EndTabItem();
	}

	//Network Tab
	if (ImGui::BeginTabItem("Network")) {
		ImGui::Text("   ");

		// Button to trigger network reset
		if (ImGui::Button("Request A New IP Address From Your Router")) {
			IPRESET();
		}
		ImGui::Text("may not work depending on your internet provider");

		// Button to trigger network reset
		if (ImGui::Button("Clean DNS")) {
			cleandns();
		}

		// Button to trigger network reset
		if (ImGui::Button("Reset Windows Firewall")) {
			Firewallfix();
		}

		// Button to trigger network reset
		if (ImGui::Button("Restart Windows IP Tracking")) {
			windowsiptracker();
		}

		// Button to trigger network reset
		if (ImGui::Button("Full Network Spoof")) {
			fullnetworkspoof();
		}

		ImGui::Separator();
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
		ImGui::EndTabItem();
	}

	//System Info Tab
	if (ImGui::BeginTabItem("System Info")) {
		ImGui::Text("   ");



		ImGui::Separator();
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("   ");
		ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
		ImGui::EndTabItem();
	}

	//Settings
	if (ImGui::BeginTabItem("Settings")) {
		ImGui::Text("   ");

		ImGui::Text("File Permission Enforcer (forces you full permission of any file)");
		ImGui::Text("System X may freeze for a minute or two while it changes the file permissions and logs it's actions");
		ImGui::Text("Enter the file or folder path:");
		ImGui::InputText("##Path", path_buffer, sizeof(path_buffer));

		if (ImGui::Button("Grant Full Permissions")) {
			if (strlen(path_buffer) > 0) {
				std::string path = path_buffer;
				path.erase(path.find_last_not_of(" \n\r\t") + 1);

				result_message = GrantPermissions(path);
			}
			else {
				result_message = "Error: Please enter a valid path.";
			}
		}

		ImGui::TextWrapped("Result:\n%s", result_message.c_str());


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