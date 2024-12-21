//
// Created by _Kookie
//

#include <SokuLib.hpp>
#include <libloaderapi.h>
#include <string>
#include <sstream>
#include <fstream>

const float MIN_TEXT_X = 10;
const float MIN_TEXT_Y = 0;
const float MAX_TEXT_X = 630;
const float MAX_TEXT_Y = 465;

const int TEXT_REF_ADDRESS = 0x882940;

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
		lhs->valid == rhs->valid &&
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
	if (!data->valid) ss << "ui values is invalid." << std::endl;
	else {
		ss  << "Valid: " << data->valid << "\n"
			<< "Likely Desynced: " << data->likely_desynced << "\n"
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
	auto out = UIValues {
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

// Didn't set these as pointers - get_ui_values simply changes the values within these UIValue struct instances. I am worried about memory leaks when using Box in rust.
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

// ##### TEXT SPRITES #####
//static SokuLib::DrawUtils::Sprite desync_text;
//static SokuLib::Vector2i display_text_real_size;
//static int value = 0;

// not real text to be displayed. 
// Helps judge how far from the bottom left corner of the screen the desync_text should be - don't want to cover the delay number.

class NoValueField {
public:
	float x;
	const char* label;

	NoValueField(float x, const char* label_text, SokuLib::SWRFont* label_font) {
		this->x = x;
		this->label_font = label_font;

		label_sprite = SokuLib::DrawUtils::Sprite();
		label_real_size = SokuLib::Vector2i();
		setLabel(label_text);
	}

	void setLabel(const char* inp_label) {
		label = inp_label;
		label_sprite.texture.createFromText(label, *label_font, { 300, 300 }, &label_real_size);
		label_sprite.setSize(label_real_size.to<unsigned>());
		label_sprite.rect.width = label_real_size.x;
		label_sprite.rect.height = label_real_size.y;
	}

	void setPosition(float x) {
		this->x = x;
		label_sprite.setPosition(SokuLib::Vector2i{ (int)(x + MIN_TEXT_X) , (int)MAX_TEXT_Y - 2});
	}


	void render() const {
		label_sprite.draw();
	}

protected:
	SokuLib::DrawUtils::Sprite label_sprite;
	SokuLib::Vector2i label_real_size;
	SokuLib::SWRFont* label_font;
};


class ValueField: public NoValueField {
public:
	int value = 0;

	ValueField(float x, const char* label, SokuLib::SWRFont* label_font): NoValueField(x, label, label_font) {}

	void render() const {
		std::stringstream ss;
		ss << "value: " << value << std::endl;
		puts(ss.str().c_str());
		//NoValueField::render();

		// ((SokuLib::CNumber*)TEXT_REF_ADDRESS)->render(
		// 	value,
		// 	MIN_TEXT_X + x + (float)label_sprite.rect.width + (float)5,
		// 	MAX_TEXT_Y,
		// 	0,
		// 	false);
	}
};

class FieldController {
public:
	NoValueField* desynced_field;
	ValueField* player_delay_field;

	FieldController() {}

	void create() {
		loadFonts();
		desynced_field = new NoValueField(50, "DESYNCED", &desync_detect_font); // doing this because non-pointer desynced_field will give an error.
		desynced_field->setPosition(50.0f);

		player_delay_field = new ValueField(50, "PL_DLY: ", &player_delay_font);
		player_delay_field->setPosition(0.0f);
	}

	void loadFonts()
	{
		std::ofstream outfile("unique_filename.txt");
		outfile << "initialised fonts." << std::endl;
		outfile.close();
		// Only call this when a match has started.
		SokuLib::FontDescription desync_desc;

		desync_desc.r1 = 255;
		desync_desc.g1 = 0;
		desync_desc.b1 = 0;

		desync_desc.r2 = 255;
		desync_desc.g2 = 0;
		desync_desc.b2 = 0;

		desync_desc.height = 13;
		desync_desc.weight = FW_NORMAL;
		desync_desc.italic = 0;
		desync_desc.shadow = 4;
		desync_desc.useOffset = 0;
		desync_desc.bufferSize = 1000000;

		desync_desc.offsetX = 0;
		desync_desc.offsetY = 0;
		desync_desc.charSpaceX = 0;
		desync_desc.charSpaceY = 0;
		strcpy(desync_desc.faceName, "MonoSpatialModSWR");
		desync_detect_font = SokuLib::SWRFont();
		desync_detect_font.create();
		desync_detect_font.setIndirect(desync_desc);

		SokuLib::FontDescription player_delay_desc;

		player_delay_desc.r1 = 255;
		player_delay_desc.g1 = 255;
		player_delay_desc.b1 = 255;

		player_delay_desc.r2 = 255;
		player_delay_desc.g2 = 255;
		player_delay_desc.b2 = 255;

		player_delay_desc.height = 13;
		player_delay_desc.weight = FW_NORMAL;
		player_delay_desc.italic = 0;
		player_delay_desc.shadow = 4;
		player_delay_desc.useOffset = 0;
		player_delay_desc.bufferSize = 1000000;

		player_delay_desc.offsetX = 0;
		player_delay_desc.offsetY = 0;
		player_delay_desc.charSpaceX = 0;
		player_delay_desc.charSpaceY = 0;
		strcpy(player_delay_desc.faceName, "MonoSpatialModSWR");
		player_delay_font = SokuLib::SWRFont();
		player_delay_font.create();
		player_delay_font.setIndirect(player_delay_desc);
	}

	void render(bool is_likely_desynced) const {
		if (is_likely_desynced) {
			desynced_field->render();
		}
		//player_delay_field->render();
	}

private:
	SokuLib::SWRFont desync_detect_font;
	SokuLib::SWRFont player_delay_font;

};

//static NoValueField desynced_field(50, "DESYNCED");
//static ValueField ping_field(0, "Ping:");
static FieldController field_controller;

// ##### GIUROLL HANDLE #####
static HMODULE giuroll;

// static int myValue = 74;
// static int num = ((SokuLib::CNumber*)0x882940)->value->getInt();
// static int x = 200;
// static int y = 100;
// static int h = 50;
// static int w = 50;
// static int fontSpacing = 5;
// static float textSpacing = 5;
// static int size = 60;
// static int floatSize = 60;
// static bool isAbsolute = false;
// static SokuLib::CNumber number(num, x, y, x + w, y + h, fontSpacing, textSpacing, size, floatSize, isAbsolute);

typedef void(__cdecl* ui_values_func)(UIValues*);
ui_values_func get_ui_values;

bool __fastcall giuroll_loaded() { return giuroll != NULL; }
bool __fastcall ui_values_func_detected() { return get_ui_values != NULL; }

// void setText(SokuLib::DrawUtils::Sprite& text, const char* content) {
// 	/*
// 	* Sets the content of the text.
// 	*/
// 	text.texture.createFromText(content, font, { 300, 300 }, &display_text_real_size);
// 	text.setSize(display_text_real_size.to<unsigned>());
// 	text.rect.width = display_text_real_size.x;
// 	text.rect.height = display_text_real_size.y;
// }

// void moveText(SokuLib::DrawUtils::Sprite& text, int xpos, int ypos) {
// 	/*
// 	* Moves the text to the corresponding x and y position.
// 	* The origin (0,0) is in the top left corner of the game window.
// 	*/
// 	text.setPosition(SokuLib::Vector2i{ xpos, ypos });
// }


void __fastcall Generic_OnRender() {
	//if (!ui_data_equal(&uidata, &null_uidata) && uidata.likely_desynced) desynced_field.render();
	//((SokuLib::CNumber*)0x882940)->render(uidata.player_delay, MIN_TEXT_X, MIN_TEXT_Y, 0, false);
	//ping_field.render();
	//number.render(200, 100);

	field_controller.render(uidata.likely_desynced);
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
	field_controller.create();
	// set delay spacing text
	//delay_spacing_text.texture.createFromText("00", font, {300, 300}, &delay_text_real_size);

	// set desync text.
	//setText(desync_text, "DESYNCED");
	//moveText(desync_text, delay_text_real_size.x, 480 - display_text_real_size.y - 3);

	frame_changed = true;
}

void __fastcall Generic_OnProcess(void* This, char* init_message) {
	if (!init) {
		puts(init_message);
		OnLoad(This);
		init = true;
	}
	if (frame_changed) {
		if (ui_values_func_detected()) {
			get_ui_values(&uidata);
			//field_controller.player_delay_field->value = uidata.player_delay;
			//ping_field.value = uidata.ping;
			// if (!ui_data_equal(&uidata, &previous_uidata)) {
			// 	puts(ui_data_to_str(&uidata).c_str());
			// 	previous_uidata = uidata; // TODO: THIS IS TEMPORARY. REMOVE THIS AFTER CHECKING THAT THE UI DETAILS MATCH WHILE PLAYING.
			// }
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

		get_ui_values = (ui_values_func)GetProcAddress(giuroll, "get_ui_values");

		if (gl) {
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
