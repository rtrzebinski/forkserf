/* freeserf.h */

#ifndef _FREESERF_H
#define _FREESERF_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "gfx.h"
#include "map.h"

#include "building.h"
#include "serf.h"


#define INVENTORY_INDEX(ptr)  ((int)((ptr) - globals.inventories))

#define DIR_REVERSE(dir)  (((dir) + 3) % 6)


typedef enum {
	DIR_RIGHT = 0,
	DIR_DOWN_RIGHT,
	DIR_DOWN,
	DIR_LEFT,
	DIR_UP_LEFT,
	DIR_UP,

	DIR_UP_RIGHT,
	DIR_DOWN_LEFT
} dir_t;

typedef enum {
	SFX_MESSAGE = 1,
	SFX_ACCEPTED,
	SFX_NOT_ACCEPTED = 4,
	SFX_UNDO = 6,
	SFX_CLICK = 8,
	SFX_JUMP = 48,
	SFX_BIRD_CHIRP_0 = 70,
	SFX_BIRD_CHIRP_1 = 74,
	SFX_AHHH = 76,
	SFX_BIRD_CHIRP_2 = 78,
	SFX_BIRD_CHIRP_3 = 82
} sfx_t;

typedef enum {
	PANEL_BTN_BUILD_INACTIVE = 0,
	PANEL_BTN_BUILD_FLAG,
	PANEL_BTN_BUILD_MINE,
	PANEL_BTN_BUILD_SMALL,
	PANEL_BTN_BUILD_LARGE,
	PANEL_BTN_BUILD_CASTLE,
	PANEL_BTN_DESTROY,
	PANEL_BTN_DESTROY_INACTIVE,
	PANEL_BTN_BUILD_ROAD,
	PANEL_BTN_MAP_INACTIVE,
	PANEL_BTN_MAP,
	PANEL_BTN_STATS_INACTIVE,
	PANEL_BTN_STATS,
	PANEL_BTN_SETT_INACTIVE,
	PANEL_BTN_SETT,
	PANEL_BTN_DESTROY_ROAD,
	PANEL_BTN_GROUND_ANALYSIS,
	PANEL_BTN_BUILD_SMALL_STARRED,
	PANEL_BTN_BUILD_LARGE_STARRED,
	PANEL_BTN_MAP_STARRED,
	PANEL_BTN_STATS_STARRED,
	PANEL_BTN_SETT_STARRED,
	PANEL_BTN_GROUND_ANALYSIS_STARRED,
	PANEL_BTN_BUILD_MINE_STARRED,
	PANEL_BTN_BUILD_ROAD_STARRED
} panel_btn_t;

typedef enum {
	BOX_MAP = 1,
	BOX_MAP_OVERLAY,
	BOX_MINE_BUILDING,
	BOX_BASIC_BLD,
	BOX_BASIC_BLD_FLIP,
	BOX_ADV_1_BLD,
	BOX_ADV_2_BLD,
	BOX_STAT_SELECT,
	BOX_STAT_4,
	BOX_STAT_BLD_1,
	BOX_STAT_BLD_2,
	BOX_STAT_BLD_3,
	BOX_STAT_BLD_4,
	BOX_STAT_8,
	BOX_STAT_7,
	BOX_STAT_1,
	BOX_STAT_2,
	BOX_STAT_6,
	BOX_STAT_3,
	BOX_START_ATTACK,
	BOX_START_ATTACK_REDRAW,
	BOX_GROUND_ANALYSIS,
	BOX_LOAD_ARCHIVE,
	BOX_LOAD_SAVE,
	BOX_25,
	BOX_DISK_MSG,
	BOX_SETT_SELECT,
	BOX_SETT_1,
	BOX_SETT_2,
	BOX_SETT_3,
	BOX_KNIGHT_LEVEL,
	BOX_SETT_4,
	BOX_SETT_5,
	BOX_QUIT_CONFIRM,
	BOX_NO_SAVE_QUIT_CONFIRM,
	BOX_SETT_SELECT_FILE,
	BOX_OPTIONS,
	BOX_CASTLE_RES,
	BOX_MINE_OUTPUT,
	BOX_ORDERED_BLD,
	BOX_DEFENDERS,
	BOX_TRANSPORT_INFO,
	BOX_CASTLE_SERF,
	BOX_RESDIR,
	BOX_SETT_8,
	BOX_SETT_6,
	BOX_BLD_1,
	BOX_BLD_2,
	BOX_BLD_3,
	BOX_BLD_4,
	BOX_MESSAGE,
	BOX_BLD_STOCK,
	BOX_PLAYER_FACES,
	BOX_GAME_END,
	BOX_DEMOLISH,
	BOX_JS_CALIB,
	BOX_JS_CALIB_UPLEFT,
	BOX_JS_CALIB_DOWNRIGHT,
	BOX_JS_CALIB_CENTER,
	BOX_CTRLS_INFO
} box_t;

typedef enum {
	ACTION_BUILD_STONEMINE = 5,
	ACTION_BUILD_COALMINE,
	ACTION_BUILD_IRONMINE,
	ACTION_BUILD_GOLDMINE,
	ACTION_BUILD_FLAG,
	ACTION_BUILD_STONECUTTER,
	ACTION_BUILD_HUT,
	ACTION_BUILD_LUMBERJACK,
	ACTION_BUILD_FORESTER,
	ACTION_BUILD_FISHER,
	ACTION_BUILD_MILL,
	ACTION_BUILD_BOATBUILDER,
	ACTION_BUILD_BUTCHER,
	ACTION_BUILD_WEAPONSMITH,
	ACTION_BUILD_STEELSMELTER,
	ACTION_BUILD_SAWMILL,
	ACTION_BUILD_BAKER,
	ACTION_BUILD_GOLDSMELTER,
	ACTION_BUILD_FORTRESS,
	ACTION_BUILD_TOWER,
	ACTION_BUILD_TOOLMAKER,
	ACTION_BUILD_FARM,
	ACTION_BUILD_PIGFARM,
	ACTION_BLD_FLIP_PAGE,
	ACTION_SHOW_STAT_1,
	ACTION_SHOW_STAT_2,
	ACTION_SHOW_STAT_8,
	ACTION_SHOW_STAT_5,
	ACTION_SHOW_STAT_6,
	ACTION_SHOW_STAT_7,
	ACTION_SHOW_STAT_4,
	ACTION_SHOW_STAT_3,
	ACTION_SHOW_STAT_SELECT,
	ACTION_38,
	ACTION_CLOSE_BOX,
	/* ... */
	ACTION_CLOSE_SETT_BOX = 92,
	ACTION_SHOW_SETT_1,
	ACTION_SHOW_SETT_2,
	ACTION_SHOW_SETT_3,
	ACTION_SHOW_SETT_7,
	ACTION_SHOW_SETT_4,
	ACTION_SHOW_SETT_5,
	ACTION_SHOW_SETT_SELECT,
	ACTION_SETT_1_ADJUST_STONEMINE,
	ACTION_SETT_1_ADJUST_COALMINE,
	ACTION_SETT_1_ADJUST_IRONMINE,
	ACTION_SETT_1_ADJUST_GOLDMINE,
	ACTION_SETT_2_ADJUST_CONSTRUCTION,
	ACTION_SETT_2_ADJUST_BOATBUILDER,
	ACTION_SETT_2_ADJUST_TOOLMAKER_PLANKS,
	ACTION_SETT_2_ADJUST_TOOLMAKER_STEEL,
	ACTION_SETT_2_ADJUST_WEAPONSMITH,
	ACTION_SETT_3_ADJUST_STEELSMELTER,
	ACTION_SETT_3_ADJUST_GOLDSMELTER,
	ACTION_SETT_3_ADJUST_WEAPONSMITH,
	ACTION_SETT_3_ADJUST_PIGFARM,
	ACTION_SETT_3_ADJUST_MILL,
	ACTION_KNIGHT_LEVEL_CLOSEST_MIN_DEC,
	ACTION_KNIGHT_LEVEL_CLOSEST_MIN_INC,
	ACTION_KNIGHT_LEVEL_CLOSEST_MAX_DEC,
	ACTION_KNIGHT_LEVEL_CLOSEST_MAX_INC,
	ACTION_KNIGHT_LEVEL_CLOSE_MIN_DEC,
	ACTION_KNIGHT_LEVEL_CLOSE_MIN_INC,
	ACTION_KNIGHT_LEVEL_CLOSE_MAX_DEC,
	ACTION_KNIGHT_LEVEL_CLOSE_MAX_INC,
	ACTION_KNIGHT_LEVEL_FAR_MIN_DEC,
	ACTION_KNIGHT_LEVEL_FAR_MIN_INC,
	ACTION_KNIGHT_LEVEL_FAR_MAX_DEC,
	ACTION_KNIGHT_LEVEL_FAR_MAX_INC,
	ACTION_KNIGHT_LEVEL_FARTHEST_MIN_DEC,
	ACTION_KNIGHT_LEVEL_FARTHEST_MIN_INC,
	ACTION_KNIGHT_LEVEL_FARTHEST_MAX_DEC,
	ACTION_KNIGHT_LEVEL_FARTHEST_MAX_INC,
	/* ... */
	ACTION_SETT_5_6_TOP = 165,
	ACTION_SETT_5_6_UP,
	ACTION_SETT_5_6_DOWN,
	ACTION_SETT_5_6_BOTTOM,
	ACTION_QUIT_CONFIRM,
	ACTION_QUIT_CANCEL,
	ACTION_NO_SAVE_QUIT_CONFIRM,
	ACTION_SHOW_QUIT,
	ACTION_SHOW_OPTIONS,
	ACTION_SHOW_SAVE,
	/* ... */
	ACTION_SHOW_SETT_SELECT_FILE = 185,
	ACTION_SHOW_STAT_SELECT_FILE,
	ACTION_DEFAULT_SETT_1,
	ACTION_DEFAULT_SETT_2,
	ACTION_DEFAULT_SETT_5_6,
	ACTION_BUILD_STOCK,
	ACTION_SHOW_CASTLE_SERF,
	ACTION_SHOW_RESDIR,
	ACTION_SHOW_CASTLE_RES,
	ACTION_SEND_GEOLOGIST,
	ACTION_RES_MODE_IN,
	ACTION_RES_MODE_STOP,
	ACTION_RES_MODE_OUT,
	ACTION_SERF_MODE_IN,
	ACTION_SERF_MODE_STOP,
	ACTION_SERF_MODE_OUT,
	ACTION_SHOW_SETT_8,
	ACTION_SHOW_SETT_6,
	/* ... */
	ACTION_DEFAULT_SETT_3 = 208,
	/* ... */
	ACTION_CLOSE_MESSAGE = 241,
	ACTION_DEFAULT_SETT_4,
	ACTION_SHOW_PLAYER_FACES,
	/* ... */
	ACTION_SHOW_CTRLS_INFO = 245,
	ACTION_CLOSE_GROUND_ANALYSIS,
	ACTION_UNKNOWN_TP_INFO_FLAG,
	/* ... */
	ACTION_DEMOLISH = 254
} action_t;

typedef enum {
	RESOURCE_FISH = 0,
	RESOURCE_PIG,
	RESOURCE_MEAT,
	RESOURCE_WHEAT,
	RESOURCE_FLOUR,
	RESOURCE_BREAD,
	RESOURCE_LUMBER,
	RESOURCE_PLANK,
	RESOURCE_BOAT,
	RESOURCE_STONE,
	RESOURCE_IRONORE,
	RESOURCE_STEEL,
	RESOURCE_COAL,
	RESOURCE_GOLDORE,
	RESOURCE_GOLDBAR,
	RESOURCE_SHOVEL,
	RESOURCE_HAMMER,
	RESOURCE_ROD,
	RESOURCE_CLEAVER,
	RESOURCE_SCYTHE,
	RESOURCE_AXE,
	RESOURCE_SAW,
	RESOURCE_PICK,
	RESOURCE_PINCER,
	RESOURCE_SWORD,
	RESOURCE_SHIELD
} resource_type_t;

typedef struct {
	int sprite;
	int x, y;
} sprite_loc_t;

typedef struct {
	int face;
	int supplies;
	int intelligence;
	int reproduction;
} player_init_t;

typedef struct {
	/* 8 */
	uint16_t rnd_1;
	uint16_t rnd_2;
	uint16_t rnd_3;
	int pl_0_supplies;
	int pl_0_reproduction;
	int pl_1_face;
	int pl_1_intelligence;
	int pl_1_supplies;
	int pl_1_reproduction;
	int pl_2_face;
	int pl_2_intelligence;
	int pl_2_supplies;
	int pl_2_reproduction;
	int pl_3_face;
	int pl_3_intelligence;
	int pl_3_supplies;
	int pl_3_reproduction;
	/* ... */
} map_spec_t;

typedef struct inventory inventory_t;

struct inventory {
	int player_num;
	int res_dir;
	int flg_index;
	int bld_index;
	int resources[26];
	int out_queue[2];
	int out_dest[2];
	int spawn_priority;
	int serfs[27];
};


static const int map_building_sprite[] = {
	0, 0xa7, 0xa8, 0xae, 0xa9,
	0xa3, 0xa4, 0xa5, 0xa6,
	0xaa, 0xc0, 0xab, 0x9a, 0x9c, 0x9b, 0xbc,
	0xa2, 0xa0, 0xa1, 0x99, 0x9d, 0x9e, 0x98, 0x9f, 0xb2
};


int init_flag_search();

building_t *get_building(int index);
inventory_t *get_inventory(int index);

serf_t *get_serf(int index);
void free_serf(int index);

void create_notification_message(int type, map_pos_t pos, int player);

void calculate_military_flag_state(building_t *building);
void update_land_ownership(int col, int row);

#endif /* ! _FREESERF_H */
