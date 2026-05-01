#include "Spoofer.h"
#include <Windows.h>

int main() {
	Spoofer spoofer;
	spoofer.SpoofAll();
	MessageBoxA(NULL, "Spoofing Finished, you can now close this window and start Roblox\n", "", MB_OK);
}
