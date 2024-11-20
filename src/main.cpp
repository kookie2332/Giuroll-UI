//
// Created by PinkySmile on 31/10/2020
//

#include <SokuLib.hpp>
#include <libloaderapi.h>
#include <string>

static bool init = false;
static bool frame_changed = true;
static int ping = -1;
static int player_delay = -1;
static int opponent_delay = -1;
static int current_rollback = -1;
static uint8_t max_rollback = 0;

static int (SokuLib::BattleClient::* ogBattleClientOnProcess)();
static int (SokuLib::BattleClient::* ogBattleClientOnRender)();
static SokuLib::BattleClient* (SokuLib::BattleClient::* ogBattleClientDestructor)(char unknown);

static int (SokuLib::BattleServer::* ogBattleServerOnProcess)();
static int (SokuLib::BattleServer::* ogBattleServerOnRender)();
static SokuLib::BattleServer* (SokuLib::BattleServer::* ogBattleServerDestructor)(char unknown);

static bool network_init = false;
static int (SokuLib::MenuConnect::* ogNetworkMenuOnProcess)();
static SokuLib::MenuConnect* (SokuLib::MenuConnect::* ogNetworkMenuOnDestruct)(unsigned char param);

static SokuLib::DrawUtils::Sprite desync_text;
static SokuLib::Vector2i display_text_real_size;

// not real text to be displayed. 
// Helps judge how far from the bottom left corner of the screen the desync_text should be - don't want to cover the delay number.
static SokuLib::DrawUtils::Sprite delay_spacing_text;
static SokuLib::Vector2i delay_text_real_size;

static SokuLib::SWRFont font;
static HMODULE giuroll;
static bool desynced;

typedef bool(__cdecl *desync_func)(void);
desync_func is_likely_desynced;

typedef int(__cdecl* ping_func)(void);
ping_func get_ping;

typedef int(__cdecl* player_delay_func)(void);
player_delay_func get_player_delay;

typedef int(__cdecl* opponent_delay_func)(void);
opponent_delay_func get_opponent_delay;

typedef int(__cdecl* current_rollback_func)(void);
current_rollback_func get_current_rollback;

typedef uint8_t(__cdecl* max_rollback_func)(void);
max_rollback_func get_max_rollback;

bool __fastcall giuroll_loaded() { return giuroll != NULL; }

/** 
* Function that checks if the is_likely_desynced function has been loaded or not.
* @return true if the function is not NULL, and false otherwise.
*/
bool __fastcall desync_func_detected() { return is_likely_desynced != NULL; }

/**
* Function that checks if the get_ping function has been loaded or not.
*/
bool __fastcall ping_func_detected() { return get_ping != NULL; }

bool __fastcall player_delay_func_detected() { return get_player_delay != NULL; }

bool __fastcall opponent_delay_func_detected() { return get_opponent_delay != NULL; }

bool __fastcall current_rollback_func_detected() { return get_current_rollback != NULL; }

bool __fastcall max_rollback_func_detected() { return get_max_rollback != NULL; }

void setText(const char* content) {
	/*
	* Sets the content of the text.
	*/
	desync_text.texture.createFromText(content, font, { 300, 300 }, &display_text_real_size);
	desync_text.setSize(display_text_real_size.to<unsigned>());
	desync_text.rect.width = display_text_real_size.x;
	desync_text.rect.height = display_text_real_size.y;
}

void moveText(int xpos, int ypos) {
	/*
	* Moves the text to the corresponding x and y position.
	* The origin (0,0) is in the top left corner of the game window.
	*/
	desync_text.setPosition(SokuLib::Vector2i{ xpos, ypos });
}

void loadFont()
{
	SokuLib::FontDescription desc;

	desc.r1 = 255;
	desc.g1 = 0;
	desc.b1 = 0;

	desc.r2 = 255;
	desc.g2 = 0;
	desc.b2 = 0;

	desc.height = 13;
	desc.weight = FW_NORMAL;
	desc.italic = 0;
	desc.shadow = 4;
	desc.bufferSize = 1000000;
	desc.charSpaceX = 0;
	desc.charSpaceY = 0;
	desc.offsetX = 0;
	desc.offsetY = 0;
	desc.useOffset = 0;
	strcpy(desc.faceName, "MonoSpatialModSWR");
	font.create();
	font.setIndirect(desc);
}

void __fastcall Generic_OnRender() {
	if (desynced) desync_text.draw();
	frame_changed = true;
}

int __fastcall CBattleServer_OnRender(SokuLib::BattleServer* This) {
	int out = (This->*ogBattleServerOnRender)();

	Generic_OnRender();
	return out;
}

int __fastcall CBattleClient_OnRender(SokuLib::BattleClient* This) {
	int out = (This->*ogBattleClientOnRender)();

	Generic_OnRender();
	return out;
}

void __fastcall OnLoad(void *This) {
	// Code to be run once at the start of a game.
	loadFont();

	// set delay spacing text
	delay_spacing_text.texture.createFromText("00", font, {300, 300}, &delay_text_real_size);

	// set desync text.
	setText("DESYNCED");
	moveText(delay_text_real_size.x, 480 - display_text_real_size.y - 3);

	desynced = false;
	frame_changed = true;
}

void __fastcall Generic_OnProcess(void* This, char* init_message) {
	if (!init) {
		puts(init_message);
		OnLoad(This);
		init = true;
	}
	if (frame_changed) {
		if (desync_func_detected()) desynced = is_likely_desynced();
		if (ping_func_detected()) ping = get_ping();
		if (player_delay_func_detected())player_delay = get_player_delay();
		if (opponent_delay_func_detected()) opponent_delay = get_opponent_delay();
		if (current_rollback_func_detected()) current_rollback = get_current_rollback();
		if (max_rollback_func_detected()) {
			max_rollback = get_max_rollback();
			puts(("Max rollback: " + std::to_string(max_rollback)).c_str());
		}

		//int fps = *(int*)0x8985d8;
		//puts(("fps: " + std::to_string(fps)).c_str());
		frame_changed = false;
	}
}

int __fastcall CBattleServer_OnProcess(SokuLib::BattleServer* This) {
	Generic_OnProcess(This, "Server Initialised.");
	return (This->*ogBattleServerOnProcess)();
}

int __fastcall CBattleClient_OnProcess(SokuLib::BattleClient* This) {
	Generic_OnProcess(This, "Client Initialised.");
	return (This->*ogBattleClientOnProcess)();
}

bool __fastcall CNetworkMenu_OnProcess(SokuLib::MenuConnect* This) {
	if (!network_init) {
		giuroll = GetModuleHandleA("giuroll.dll");
		bool gl = giuroll_loaded();

		if (!gl) puts("WARNING: GIUROLL NOT DETECTED.");

		is_likely_desynced = (desync_func)GetProcAddress(giuroll, "is_likely_desynced");
		get_ping = (ping_func)GetProcAddress(giuroll, "get_ping");
		get_player_delay = (player_delay_func)GetProcAddress(giuroll, "get_player_delay");
		get_opponent_delay = (opponent_delay_func)GetProcAddress(giuroll, "get_opponent_delay");
		get_current_rollback = (current_rollback_func)GetProcAddress(giuroll, "get_current_rollback");
		get_max_rollback = (max_rollback_func)GetProcAddress(giuroll, "get_max_rollback");

		if (gl) {
			bool warnings_present = false;
			std::string warning_message = "";
			if (!desync_func_detected()) {
				puts("WARNING: DESYNC FUNCTION NOT DETECTED. MAKE SURE YOU ARE USING GIUROLL VERSION 0.6.13 OR LATER.");
				warning_message += "Desync function not detected (Giuroll >= 0.6.13).\n";
			}
			else {
				puts("Hooked desync func.");
			}

			if (!ping_func_detected()) {
				puts("WARNING: PING FUNCTION NOT DETECTED. MAKE SURE YOU ARE USING GIUROLL VERSION 0.6.18 OR LATER.");
				warning_message += "Ping function not detected (Giuroll >= 0.6.18).\n";
			}
			else {
				puts("Hooked ping func.");
			}

			if (!player_delay_func_detected()) {
				puts("WARNING: PLAYER DELAY FUNCTION NOT DETECTED. MAKE SURE YOU ARE USING GIUROLL VERSION 0.6.18 OR LATER.");
				warning_message += "Player delay function not detected (Giuroll >= 0.6.18).\n";
			}
			else {
				puts("Hooked player delay func.");
			}

			if (!opponent_delay_func_detected()) {
				puts("WARNING: OPPONENT DELAY FUNCTION NOT DETECTED. MAKE SURE YOU ARE USING GIUROLL VERSION 0.6.18 OR LATER.");
				warning_message += "Opponent delay function not detected (Giuroll >= 0.6.18).\n";
			}
			else {
				puts("Hooked opponent delay func.");
			}

			if (!current_rollback_func_detected()) {
				puts("WARNING: CURRENT ROLLBACK FUNCTION NOT DETECTED. MAKE SURE YOU ARE USING GIUROLL VERSION 0.6.18 OR LATER.");
				warning_message += "Current rollback function not detected (Giuroll >= 0.6.18).\n";
			}
			else {
				puts("Hooked current rollback func.");
			}

			if (!max_rollback_func_detected()) {
				puts("WARNING: MAX ROLLBACK FUNCTION NOT DETECTED. MAKE SURE YOU ARE USING GIUROLL VERSION 0.6.18 OR LATER.");
				warning_message += "Max rollback function not detected (Giuroll >= 0.6.18).\n";
			}
			else {
				puts("Hooked max rollback func.");
			}

			if (!warning_message.empty()) {
				int msgboxID = MessageBox(
					NULL,
					(LPCTSTR)warning_message.c_str(),
					(LPCTSTR)"Warning",
					MB_OK | MB_ICONWARNING
				);
			}
		}
		
		network_init = true;
	}
	return (This->*ogNetworkMenuOnProcess)();
}

SokuLib::BattleServer* __fastcall CBattleServer_Destructor(SokuLib::BattleServer* This, int junk, char unknown) {
	puts("Server destruct.");
	init = false;
	return (This->*ogBattleServerDestructor)(unknown);
}

SokuLib::BattleClient* __fastcall CBattleClient_Destructor(SokuLib::BattleClient* This, int junk, char unknown) {
	puts("Client destruct.");
	init = false;
	return (This->*ogBattleClientDestructor)(unknown);
}

SokuLib::MenuConnect* __fastcall CNetworkMenu_OnDestruct(SokuLib::MenuConnect* This, int junk, unsigned char param) {
	network_init = false;
	return (This->*ogNetworkMenuOnDestruct)(param);
}

// We check if the game version is what we target (in our case, Soku 1.10a).
extern "C" __declspec(dllexport) bool CheckVersion(const BYTE hash[16])
{
	return memcmp(hash, SokuLib::targetHash, sizeof(SokuLib::targetHash)) == 0;
}

// Called when the mod loader is ready to initialize this module.
// All hooks should be placed here. It's also a good moment to load settings from the ini.
extern "C" __declspec(dllexport) bool Initialize(HMODULE hMyModule, HMODULE hParentModule)
{
	DWORD old;

#ifdef _DEBUG
	FILE* _;

	AllocConsole();
	freopen_s(&_, "CONOUT$", "w", stdout);
	freopen_s(&_, "CONOUT$", "w", stderr);
#endif

	puts("Hello, world!");
	VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, PAGE_EXECUTE_WRITECOPY, &old);
	ogBattleClientDestructor = SokuLib::TamperDword(&SokuLib::VTable_BattleClient.destructor, CBattleClient_Destructor);
	ogBattleClientOnRender = SokuLib::TamperDword(&SokuLib::VTable_BattleClient.onRender, CBattleClient_OnRender);
	ogBattleClientOnProcess = SokuLib::TamperDword(&SokuLib::VTable_BattleClient.onProcess, CBattleClient_OnProcess);

	ogBattleServerDestructor = SokuLib::TamperDword(&SokuLib::VTable_BattleServer.destructor, CBattleServer_Destructor);
	ogBattleServerOnRender = SokuLib::TamperDword(&SokuLib::VTable_BattleServer.onRender, CBattleServer_OnRender);
	ogBattleServerOnProcess = SokuLib::TamperDword(&SokuLib::VTable_BattleServer.onProcess, CBattleServer_OnProcess);

	ogNetworkMenuOnProcess = SokuLib::TamperDword(&SokuLib::VTable_ConnectMenu.onProcess, CNetworkMenu_OnProcess);
	ogNetworkMenuOnDestruct = SokuLib::TamperDword(&SokuLib::VTable_ConnectMenu.onDestruct, CNetworkMenu_OnDestruct);
	VirtualProtect((PVOID)RDATA_SECTION_OFFSET, RDATA_SECTION_SIZE, old, &old);

	FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
	return true;
}

extern "C" int APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	return TRUE;
}

// New mod loader functions
// Loading priority. Mods are loaded in order by ascending level of priority (the highest first).
// When 2 mods define the same loading priority the loading order is undefined.
extern "C" __declspec(dllexport) int getPriority()
{
	return 0;
}

// Not yet implemented in the mod loader, subject to change
// SokuModLoader::IValue **getConfig();
// void freeConfig(SokuModLoader::IValue **v);
// bool commitConfig(SokuModLoader::IValue *);
// const char *getFailureReason();
// bool hasChainedHooks();
// void unHook();
