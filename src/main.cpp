//
// Created by PinkySmile on 31/10/2020
//

#include <SokuLib.hpp>
#include <libloaderapi.h>
#include <string>
#include <sstream>

struct UIValues {
	bool valid;
	bool likely_desynced;
	int ping;
	int current_rollback;
	int max_rollback;
	int player_delay;
	int opponent_delay;
	bool toggle_stats; // TODO: REMOVE THIS FIELD ONCE THIS CODE CAN ACTIVATE UI DISPLAY ON ITS OWN.
};

bool ui_data_equal(UIValues* lhs, UIValues* rhs) {
	if (lhs == nullptr && rhs == nullptr) return true;
	if (lhs == nullptr || rhs == nullptr) return false;
	return
		lhs->likely_desynced == rhs->likely_desynced &&
		lhs->ping == rhs->ping &&
		lhs->current_rollback == rhs->current_rollback &&
		lhs->max_rollback == rhs->max_rollback &&
		lhs->player_delay == rhs->player_delay &&
		lhs->opponent_delay == rhs->opponent_delay &&
		lhs->toggle_stats == rhs->toggle_stats;
}

std::string ui_data_to_str(UIValues* data) {
	std::stringstream ss;
	if (data == nullptr) ss << "ui values is nullptr." << std::endl;
	else {
		ss << "Likely Desynced: " << data->likely_desynced << "\n"
			<< "Ping: " << data->ping << "\n"
			<< "Current Rollback: " << data->current_rollback << "\n"
			<< "Maximum Rollback: " << data->max_rollback << "\n"
			<< "Player Delay: " << data->player_delay << "\n"
			<< "Opponent Delay: " << data->opponent_delay << "\n"
			<< "Toggle Stats: " << data->toggle_stats << std::endl;
	}
	return ss.str();
}

UIValues get_null_uidata() {
	UIValues out = UIValues{
		false,
		false,
		-1,
		-1,
		-1,
		-1,
		-1,
		false
	};
	return out;
}

static bool init = false;
static bool frame_changed = true;

/*static bool desynced;
static int ping = -1;
static int current_rollback = -1;
static uint8_t max_rollback = 0;
static int player_delay = -1;
static int opponent_delay = -1;*/
static UIValues null_uidata = get_null_uidata();
static UIValues uidata = null_uidata;
static UIValues previous_uidata = null_uidata;

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

/*typedef bool(__cdecl* desync_func)(void);
typedef int(__cdecl* ping_func)(void);
typedef int(__cdecl* player_delay_func)(void);
typedef int(__cdecl* opponent_delay_func)(void);
typedef int(__cdecl* current_rollback_func)(void);
typedef uint8_t(__cdecl* max_rollback_func)(void);*/
typedef void(__cdecl* ui_values_func)(UIValues*);

/*desync_func is_likely_desynced;
ping_func get_ping;
player_delay_func get_player_delay;
opponent_delay_func get_opponent_delay;
current_rollback_func get_current_rollback;
max_rollback_func get_max_rollback;*/
ui_values_func get_ui_values;

bool __fastcall giuroll_loaded() { return giuroll != NULL; }
/*bool __fastcall desync_func_detected() { return is_likely_desynced != NULL; }
bool __fastcall ping_func_detected() { return get_ping != NULL; }
bool __fastcall player_delay_func_detected() { return get_player_delay != NULL; }
bool __fastcall opponent_delay_func_detected() { return get_opponent_delay != NULL; }
bool __fastcall current_rollback_func_detected() { return get_current_rollback != NULL; }
bool __fastcall max_rollback_func_detected() { return get_max_rollback != NULL; }*/
bool __fastcall ui_values_func_detected() { return get_ui_values != NULL; }

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
	if (!ui_data_equal(&uidata, &null_uidata) && uidata.likely_desynced) desync_text.draw();
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

	frame_changed = true;
}

void __fastcall Generic_OnProcess(void* This, char* init_message) {
	if (!init) {
		puts(init_message);
		OnLoad(This);
		init = true;
	}
	if (frame_changed) {
		/*if (desync_func_detected()) desynced = is_likely_desynced();
		if (ping_func_detected()) ping = get_ping();
		if (player_delay_func_detected()) player_delay = get_player_delay();
		if (opponent_delay_func_detected()) opponent_delay = get_opponent_delay();
		if (current_rollback_func_detected()) current_rollback = get_current_rollback();
		if (max_rollback_func_detected()) max_rollback = get_max_rollback();*/
		if (ui_values_func_detected()) {
			get_ui_values(&uidata);
			//puts(std::to_string(uidata.ping).c_str());
			if (!ui_data_equal(&uidata, &previous_uidata)) {
				puts(ui_data_to_str(&uidata).c_str());
				previous_uidata = uidata; // TODO: THIS IS TEMPORARY. REMOVE THIS AFTER CHECKING THAT THE UI DETAILS MATCH WHILE PLAYING.
			}
		}
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
		//bool warnings_present = false;
		std::string warning_message = "";

		/*is_likely_desynced = (desync_func)GetProcAddress(giuroll, "is_likely_desynced");
		get_ping = (ping_func)GetProcAddress(giuroll, "get_ping");
		get_player_delay = (player_delay_func)GetProcAddress(giuroll, "get_player_delay");
		get_opponent_delay = (opponent_delay_func)GetProcAddress(giuroll, "get_opponent_delay");
		get_current_rollback = (current_rollback_func)GetProcAddress(giuroll, "get_current_rollback");
		get_max_rollback = (max_rollback_func)GetProcAddress(giuroll, "get_max_rollback");*/
		get_ui_values = (ui_values_func)GetProcAddress(giuroll, "get_ui_values");

		if (gl) {
			/*if (!desync_func_detected()) {
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
			}*/

			if (!ui_values_func_detected()) {
				puts("WARNING: UI VALUES FUNCTION NOT DETECTED. MAKE SURE YOU ARE USING GIUROLL VERSION 0.6.18 OR LATER.");
				warning_message += "UI values function not detected (Giuroll >= 0.6.18).\n";
			}
			else {
				puts("Hooked UI values func.");
			}
		}
		else {
			puts("WARNING: GIUROLL NOT DETECTED.");
			// Didn't add to warning_message because most people would theoretically turn off only giuroll to use sokuroll. They would probably leave this mod active.
		}

		if (!warning_message.empty()) {
			int msgboxID = MessageBox(
				NULL,
				(LPCTSTR)warning_message.c_str(),
				(LPCTSTR)"Warning",
				MB_OK | MB_ICONWARNING
			);
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
