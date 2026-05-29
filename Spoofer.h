#pragma once

#include <Windows.h>
#include <random>
#include <string>

class Spoofer {
private:
	std::mt19937_64 m_mt;
	static std::mt19937_64 InitializeEngineWithGeneratedSeedSequence(size_t seedsCount = 624);
public:
	Spoofer();
	Spoofer(std::seed_seq seed_seq);
	Spoofer(uint32_t seed);

	void SpoofAll();

	template <typename T>
	T GetRandomNumber(T min, T max);

	static void DeleteRobloxAccountData();
	static void TerminateAllRobloxInstances();

	std::wstring GetRandomHexByteString(
		size_t byteCount,
		bool uppercase = true,
		const wchar_t *separator = L"\0",
		bool padding = true
	);

	static void DeleteRobloxRegistry();
	static void RestartNetworkAdapters();

	std::wstring GenerateMacAddress(
		BOOL isMulticast = FALSE, 
		BOOL isLocallyAdministered = TRUE
	);

	void ChangeMacRegistry();

	void ChangeEDIDRegistry();
	
	[[noreturn]] static void PauseExit();
	
	[[noreturn]] static void ErrorWithMessageFormatted(const char *format, ...);
	[[noreturn]] static void ErrorWithMessageFormatted(const wchar_t *format, ...);

};