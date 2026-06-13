#define NOMINMAX
#include <Windows.h>
#include <string>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <TlHelp32.h>
#include <ntstatus.h>
#include <cwctype>
#include <numeric>
#include <thread>
#include <cstdarg>
#include <cwchar>
#include <conio.h>

#include "Spoofer.h"
#include "ntapi.h"

namespace fs = std::filesystem;

#define BASEBOARD_SERIAL_LENGTH 16

std::mt19937_64 Spoofer::InitializeEngineWithGeneratedSeedSequence(size_t seedsCount) {
	if (seedsCount > 624) {
		throw std::runtime_error("more than 624 seeds does not increase engine entropy");
	}
	std::random_device rd;
	std::vector<uint32_t> seeds(seedsCount);
	for (auto &seed : seeds) {
		seed = rd();
	}
	std::seed_seq seed_seq(seeds.begin(), seeds.end());
	return std::mt19937_64(seed_seq);
}

Spoofer::Spoofer() : m_mt(Spoofer::InitializeEngineWithGeneratedSeedSequence()) {};
Spoofer::Spoofer(std::seed_seq seed_seq) : m_mt(seed_seq) {};
Spoofer::Spoofer(uint32_t seed) : m_mt(seed) {};

template <typename T>
T Spoofer::GetRandomNumber(T min, T max) {
	std::uniform_int_distribution<T> distribution(min, max);
	return distribution(m_mt);
}

std::wstring Spoofer::GetRandomHexByteString(
	size_t byteCount,
	bool uppercase,
	const wchar_t *separator,
	bool padding
) {
	std::wstringstream wss;

	if (uppercase)
		wss << std::uppercase;
	else
		wss << std::nouppercase;

	wss << std::hex << std::noshowbase;

	for (size_t i = 0; i < byteCount; ++i) {
		int byte = this->GetRandomNumber<int>(0,0xFF);
		
		wss << std::setw(2) << std::setfill(L'0') << byte;

		if (separator != L"\0" && i < byteCount - 1) {
			wss << separator;
		}
	}

	return wss.str();
}

void Spoofer::DeleteRobloxAccountData() {
	fs::path localAppData(std::getenv("LOCALAPPDATA"));
	fs::path robloxAccountData(localAppData / "Roblox" / "LocalStorage");
	
	if (!fs::exists(robloxAccountData)) {
		return;
	}

	std::error_code ec{};
	while (true) {
		ec.clear();
		fs::remove_all(robloxAccountData, ec);
		if (!ec) {
			break;
		}
		else if (ec.value() != ERROR_SHARING_VIOLATION) {
			Spoofer::ErrorWithMessageFormatted("Recursive delete thrown unknown exception\n");
		}
		Sleep(1);
	}

	printf("Successfully Deleted Roblox Account Data\n");

}

void Spoofer::TerminateAllRobloxInstances() {
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	PROCESSENTRY32W pe{};
	pe.dwSize = sizeof(pe);

	BOOL ok = Process32FirstW(hSnap, &pe);

	BOOL terminatedAny = FALSE;

	while (ok) {
		if (wcscmp(pe.szExeFile, L"RobloxPlayerBeta.exe") == 0) {
			HANDLE hRoblox = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
			if (hRoblox) {
				TerminateProcess(hRoblox, NULL);
				WaitForSingleObject(hRoblox, INFINITE);
				CloseHandle(hRoblox);
				terminatedAny = TRUE;
			}
		}
		ok = Process32NextW(hSnap, &pe);
	}

	CloseHandle(hSnap);

	if (terminatedAny) {
		printf("Successfully Terminated All Roblox Instances\n");
	}
}

void Spoofer::DeleteRobloxRegistry() {
	{
		LSTATUS ok = RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\ROBLOX Corporation");
		if (ok == ERROR_SUCCESS) {
			printf("Successfully Deleted HKCU\\Software\\ROBLOX Corporation\n");
		}
		else if (ok != ERROR_FILE_NOT_FOUND) {
			printf("Failed to delete HKCU\\Software\\ROBLOX Corporation: 0x%X\n", ok);
		}
		else {
			printf("HKCU\\Software\\ROBLOX Corporation not found\n");
		}
	}

	{
		HKEY hControl;
		{
			LSTATUS ok = RegOpenKeyExW(HKEY_CURRENT_USER, L"System\\CurrentControlSet\\Control", 0, KEY_ALL_ACCESS, &hControl);
			if (ok != ERROR_SUCCESS) {
				printf("Failed to open HKCU\\System\\CurrentControlSet\\Control\n");
			}
		}

		NtDeleteValueKey_t NtDeleteValueKey;
		
		{
			HMODULE ntdll = GetModuleHandleA("ntdll.dll");
			if (!ntdll) {
				Spoofer::ErrorWithMessageFormatted("Failed to get handle to ntdll\n");
			}
			NtDeleteValueKey = (NtDeleteValueKey_t)GetProcAddress(ntdll, "NtDeleteValueKey");
			if (!NtDeleteValueKey) {
				Spoofer::ErrorWithMessageFormatted("Failed to get NtDeleteValueKey\n");
			}
		}

		{
			UNICODE_STRING us{};

			wchar_t buffer[] = L"\0SystemReg";
			USHORT bufferLength = sizeof(buffer);
			us.Buffer = buffer;
			us.Length = bufferLength;
			us.MaximumLength = bufferLength;

			NTSTATUS ok = NtDeleteValueKey(hControl, &us);

			if (NT_SUCCESS(ok)) {
				printf("Successfully Deleted HKCU\\System\\CurrentControlSet\\Control\\0SystemReg\n");
			}
			else if (ok == STATUS_OBJECT_NAME_NOT_FOUND) {
				printf("HKCU\\System\\CurrentControlSet\\Control\\0SystemReg not found\n");
			}
			else {
				printf("Failed to delete HKCU\\System\\CurrentControlSet\\Control\\0SystemReg: 0x%X\n", ok);
			}

		}

		{
			LSTATUS ok = RegCloseKey(hControl);
			if (ok != ERROR_SUCCESS) {
				printf("Failed to close HKCU\\System\\CurrentControlSet\\Control\n");
			}
		}

	}
}

void Spoofer::RestartNetworkAdapters() {

	STARTUPINFOW si{};
	si.cb = sizeof(STARTUPINFOW);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION pi{};
	
	{
		wchar_t cmd[] = L"powershell.exe -ExecutionPolicy Bypass -NoLogo -NoProfile -Command \"Get-NetAdapter | Restart-NetAdapter -Confirm:$false\"";
		BOOL ok = CreateProcessW(
			NULL,
			cmd,
			NULL,
			NULL,
			FALSE,
			CREATE_NO_WINDOW,
			NULL,
			NULL,
			&si,
			&pi
		);
		
		if (!ok) {
			Spoofer::ErrorWithMessageFormatted("failed to create powershell subprocess\n");
		}
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	printf("Successfully Restarted All Network Adapters\n");
}

// when it comes to WIFI network adapters, windows only lets you assign a mac address that has the isLocallyAdministered bit set
std::wstring Spoofer::GenerateMacAddress(BOOL isMulticast, BOOL isLocallyAdministered) {
	int preset = (isLocallyAdministered << 1) | isMulticast;
	int firstByte = this->GetRandomNumber<int>(0, 0xFF);
	
	// clear 2 lsb
	firstByte &= ~0b11;
	
	// apply isMulticast and isLocallyAdministered
	firstByte |= preset;

	std::wstringstream wss;
	wss << std::uppercase << std::hex << std::setw(2) << std::setfill(L'0') << firstByte;

	return wss.str() + this->GetRandomHexByteString(5);
}

void Spoofer::SpoofMacRegistry() {
	const wchar_t *networkAdaptersPath = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}";
	HKEY hNetworkAdapters;

	{
		LSTATUS ok = RegOpenKeyExW(
			HKEY_LOCAL_MACHINE,
			networkAdaptersPath,
			NULL,
			KEY_ENUMERATE_SUB_KEYS,
			&hNetworkAdapters
		);
		if (ok != ERROR_SUCCESS) {
			this->ErrorWithMessageFormatted("Failed to open handle to network adapters\n");
		}
	}

	int keyIndex = -1;

	// https://learn.microsoft.com/en-us/windows/win32/sysinfo/registry-element-size-limits
	// A pointer to a buffer that receives the name of the subkey, including the terminating null character.
	TCHAR keyNameBuffer[0xFF + 1];

	while (true) {
		++keyIndex;

		std::wstring newMacAddress = this->GenerateMacAddress();

		// RegEnumKeyW does not clear the buffer
		memset(keyNameBuffer, 0, sizeof(keyNameBuffer));

		{
			LSTATUS ok = RegEnumKeyW(
				hNetworkAdapters,
				(DWORD)keyIndex,
				keyNameBuffer,
				(DWORD)_countof(keyNameBuffer)
			);
			if (ok == ERROR_NO_MORE_ITEMS) {
				break;
			}
			else if (ok != ERROR_SUCCESS) {
				continue;
			}
		}

		// there are other subkeys in the class that are not network interfaces, adapters have only numbers in their names
		BOOL shouldSkip = FALSE;
		for (wchar_t chr : keyNameBuffer) {
			if (chr == L'0') {
				break;
			}
			else if (!iswdigit(chr)) {
				shouldSkip = TRUE;
				break;
			}
		}

		if (shouldSkip) {
			//wprintf(L"Skipping Entry: %s\n", keyNameBuffer);
			continue;
		}

		HKEY hAdapter;

		{
			LSTATUS ok = RegOpenKeyExW(
				hNetworkAdapters,
				keyNameBuffer,
				NULL,
				KEY_SET_VALUE | KEY_READ,
				&hAdapter
			);
			if (ok != ERROR_SUCCESS) {
				this->ErrorWithMessageFormatted(L"Failed to open Network Adapter: %s\n", keyNameBuffer);
			}
		}

		{
			LSTATUS ok = RegSetValueExW(
				hAdapter,
				L"NetworkAddress",
				NULL,
				REG_SZ,
				(BYTE *)(newMacAddress.c_str()),
				(DWORD)((newMacAddress.size() + 1) * sizeof(wchar_t))
			);
			if (ok != ERROR_SUCCESS) {
				this->ErrorWithMessageFormatted(L"Failed to set mac for Network Adapter: %s\n", keyNameBuffer);
			}
		}

		//wprintf(L"Successfully set new MAC for Network Adapter: %s\n", keyNameBuffer);
		
		RegCloseKey(hAdapter);
	}

	RegCloseKey(hNetworkAdapters);

	printf("Successfully Set New MAC for all Network Adapters\n");
}

void Spoofer::PauseExit() {
	(void)_getch();
	std::exit(0);
}

void Spoofer::ErrorWithMessageFormatted(const char *format, ...) {
	va_list args;
	va_start(args, format);

	vprintf(format, args);

	Spoofer::PauseExit();
}

void Spoofer::ErrorWithMessageFormatted(const wchar_t *format, ...) {
	va_list args;
	va_start(args, format);

	vwprintf(format,args);

	Spoofer::PauseExit();
}

void Spoofer::SpoofEDIDRegistry() {
	const wchar_t *displayPath = L"SYSTEM\\CurrentControlSet\\Enum\\DISPLAY";
	HKEY hDisplays;

	{
		LSTATUS ok = RegOpenKeyExW(
			HKEY_LOCAL_MACHINE,
			displayPath,
			NULL,
			KEY_ALL_ACCESS,
			&hDisplays
		);
		if (ok != ERROR_SUCCESS) {
			this->ErrorWithMessageFormatted("Failed to open handle to displays\n");
		}
	}

	int keyIndex = -1;
	TCHAR monitorIdBuffer[0xFF + 1];

	while (true) {
		memset(monitorIdBuffer, 0, sizeof(monitorIdBuffer));
		++keyIndex;
		{
			LSTATUS ok = RegEnumKeyW(
				hDisplays,
				(DWORD)keyIndex,
				monitorIdBuffer,
				(DWORD)_countof(monitorIdBuffer)
			);
			if (ok == ERROR_NO_MORE_ITEMS) {
				//printf("No More Monitors\n");
				break;
			}
			else if (ok != ERROR_SUCCESS) {
				printf("Another error when enumerating monitors, skipping monitor");
				continue;
			}
		}

		HKEY hMonitor;

		{
			LSTATUS ok = RegOpenKeyExW(
				hDisplays,
				monitorIdBuffer,
				NULL,
				KEY_ALL_ACCESS,
				&hMonitor
			);
			if (ok != ERROR_SUCCESS) {
				wprintf(L"Failed to open handle to monitor key: %s\n", monitorIdBuffer);
				continue;
			}
		}

		int monitorSubkeyIndex = -1;
		TCHAR monitorSubDeviceNameBuffer[0xFF + 1];

		while (true) {
			memset(monitorSubDeviceNameBuffer, 0, sizeof(monitorSubDeviceNameBuffer));
			++monitorSubkeyIndex;
			
			{
				LSTATUS ok = RegEnumKeyW(
					hMonitor,
					(DWORD)monitorSubkeyIndex,
					monitorSubDeviceNameBuffer,
					(DWORD)_countof(monitorSubDeviceNameBuffer)
				);
				if (ok == ERROR_NO_MORE_ITEMS) {
					//wprintf(L"No more monitor subdevices for %s\n", monitorIdBuffer);
					break;
				}
			}

			HKEY hMonitorDevice;

			{
				LSTATUS ok = RegOpenKeyExW(
					hMonitor,
					monitorSubDeviceNameBuffer,
					NULL,
					KEY_ALL_ACCESS,
					&hMonitorDevice
				);
				if (ok != ERROR_SUCCESS) {
					wprintf(L"Couldnt open handle to monitor subdevice %s/%s\n",monitorIdBuffer, monitorSubDeviceNameBuffer);
					continue;
				}
			}

			HKEY hMonitorDeviceParameters;

			{
				LSTATUS ok = RegOpenKeyExW(
					hMonitorDevice,
					L"Device Parameters",
					NULL,
					KEY_ALL_ACCESS,
					&hMonitorDeviceParameters
				);
				if (ok != ERROR_SUCCESS) {
					wprintf(L"Couldnt open handle to monitor subdevice parameters %s/%s\n",monitorIdBuffer, monitorSubDeviceNameBuffer);
					continue;
				}
			}

			DWORD edidSize = 0;
			{
				LSTATUS ok = RegQueryValueExW(
					hMonitorDeviceParameters, 
					L"EDID", 
					NULL, 
					NULL,
					NULL,
					&edidSize
				);
				if (ok != ERROR_SUCCESS) {
					wprintf(L"Failed to Query EDID size from %s/%s error code: %ld\n", monitorIdBuffer, monitorSubDeviceNameBuffer, ok);
					continue;
				}
			}

			std::vector<BYTE> EDID(edidSize);

			{
				LSTATUS ok = RegQueryValueExW(
					hMonitorDeviceParameters,
					L"EDID",
					NULL,
					NULL,
					EDID.data(),
					&edidSize
				);
				if (ok != ERROR_SUCCESS) {
					wprintf(L"Failed to Query EDID from %s/%s error code: %ld\n", monitorIdBuffer, monitorSubDeviceNameBuffer, ok);
					continue;
				}
			}

			/*
			wprintf(L"EDID for monitor %s/%s is:\n", monitorIdBuffer, monitorSubDeviceNameBuffer);
			for (BYTE byte : EDID) {
				printf("%02X ",byte);
			}
			printf("\n");
			

			uint8_t oldChecksum = std::accumulate(EDID.begin(), EDID.end(), 0);
			*/

			//printf("Monitor Serial Number from EDID: ");
			for (int i = 12; i <= 15; ++i) {
				//printf("%02X ", EDID[i]);
				EDID[i] = this->GetRandomNumber<int>(0, 0xFF);
			}
			//printf("\n");

			/*
			
			printf("New EDID: ");
			for (int i = 12; i <= 15; ++i) {
				printf("%02X ", EDID[i]);
			}
			printf("\n");

			printf("Initial Checksum: %d\n", oldChecksum);
			*/

			uint8_t newChecksum = std::accumulate(EDID.begin(), EDID.end(), 0);
			
			//printf("Modified serial Checksum: %d\n", newChecksum);
			
			// need 256 to actually wrap around
			uint8_t missing = 0xFF + 1 - newChecksum;

			EDID.back() += missing;

			//uint8_t fixedChecksum = std::accumulate(EDID.begin(), EDID.end(), 0);
			//printf("Fixed serial Checksum: %d\n", fixedChecksum);

			{
				LSTATUS ok = RegSetValueExW(
					hMonitorDeviceParameters,
					L"EDID",
					NULL,
					REG_BINARY,
					EDID.data(),
					(DWORD)EDID.size()*sizeof(BYTE) // to clarify we dont need to multiply it by sizeof(BYTE) but its good practice so we dont forget it ever for any other type
				);
				if (ok != ERROR_SUCCESS) {
					wprintf(L"Failed to set new EDID for monitor %ls/%ls\n", monitorIdBuffer, monitorSubDeviceNameBuffer);
					continue;
				}
			}

			//wprintf(L"Successfully set new EDID for monitor %ls/%ls\n", monitorIdBuffer, monitorSubDeviceNameBuffer);

			RegCloseKey(hMonitorDeviceParameters);
			RegCloseKey(hMonitorDevice);

		}

		RegCloseKey(hMonitor);

	}

	RegCloseKey(hDisplays);

	printf("Successfully Set New EDID for all monitors\n");
}

void Spoofer::SpoofSMBIOSSystemUUID() {
	std::string command = "AMIDEWINx64.EXE /SU";

	std::wstring uuid = this->GetRandomHexByteString(16);

	command += " ";

	command += std::string(uuid.begin(), uuid.end());

	command += " >nul";

	int OK = system(command.c_str());
	if (OK == 0) {
		printf("System UUID spoofed successfully\n");
	}
	else {
		printf("Failed to spoof System UUID: %d\n", OK);
	}
}

void Spoofer::SpoofSMBIOSBaseboardSerial() {
	std::string command = "AMIDEWINx64.EXE /BS";
	command += " ";
	for (int i = 0; i < BASEBOARD_SERIAL_LENGTH; ++i) {
		command += std::to_string(this->GetRandomNumber<int>(0,9));
	}
	command += " ";
	command += ">nul";
	int OK = system(command.c_str());
	if (OK == 0) {
		printf("Baseboard serial spoofed successfully\n");
	}
	else {
		printf("Failed to spoof Baseboard serial: %d\n", OK);
	}
}

void Spoofer::RestartWinMgmtService() {
	int OK = system("powershell -NoProfile -Command \"Restart-Service winmgmt -Force > $null\"");
	if (OK == 0) {
		printf("winmgmt restarted successfully\n");
	}
	else {
		printf("failed to restart winmgmt: %d\n", OK);
	}
}

void Spoofer::SpoofAll() {
	Spoofer::TerminateAllRobloxInstances();

	Spoofer::DeleteRobloxAccountData();
	this->DeleteRobloxRegistry();
	this->SpoofEDIDRegistry();

	this->SpoofSMBIOSSystemUUID();
	this->SpoofSMBIOSBaseboardSerial();

	// this takes some time too but we cannot put it in threads because it messes up restarting network adapters
	Spoofer::RestartWinMgmtService();

	this->SpoofMacRegistry();
	Spoofer::RestartNetworkAdapters();
}