#include "../../dllmain.hpp"
#include "../xlln-module.hpp"
#include "coop.hpp"
#include "../../utils/util-hook.hpp"
#include "../../xlivelessness.hpp"
#include "../title-modules/Networking/Memory/bitstream.hpp"
#include "../title-modules/Blam/Cache/DataTypes.h"
#include "../title-modules/Blam/Engine/Objects/ObjectPlacementData.h"
#include "../title-modules/Blam/Engine/Players/Players.h"
#include <chrono>
#include <map>

bool coop_mode_fix_active = false;
uint8_t coop_mode_fix_setting_state = 0;
static bool is_in_game = false;
static bool has_first_spawn_occurred = false;
static std::chrono::steady_clock::time_point *time_all_dead = 0;
static real_point3d *checkpoint_player_position = 0;
static bool triggered_non_host_fixes = false;

s_datum_array* game_state_actors = nullptr;
s_datum_array* game_state_objects_header = nullptr;

// FIXME put this somewhere else.
void get_object_table_memory()
{
	game_state_actors = *GetOffsetAddressCS<s_datum_array**>(0xA965DC, 0x9A1C5C);
	game_state_objects_header = *GetOffsetAddressCS<s_datum_array**>(0x4E461C, 0x50C8EC);
}

#pragma region Local_Functions

static int get_unit_datum_from_player_index(int player_index)
{
	int unit = 0;
	uint32_t player_table_ptr = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004a8260);
	player_table_ptr += 0x44;
	
	unit = (int)*(int*)(*(uint32_t*)player_table_ptr + (player_index * 0x204) + 0x28);
	
	return unit;
}

static int GetPlayerDyanamic(int index)
{
	int dyanamic = 0;
	int playerdatum = get_unit_datum_from_player_index(index);
	
	if (playerdatum != -1) {
		dyanamic = *(uint32_t*)(*(uint32_t*)(0x3003cef0 + 68) + 12 * (uint16_t)playerdatum + 8);
	}
	
	return dyanamic;
}

static int GetNumPlayersAlive()
{
	int playerAliveCount = 0;
	
	for (int i = 0; i < 15; i++) {
		if (GetPlayerDyanamic(i) != 0) {
			if (*(BYTE*)(GetPlayerDyanamic(i) + 0x3F4) == 0x1) {
				playerAliveCount++;
			}
		}
	}
	
	return playerAliveCount;
}

static void call_SpawnPlayer(int player_index)
{
	typedef void(__cdecl * Spawn)(int32_t);
	Spawn pSpawn = (Spawn)GetOffsetAddress(CLIENT_11122, 0x00055952);
	pSpawn(player_index);
}

char call_RestartLevel()
{
	typedef char(__cdecl * Rest)();
	Rest pRest = (Rest)GetOffsetAddress(CLIENT_11122, 0x0023e6bc);
	return pRest();
}

typedef int(__cdecl p_unit_kill)(datum UnitDatum);
p_unit_kill* c_unit_kill;
void UnitKill(datum UnitDatum)
{
	c_unit_kill	= GetOffsetAddressCS<p_unit_kill*>(0x13B514, 0x12A363);
	c_unit_kill(UnitDatum);
}

void __cdecl call_object_placement_data_new(ObjectPlacementData* s_object_placement_data, datum object_definition_index, datum object_owner, int unk)
{
	//LOG_TRACE_GAME("object_placement_data_new(s_object_placement_data: %08X,",s_object_placement_data);
	//LOG_TRACE_GAME("object_definition_index: %08X, object_owner: %08X, unk: %08X)", object_definition_index, object_owner, unk);
	
	typedef void(__cdecl object_placement_data_new)(void*, datum, datum, int);
	auto pobject_placement_data_new = GetOffsetAddressCS<object_placement_data_new*>(0x132163, 0x121033);
	
	pobject_placement_data_new(s_object_placement_data, object_definition_index, object_owner, unk);
}

bool __cdecl call_add_object_to_sync(datum gamestate_object_datum)
{
	typedef int(__cdecl add_object_to_sync)(datum gamestate_object_datum);
	auto p_add_object_to_sync = GetOffsetAddressCS<add_object_to_sync*>(0x1B8D14, 0x1B2C44);
	
	return p_add_object_to_sync(gamestate_object_datum) != 0;
}

static bool IsLobbyHost()
{
	uint32_t LB = (uint32_t)*(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00420fe8);
	uint32_t HID = *(uint32_t*)(LB + 0x125c);
	uint32_t LID = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x0051a629);
	// Check User Status(Ingame/Lobby etc..)
	uint32_t &stausCheck = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00420fc4);
	
	if (stausCheck > 0) {
		if (LID == HID) {
			return true;
		}
		else {
			return false;
		}
	}
	else {
		return false;
	}
}

BYTE* get_player_unit_from_player_index(int playerIndex)
{
	datum unit_datum = Player::getPlayerUnitDatumIndex(playerIndex);
	if (unit_datum.IsNull()) {
		return nullptr;
	}
	
	DatumIterator<ObjectHeader> objectsIt(game_state_objects_header);
	return (BYTE*)objectsIt.get_data_at_index(unit_datum.ToAbsoluteIndex())->object;
}

real_point3d* get_player_unit_coords(int playerIndex)
{
	BYTE* player_unit = get_player_unit_from_player_index(playerIndex);
	if (player_unit != nullptr) {
		return reinterpret_cast<real_point3d*>(player_unit + 0x64);
	}
	
	return nullptr;
}

// controller index aka local player index -> player index
datum get_player_datum_index_from_controller_index(int controller_index)
{
	typedef int(__cdecl* get_local_player_index)(int controller_index);
	auto p_get_local_player_index = reinterpret_cast<get_local_player_index>(GetOffsetAddress(CLIENT_11122, 0x5141D));
	return p_get_local_player_index(controller_index);
}

BipedObjectDefinition* GetPlayerBipedObjectDefinition(datum unit_datum)
{
	DatumIterator<ObjectHeader> objectIt(game_state_objects_header);
	BipedObjectDefinition* playerUnit = (BipedObjectDefinition*)objectIt.get_data_at_index(unit_datum.ToAbsoluteIndex())->object;
	
	return playerUnit;
}

// FIXME
float* convert_quaternion_to_euler_angles_2d(float* euler_rotation, float* quaternion_rotation)
{
	return euler_rotation;
}

char* __cdecl call_object_try_and_get_data_with_type(datum object_datum_index, int object_type_flags)
{
	//LOG_TRACE_GAME("call_get_object( object_datum_index: %08X, object_type: %08X )", object_datum_index, object_type);
	
	typedef char*(__cdecl get_object)(datum object_datum_index, int object_type_flags);
	auto p_get_object = GetOffsetAddressCS<get_object*>(0x1304E3, 0x11F3A6);
	return p_get_object(object_datum_index, object_type_flags);
}

int object_get_position_rotation(datum unit_datum_index, float* position, float* euler_rotation)
{
	int unit_object = (int)call_object_try_and_get_data_with_type(unit_datum_index, 3);
	
	if (!unit_object) {
		return 0;
	}
	
	if (position) {
		position[0] = *(float*)(unit_object + 0x64);
		position[1] = *(float*)(unit_object + 0x68);
		position[2] = *(float*)(unit_object + 0x6C);
	}
	if (euler_rotation) {
		float quaternionRotation[3];
		quaternionRotation[0] = *(float*)(unit_object + 0x70);
		quaternionRotation[1] = *(float*)(unit_object + 0x74);
		quaternionRotation[2] = *(float*)(unit_object + 0x78);
		convert_quaternion_to_euler_angles_2d(euler_rotation, quaternionRotation);
	}
	return unit_object;
}

static bool IsCampaignMap()
{
	char *mapName = (char*)GetOffsetAddress(CLIENT_11122, 0x0047cf0c);
	
	if (
		!strcmp(mapName, "01a_tutorial")
		|| !strcmp(mapName, "01b_spacestation")
		|| !strcmp(mapName, "03a_oldmombasa")
		|| !strcmp(mapName, "03b_newmombasa")
		|| !strcmp(mapName, "04a_gasgiant")
		|| !strcmp(mapName, "04b_floodlab")
		|| !strcmp(mapName, "05a_deltaapproach")
		|| !strcmp(mapName, "05b_deltatowers")
		|| !strcmp(mapName, "06a_sentinelwalls")
		|| !strcmp(mapName, "06b_floodzone")
		|| !strcmp(mapName, "07a_highcharity")
		|| !strcmp(mapName, "07b_forerunnership")
		|| !strcmp(mapName, "08a_deltacliffs")
		|| !strcmp(mapName, "08b_deltacontrol")
	) {
		return true;
	}
	
	return false;
}

static void GetSetMapThings(uint8_t *coop_player_biped, uint8_t *coop_player_team)
{
	char *mapName = (char*)GetOffsetAddress(CLIENT_11122, 0x0047cf0c);
	
	if (coop_player_biped) {
		*coop_player_biped = 0;
	}
	if (coop_player_team) {
		*coop_player_team = 0;
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Current map name (%s)."
		, __func__
		, mapName
	);
	
	if (
		!strcmp(mapName, "01a_tutorial")
		|| !strcmp(mapName, "01b_spacestation")
		|| !strcmp(mapName, "03a_oldmombasa")
		|| !strcmp(mapName, "03b_newmombasa")
		|| !strcmp(mapName, "05a_deltaapproach")
		|| !strcmp(mapName, "05b_deltatowers")
		|| !strcmp(mapName, "07a_highcharity")
		|| !strcmp(mapName, "07b_forerunnership")
	) {
		if (coop_player_biped) {
			*coop_player_biped = 0;
		}
		if (coop_player_team) {
			*coop_player_team = 2;
		}
	}
	else if (
		!strcmp(mapName, "04a_gasgiant")
		|| !strcmp(mapName, "04b_floodlab")
		|| !strcmp(mapName, "06a_sentinelwalls")
		|| !strcmp(mapName, "06b_floodzone")
		|| !strcmp(mapName, "08a_deltacliffs")
		|| !strcmp(mapName, "08b_deltacontrol")
	) {
		if (coop_player_biped) {
			*coop_player_biped = 1;
		}
		if (coop_player_team) {
			*coop_player_team = 3;
		}
	}
	else {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "%s Unknown Campaign map (%s)."
			, __func__
			, mapName
		);
	}
	
	// Test Override.
	if (coop_player_team) {
		//*coop_player_team = 8; // Default Team.
	}
}

uint32_t GetMapFloodInfectionFormObjectDefinition()
{
	uint32_t objectDefinition = 0;
	
	char *mapName = (char*)GetOffsetAddress(CLIENT_11122, 0x0047cf0c);
	
	if (!strcmp(mapName, "01a_tutorial")) {
	}
	else if (!strcmp(mapName, "01b_spacestation")) {
	}
	else if (!strcmp(mapName, "03a_oldmombasa")) {
	}
	else if (!strcmp(mapName, "03b_newmombasa")) {
	}
	else if (!strcmp(mapName, "04a_gasgiant")) {
	}
	else if (!strcmp(mapName, "04b_floodlab")) {
		objectDefinition = 0xe92d07b6;
	}
	else if (!strcmp(mapName, "05a_deltaapproach")) {
	}
	else if (!strcmp(mapName, "05b_deltatowers")) {
	}
	else if (!strcmp(mapName, "06a_sentinelwalls")) {
		objectDefinition = 0xF39A1224;
	}
	else if (!strcmp(mapName, "06b_floodzone")) {
		objectDefinition = 0xEE140C9E;
	}
	else if (!strcmp(mapName, "07a_highcharity")) {
		objectDefinition = 0xF3A511F7;
	}
	else if (!strcmp(mapName, "07b_forerunnership")) {
		objectDefinition = 0xF04B0E2A;
	}
	else if (!strcmp(mapName, "08a_deltacliffs")) {
	}
	else if (!strcmp(mapName, "08b_deltacontrol")) {
	}
	
	return objectDefinition;
}

static void FixPlayerBipedAndTeams()
{
	uint8_t &playerCount = *(uint8_t*)0x30004b60;
	
	uint8_t coopPlayerBiped = 0;
	uint8_t coopPlayerTeam = 0;
	GetSetMapThings(&coopPlayerBiped, &coopPlayerTeam);
	
	for (int i = 0; i < playerCount; i++) {
		*(int*)(0x30002BA0 + (i * 0x204)) = coopPlayerBiped;
		*(int*)(0x30002BD8 + (i * 0x204)) = coopPlayerTeam;
	}
}

static void ClientFixes()
{
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s()."
		, __func__
	);
	
	uint32_t &gameGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00482d3c);
	uint32_t &gameTimeGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004c06e4);
	
	uint8_t &gameState = *(uint8_t*)GetOffsetAddress(CLIENT_11122, 0x00420fc4);
	uint8_t &playerCount = *(uint8_t*)0x30004b60;
	uint8_t &isGameLost = *(uint8_t*)0x30004b64;
	uint8_t &gameEngine = *(uint8_t*)(gameGlobals + 0x8);
	uint8_t &coopSpawns = *(uint8_t*)(gameGlobals + 0x2A4);
	uint8_t &gameIsPaused = *(uint8_t*)(gameTimeGlobals + 0x1);
	
	// Engine Hack.
	gameEngine = 1; // Single Player.
	// Lets Enable Coop Play.
	//coopSpawns = 1;
	
	return;
	
	void(*AiActivate)();
	AiActivate =(void (*)(void))GetOffsetAddress(CLIENT_11122, 0x0030e67c);
	AiActivate();
	
	signed int(*ObjScripting)();
	ObjScripting =(signed int (*)(void))GetOffsetAddress(CLIENT_11122, 0x0014b083);
	ObjScripting();
	
	void(*ObjEarlyMovers)();
	ObjEarlyMovers =(void (*)(void))GetOffsetAddress(CLIENT_11122, 0x0014b9d8);
	ObjEarlyMovers();
	
	int(*GameTime)();
	GameTime = (int(*)(void))GetOffsetAddress(CLIENT_11122, 0x0007c13d);
	GameTime();
	
	char(*CinematicGlobals)();
	CinematicGlobals = (char(*)(void))GetOffsetAddress(CLIENT_11122, 0x0003a5f3);
	CinematicGlobals();
	
	void(*UserScreen)();
	UserScreen = (void(*)(void))GetOffsetAddress(CLIENT_11122, 0x00227862);
	UserScreen();
	
	int(*SaveState)();
	SaveState =(int (*)(void))GetOffsetAddress(CLIENT_11122, 0x0009e455);
	SaveState();
	
}

#pragma endregion

#pragma region Sync_Units

typedef uint32_t(__cdecl *tunit_entity_creation_definition_packet_size)();
static tunit_entity_creation_definition_packet_size punit_entity_creation_definition_packet_size;

static uint32_t __cdecl unit_entity_creation_definition_packet_size()
{
	uint32_t result;
	if (coop_mode_fix_active) {
		result = 0x30;
	}
	else {
		result = punit_entity_creation_definition_packet_size();
	}
	return result;
}

typedef void(__stdcall *tc_simulation_unit_entity_definition_creation_encode)(void* thisptr, int creation_data_size, void* creation_data, int a3, bitstream* stream);
tc_simulation_unit_entity_definition_creation_encode pc_simulation_unit_entity_definition_encode;

void __stdcall c_simulation_unit_entity_definition_creation_encode(void *thisptr, int creation_data_size, void* creation_data, int a3, bitstream* stream)
{
	if (coop_mode_fix_active) {
		//LOG_TRACE_GAME_N("c_simulation_unit_entity_definition_creation_encode()\r\nthisptr: %08X, creation_data_size: %i, creation_data: %08X, a3: %i, packet: %08X", thisptr, creation_data_size, creation_data, a3, packet);
		int object_permutation_index = *(int*)((char*)creation_data + 0x24);
		if (object_permutation_index != NONE) {
			//LOG_TRACE_GAME_N("creation_data+0x24: %08X", object_permutation_index);
			
			stream->data_encode_bool("object-permutation-exists", 1);
			stream->data_encode_integer("object-permutation-index", object_permutation_index, 32);
			//LOG_TRACE_GAME_N("c_simulation_unit_entity_encode - object-permutation-exists packet: %08X, *packet: %08X", packet, *(int*)packet);
		}
		else {
			stream->data_encode_bool("object-permutation-exists", 0);
		}
	}
	
	pc_simulation_unit_entity_definition_encode(thisptr, creation_data_size, creation_data, a3, stream);
}

typedef bool(__stdcall *tc_simulation_unit_entity_definition_creation_decode)(void* thisptr, int creation_data_size, void* creation_data, bitstream* stream);
tc_simulation_unit_entity_definition_creation_decode pc_simulation_unit_entity_definition_decode;

bool __stdcall c_simulation_unit_entity_definition_creation_decode(void *thisptr, int creation_data_size, void* creation_data, bitstream* stream)
{
	if (coop_mode_fix_active) {
		//LOG_TRACE_GAME_N("c_simulation_unit_entity_definition_creation_decode()\r\nthisptr: %08X, creation_data_size: %i, creation_data: %08X, packet: %08X", thisptr, creation_data_size, creation_data, packet);
		
		if (stream->data_decode_bool("object-permutation-exists")) {
			//LOG_TRACE_GAME_N("c_simulation_unit_entity_decode - object-permutation-exists packet: %08X, *packet: %08X", packet, *(int*)packet);
			int object_permutation_index = stream->data_decode_integer("object-permutation-index", 32);
			*(int*)((char*)creation_data + 0x24) = object_permutation_index;
			
			//LOG_TRACE_GAME_N("object_permutation_index: %08X", object_permutation_index);
		}
		else {
			*(int*)((char*)creation_data + 0x24) = NONE;
		}
	}
	
	return pc_simulation_unit_entity_definition_decode(thisptr, creation_data_size, creation_data, stream);
}

static std::map<int, int> object_to_variant;

static int __cdecl call_object_new(ObjectPlacementData* pObject)
{
	//LOG_TRACE_GAME("object_new(pObject: %08X)", pObject);
	
	typedef int(__cdecl object_new)(void*);
	auto p_object_new = GetOffsetAddressCS<object_new*>(0x136CA7, 0x125B77);
	
	return p_object_new(pObject);
}

signed int __cdecl object_new_hook(ObjectPlacementData* new_object)
{
	int variant_index = *(int*)((char*)new_object + 0xC);
	int result = call_object_new(new_object);
	
	//unsigned __int16 object_index = result & 0xFFFF;
	
	if (result != NONE) {
		object_to_variant[result] = variant_index;
	}
	
	//wchar_t DebugText[255] = { 0 };
	//SecureZeroMemory(DebugText, sizeof(DebugText));
	//wsprintf(DebugText, L"AI object_new hook - object_index: %08X - variant_index: %08X - datum: %08X", result, variant_index);
	
	//LOG_TRACE_GAME_N("AI object_new hook - object_index: %08X - variant_index: %08X - datum: %08X", result, variant_index);
	
	//addDebugText(DebugText);
	
	return result;
}

void __stdcall set_unit_creation_data_hook(unsigned int object_index, void* object_creation_data)
{
	typedef void(__stdcall *tset_unit_creation_data)(unsigned int object_index, void* object_creation_data);
	GetOffsetAddressCS<tset_unit_creation_data>(0x1F24ED, 0x1DD586)(object_index, object_creation_data);
	
	if (coop_mode_fix_active) {
		if (object_to_variant.find(object_index) != object_to_variant.end()) {
			// We should have allocated an additional 4 bytes above 0x24 so we'll write our in between 0x24 and 0x28.
			*(int*)((char*)object_creation_data + 0x24) = object_to_variant[object_index];
			
			//wchar_t DebugText[255] = { 0 };
			//SecureZeroMemory(DebugText, sizeof(DebugText));
			//wsprintf(DebugText, L"AI unit_creation_data_setup hook - object_index: %08X - variant_index: %08X", object_index, object_to_variant[object_index]);
			
			//LOG_TRACE_GAME_N("set_unit_creation_data_hook - object_index: %08X, variant_index: %08X", object_index, object_to_variant[object_index]);
			//addDebugText(DebugText);
		}
		else {
			*(int*)((char*)object_creation_data + 0x24) = NONE;
		}
	}
}

bool __stdcall create_unit_hook(void* pCreationData, int a2, int a3, void* pObject)
{
	typedef bool(__stdcall *tcreate_unit_hook)(void*, int, int, void*);
	
	if (coop_mode_fix_active) {
		if (*(int*)((char*)pCreationData + 0x24) != NONE) {
			//wchar_t DebugText[255] = { 0 };
			//SecureZeroMemory(DebugText, sizeof(DebugText));
			//wsprintf(DebugText, L"create_unit_hook - variant type: %08X - ", *(int*)((char*)pCreationData + 0x24));
			
			//addDebugText(DebugText);
			
			*(int*)((char*)pObject + 0xC) = *(int*)((char*)pCreationData + 0x24);
		}
	}
	
	return GetOffsetAddressCS<tcreate_unit_hook>(0x1F32DB, 0x1DE374)(pCreationData, a2, a3, pObject);
}

typedef bool(__cdecl *tset_unit_color_data)(int, unsigned __int16, int a3);
tset_unit_color_data pset_unit_color_data;

bool __cdecl set_unit_color_data_hook(int a1, unsigned __int16 a2, int a3)
{
	if (!coop_mode_fix_active) {
		bool result = pset_unit_color_data(a1, a2, a3);
		return result;
	}
	
	int object_creation_data = a1 - 0x10;
	int object_permutation_index = *(int*)((char*)object_creation_data + 0x24);
	
	//LOG_TRACE_GAME("set_unit_color_data_hook - {:x}", object_permutation_index);
	
	if (object_permutation_index == NONE) {
		return pset_unit_color_data(a1, a2, a3);
	}
	
	return 0;
}

#pragma endregion

#pragma region Hooks

void CoopInitializeGameEngineSystems(uint8_t *a1)
{
	uint32_t &gameGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00482d3c);
	uint8_t &gameEnginePrev = *(uint8_t*)(gameGlobals + 0x8);
	
	uint8_t &gameEngine = *(uint8_t*)a1;
	uint8_t &coopSpawns = *(uint8_t*)(a1 + 0x2a4 - 8);
	uint8_t &someCheck = *(uint8_t*)(a1 + 0x2a8 - 8);
	uint8_t &gameType = *(uint8_t*)(a1 + 0x2ec - 8);
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s EngineMode is (%d)."
		, __func__
		, gameEngine
	);
	
	has_first_spawn_occurred = false;
	triggered_non_host_fixes = false;
	is_in_game = (gameEngine == 1 || gameEngine == 2);
	
	if (coop_mode_fix_setting_state == 0) {
		coop_mode_fix_active = ((gameEnginePrev != 1 && gameEngine != 1) || coop_mode_fix_active) && (is_in_game && IsCampaignMap());
		
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "%s Coop Mode fix %s."
			, __func__
			, coop_mode_fix_active ? "activated" : "deactivated"
		);
	}
	
	if (coop_mode_fix_active) {
		if (is_in_game) {
			if (IsLobbyHost()) {
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
					, "%s EngineMode is (%d) setting it to 1."
					, __func__
					, gameEngine
				);
				
				gameEngine = 1; // Single Player.
				//coopSpawns = 1; // Enable coop spawns.
				someCheck = 0; // Change some check to 0.
				gameType = 0; // Single Player.
			}
		}
		else {
			
		}
	}
}

void CoopSpawnPlayer(int32_t player_index)
{
	if (coop_mode_fix_active && is_in_game) {
		if (!has_first_spawn_occurred) {
			has_first_spawn_occurred = true;
			
			if (IsLobbyHost()) {
				FixPlayerBipedAndTeams();
			}
			else {
				FixPlayerBipedAndTeams();
			}
		}
	}
}

void CoopSetPlayerUnitDatumAndModel(int32_t player_index, int32_t *unit_datum, int32_t *player_model)
{
	if (coop_mode_fix_active && is_in_game) {
		if (!IsLobbyHost()) {
			uint8_t coopPlayerBiped = 0;
			GetSetMapThings(&coopPlayerBiped, 0);
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
				, "%s Setting Player Index (%d) player model from (%d) to (%hhu)."
				, __func__
				, player_index
				, *player_model
				, coopPlayerBiped
			);
			
			*player_model = coopPlayerBiped;
			
			FixPlayerBipedAndTeams();
			
			// Engine mode swap to single player breaks audio for non-hosts if done at different points in time. This position appears to work still.
			if (!triggered_non_host_fixes) {
				triggered_non_host_fixes = true;
				ClientFixes();
			}
		}
	}
}

typedef uint32_t(__cdecl *tRestartMissionDeadCoopPlayerLegendaryDifficulty)();
static tRestartMissionDeadCoopPlayerLegendaryDifficulty pRestartMissionDeadCoopPlayerLegendaryDifficulty;

static uint32_t __cdecl RestartMissionDeadCoopPlayerLegendaryDifficulty()
{
	uint32_t result = pRestartMissionDeadCoopPlayerLegendaryDifficulty();
	
	if (coop_mode_fix_active && is_in_game) {
		// Do not restart the mission if a player has died on legendary difficulty.
		result = 1;
	}
	
	return result;
}

static uint32_t __cdecl EnableCoopPlayerWaypointIndicator1()
{
	uint32_t result = 1;
	
	return result;
}

static uint32_t __cdecl EnableCoopPlayerWaypointIndicator2()
{
	uint32_t result = 0;
	
	return result;
}

typedef uint32_t(__cdecl *tSaveCheckpoint)(uint8_t);
static tSaveCheckpoint pSaveCheckpoint;

static uint32_t __cdecl SaveCheckpoint(uint8_t a1)
{
	uint32_t result = pSaveCheckpoint(a1);
	
	if (coop_mode_fix_active && is_in_game && a1 == 0) {
		int localPlayerIndex = get_player_datum_index_from_controller_index(0).ToAbsoluteIndex();
		
		// Is player alive.
		if (GetPlayerDyanamic(localPlayerIndex) != 0 && *(BYTE*)(GetPlayerDyanamic(localPlayerIndex) + 0x3F4) == 0x1) {
			real_point3d checkpointPosition = *get_player_unit_coords(localPlayerIndex);
			
			datum unit_datum = Player::getPlayerUnitDatumIndex(localPlayerIndex);
			BipedObjectDefinition *bipedObjectDefinition = GetPlayerBipedObjectDefinition(unit_datum);
			
			while (!bipedObjectDefinition->ParentIndex.IsNull()) {
				object_get_position_rotation(bipedObjectDefinition->ParentIndex, (float*)&checkpointPosition, 0);
				checkpointPosition.z += 1;
				
				bipedObjectDefinition = GetPlayerBipedObjectDefinition(bipedObjectDefinition->ParentIndex);
			}
			
			if (checkpoint_player_position) {
				delete checkpoint_player_position;
			}
			checkpoint_player_position = new real_point3d();
			*checkpoint_player_position = checkpointPosition;
			
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
				, "%s Coop Mode spawn point set at (x,y,z) (%.2f, %.2f, %.2f)."
				, __func__
				, checkpoint_player_position->x
				, checkpoint_player_position->y
				, checkpoint_player_position->z
			);
		}
	}
	
	return result;
}

typedef uint8_t(__cdecl *tPauseGame)(uint8_t);
static tPauseGame pPauseGame;

static uint8_t __cdecl PauseGame(uint8_t pause_game)
{
	if (coop_mode_fix_active && is_in_game) {
		// To fix the pausing of the game when the escape pause screen is opened.
		pause_game = 0;
	}
	
	uint8_t result = pPauseGame(pause_game);
	
	return result;
}

typedef uint32_t(__cdecl *tSpawnFloodCarrierChildren)(uint32_t, uint32_t, uint32_t object_definition, uint32_t, uint8_t);
static tSpawnFloodCarrierChildren pSpawnFloodCarrierChildren;

bool block_flood_infection_forms = false;
static uint32_t __cdecl SpawnFloodCarrierChildren(uint32_t a1, uint32_t a2, uint32_t object_id, uint32_t a4, uint8_t a5)
{
	if (block_flood_infection_forms) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "%s Blocking spawn of Flood Infection Form.\nObject Definition: 0x%08x."
			, __func__
			, object_id
		);
		
		return -1;
	}
	
	uint32_t result = pSpawnFloodCarrierChildren(a1, a2, object_id, a4, a5);
	
	// call_add_object_to_sync(result);
	
	return result;
}

typedef void(__cdecl *tobject_placement_data_new)(ObjectPlacementData *s_object_placement_data, datum object_definition_index, datum object_owner, int unk);
static tobject_placement_data_new pobject_placement_data_new;

static void __cdecl object_placement_data_new(ObjectPlacementData *s_object_placement_data, datum object_definition_index, datum object_owner, int unk)
{
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Object Definition: 0x%08x."
		, __func__
		, (uint32_t)object_definition_index.data
	);
	pobject_placement_data_new(s_object_placement_data, object_definition_index, object_owner, unk);
}

#pragma endregion

#pragma region Improper crash fix hooks

typedef uint8_t(__stdcall *tCrashy)(void*, uint32_t, uint32_t, uint32_t, uint32_t);
static tCrashy pCrashy001;

// Function at which game usually crashes.
static uint8_t __stdcall h_Crashy001(void *thisPtr, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	uint32_t myv5 = *(uint32_t*)(a2 + 8);
	//uint32_t mycrashy= (*(uint32_t*)(*(uint32_t*)(0x3003CEF0 + 68) + 12 * (uint16_t)myv5 + 8) + 0xAA);
	
	if ((int16_t)myv5 == -1) {
		//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		//	, "%s Blocked Object Datum: 0x%08x."
		//	, __func__
		//	, (uint32_t)myv5
		//);
		return 0;
	}
	
	uint32_t D1 = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004e461c);
	uint32_t D2 = *(uint32_t*)(D1 + 68);
	uint32_t D3 = (D2 + 12 * (uint16_t)myv5 + 8);
	uint32_t D4 = *(uint32_t*)D3;
	
	//uint32_t DA = *(uint32_t*)(0x3003CEF0 + 68) + 12 * (uint16_t)myv5 + 8;
	
	//if (Coop.CoopGame == TRUE) {
		//*(uint8_t*)mycrashy = 1;
	//}
	
	if (D4 != 0) {
		return pCrashy001(thisPtr, a2, a3, a4, a5);
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s D4 Blocked Object Datum: 0x%08x."
		, __func__
		, (uint32_t)myv5
	);
	
	// typedef void(__cdecl *tdatum_delete)(uint32_t, uint16_t datum_index);
	// tdatum_delete pdatum_delete = (tdatum_delete)GetOffsetAddress(CLIENT_11122, 0x0006693e);
	// pdatum_delete(D1, (uint16_t)myv5);
	
	return 0;
}

typedef int32_t(__cdecl *tCrashy2)(uint16_t a1, int16_t a2);
static tCrashy2 pCrashy2;

static int32_t __cdecl Crashy2(uint16_t a1, int16_t a2)
{
	//int32_t DA = *(uint32_t*)(0x3003CEF0 + 68) + 12 * a1 + 8;
	
	int32_t result = pCrashy2(a1, a2);
	
	if (result != -1) {
		//uint32_t DA = *(uint32_t*)(*(uint32_t*)(*(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004e461c) + 68) + 12 * (uint16_t)result + 8);
		uint32_t D1 = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004e461c);
		uint32_t D2 = *(uint32_t*)(D1 + 68);
		uint32_t D3 = (D2 + 12 * (uint16_t)result + 8);
		uint32_t D4 = *(uint32_t*)D3;
		if (D4 == 0 || D4 > 0xa0000000) {
			XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
				, "%s D4 Blocked Object Datum: 0x%08x."
				, __func__
				, (uint32_t)result
			);
			
			// typedef void(__cdecl *tdatum_delete)(uint32_t, uint16_t datum_index);
			// tdatum_delete pdatum_delete = (tdatum_delete)GetOffsetAddress(CLIENT_11122, 0x0006693e);
			// pdatum_delete(D1, (uint16_t)result);
			
			return -1;
		}
	}
	
	return result;
}

typedef void(__cdecl *tsimulation_game_process_player_prediction)(int32_t player_index, uint32_t *prediction_axes);
static tsimulation_game_process_player_prediction psimulation_game_process_player_prediction;

static void __cdecl simulation_game_process_player_prediction(int32_t player_index, uint32_t *prediction_axes)
{
	typedef int32_t(__cdecl *tsimulation_gamestate_entity_get_object_index)(uint32_t);
	tsimulation_gamestate_entity_get_object_index psimulation_gamestate_entity_get_object_index = (tsimulation_gamestate_entity_get_object_index)GetOffsetAddress(CLIENT_11122, 0x001f2211);
	int32_t objectIndex = psimulation_gamestate_entity_get_object_index(prediction_axes[0]);
	
	if ((int16_t)objectIndex == -1) {
		//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		//	, "%s Blocked Object Datum: 0x%08x."
		//	, __func__
		//	, (uint32_t)objectIndex
		//);
		return;
	}
	
	//0x8C11F;
	//0x13690B;
	
	//uint32_t DA = *(uint32_t*)(*(uint32_t*)(*(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004e461c) + 68) + 12 * (uint16_t)result + 8);
	uint32_t D1 = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004e461c);
	uint32_t D2 = *(uint32_t*)(D1 + 68);
	uint32_t D3 = (D2 + 12 * (uint16_t)objectIndex + 8);
	uint32_t D4 = *(uint32_t*)D3;
	if (D4 == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "%s D4 Blocked Object Datum: 0x%08x."
			, __func__
			, (uint32_t)objectIndex
		);
		
		// typedef void(__cdecl *tdatum_delete)(uint32_t, uint16_t datum_index);
		// tdatum_delete pdatum_delete = (tdatum_delete)GetOffsetAddress(CLIENT_11122, 0x0006693e);
		// pdatum_delete(D1, (uint16_t)objectIndex);
		
		return;
	}
	
	psimulation_game_process_player_prediction(player_index, prediction_axes);
}

typedef void(__cdecl *tsimulation_action_actor_create)(datum);
static tsimulation_action_actor_create psimulation_action_actor_create;

static void __cdecl simulation_action_actor_create(datum object_datum)
{
	if (object_datum.Index == -1) {
		//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		//	, "%s Blocked Object Datum: 0x%08x."
		//	, __func__
		//	, (uint32_t)object_datum.data
		//);
		return;
	}
	uint32_t D1 = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004e461c);
	uint32_t D2 = *(uint32_t*)(D1 + 68);
	uint32_t D3 = (D2 + 12 * (uint16_t)object_datum.data + 8);
	uint32_t D4 = *(uint32_t*)D3;
	if (D4 == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "%s D4 Blocked Object Datum: 0x%08x."
			, __func__
			, (uint32_t)object_datum.data
		);
		return;
	}
	psimulation_action_actor_create(object_datum);
}

typedef int32_t(__cdecl *titem_in_unit_inventory)(datum);
static titem_in_unit_inventory pitem_in_unit_inventory;

static int32_t __cdecl item_in_unit_inventory(datum object_datum)
{
	if (object_datum.Index == -1) {
		//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		//	, "%s Blocked Object Datum: 0x%08x."
		//	, __func__
		//	, (uint32_t)object_datum.data
		//);
		return 0;
	}
	uint32_t D1 = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004e461c);
	uint32_t D2 = *(uint32_t*)(D1 + 68);
	uint32_t D3 = (D2 + 12 * (uint16_t)object_datum.data + 8);
	uint32_t D4 = *(uint32_t*)D3;
	if (D4 == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "%s D4 Blocked Object Datum: 0x%08x."
			, __func__
			, (uint32_t)object_datum.data
		);
		return 0;
	}
	int32_t result = pitem_in_unit_inventory(object_datum);
	return result;
}

typedef uint8_t(__stdcall *tsub_001fa92b)(uint32_t, uint32_t*, uint32_t, int16_t*);
static tsub_001fa92b psub_001fa92b;

static uint8_t __stdcall sub_001fa92b(uint32_t a1, uint32_t *a2, uint32_t a3, int16_t *a4)
{
	typedef uint32_t(__cdecl *tsimulation_gamestate_entity_get_object_index)(uint32_t entity_seed);
	tsimulation_gamestate_entity_get_object_index psimulation_gamestate_entity_get_object_index = (tsimulation_gamestate_entity_get_object_index)GetOffsetAddress(CLIENT_11122, 0x001f2211);
	datum object_datum = psimulation_gamestate_entity_get_object_index(*a2);
	
	if (object_datum.Index == -1) {
		//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		//	, "%s Blocked Object Datum: 0x%08x."
		//	, __func__
		//	, (uint32_t)object_datum.data
		//);
		return 0;
	}
	uint32_t D1 = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004e461c);
	uint32_t D2 = *(uint32_t*)(D1 + 68);
	uint32_t D3 = (D2 + 12 * (uint16_t)object_datum.data + 8);
	uint32_t D4 = *(uint32_t*)D3;
	if (D4 == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "%s D4 Blocked Object Datum: 0x%08x."
			, __func__
			, (uint32_t)object_datum.data
		);
		return 0;
	}
	uint8_t result = psub_001fa92b(a1, a2, a3, a4);
	return result;
}

typedef uint32_t(__cdecl *tsub_001329b8)(uint16_t, uint32_t);
static tsub_001329b8 psub_001329b8;

// object_get_origin.
static uint32_t __cdecl sub_001329b8(uint16_t a1, uint32_t a2)
{
	if (a1 == -1) {
		//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		//	, "%s Blocked Object Datum: 0x%04hx."
		//	, __func__
		//	, a1
		//);
		return 0;
	}
	uint32_t D1 = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004e461c);
	uint32_t D2 = *(uint32_t*)(D1 + 68);
	uint32_t D3 = (D2 + 12 * (uint16_t)a1 + 8);
	uint32_t D4 = *(uint32_t*)D3;
	if (D4 == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "%s D4 Blocked Object Datum: 0x%04hx."
			, __func__
			, a1
		);
		return 0;
	}
	uint32_t result = psub_001329b8(a1, a2);
	return result;
}

typedef uint32_t(__cdecl *tsub_001305f8)(datum);
static tsub_001305f8 psub_001305f8;

// object_get_ultimate_parent.
static uint32_t __cdecl sub_001305f8(datum object_datum)
{
	if (object_datum.Index == -1) {
		//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		//	, "%s Blocked Object Datum: 0x%08x."
		//	, __func__
		//	, (uint32_t)object_datum.data
		//);
		return -1;
	}
	uint32_t D1 = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004e461c);
	uint32_t D2 = *(uint32_t*)(D1 + 68);
	uint32_t D3 = (D2 + 12 * (uint16_t)object_datum.Index + 8);
	uint32_t D4 = *(uint32_t*)D3;
	if (D4 == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "%s D4 Blocked Object Datum: 0x%08x."
			, __func__
			, (uint32_t)object_datum.data
		);
		return -1;
	}
	uint32_t result = psub_001305f8(object_datum);
	return result;
}

typedef void(__cdecl *tsimulation_game_interface_motion_from_control)(uint32_t, datum, real_euler_angles2d*);
static tsimulation_game_interface_motion_from_control psimulation_game_interface_motion_from_control;

static void __cdecl simulation_game_interface_motion_from_control(uint32_t a1, datum object_datum, real_euler_angles2d *a3)
{
	if (object_datum.Index == -1) {
		//XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		//	, "%s Blocked Object Datum: 0x%08x."
		//	, __func__
		//	, (uint32_t)object_datum.data
		//);
		return;
	}
	uint32_t D1 = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004e461c);
	uint32_t D2 = *(uint32_t*)(D1 + 68);
	uint32_t D3 = (D2 + 12 * (uint16_t)object_datum.data + 8);
	uint32_t D4 = *(uint32_t*)D3;
	if (D4 == 0) {
		XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
			, "%s D4 Blocked Object Datum: 0x%08x."
			, __func__
			, (uint32_t)object_datum.data
		);
		return;
	}
	psimulation_game_interface_motion_from_control(a1, object_datum, a3);
}

#pragma endregion

#pragma region Main

// For testing.
void FloodSpawner1()
{
	uint32_t hostUnitDatum = 0xFFFFFFFF;
	const uint32_t objectDefinitionFloodInfectionForm = GetMapFloodInfectionFormObjectDefinition();
	
	datum objectDatum = objectDefinitionFloodInfectionForm;
	
	ObjectPlacementData nObject;
	
	if (objectDatum.IsNull()) {
		return;
	}
	
	datum playerDatum1 = get_player_datum_index_from_controller_index(0);
	datum playerDatum2 = Player::getPlayerUnitDatumIndex(playerDatum1.Index);
	real_point3d* player_position = get_player_unit_coords(playerDatum1.Index);
	
	datum hostUnit = hostUnitDatum;
	
	uint32_t a4[13];
	memset(a4, 0, sizeof(a4));
	*(real_point3d*)&a4[1] = *checkpoint_player_position;
	//a4[4] = playerDatum2.data; hostUnitDatum;
	((real_point3d*)&a4[5])->x = 0;
	((real_point3d*)&a4[5])->y = 0;
	((real_point3d*)&a4[5])->z = 0;
	a4[12] = 0;
	
	int32_t result = pSpawnFloodCarrierChildren(0, 0xFFFFFFFF, objectDatum.data, (uint32_t)a4, 0);
	datum unit_datum = result;
	
	if (!unit_datum.IsNull()) {
		int(__cdecl* object_set_position)(datum, real_point3d*, real_point3d*, float*, float*);
		object_set_position = (int(__cdecl*)(datum, real_point3d*, real_point3d*, float*, float*))GetOffsetAddress(CLIENT_11122, 0x136b7f);
		object_set_position(unit_datum, checkpoint_player_position, 0, 0, 0);
	}
}

// For testing.
void FloodSpawner2()
{
	const uint32_t objectDefinitionFloodInfectionForm = GetMapFloodInfectionFormObjectDefinition();
	
	datum objectDatum = objectDefinitionFloodInfectionForm;
	
	ObjectPlacementData nObject;
	
	if (objectDatum.IsNull()) {
		return;
	}
	
	datum playerDatum1 = get_player_datum_index_from_controller_index(0);
	datum playerDatum2 = Player::getPlayerUnitDatumIndex(playerDatum1.Index);
	real_point3d* player_position = get_player_unit_coords(playerDatum1.Index);
	
	call_object_placement_data_new(&nObject, objectDatum, playerDatum2, 0);
	
	if (player_position != nullptr) {
		nObject.Placement.x = player_position->x;
		nObject.Placement.y = player_position->y;
		nObject.Placement.z = (player_position->z + 2.0f);
	}
	
	unsigned int object_gamestate_datum = call_object_new(&nObject);
	call_add_object_to_sync(object_gamestate_datum);
}

void ToggleCoopModeFix(uint8_t setting_state)
{
	if (coop_mode_fix_setting_state == setting_state) {
		return;
	}
	
	coop_mode_fix_setting_state = setting_state;
	
	uint32_t &gameGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00482d3c);
	
	uint8_t &gameEngine = *(uint8_t*)(gameGlobals + 0x8);
	
	is_in_game = (gameEngine == 1 || gameEngine == 2);
	
	switch (coop_mode_fix_setting_state) {
		case 0: {
			coop_mode_fix_active = (is_in_game && IsCampaignMap());
			break;
		}
		case 1: {
			coop_mode_fix_active = true;
			break;
		}
		case 2: {
			coop_mode_fix_active = false;
			break;
		}
	}
	
	XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
		, "%s Coop Mode fix %s."
		, __func__
		, coop_mode_fix_active ? "activated" : "deactivated"
	);
	
	if (coop_mode_fix_active) {
		
	}
	else {
		uint32_t &gameGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00482d3c);
		
		uint8_t &coopSpawns = *(uint8_t*)(gameGlobals + 0x2A4);
		
		coopSpawns = 0;
		
		if (time_all_dead) {
			delete time_all_dead;
			time_all_dead = 0;
		}
	}
}

void LoopCoop()
{
	if (Title_Version != CLIENT_11122) {
		return;
	}
	
	if (!coop_mode_fix_active || !is_in_game) {
		return;
	}
	
	uint32_t &gameGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x00482d3c);
	uint32_t &gameTimeGlobals = *(uint32_t*)GetOffsetAddress(CLIENT_11122, 0x004c06e4);
	
	uint8_t &gameState = *(uint8_t*)GetOffsetAddress(CLIENT_11122, 0x00420fc4);
	uint8_t &playerCount = *(uint8_t*)0x30004b60;
	uint8_t &isGameLost = *(uint8_t*)0x30004b64;
	uint8_t &gameEngine = *(uint8_t*)(gameGlobals + 0x8);
	uint8_t &coopSpawns = *(uint8_t*)(gameGlobals + 0x2A4);
	uint8_t &gameIsPaused = *(uint8_t*)(gameTimeGlobals + 0x1);
	
	if (IsLobbyHost()) {
		if (playerCount > 1) {
			if (has_first_spawn_occurred) {
				coopSpawns = 1;
			}
			else {
				coopSpawns = 0;
			}
		}
		else {
			coopSpawns = 0;
		}
		
		if (has_first_spawn_occurred && isGameLost == 1 && GetNumPlayersAlive() <= 0) {
			const uint32_t millisecondsRespawnDelay = 3000;
			if (time_all_dead) {
				if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - *time_all_dead).count() > millisecondsRespawnDelay) {
					delete time_all_dead;
					time_all_dead = 0;
					
					XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
						, "%s Respawning host player now."
						, __func__
					);
					
					int localPlayerIndex = get_player_datum_index_from_controller_index(0).ToAbsoluteIndex();
					
					call_SpawnPlayer(localPlayerIndex);
					
					if (checkpoint_player_position) {
						datum unit_datum = Player::getPlayerUnitDatumIndex(localPlayerIndex);
						if (!unit_datum.IsNull()) {
							int(__cdecl* object_set_position)(datum, real_point3d*, real_point3d*, float*, float*);
							object_set_position = (int(__cdecl*)(datum, real_point3d*, real_point3d*, float*, float*))GetOffsetAddress(CLIENT_11122, 0x136b7f);
							object_set_position(unit_datum, checkpoint_player_position, 0, 0, 0);
						}
					}
				}
			}
			else {
				time_all_dead = new std::chrono::steady_clock::time_point;
				*time_all_dead = std::chrono::high_resolution_clock::now();
				
				XLLN_DEBUG_LOG(XLLN_LOG_CONTEXT_XLLN_MODULE | XLLN_LOG_LEVEL_INFO
					, "%s All players are dead. Respawning them in (%d) milliseconds."
					, __func__
					, millisecondsRespawnDelay
				);
			}
		}
	}
}

bool InitCoop()
{
	// Not supporting dedicated servers atm.
	if (Title_Version != CLIENT_11122) {
		return true;
	}
	
	uint32_t patchAddress = 0;
	
	// Increase the size of the unit entity creation definition packet.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x001f8028)) || (patchAddress = GetOffsetAddress(SERVER_11122, 0x001e1d8e))) {
		punit_entity_creation_definition_packet_size = (tunit_entity_creation_definition_packet_size)DetourFunc((uint8_t*)patchAddress, (uint8_t*)unit_entity_creation_definition_packet_size, 5);
	}
	
	// This encodes the unit creation packet, only gets executed on host.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x001f8503)) || (patchAddress = GetOffsetAddress(SERVER_11122, 0x001e2269))) {
		pc_simulation_unit_entity_definition_encode = (tc_simulation_unit_entity_definition_creation_encode)DetourClassFunc((uint8_t*)patchAddress, (uint8_t*)c_simulation_unit_entity_definition_creation_encode, 10);
	}
	
	// This decodes the unit creation packet, only gets executed on client.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x001f8557)) || (patchAddress = GetOffsetAddress(SERVER_11122, 0x001e22bd))) {
		pc_simulation_unit_entity_definition_decode = (tc_simulation_unit_entity_definition_creation_decode)DetourClassFunc((uint8_t*)patchAddress, (uint8_t*)c_simulation_unit_entity_definition_creation_decode, 11);
	}
	
	// Only patch the object_new call on host during AI_Place function, no reason to hook all object_new calls.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x00318dec)) || (patchAddress = GetOffsetAddress(SERVER_11122, 0x002c3b56))) {
		PatchCall(patchAddress, object_new_hook);
	}
	
	// We update creation data here which is used later on to add data to the packet.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x001f807a)) || (patchAddress = GetOffsetAddress(SERVER_11122, 0x001e1de0))) {
		PatchCall(patchAddress, set_unit_creation_data_hook);
	}
	
	// Hooks a call within the creat_unit property on the client side in order to set their permutation index before spawn.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x001f9e6c)) || (patchAddress = GetOffsetAddress(SERVER_11122, 0x001e3bd4))) {
		PatchCall(patchAddress, create_unit_hook);
	}
	
	// Hooks the part of the unit spawn from simulation that handles setting their color data in order to ensure AI do not have their color overridden.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x001f9e34)) || (patchAddress = GetOffsetAddress(SERVER_11122, 0x001e3b9c))) {
		PatchCall(patchAddress, set_unit_color_data_hook);
	}
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x0006e5c3)) || (patchAddress = GetOffsetAddress(SERVER_11122, 0x0006d1bf))) {
		pset_unit_color_data = (tset_unit_color_data)patchAddress;
	}
	
	if (Title_Version == CLIENT_11122) {
		// Function has call in first 5 bytes, need to hook differently.
		pRestartMissionDeadCoopPlayerLegendaryDifficulty = (tRestartMissionDeadCoopPlayerLegendaryDifficulty)GetOffsetAddress(CLIENT_11122, 0x00049fa8);
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x0004a416), RestartMissionDeadCoopPlayerLegendaryDifficulty);
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x00058249), RestartMissionDeadCoopPlayerLegendaryDifficulty);
		
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x002219d6), EnableCoopPlayerWaypointIndicator1);
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x00221a22), EnableCoopPlayerWaypointIndicator2);
		
		// Function has call in first 5 bytes, need to hook differently.
		pSaveCheckpoint = (tSaveCheckpoint)GetOffsetAddress(CLIENT_11122, 0x00220d27);
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x00039825), SaveCheckpoint);
	}
	
	// PauseGame.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x0007c015))) {
		pPauseGame = (tPauseGame)DetourFunc((uint8_t*)patchAddress, (uint8_t*)PauseGame, 10);
	}
	
	// SpawnFloodCarrierChildren.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x00358ab8))) {
		pSpawnFloodCarrierChildren = (tSpawnFloodCarrierChildren)DetourFunc((uint8_t*)patchAddress, (uint8_t*)SpawnFloodCarrierChildren, 6);
	}
	
	// object_placement_data_new.
	//if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x00132163))) {
	//	pobject_placement_data_new = (tobject_placement_data_new)DetourFunc((uint8_t*)patchAddress, (uint8_t*)object_placement_data_new, 6);
	//}
	
	// Crashy.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x001f2337))) {
		pCrashy001 = (tCrashy)DetourClassFunc((uint8_t*)patchAddress, (uint8_t*)h_Crashy001, 9);
	}
	
	// Crashy2.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x0013d9ad))) {
		pCrashy2 = (tCrashy2)DetourFunc((uint8_t*)patchAddress, (uint8_t*)Crashy2, 5);
	}
	
	// simulation_game_process_player_prediction.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x001db8be))) {
		psimulation_game_process_player_prediction = (tsimulation_game_process_player_prediction)DetourFunc((uint8_t*)patchAddress, (uint8_t*)simulation_game_process_player_prediction, 5);
	}
	
	// item_in_unit_inventory.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x00181555))) {
		pitem_in_unit_inventory = (titem_in_unit_inventory)DetourFunc((uint8_t*)patchAddress, (uint8_t*)item_in_unit_inventory, 6);
	}
	
	// sub_001fa92b.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x001fa92b))) {
		psub_001fa92b = (tsub_001fa92b)DetourFunc((uint8_t*)patchAddress, (uint8_t*)sub_001fa92b, 8);
	}
	
	// sub_001329b8.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x001329b8))) {
		psub_001329b8 = (tsub_001329b8)DetourFunc((uint8_t*)patchAddress, (uint8_t*)sub_001329b8, 5);
	}
	
	// sub_001305f8.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x001305f8))) {
		psub_001305f8 = (tsub_001305f8)DetourFunc((uint8_t*)patchAddress, (uint8_t*)sub_001305f8, 7);
	}
	
	// simulation_game_interface_motion_from_control.
	if ((patchAddress = GetOffsetAddress(CLIENT_11122, 0x001dbd10))) {
		psimulation_game_interface_motion_from_control = (tsimulation_game_interface_motion_from_control)DetourFunc((uint8_t*)patchAddress, (uint8_t*)simulation_game_interface_motion_from_control, 6);
	}
	
	// simulation_action_actor_create.
	if (Title_Version == CLIENT_11122) {
		// Function has call in first 5 bytes, need to hook differently.
		psimulation_action_actor_create = (tsimulation_action_actor_create)GetOffsetAddress(CLIENT_11122, 0x001b8e08);
		PatchCall(GetOffsetAddress(CLIENT_11122, 0x0031900b), simulation_action_actor_create);
	}
	
	return true;
}

bool UninitCoop()
{
	if (Title_Version != CLIENT_11122) {
		return true;
	}
	
	return true;
}

#pragma endregion
