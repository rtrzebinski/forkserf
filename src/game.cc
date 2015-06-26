/*
 * game.cc - Gameplay related functions
 *
 * Copyright (C) 2013  Jon Lund Steffensen <jonlst@gmail.com>
 *
 * This file is part of freeserf.
 *
 * freeserf is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * freeserf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with freeserf.  If not, see <http://www.gnu.org/licenses/>.
 */

/* The following are functions that concern the gameplay as
   a whole. Functions that only act on a specific game object should
   go in the respective source file. */

#include "src/game.h"

#include <cstdlib>
#include <cassert>
#include <cstring>
#include <algorithm>

#include "src/mission.h"
#include "src/savegame.h"
#include "src/debug.h"
#include "src/log.h"
#include "src/misc.h"
#include "src/inventory.h"

#define GROUND_ANALYSIS_RADIUS  25

game_t game = {0};

/* Facilitates quick lookup of offsets following a spiral pattern in the map data.
 The columns following the second are filled out by setup_spiral_pattern(). */
static int spiral_pattern[] = {
  0, 0,
  1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  3, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  4, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  4, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  5, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  5, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  5, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  6, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  6, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  6, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  6, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  6, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  7, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  7, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  7, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  7, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  7, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  16, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  24, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  24, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Initialize the global spiral_pattern. */
static void
init_spiral_pattern() {
  static const int spiral_matrix[] = {
    1,  0,  0,  1,
    1,  1, -1,  0,
    0,  1, -1, -1,
    -1,  0,  0, -1,
    -1, -1,  1,  0,
    0, -1,  1,  1
  };

  game.spiral_pattern = spiral_pattern;

  for (int i = 0; i < 49; i++) {
    int x = spiral_pattern[2 + 12*i];
    int y = spiral_pattern[2 + 12*i + 1];

    for (int j = 0; j < 6; j++) {
      spiral_pattern[2+12*i+2*j] = x*spiral_matrix[4*j+0] +
                                   y*spiral_matrix[4*j+2];
      spiral_pattern[2+12*i+2*j+1] = x*spiral_matrix[4*j+1] +
                                     y*spiral_matrix[4*j+3];
    }
  }
}

/* Allocate and initialize a new serf_t object.
   Return -1 if no more serfs can be allocated, otherwise 0. */
int
game_alloc_serf(serf_t **serf, int *index) {
  for (unsigned int i = 0; i < game.serf_limit; i++) {
    if (SERF_ALLOCATED(i)) continue;
    game.serf_bitmap[i/8] |= BIT(7-(i&7));

    if (i == game.max_serf_index) game.max_serf_index += 1;

    serf_t *s = &game.serfs[i];

    if (serf != NULL) *serf = s;
    if (index != NULL) *index = i;

    return 0;
  }

  return -1;
}

/* Return serf_t object with index. */
serf_t *
game_get_serf(int index) {
  assert(index > 0 && index < static_cast<int>(game.serf_limit));
  assert(SERF_ALLOCATED(index));
  return &game.serfs[index];
}

/* Deallocate and serf_t object. */
void
game_free_serf(int index) {
  /* Remove serf from allocation bitmap. */
  game.serf_bitmap[index/8] &= ~BIT(7-(index&7));

  /* Decrement max_serf_index as much as possible. */
  if (index == game.max_serf_index + 1) {
    while (--game.max_serf_index > 0) {
      index -= 1;
      if (SERF_ALLOCATED(index)) break;
    }
  }
}


/* Spawn new serf. Returns 0 on success.
   The serf_t object and inventory are returned if non-NULL. */
static int
spawn_serf(player_t *player, serf_t **serf, inventory_t **inventory,
           int want_knight) {
  if (!PLAYER_CAN_SPAWN(player)) return -1;
  if (game.inventories.size() < 1) return -1;

  inventory_t *inv = NULL;

  for (inventories_t::iterator i = game.inventories.begin();
       i != game.inventories.end(); ++i) {
    inventory_t *loop_inv = *i;
    if (loop_inv->get_player_num() == player->player_num &&
        (loop_inv->get_serf_mode() == mode_in)) { /* serf IN mode */
      if (want_knight && (loop_inv->get_count_of(RESOURCE_SWORD) == 0 ||
                          loop_inv->get_count_of(RESOURCE_SHIELD) == 0)) {
        continue;
      } else if (loop_inv->free_serf_count() == 0) {
        inv = loop_inv;
        break;
      } else if (inv == NULL ||
                 loop_inv->free_serf_count() < inv->free_serf_count()) {
        inv = loop_inv;
      }
    }
  }

  if (inv == NULL) {
    if (want_knight) {
      return spawn_serf(player, serf, inventory, 0);
    } else {
      return -1;
    }
  }

  serf_t *s = inv->spawn_serf_generic();
  if (s == NULL) {
    return -1;
  }

  if (serf) *serf = s;
  if (inventory) *inventory = inv;

  return 0;
}

/* Update player game state as part of the game progression. */
static void
update_player(player_t *player) {
  uint16_t delta = game.tick - player->last_tick;
  player->last_tick = game.tick;

  if (player->total_land_area > 0xffff0000) {
    player->total_land_area = 0;
  }
  if (player->total_military_score > 0xffff0000) {
    player->total_military_score = 0;
  }
  if (player->total_building_score > 0xffff0000) {
    player->total_building_score = 0;
  }

  if (PLAYER_IS_AI(player)) {
    /*if (player->field_1B2 != 0) player->field_1B2 -= 1;*/
    /*if (player->field_1B0 != 0) player->field_1B0 -= 1;*/
  }

  if (PLAYER_CYCLING_KNIGHTS(player)) {
    player->knight_cycle_counter -= delta;
    if (player->knight_cycle_counter < 1) {
      player->flags &= ~BIT(5);
      player->flags &= ~BIT(2);
    } else if (player->knight_cycle_counter < 2048 &&
         PLAYER_REDUCED_KNIGHT_LEVEL(player)) {
      player->flags |= BIT(5);
      player->flags &= ~BIT(4);
    }
  }

  if (PLAYER_HAS_CASTLE(player)) {
    player->reproduction_counter -= delta;

    while (player->reproduction_counter < 0) {
      player->serf_to_knight_counter += player->serf_to_knight_rate;
      if (player->serf_to_knight_counter < player->serf_to_knight_rate) {
        player->knights_to_spawn += 1;
        if (player->knights_to_spawn > 2) player->knights_to_spawn = 2;
      }

      if (player->knights_to_spawn == 0) {
        /* Create unassigned serf */
        spawn_serf(player, NULL, NULL, 0);
      } else {
        /* Create knight serf */
        serf_t *serf;
        inventory_t *inventory;
        int r = spawn_serf(player, &serf, &inventory, 1);
        if (r >= 0) {
          if (inventory->get_count_of(RESOURCE_SWORD) != 0 &&
              inventory->get_count_of(RESOURCE_SHIELD) != 0) {
            player->knights_to_spawn -= 1;
            inventory->specialize_serf(serf, SERF_KNIGHT_0);
          }
        }
      }

      player->reproduction_counter += player->reproduction_reset;
    }
  }
}

/* Clear the serf request bit of all flags and buildings.
   This allows the flag or building to try and request a
   serf again. */
static void
clear_serf_request_failure() {
  for (buildings_t::iterator i = game.buildings.begin();
       i != game.buildings.end(); ++i) {
    building_t *building = *i;
    building->clear_serf_request_failure();
  }

  for (flags_t::iterator i = game.flags.begin();
       i != game.flags.end(); ++i) {
    flag_t *flag = *i;
    flag->serf_request_clear();
  }
}

static void
update_knight_morale() {
  unsigned int inventory_gold[GAME_MAX_PLAYER_COUNT] = {0};
  unsigned int military_gold[GAME_MAX_PLAYER_COUNT] = {0};

  /* Sum gold collected in inventories */
  for (inventories_t::iterator i = game.inventories.begin();
       i != game.inventories.end(); ++i) {
    inventory_t *inventory = *i;
    inventory_gold[inventory->get_player_num()] +=
      inventory->get_count_of(RESOURCE_GOLDBAR);
  }

  /* Sum gold deposited in military buildings */
  for (buildings_t::iterator i = game.buildings.begin();
       i != game.buildings.end(); ++i) {
    building_t *building = *i;
    military_gold[building->get_player()] += building->military_gold_count();
  }

  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    player_t *player = game.player[i];
    if (!PLAYER_IS_ACTIVE(player)) continue;

    unsigned int depot = inventory_gold[i] + military_gold[i];
    player->gold_deposited = depot;

    /* Calculate according to gold collected. */
    unsigned int map_gold = game.map_gold_deposit;
    if (map_gold != 0) {
      while (map_gold > 0xffff) {
        map_gold >>= 1;
        depot >>= 1;
      }
      depot = std::min(depot, map_gold-1);
      player->knight_morale = 1024 +
                              (game.map_gold_morale_factor * depot)/map_gold;
    } else {
      player->knight_morale = 4096;
    }

    /* Adjust based on castle score. */
    int castle_score = player->castle_score;
    if (castle_score < 0) {
      player->knight_morale = std::max(1, player->knight_morale - 1023);
    } else if (castle_score > 0) {
      player->knight_morale = std::min(player->knight_morale +
                                       1024*castle_score, 0xffff);
    }

    unsigned int military_score = player->total_military_score;
    unsigned int morale = player->knight_morale >> 5;
    while (military_score > 0xffff) {
      military_score >>= 1;
      morale <<= 1;
    }

    /* Calculate fractional score used by AI */
    unsigned int player_score = (military_score * morale) >> 7;
    unsigned int enemy_score = 0;
    for (int j = 0; j < GAME_MAX_PLAYER_COUNT; j++) {
      if (PLAYER_IS_ACTIVE(game.player[j]) && i != j) {
        enemy_score += game.player[j]->total_military_score;
      }
    }

    while (player_score > 0xffff &&
           enemy_score > 0xffff) {
      player_score >>= 1;
      enemy_score >>= 1;
    }

    player_score >>= 1;
    unsigned int frac_score = 0;
    if (player_score != 0 && enemy_score != 0) {
      if (player_score > enemy_score) {
        frac_score = 0xffffffff;
      } else {
        frac_score = (player_score*0x10000)/enemy_score;
      }
    }

    player->military_max_gold = 0;
  }
}

typedef struct {
  resource_type_t resource;
  int *max_prio;
  flag_t **flags;
} update_inventories_data_t;

static int
update_inventories_cb(flag_t *flag, update_inventories_data_t *data) {
  int inv = flag->get_search_dir();
  if (data->max_prio[inv] < 255 &&
    flag->has_building()) {
    building_t *building = flag->get_building();

    int bld_prio = building->get_max_priority_for_resource(data->resource, 16);
    if (bld_prio > data->max_prio[inv]) {
      data->max_prio[inv] = bld_prio;
      data->flags[inv] = flag;
    }
  }

  return 0;
}

/* Check which players are allowed to spawn new serfs. */
static void
check_max_serfs_reached() {
  unsigned int land_area = 0;
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    if (!PLAYER_IS_ACTIVE(game.player[i])) continue;
    land_area += game.player[i]->total_land_area;
  }

  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    player_t *player = game.player[i];
    if (!PLAYER_IS_ACTIVE(player)) continue;

    unsigned int max_serfs = game.max_serfs_per_player;
    if (land_area != 0) {
      max_serfs += (game.max_serfs_from_land*player->total_land_area)/land_area;
    }

    unsigned int serf_count = 0;
    for (int j = 0; j < 27; j++) {
      serf_count += player->serf_count[j];
    }

    if (serf_count < max_serfs) {
      player->build |= BIT(2);
    } else {
      player->build &= ~BIT(2);
    }
  }
}

/* Update inventories as part of the game progression. Moves the appropriate
   resources that are needed outside of the inventory into the out queue. */
static void
update_inventories() {
  const resource_type_t arr_1[] = {
    RESOURCE_PLANK,
    RESOURCE_STONE,
    RESOURCE_STEEL,
    RESOURCE_COAL,
    RESOURCE_LUMBER,
    RESOURCE_IRONORE,
    RESOURCE_GROUP_FOOD,
    RESOURCE_PIG,
    RESOURCE_FLOUR,
    RESOURCE_WHEAT,
    RESOURCE_GOLDBAR,
    RESOURCE_GOLDORE,
    RESOURCE_NONE,
  };

  const resource_type_t arr_2[] = {
    RESOURCE_STONE,
    RESOURCE_IRONORE,
    RESOURCE_GOLDORE,
    RESOURCE_COAL,
    RESOURCE_STEEL,
    RESOURCE_GOLDBAR,
    RESOURCE_GROUP_FOOD,
    RESOURCE_PIG,
    RESOURCE_FLOUR,
    RESOURCE_WHEAT,
    RESOURCE_LUMBER,
    RESOURCE_PLANK,
    RESOURCE_NONE,
  };

  const resource_type_t arr_3[] = {
    RESOURCE_GROUP_FOOD,
    RESOURCE_WHEAT,
    RESOURCE_PIG,
    RESOURCE_FLOUR,
    RESOURCE_GOLDBAR,
    RESOURCE_STONE,
    RESOURCE_PLANK,
    RESOURCE_STEEL,
    RESOURCE_COAL,
    RESOURCE_LUMBER,
    RESOURCE_GOLDORE,
    RESOURCE_IRONORE,
    RESOURCE_NONE,
  };

  check_max_serfs_reached();
  /* AI: TODO */

  const resource_type_t *arr = NULL;
  switch (game_random_int() & 7) {
  case 0: arr = arr_2; break;
  case 1: arr = arr_3; break;
  default: arr = arr_1; break;
  }

  while (arr[0] != RESOURCE_NONE) {
    for (int p = 0; p < GAME_MAX_PLAYER_COUNT; p++) {
      inventory_t *invs[256];
      int n = 0;
      for (inventories_t::iterator i = game.inventories.begin();
           i != game.inventories.end(); ++i) {
        inventory_t *inventory = *i;
        if (inventory->get_player_num() == p &&
            !inventory->is_queue_full()) {
          inventory_mode_t res_dir = inventory->get_res_mode();
          if (res_dir == mode_in || res_dir == mode_stop) {  // In mode,
                                                             // stop mode
            if (arr[0] == RESOURCE_GROUP_FOOD) {
              if (inventory->has_food()) {
                invs[n++] = inventory;
                if (n == 256) break;
              }
            } else if (inventory->get_count_of(arr[0]) != 0) {
              invs[n++] = inventory;
              if (n == 256) break;
            }
          } else { /* Out mode */
            player_t *player = game.player[p];

            int prio = 0;
            resource_type_t type = RESOURCE_NONE;
            for (int i = 0; i < 26; i++) {
              if (inventory->get_count_of((resource_type_t)i) != 0 &&
                  player->inventory_prio[i] >= prio) {
                prio = player->inventory_prio[i];
                type = (resource_type_t)i;
              }
            }

            if (type != RESOURCE_NONE) {
              inventory->add_to_queue(type, 0);
            }
          }
        }
      }

      if (n == 0) continue;

      flag_search_t search;

      int max_prio[256];
      flag_t *flags[256];

      for (int i = 0; i < n; i++) {
        max_prio[i] = 0;
        flags[i] = NULL;
        flag_t *flag = game.flags[invs[i]->get_flag_index()];
        flag->set_search_dir((dir_t)i);
        search.add_source(flag);
      }

      update_inventories_data_t data;
      data.resource = arr[0];
      data.max_prio = max_prio;
      data.flags = flags;
      search.execute(reinterpret_cast<flag_search_func*>(update_inventories_cb),
                     false, true, &data);

      for (int i = 0; i < n; i++) {
        if (max_prio[i] > 0) {
          LOGV("game", " dest for inventory %i found", i);
          resource_type_t res = (resource_type_t)arr[0];

          building_t *dest_bld = flags[i]->get_building();
          assert(dest_bld->add_requested_resource(res, false));

          /* Put resource in out queue */
          inventory_t *src_inv = invs[i];
          src_inv->add_to_queue(res, dest_bld->get_flag_index());
        }
      }
    }
    arr += 1;
  }
}

/* Update flags as part of the game progression. */
static void
update_flags() {
  for (flags_t::iterator i = game.flags.begin();
       i != game.flags.end(); ++i) {
    flag_t *flag = *i;
    flag->update();
  }
}

typedef struct {
  inventory_t *inventory;
  building_t *building;
  int serf_type;
  int dest_index;
  resource_type_t res1;
  resource_type_t res2;
} send_serf_to_flag_data_t;

static int
send_serf_to_flag_search_cb(flag_t *flag, send_serf_to_flag_data_t *data) {
  if (flag->has_inventory()) {
    /* Inventory reached */
    building_t *building = flag->get_building();
    inventory_t *inv = building->get_inventory();

    int type = data->serf_type;
    if (type < 0) {
      int knight_type = -1;
      for (int i = 4; i >= -type-1; i--) {
        if (inv->have_serf((serf_type_t)(SERF_KNIGHT_0+i))) {
          knight_type = i;
          break;
        }
      }

      if (knight_type >= 0) {
        /* Knight of appropriate type was found. */
        serf_t *serf =
                   inv->call_out_serf((serf_type_t)(SERF_KNIGHT_0+knight_type));

        data->building->knight_request_granted();

        serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE_INVENTORY);
        serf->state = SERF_STATE_READY_TO_LEAVE_INVENTORY;
        serf->s.ready_to_leave_inventory.mode = -1;
        serf->s.ready_to_leave_inventory.dest =
                                               data->building->get_flag_index();
        serf->s.ready_to_leave_inventory.inv_index = inv->get_index();

        return 1;
      } else if (type == -1) {
        /* See if a knight can be created here. */
        if (inv->have_serf(SERF_GENERIC) &&
            inv->get_count_of(RESOURCE_SWORD) > 0 &&
            inv->get_count_of(RESOURCE_SHIELD) > 0) {
          data->inventory = inv;
        }
      }
    } else {
      if (inv->have_serf((serf_type_t)type)) {
        if (type != SERF_GENERIC || inv->free_serf_count() > 4) {
          serf_t *serf = inv->call_out_serf((serf_type_t)type);

          serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE_INVENTORY);
          serf->state = SERF_STATE_READY_TO_LEAVE_INVENTORY;
          serf->s.ready_to_leave_inventory.inv_index = inv->get_index();
          serf->s.ready_to_leave_inventory.dest = data->dest_index;

          if (type == SERF_GENERIC) {
            serf->s.ready_to_leave_inventory.mode = -2;
          } else if (type == SERF_GEOLOGIST) {
            serf->s.ready_to_leave_inventory.mode = 6;
          } else {
            building_t *dest_bld = game.flags[data->dest_index]->get_building();
            dest_bld->request_serf();
            serf->s.ready_to_leave_inventory.mode = -1;
          }

          return 1;
        }
      } else {
        if (data->inventory == NULL &&
            inv->have_serf(SERF_GENERIC) &&
            (data->res1 == -1 || inv->get_count_of(data->res1) > 0) &&
            (data->res2 == -1 || inv->get_count_of(data->res2) > 0)) {
          data->inventory = inv;
          /* player_t *player = globals->player[SERF_PLAYER(serf)]; */
          /* game.field_340 = player->cont_search_after_non_optimal_find; */
        }
      }
    }
  }

  return 0;
}

/* Dispatch serf from (nearest?) inventory to flag. */
int
game_send_serf_to_flag(flag_t *dest, int type, resource_type_t res1,
                       resource_type_t res2) {
  building_t *building = NULL;
  if (dest->has_building()) {
    building = dest->get_building();
  }

  /* If type is negative, building is non-NULL. */
  if (type < 0) {
    player_t *player = game.player[building->get_player()];
    if (PLAYER_CYCLING_SECOND(player)) {
      type = -((player->knight_cycle_counter >> 8) + 1);
    }
  }

  send_serf_to_flag_data_t data;
  data.inventory = NULL;
  data.building = building;
  data.serf_type = type;
  data.dest_index = dest->get_index();
  data.res1 = res1;
  data.res2 = res2;

  int r = flag_search_t::single(dest,
               reinterpret_cast<flag_search_func*>(send_serf_to_flag_search_cb),
                                true, false, &data);
  if (r == 0) {
    return 0;
  } else if (data.inventory != NULL) {
    inventory_t *inventory = data.inventory;
    serf_t *serf = inventory->call_out_serf(SERF_GENERIC);

    if (type < 0) {
      /* Knight */
      building->knight_request_granted();

      serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE_INVENTORY);
      serf->state = SERF_STATE_READY_TO_LEAVE_INVENTORY;
      serf->s.ready_to_leave_inventory.mode = -1;
      serf->s.ready_to_leave_inventory.dest = building->get_flag_index();
      serf->s.ready_to_leave_inventory.inv_index = inventory->get_index();
      serf_set_type(serf, SERF_KNIGHT_0);

      inventory->pop_resource(RESOURCE_SWORD);
      inventory->pop_resource(RESOURCE_SHIELD);
    } else {
      serf_log_state_change(serf, SERF_STATE_READY_TO_LEAVE_INVENTORY);
      serf->state = SERF_STATE_READY_TO_LEAVE_INVENTORY;
      serf->s.ready_to_leave_inventory.inv_index = inventory->get_index();

      if (type == SERF_GEOLOGIST) {
        serf->s.ready_to_leave_inventory.mode = 6;
        serf->s.ready_to_leave_inventory.dest = dest->get_index();
      } else {
        building->request_serf();
        serf->s.ready_to_leave_inventory.mode = -1;
        serf->s.ready_to_leave_inventory.dest = dest->get_index();
      }
      serf_set_type(serf, (serf_type_t)type);

      if (res1 != RESOURCE_NONE) inventory->pop_resource(res1);
      if (res2 != RESOURCE_NONE) inventory->pop_resource(res2);
    }

    return 0;
  }

  return -1;
}

/* Dispatch geologist to flag. */
int
game_send_geologist(flag_t *dest) {
  return game_send_serf_to_flag(dest, SERF_GEOLOGIST, RESOURCE_HAMMER,
                                RESOURCE_NONE);
}

/* Update buildings as part of the game progression. */
static void
update_buildings() {
  buildings_t::iterator i = game.buildings.begin();
  while (i != game.buildings.end()) {
    building_t *building = *i;
    ++i;
    building->update(game.tick);
  }
}

/* Update serfs as part of the game progression. */
static void
update_serfs() {
  for (unsigned int i = 1; i < game.max_serf_index; i++) {
    if (SERF_ALLOCATED(i)) {
      serf_t *serf = game_get_serf(i);
      update_serf(serf);
    }
  }
}

/* Update historical player statistics for one measure. */
static void
record_player_history(player_t *player[], int pl_count, int max_level,
                      int aspect, const int history_index[],
                      const unsigned int values[]) {
  unsigned int total = 0;
  for (int i = 0; i < pl_count; i++) total += values[i];
  total = std::max(1u, total);

  for (int i = 0; i < max_level+1; i++) {
    int mode = (aspect << 2) | i;
    int index = history_index[i];
    for (int j = 0; j < pl_count; j++) {
      uint64_t value = values[j];
      player[j]->player_stat_history[mode][index] =
                                            static_cast<int>((100*value)/total);
    }
  }
}

/* Calculate whether one player has enough advantage to be
   considered a clear winner regarding one aspect.
   Return -1 if there is no clear winner. */
static int
calculate_clear_winner(int pl_count, const unsigned int values[]) {
  int total = 0;
  for (int i = 0; i < pl_count; i++) total += values[i];
  total = std::max(1, total);

  for (int i = 0; i < pl_count; i++) {
    uint64_t value = values[i];
    if ((100*value)/total >= 75) return i;
  }

  return -1;
}

/* Calculate condensed score from military score and knight morale. */
static int
calculate_military_score(int military, int morale) {
  return (2048 + (morale >> 1)) * (military << 6);
}

/* Update statistics of the game. */
static void
update_game_stats() {
  if (static_cast<int>(game.game_stats_counter) > game.tick_diff) {
    game.game_stats_counter -= game.tick_diff;
  } else {
    game.game_stats_counter += 1500 - game.tick_diff;

    game.player_score_leader = 0;

    int update_level = 0;

    /* Update first level index */
    game.player_history_index[0] =
      game.player_history_index[0]+1 < 112 ?
      game.player_history_index[0]+1 : 0;

    game.player_history_counter[0] -= 1;
    if (game.player_history_counter[0] < 0) {
      update_level = 1;
      game.player_history_counter[0] = 3;

      /* Update second level index */
      game.player_history_index[1] =
        game.player_history_index[1]+1 < 112 ?
        game.player_history_index[1]+1 : 0;

      game.player_history_counter[1] -= 1;
      if (game.player_history_counter[1] < 0) {
        update_level = 2;
        game.player_history_counter[1] = 4;

        /* Update third level index */
        game.player_history_index[2] =
          game.player_history_index[2]+1 < 112 ?
          game.player_history_index[2]+1 : 0;

        game.player_history_counter[2] -= 1;
        if (game.player_history_counter[2] < 0) {
          update_level = 3;

          game.player_history_counter[2] = 4;

          /* Update fourth level index */
          game.player_history_index[3] =
            game.player_history_index[3]+1 < 112 ?
            game.player_history_index[3]+1 : 0;
        }
      }
    }

    unsigned int values[GAME_MAX_PLAYER_COUNT];

    /* Store land area stats in history. */
    int pl_count = 0;
    for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
      if (PLAYER_IS_ACTIVE(game.player[i])) {
        values[i] = game.player[i]->total_land_area;
        pl_count += 1;
      }
    }
    record_player_history(game.player, pl_count, update_level, 1,
                          game.player_history_index, values);
    game.player_score_leader |= BIT(calculate_clear_winner(pl_count, values));

    /* Store building stats in history. */
    for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
      if (PLAYER_IS_ACTIVE(game.player[i])) {
        values[i] = game.player[i]->total_building_score;
      }
    }
    record_player_history(game.player, pl_count, update_level, 2,
              game.player_history_index, values);

    /* Store military stats in history. */
    for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
      if (PLAYER_IS_ACTIVE(game.player[i])) {
        values[i] =
                  calculate_military_score(game.player[i]->total_military_score,
                                           game.player[i]->knight_morale);
      }
    }
    record_player_history(game.player, pl_count, update_level, 3,
                          game.player_history_index, values);
    game.player_score_leader |= BIT(calculate_clear_winner(pl_count, values)) <<
                                4;

    /* Store condensed score of all aspects in history. */
    for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
      if (PLAYER_IS_ACTIVE(game.player[i])) {
        int mil_score =
                  calculate_military_score(game.player[i]->total_military_score,
                                           game.player[i]->knight_morale);
        values[i] = game.player[i]->total_building_score +
          ((game.player[i]->total_land_area + mil_score) >> 4);
      }
    }
    record_player_history(game.player, pl_count, update_level, 0,
              game.player_history_index, values);

    /* TODO Determine winner based on game.player_score_leader */
  }

  if (static_cast<int>(game.history_counter) > game.tick_diff) {
    game.history_counter -= game.tick_diff;
  } else {
    game.history_counter += 6000 - game.tick_diff;

    int index = game.resource_history_index;

    for (int res = 0; res < 26; res++) {
      for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
        player_t *player = game.player[i];
        if (PLAYER_IS_ACTIVE(player)) {
          player->resource_count_history[res][index] =
                                                    player->resource_count[res];
          player->resource_count[res] = 0;
        }
      }
    }

    game.resource_history_index = index+1 < 120 ? index+1 : 0;
  }
}

/* Update game state after tick increment. */
void
game_update() {
  /* Increment tick counters */
  game.const_tick += 1;

  /* Update tick counters based on game speed */
  game.last_tick = game.tick;
  game.tick += game.game_speed;
  game.tick_diff = game.tick - game.last_tick;

  clear_serf_request_failure();
  map_update();

  /* Update players */
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    if (PLAYER_IS_ACTIVE(game.player[i])) {
      update_player(game.player[i]);
    }
  }

  /* Update knight morale */
  game.knight_morale_counter -= game.tick_diff;
  if (game.knight_morale_counter < 0) {
    update_knight_morale();
    game.knight_morale_counter += 256;
  }

  /* Schedule resources to go out of inventories */
  game.inventory_schedule_counter -= game.tick_diff;
  if (game.inventory_schedule_counter < 0) {
    update_inventories();
    game.inventory_schedule_counter += 64;
  }

#if 0
  /* AI related updates */
  game.next_index = (game.next_index + 1) % game.max_next_index;
  if (game.next_index > 32) {
    for (int i = 0; i < game.max_next_index) {
      int i = 33 - game.next_index;
      player_t *player = game.player[i & 3];
      if (PLAYER_IS_ACTIVE(player) && PLAYER_IS_AI(player)) {
        /* AI */
        /* TODO */
      }
      game.next_index += 1;
    }
  } else if (game.game_speed > 0 &&
       game.max_flag_index < 50) {
    player_t *player = game.player[game.next_index & 3];
    if (PLAYER_IS_ACTIVE(player) && PLAYER_IS_AI(player)) {
      /* AI */
      /* TODO */
    }
  }
#endif

  update_flags();
  update_buildings();
  update_serfs();
  update_game_stats();
}

/* Pause or unpause the game. */
void
game_pause(int enable) {
  if (enable) {
    game.game_speed_save = game.game_speed;
    game.game_speed = 0;
  } else {
    game.game_speed = game.game_speed_save;
  }

  LOGI("game", "Game speed: %u", game.game_speed);
}

/* Generate an estimate of the amount of resources in the ground at map pos.*/
static void
get_resource_estimate(map_pos_t pos, int weight, int estimates[5]) {
  if ((MAP_OBJ(pos) == MAP_OBJ_NONE ||
       MAP_OBJ(pos) >= MAP_OBJ_TREE_0) &&
       MAP_RES_TYPE(pos) != GROUND_DEPOSIT_NONE) {
    int value = weight*MAP_RES_AMOUNT(pos);
    estimates[MAP_RES_TYPE(pos)] += value;
  }
}

/* Prepare a ground analysis at position. */
void
game_prepare_ground_analysis(map_pos_t pos, int estimates[5]) {
  for (int i = 0; i < 5; i++) estimates[i] = 0;

  /* Sample the cursor position with maximum weighting. */
  get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS, estimates);

  /* Move outward in a spiral around the initial pos.
     The weighting of the samples attenuates linearly
     with the distance to the center. */
  for (int i = 0; i < GROUND_ANALYSIS_RADIUS-1; i++) {
    pos = MAP_MOVE_RIGHT(pos);

    for (int j = 0; j < i+1; j++) {
      get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, estimates);
      pos = MAP_MOVE_DOWN(pos);
    }

    for (int j = 0; j < i+1; j++) {
      get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, estimates);
      pos = MAP_MOVE_LEFT(pos);
    }

    for (int j = 0; j < i+1; j++) {
      get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, estimates);
      pos = MAP_MOVE_UP_LEFT(pos);
    }

    for (int j = 0; j < i+1; j++) {
      get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, estimates);
      pos = MAP_MOVE_UP(pos);
    }

    for (int j = 0; j < i+1; j++) {
      get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, estimates);
      pos = MAP_MOVE_RIGHT(pos);
    }

    for (int j = 0; j < i+1; j++) {
      get_resource_estimate(pos, GROUND_ANALYSIS_RADIUS-i, estimates);
      pos = MAP_MOVE_DOWN_RIGHT(pos);
    }
  }

  /* Process the samples. */
  for (int i = 0; i < 5; i++) {
    estimates[i] >>= 4;
    estimates[i] = std::min(estimates[i], 999);
  }
}

/* Return non-zero if the road segment from pos in direction dir
can be successfully constructed at the current time. */
int
game_road_segment_valid(map_pos_t pos, dir_t dir) {
  map_pos_t other_pos = MAP_MOVE(pos, dir);

  map_obj_t obj = MAP_OBJ(other_pos);
  if ((MAP_PATHS(other_pos) != 0 && obj != MAP_OBJ_FLAG) ||
    map_space_from_obj[obj] >= MAP_SPACE_SEMIPASSABLE) {
    return 0;
  }

  if (!MAP_HAS_OWNER(other_pos) ||
    MAP_OWNER(other_pos) != MAP_OWNER(pos)) {
    return 0;
  }

  if (MAP_IN_WATER(pos) != MAP_IN_WATER(other_pos) &&
    !(MAP_HAS_FLAG(pos) || MAP_HAS_FLAG(other_pos))) {
    return 0;
  }

  return 1;
}

static int
road_segment_in_water(map_pos_t pos, dir_t dir) {
  if (dir > DIR_DOWN) {
    pos = MAP_MOVE(pos, dir);
    dir = DIR_REVERSE(dir);
  }

  int water = 0;

  switch (dir) {
  case DIR_RIGHT:
    if (MAP_TYPE_DOWN(pos) < 4 &&
        MAP_TYPE_UP(MAP_MOVE_UP(pos)) < 4) {
      water = 1;
    }
    break;
  case DIR_DOWN_RIGHT:
    if (MAP_TYPE_UP(pos) < 4 &&
        MAP_TYPE_DOWN(pos) < 4) {
      water = 1;
    }
    break;
  case DIR_DOWN:
    if (MAP_TYPE_UP(pos) < 4 &&
        MAP_TYPE_DOWN(MAP_MOVE_LEFT(pos)) < 4) {
      water = 1;
    }
    break;
  default:
    NOT_REACHED();
    break;
  }

  return water;
}

/* Test whether a given road can be constructed by player. The final
   destination will be returned in dest, and water will be set if the
   resulting path is a water path.
   This will return success even if the destination does _not_ contain
   a flag, and therefore partial paths can be validated with this function. */
int
game_can_build_road(map_pos_t source, const dir_t dirs[], unsigned int length,
                    const player_t *player, map_pos_t *dest, int *water) {
  /* Follow along path to other flag. Test along the way
     whether the path is on ground or in water. */
  map_pos_t pos = source;
  int test = 0;

  if (!MAP_HAS_OWNER(pos) || MAP_OWNER(pos) != player->player_num ||
      !MAP_HAS_FLAG(pos)) {
    return 0;
  }

  for (unsigned int i = 0; i < length; i++) {
    dir_t dir = dirs[i];

    if (!game_road_segment_valid(pos, dir)) {
      return -1;
    }

    if (road_segment_in_water(pos, dir)) {
      test |= BIT(1);
    } else {
      test |= BIT(0);
    }

    pos = MAP_MOVE(pos, dir);

    /* Check that owner is correct, and that only the destination
       has a flag. */
    if (!MAP_HAS_OWNER(pos) || MAP_OWNER(pos) != player->player_num ||
        (MAP_HAS_FLAG(pos) && i != length-1)) {
      return 0;
    }
  }

  map_pos_t d = pos;
  if (dest != NULL) *dest = d;

  /* Bit 0 indicates a ground path, bit 1 indicates
     water path. Abort if path went through both
     ground and water. */
  int w = 0;
  if (BIT_TEST(test, 1)) {
    w = 1;
    if (BIT_TEST(test, 0)) return 0;
  }
  if (water != NULL) *water = w;

  return 1;
}

/* Construct a road spefified by a source and a list
   of directions. */
int
game_build_road(map_pos_t source, const dir_t dirs[], unsigned int length,
                const player_t *player) {
  if (length < 1) return -1;

  map_pos_t dest;
  int water_path;
  int r = game_can_build_road(source, dirs, length,
            player, &dest, &water_path);
  if (!r) return -1;
  if (!MAP_HAS_FLAG(dest)) return -1;

  map_tile_t *tiles = game.map.tiles;

  dir_t out_dir = dirs[0];
  dir_t in_dir = DIR_REVERSE(dirs[length-1]);

  /* Actually place road segments */
  map_pos_t pos = source;
  for (unsigned int i = 0; i < length; i++) {
    dir_t dir = dirs[i];
    dir_t rev_dir = DIR_REVERSE(dir);

    if (!game_road_segment_valid(pos, dir)) {
      /* Not valid after all. Backtrack and abort.
         This is needed to check that the road
         does not cross itself. */
      for (int j = i-1; j >= 0; j--) {
        dir_t rev_dir = dirs[j];
        dir_t dir = DIR_REVERSE(rev_dir);

        tiles[pos].paths &= ~BIT(dir);
        tiles[MAP_MOVE(pos, dir)].paths &= ~BIT(rev_dir);

        pos = MAP_MOVE(pos, dir);
      }

      return -1;
    }

    tiles[pos].paths |= BIT(dir);
    tiles[MAP_MOVE(pos, dir)].paths |= BIT(rev_dir);

    pos = MAP_MOVE(pos, dir);
  }

  /* Connect flags */
  flag_t *src_flag = game.flags[MAP_OBJ_INDEX(source)];
  flag_t *dest_flag = game.flags[MAP_OBJ_INDEX(dest)];

  src_flag->link_with_flag(dest_flag, water_path == 1, length, in_dir, out_dir);

  return 0;
}

static void
flag_reset_transport(flag_t *flag) {
  /* Clear destination for any serf with resources for this flag. */
  for (unsigned int i = 1; i < game.max_serf_index; i++) {
    if (SERF_ALLOCATED(i)) {
      serf_t *serf = game_get_serf(i);

      if (serf->state == SERF_STATE_WALKING &&
          serf->s.walking.dest == flag->get_index() &&
          serf->s.walking.res < 0) {
        serf->s.walking.res = -2;
        serf->s.walking.dest = 0;
      } else if (serf->state == SERF_STATE_READY_TO_LEAVE_INVENTORY &&
           serf->s.ready_to_leave_inventory.dest == flag->get_index() &&
           serf->s.ready_to_leave_inventory.mode < 0) {
        serf->s.ready_to_leave_inventory.mode = -2;
        serf->s.ready_to_leave_inventory.dest = 0;
      } else if ((serf->state == SERF_STATE_LEAVING_BUILDING ||
            serf->state == SERF_STATE_READY_TO_LEAVE) &&
           serf->s.leaving_building.next_state == SERF_STATE_WALKING &&
           serf->s.leaving_building.dest == flag->get_index() &&
           serf->s.leaving_building.field_B < 0) {
        serf->s.leaving_building.field_B = -2;
        serf->s.leaving_building.dest = 0;
      } else if (serf->state == SERF_STATE_TRANSPORTING &&
           serf->s.walking.dest == flag->get_index()) {
        serf->s.walking.dest = 0;
      } else if (serf->state == SERF_STATE_MOVE_RESOURCE_OUT &&
                 serf->s.move_resource_out.next_state ==
                   SERF_STATE_DROP_RESOURCE_OUT &&
                 serf->s.move_resource_out.res_dest == flag->get_index()) {
        serf->s.move_resource_out.res_dest = 0;
      } else if (serf->state == SERF_STATE_DROP_RESOURCE_OUT &&
                 serf->s.move_resource_out.res_dest == flag->get_index()) {
        serf->s.move_resource_out.res_dest = 0;
      } else if (serf->state == SERF_STATE_LEAVING_BUILDING &&
                 serf->s.leaving_building.next_state ==
                   SERF_STATE_DROP_RESOURCE_OUT &&
                 serf->s.leaving_building.dest == flag->get_index()) {
        serf->s.leaving_building.dest = 0;
      }
    }
  }

  /* Flag. */
  for (flags_t::iterator i = game.flags.begin();
       i != game.flags.end(); ++i) {
    flag->reset_transport(*i);
  }

  /* Inventories. */
  for (inventories_t::iterator i = game.inventories.begin();
       i != game.inventories.end(); ++i) {
    inventory_t *inventory = *i;
    inventory->reset_queue_for_dest(flag);
  }
}

static void
building_remove_player_refs(building_t *building) {
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    if (PLAYER_IS_ACTIVE(game.player[i])) {
      if (game.player[i]->index == building->get_index()) {
        game.player[i]->index = 0;
      }
    }
  }
}

static int
remove_road_backref_until_flag(map_pos_t pos, dir_t dir) {
  map_tile_t *tiles = game.map.tiles;

  while (1) {
    pos = MAP_MOVE(pos, dir);

    /* Clear backreference */
    tiles[pos].paths &= ~BIT(DIR_REVERSE(dir));

    if (MAP_OBJ(pos) == MAP_OBJ_FLAG) break;

    /* Find next direction of path. */
    dir = DIR_NONE;
    for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
      if (BIT_TEST(MAP_PATHS(pos), d)) {
        dir = (dir_t)d;
        break;
      }
    }

    if (dir == -1) return -1;
  }

  return 0;
}

static int
remove_road_backrefs(map_pos_t pos) {
  if (MAP_PATHS(pos) == 0) return -1;

  /* Find directions of path segments to be split. */
  dir_t path_1_dir = DIR_NONE;
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    if (BIT_TEST(MAP_PATHS(pos), d)) {
      path_1_dir = (dir_t)d;
      break;
    }
  }

  dir_t path_2_dir = DIR_NONE;
  for (int d = path_1_dir+1; d <= DIR_UP; d++) {
    if (BIT_TEST(MAP_PATHS(pos), d)) {
      path_2_dir = (dir_t)d;
      break;
    }
  }

  if (path_1_dir == -1 || path_2_dir == -1) return -1;

  int r = remove_road_backref_until_flag(pos, path_1_dir);
  if (r < 0) return -1;

  r = remove_road_backref_until_flag(pos, path_2_dir);
  if (r < 0) return -1;

  return 0;
}

static int
path_serf_idle_to_wait_state(map_pos_t pos) {
  /* Look through serf array for the corresponding serf. */
  for (unsigned int i = 1; i < game.max_serf_index; i++) {
    if (SERF_ALLOCATED(i)) {
      serf_t *serf = game_get_serf(i);
      if (serf->pos == pos &&
          (serf->state == SERF_STATE_IDLE_ON_PATH ||
           serf->state == SERF_STATE_WAIT_IDLE_ON_PATH ||
           serf->state == SERF_STATE_WAKE_AT_FLAG ||
           serf->state == SERF_STATE_WAKE_ON_PATH)) {
        serf_log_state_change(serf, SERF_STATE_WAKE_AT_FLAG);
        serf->state = SERF_STATE_WAKE_AT_FLAG;
        return 0;
      }
    }
  }

  return -1;
}

static void
remove_road_forwards(map_pos_t pos, dir_t dir) {
  map_tile_t *tiles = game.map.tiles;
  dir_t in_dir = DIR_NONE;

  while (1) {
    if (MAP_IDLE_SERF(pos)) {
      path_serf_idle_to_wait_state(pos);
    }

    if (MAP_SERF_INDEX(pos) != 0) {
      serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
      if (!MAP_HAS_FLAG(pos)) {
        serf_set_lost_state(serf);
      } else {
        /* Handle serf close to flag, where
           it should only be lost if walking
           in the wrong direction. */
        int d = serf->s.walking.dir;
        if (d < 0) d += 6;
        if (d == DIR_REVERSE(dir)) {
          serf_set_lost_state(serf);
        }
      }
    }

    if (MAP_HAS_FLAG(pos)) {
      flag_t *flag = game.flags[MAP_OBJ_INDEX(pos)];
      dir_t rev_dir = DIR_REVERSE(in_dir);

      flag->del_path(rev_dir);

      break;
    }

    /* Clear forward reference. */
    tiles[pos].paths &= ~BIT(dir);
    pos = MAP_MOVE(pos, dir);
    in_dir = dir;

    /* Clear backreference. */
    tiles[pos].paths &= ~BIT(DIR_REVERSE(dir));

    /* Find next direction of path. */
    dir = DIR_NONE;
    for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
      if (BIT_TEST(MAP_PATHS(pos), d)) {
        dir = (dir_t)d;
        break;
      }
    }
  }
}

static int
demolish_road(map_pos_t pos) {
  /* TODO necessary?
  game.player[0]->flags |= BIT(4);
  game.player[1]->flags |= BIT(4);
  */

  int r = remove_road_backrefs(pos);
  if (r < 0) {
    /* TODO */
    return -1;
  }

  /* Find directions of path segments to be split. */
  dir_t path_1_dir = DIR_NONE;
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    if (BIT_TEST(MAP_PATHS(pos), d)) {
      path_1_dir = (dir_t)d;
      break;
    }
  }

  dir_t path_2_dir = DIR_NONE;
  for (int d = path_1_dir+1; d <= DIR_UP; d++) {
    if (BIT_TEST(MAP_PATHS(pos), d)) {
      path_2_dir = (dir_t)d;
      break;
    }
  }

  /* If last segment direction is UP LEFT it could
     be to a building and the real path is at UP. */
  if (path_2_dir == DIR_UP_LEFT &&
      BIT_TEST(MAP_PATHS(pos), DIR_UP)) {
    path_2_dir = DIR_UP;
  }

  remove_road_forwards(pos, path_1_dir);
  remove_road_forwards(pos, path_2_dir);

  return 0;
}

/* Demolish road at position. */
int
game_demolish_road(map_pos_t pos, player_t *player) {
  if (!game_can_demolish_road(pos, player)) return -1;

  return demolish_road(pos);
}

/* Build flag on existing path. Path must be split in two segments. */
static void
build_flag_split_path(map_pos_t pos) {
  /* Find directions of path segments to be split. */
  dir_t path_1_dir = DIR_NONE;
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    if (BIT_TEST(MAP_PATHS(pos), d)) {
      path_1_dir = (dir_t)d;
      break;
    }
  }

  dir_t path_2_dir = DIR_NONE;
  for (int d = path_1_dir+1; d <= DIR_UP; d++) {
    if (BIT_TEST(MAP_PATHS(pos), d)) {
      path_2_dir = (dir_t)d;
      break;
    }
  }

  /* If last segment direction is UP LEFT it could
     be to a building and the real path is at UP. */
  if (path_2_dir == DIR_UP_LEFT &&
      BIT_TEST(MAP_PATHS(pos), DIR_UP)) {
    path_2_dir = DIR_UP;
  }

  serf_path_info_t path_1_data;
  serf_path_info_t path_2_data;

  flag_t::fill_path_serf_info(pos, path_1_dir, &path_1_data);
  flag_t::fill_path_serf_info(pos, path_2_dir, &path_2_data);

  flag_t *flag_2 = game.flags[path_2_data.flag_index];
  dir_t dir_2 = path_2_data.flag_dir;

  int select = -1;
  if (flag_2->serf_requested(dir_2)) {
    for (unsigned int i = 1; i < game.max_serf_index; i++) {
      if (SERF_ALLOCATED(i)) {
        serf_t *serf = game_get_serf(i);

        if (serf->state == SERF_STATE_WALKING) {
          if (serf->s.walking.dest == path_1_data.flag_index &&
              serf->s.walking.res == path_1_data.flag_dir) {
            select = 0;
            break;
          } else if (serf->s.walking.dest == path_2_data.flag_index &&
               serf->s.walking.res == path_2_data.flag_dir) {
            select = 1;
            break;
          }
        } else if (serf->state == SERF_STATE_READY_TO_LEAVE_INVENTORY) {
          if (serf->s.ready_to_leave_inventory.dest == path_1_data.flag_index &&
              serf->s.ready_to_leave_inventory.mode == path_1_data.flag_dir) {
            select = 0;
            break;
          } else if (serf->s.ready_to_leave_inventory.dest ==
                     path_2_data.flag_index &&
               serf->s.ready_to_leave_inventory.mode == path_2_data.flag_dir) {
            select = 1;
            break;
          }
        } else if ((serf->state == SERF_STATE_READY_TO_LEAVE ||
              serf->state == SERF_STATE_LEAVING_BUILDING) &&
             serf->s.leaving_building.next_state == SERF_STATE_WALKING) {
          if (serf->s.leaving_building.dest == path_1_data.flag_index &&
              serf->s.leaving_building.field_B == path_1_data.flag_dir) {
            select = 0;
            break;
          } else if (serf->s.leaving_building.dest == path_2_data.flag_index &&
               serf->s.leaving_building.field_B == path_2_data.flag_dir) {
            select = 1;
            break;
          }
        }
      }
    }

    serf_path_info_t *path_data = &path_1_data;
    if (select == 0) path_data = &path_2_data;

    flag_t *selected_flag = game.flags[path_data->flag_index];
    selected_flag->cancel_serf_request(path_data->flag_dir);
  }

  flag_t *flag = game.flags[MAP_OBJ_INDEX(pos)];

  flag->restore_path_serf_info(path_1_dir, &path_1_data);
  flag->restore_path_serf_info(path_2_dir, &path_2_data);
}

/* Check whether player can build flag at pos. */
int
game_can_build_flag(map_pos_t pos, const player_t *player) {
  /* Check owner of land */
  if (!MAP_HAS_OWNER(pos) ||
      MAP_OWNER(pos) != player->player_num) {
    return 0;
  }

  /* Check that land is clear */
  if (map_space_from_obj[MAP_OBJ(pos)] != MAP_SPACE_OPEN) {
    return 0;
  }

  /* Check whether cursor is in water */
  if (MAP_TYPE_UP(pos) < 4 &&
      MAP_TYPE_DOWN(pos) < 4 &&
      MAP_TYPE_DOWN(MAP_MOVE_LEFT(pos)) < 4 &&
      MAP_TYPE_UP(MAP_MOVE_UP_LEFT(pos)) < 4 &&
      MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(pos)) < 4 &&
      MAP_TYPE_UP(MAP_MOVE_UP(pos)) < 4) {
    return 0;
  }

  /* Check that no flags are nearby */
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    if (MAP_OBJ(MAP_MOVE(pos, d)) == MAP_OBJ_FLAG) {
      return 0;
    }
  }

  return 1;
}

/* Build flag at pos. */
int
game_build_flag(map_pos_t pos, player_t *player) {
  if (!game_can_build_flag(pos, player)) return -1;

  flag_t *flag;
  unsigned int flg_index;
  if (!game.flags.allocate(&flag, &flg_index)) return -1;

  flag->set_player(player->player_num);
  flag->set_position(pos);
  map_set_object(pos, MAP_OBJ_FLAG, flg_index);

  if (MAP_PATHS(pos) != 0) {
    build_flag_split_path(pos);
  }

  return 0;
}

/* Check whether military buildings are allowed at pos. */
int
game_can_build_military(map_pos_t pos) {
  /* Check that no military buildings are nearby */
  for (int i = 0; i < 1+6+12; i++) {
    map_pos_t p = MAP_POS_ADD(pos, game.spiral_pos_pattern[i]);
    if (MAP_OBJ(p) >= MAP_OBJ_SMALL_BUILDING &&
        MAP_OBJ(p) <= MAP_OBJ_CASTLE) {
      building_t *bld = game.buildings[MAP_OBJ_INDEX(p)];
      if (bld->is_military()) {
        return 0;
      }
    }
  }

  return 1;
}

/* Return the height that is needed before a large building can be built.
   Returns negative if the needed height cannot be reached. */
int
game_get_leveling_height(map_pos_t pos) {
  /* Find min and max height */
  int h_min = 31;
  int h_max = 0;
  for (int i = 0; i < 12; i++) {
    map_pos_t p = MAP_POS_ADD(pos, game.spiral_pos_pattern[7+i]);
    int h = MAP_HEIGHT(p);
    if (h_min > h) h_min = h;
    if (h_max < h) h_max = h;
  }

  /* Adjust for height of adjacent unleveled buildings */
  for (int i = 0; i < 18; i++) {
    map_pos_t p = MAP_POS_ADD(pos, game.spiral_pos_pattern[19+i]);
    if (MAP_OBJ(p) == MAP_OBJ_LARGE_BUILDING) {
      building_t *bld = game.buildings[MAP_OBJ_INDEX(p)];
      if (bld->is_leveling()) { /* Leveling in progress */
        int h = bld->get_level();
        if (h_min > h) h_min = h;
        if (h_max < h) h_max = h;
      }
    }
  }

  /* Return if height difference is too big */
  if (h_max - h_min >= 9) return -1;

  /* Calculate "mean" height. Height of center is added twice. */
  int h_mean = MAP_HEIGHT(pos);
  for (int i = 0; i < 7; i++) {
    map_pos_t p = MAP_POS_ADD(pos, game.spiral_pos_pattern[i]);
    h_mean += MAP_HEIGHT(p);
  }
  h_mean >>= 3;

  /* Calcualte height after leveling */
  int h_new_min = std::max((h_max > 4) ? (h_max - 4) : 1, 1);
  int h_new_max = h_min + 4;
  int h_new = clamp(h_new_min, h_mean, h_new_max);

  return h_new;
}

static int
map_types_within(map_pos_t pos, unsigned int low, unsigned int high) {
  if ((MAP_TYPE_UP(pos) >= low &&
       MAP_TYPE_UP(pos) <= high) &&
      (MAP_TYPE_DOWN(pos) >= low &&
       MAP_TYPE_DOWN(pos) <= high) &&
      (MAP_TYPE_DOWN(MAP_MOVE_LEFT(pos)) >= low &&
       MAP_TYPE_DOWN(MAP_MOVE_LEFT(pos)) <= high) &&
      (MAP_TYPE_UP(MAP_MOVE_UP_LEFT(pos)) >= low &&
       MAP_TYPE_UP(MAP_MOVE_UP_LEFT(pos)) <= high) &&
      (MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(pos)) >= low &&
       MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(pos)) <= high) &&
      (MAP_TYPE_UP(MAP_MOVE_UP(pos)) >= low &&
       MAP_TYPE_UP(MAP_MOVE_UP(pos)) <= high)) {
    return 1;
  }

  return 0;
}

/* Checks whether a small building is possible at position.*/
int
game_can_build_small(map_pos_t pos) {
  return map_types_within(pos, 4, 7);
}

/* Checks whether a mine is possible at position. */
int
game_can_build_mine(map_pos_t pos) {
  return map_types_within(pos, 11, 14);
}

/* Checks whether a large building is possible at position. */
int
game_can_build_large(map_pos_t pos) {
  /* Check that surroundings are passable by serfs. */
  for (int i = 0; i < 6; i++) {
    map_pos_t p = MAP_POS_ADD(pos, game.spiral_pos_pattern[1+i]);
    map_space_t s = map_space_from_obj[MAP_OBJ(p)];
    if (s >= MAP_SPACE_SEMIPASSABLE) return 0;
  }

  /* Check that buildings in the second shell aren't large or castle. */
  for (int i = 0; i < 12; i++) {
    map_pos_t p = MAP_POS_ADD(pos, game.spiral_pos_pattern[7+i]);
    if (MAP_OBJ(p) >= MAP_OBJ_LARGE_BUILDING &&
        MAP_OBJ(p) <= MAP_OBJ_CASTLE) {
      return 0;
    }
  }

  /* Check if center hexagon is not type grass. */
  if (MAP_TYPE_UP(pos) != 5 ||
      MAP_TYPE_DOWN(pos) != 5 ||
      MAP_TYPE_DOWN(MAP_MOVE_LEFT(pos)) != 5 ||
      MAP_TYPE_UP(MAP_MOVE_UP_LEFT(pos)) != 5 ||
      MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(pos)) != 5 ||
      MAP_TYPE_UP(MAP_MOVE_UP(pos)) != 5) {
    return 0;
  }

  /* Check that leveling is possible */
  int r = game_get_leveling_height(pos);
  if (r < 0) return 0;

  return 1;
}

/* Checks whether a castle can be built by player at position. */
int
game_can_build_castle(map_pos_t pos, const player_t *player) {
  if (PLAYER_HAS_CASTLE(player)) return 0;

  /* Check owner of land around position */
  for (int i = 0; i < 7; i++) {
    map_pos_t p = MAP_POS_ADD(pos, game.spiral_pos_pattern[i]);
    if (MAP_HAS_OWNER(p)) return 0;
  }

  /* Check that land is clear at position */
  if (map_space_from_obj[MAP_OBJ(pos)] != MAP_SPACE_OPEN ||
      MAP_PATHS(pos) != 0) {
    return 0;
  }

  map_pos_t flag_pos = MAP_MOVE_DOWN_RIGHT(pos);

  /* Check that land is clear at position */
  if (map_space_from_obj[MAP_OBJ(flag_pos)] != MAP_SPACE_OPEN ||
      MAP_PATHS(flag_pos) != 0) {
    return 0;
  }

  if (!game_can_build_large(pos)) return 0;

  return 1;
}

/* Check whether player is allowed to build anything
   at position. To determine if the initial castle can
   be built use game_can_build_castle() instead.

   TODO Existing buildings at position should be
   disregarded so this can be used to determine what
   can be built after the existing building has been
   demolished. */
int
game_can_player_build(map_pos_t pos, const player_t *player) {
  if (!PLAYER_HAS_CASTLE(player)) return 0;

  /* Check owner of land around position */
  for (int i = 0; i < 7; i++) {
    map_pos_t p = MAP_POS_ADD(pos, game.spiral_pos_pattern[i]);
    if (!MAP_HAS_OWNER(p) ||
        MAP_OWNER(p) != player->player_num) {
      return 0;
    }
  }

  /* Check whether cursor is in water */
  if (MAP_TYPE_UP(pos) < 4 &&
      MAP_TYPE_DOWN(pos) < 4 &&
      MAP_TYPE_DOWN(MAP_MOVE_LEFT(pos)) < 4 &&
      MAP_TYPE_UP(MAP_MOVE_UP_LEFT(pos)) < 4 &&
      MAP_TYPE_DOWN(MAP_MOVE_UP_LEFT(pos)) < 4 &&
      MAP_TYPE_UP(MAP_MOVE_UP(pos)) < 4) {
    return 0;
  }

  /* Check that no paths are blocking. */
  if (MAP_PATHS(pos) != 0) return 0;

  return 1;
}

/* Checks whether a building of the specified type is possible at
   position. */
int
game_can_build_building(map_pos_t pos, building_type_t type,
                        const player_t *player) {
  if (!game_can_player_build(pos, player)) return 0;

  /* Check that space is clear */
  if (map_space_from_obj[MAP_OBJ(pos)] != MAP_SPACE_OPEN) return 0;

  /* Check that building flag is possible if it
     doesn't already exist. */
  map_pos_t flag_pos = MAP_MOVE_DOWN_RIGHT(pos);
  if (!MAP_HAS_FLAG(flag_pos) &&
      !game_can_build_flag(flag_pos, player)) {
    return 0;
  }

  /* Check if building size is possible. */
  switch (type) {
  case BUILDING_FISHER:
  case BUILDING_LUMBERJACK:
  case BUILDING_BOATBUILDER:
  case BUILDING_STONECUTTER:
  case BUILDING_FORESTER:
  case BUILDING_HUT:
  case BUILDING_MILL:
    if (!game_can_build_small(pos)) return 0;
    break;
  case BUILDING_STONEMINE:
  case BUILDING_COALMINE:
  case BUILDING_IRONMINE:
  case BUILDING_GOLDMINE:
    if (!game_can_build_mine(pos)) return 0;
    break;
  case BUILDING_STOCK:
  case BUILDING_FARM:
  case BUILDING_BUTCHER:
  case BUILDING_PIGFARM:
  case BUILDING_BAKER:
  case BUILDING_SAWMILL:
  case BUILDING_STEELSMELTER:
  case BUILDING_TOOLMAKER:
  case BUILDING_WEAPONSMITH:
  case BUILDING_TOWER:
  case BUILDING_FORTRESS:
  case BUILDING_GOLDSMELTER:
    if (!game_can_build_large(pos)) return 0;
    break;
  default:
    NOT_REACHED();
    break;
  }

  /* Check if military building is possible */
  if ((type == BUILDING_HUT ||
       type == BUILDING_TOWER ||
       type == BUILDING_FORTRESS) &&
      !game_can_build_military(pos)) {
    return 0;
  }

  return 1;
}

/* Build building at position. */
int
game_build_building(map_pos_t pos, building_type_t type, player_t *player) {
  if (!game_can_build_building(pos, type, player)) return -1;

  if (type == BUILDING_STOCK) {
    /* TODO Check that more stocks are allowed to be built */
  }

  building_t *bld;
  unsigned int bld_index;
  if (!game.buildings.allocate(&bld, &bld_index)) return -1;

  flag_t *flag = NULL;
  unsigned int flg_index = 0;
  if (MAP_OBJ(MAP_MOVE_DOWN_RIGHT(pos)) != MAP_OBJ_FLAG) {
    if (!game.flags.allocate(&flag, &flg_index)) {
      game.buildings.erase(bld_index);
      return -1;
    }
  }

  map_tile_t *tiles = game.map.tiles;

  bld->set_level(game_get_leveling_height(pos));
  bld->set_position(pos);
  player->incomplete_building_count[type] += 1;
  bld->set_player(player->player_num);
  map_obj_t map_obj = bld->start_building(type);

  int split_path = 0;
  if (MAP_OBJ(MAP_MOVE_DOWN_RIGHT(pos)) != MAP_OBJ_FLAG) {
    flag->set_player(player->player_num);
    if (MAP_PATHS(MAP_MOVE_DOWN_RIGHT(pos)) != 0) split_path = 1;
  } else {
    flg_index = MAP_OBJ_INDEX(MAP_MOVE_DOWN_RIGHT(pos));
    flag = game.flags[flg_index];
  }

  flag->set_position(MAP_MOVE_DOWN_RIGHT(pos));

  bld->link_flag(flg_index);
  flag->link_building(bld);

  flag->clear_flags();

  tiles[pos].obj &= ~BIT(7);

  map_set_object(pos, map_obj, bld_index);
  tiles[pos].paths |= BIT(1);

  if (MAP_OBJ(MAP_MOVE_DOWN_RIGHT(pos)) != MAP_OBJ_FLAG) {
    map_set_object(MAP_MOVE_DOWN_RIGHT(pos), MAP_OBJ_FLAG, flg_index);
    tiles[MAP_MOVE_DOWN_RIGHT(pos)].paths |= BIT(4);
  }

  if (split_path) build_flag_split_path(MAP_MOVE_DOWN_RIGHT(pos));

  return 0;
}

/* Create the initial serfs that occupies the castle. */
static void
create_initial_castle_serfs(player_t *player, building_t *castle) {
  player->build |= BIT(2);

  /* Spawn serf 4 */
  inventory_t *inventory = castle->get_inventory();
  serf_t *serf = inventory->spawn_serf_generic();
  if (serf == NULL) return;
  inventory->specialize_serf(serf, SERF_TRANSPORTER_INVENTORY);

  serf_log_state_change(serf, SERF_STATE_BUILDING_CASTLE);
  serf->state = SERF_STATE_BUILDING_CASTLE;
  serf->s.building_castle.inv_index = player->castle_inventory;
  map_set_serf_index(serf->pos, SERF_INDEX(serf));

  building_t *building = game.buildings[player->building];
  building->set_main_serf(SERF_INDEX(serf));

  /* Spawn generic serfs */
  for (int i = 0; i < 5; i++) {
    spawn_serf(player, NULL, NULL, 0);
  }

  /* Spawn three knights */
  for (int i = 0; i < 3; i++) {
    serf = inventory->spawn_serf_generic();
    if (serf == NULL) return;
    inventory->promote_serf_to_knight(serf);
  }

  /* Spawn toolmaker */
  serf = inventory->spawn_serf_generic();
  if (serf == NULL) return;
  inventory->specialize_serf(serf, SERF_TOOLMAKER);

  /* Spawn timberman */
  serf = inventory->spawn_serf_generic();
  if (serf == NULL) return;
  inventory->specialize_serf(serf, SERF_LUMBERJACK);

  /* Spawn sawmiller */
  serf = inventory->spawn_serf_generic();
  if (serf == NULL) return;
  inventory->specialize_serf(serf, SERF_SAWMILLER);

  /* Spawn stonecutter */
  serf = inventory->spawn_serf_generic();
  if (serf == NULL) return;
  inventory->specialize_serf(serf, SERF_STONECUTTER);

  /* Spawn digger */
  serf = inventory->spawn_serf_generic();
  if (serf == NULL) return;
  inventory->specialize_serf(serf, SERF_DIGGER);

  /* Spawn builder */
  serf = inventory->spawn_serf_generic();
  if (serf == NULL) return;
  inventory->specialize_serf(serf, SERF_BUILDER);

  /* Spawn fisherman */
  serf = inventory->spawn_serf_generic();
  if (serf == NULL) return;
  inventory->specialize_serf(serf, SERF_FISHER);

  /* Spawn two geologists */
  for (int i = 0; i < 2; i++) {
    serf = inventory->spawn_serf_generic();
    if (serf == NULL) return;
    inventory->specialize_serf(serf, SERF_GEOLOGIST);
  }

  /* Spawn two miners */
  for (int i = 0; i < 2; i++) {
    serf = inventory->spawn_serf_generic();
    if (serf == NULL) return;
    inventory->specialize_serf(serf, SERF_MINER);
  }
}

/* Build castle at position. */
int
game_build_castle(map_pos_t pos, player_t *player) {
  if (!game_can_build_castle(pos, player)) return -1;

  inventory_t *inventory;
  unsigned int inv_index;
  if (!game.inventories.allocate(&inventory, &inv_index)) return -1;

  building_t *castle;
  unsigned int bld_index;
  if (!game.buildings.allocate(&castle, &bld_index)) {
    game.inventories.erase(inv_index);
    return -1;
  }

  flag_t *flag;
  unsigned int flg_index;
  if (!game.flags.allocate(&flag, &flg_index)) {
    game.buildings.erase(bld_index);
    game.inventories.erase(inv_index);
    return -1;
  }

  player->flags |= BIT(0); /* Has castle */
  player->build |= BIT(3);
  player->total_building_score += building_get_score_from_type(BUILDING_CASTLE);
  player->castle_flag = flg_index;

  castle->start_activity();
  castle->serf_arrive();
  castle->set_inventory(inventory);
  player->castle_inventory = inv_index;
  inventory->set_building_index(bld_index);
  inventory->set_flag_index(flg_index);
  inventory->set_player_num(player->player_num);
  inventory->apply_supplies_preset(player->initial_supplies);

  game.map_gold_deposit += inventory->get_count_of(RESOURCE_GOLDBAR);
  game.map_gold_deposit += inventory->get_count_of(RESOURCE_GOLDORE);

  castle->set_position(pos);
  flag->set_position(MAP_MOVE_DOWN_RIGHT(pos));
  castle->set_player(player->player_num);
  castle->start_building(BUILDING_CASTLE);
  player->building = bld_index;
  flag->set_player(player->player_num);
  flag->set_accepts_serfs(true);
  flag->set_has_inventory();
  flag->set_accepts_resources(true);
  castle->link_flag(flg_index);
  flag->link_building(castle);

  map_tile_t *tiles = game.map.tiles;
  map_set_object(pos, MAP_OBJ_CASTLE, bld_index);
  tiles[pos].paths |= BIT(1);

  map_set_object(MAP_MOVE_DOWN_RIGHT(pos), MAP_OBJ_FLAG, flg_index);
  tiles[MAP_MOVE_DOWN_RIGHT(pos)].paths |= BIT(4);

  /* Level land in hexagon below castle */
  int h = game_get_leveling_height(pos);
  map_set_height(pos, h);
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    map_set_height(MAP_MOVE(pos, d), h);
  }

  game_update_land_ownership(pos);
  create_initial_castle_serfs(player, castle);

  player->last_tick = game.tick;

  game_calculate_military_flag_state(castle);

  return 0;
}

static void
flag_remove_player_refs(flag_t *flag) {
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    if (PLAYER_IS_ACTIVE(game.player[i])) {
      if (game.player[i]->index == flag->get_index()) {
        game.player[i]->index = 0;
      }
    }
  }
}

/* Check whether road can be demolished. */
int
game_can_demolish_road(map_pos_t pos, const player_t *player) {
  if (!MAP_HAS_OWNER(pos) ||
      MAP_OWNER(pos) != player->player_num) {
    return 0;
  }

  if (MAP_PATHS(pos) == 0 ||
      MAP_HAS_FLAG(pos) ||
      MAP_HAS_BUILDING(pos)) {
    return 0;
  }

  return 1;
}

/* Check whether flag can be demolished. */
bool
game_can_demolish_flag(map_pos_t pos, const player_t *player) {
  if (MAP_OBJ(pos) != MAP_OBJ_FLAG) return false;

  if (BIT_TEST(MAP_PATHS(pos), DIR_UP_LEFT) &&
      MAP_OBJ(MAP_MOVE_UP_LEFT(pos)) >= MAP_OBJ_SMALL_BUILDING &&
      MAP_OBJ(MAP_MOVE_UP_LEFT(pos)) <= MAP_OBJ_CASTLE) {
    return false;
  }

  if (MAP_PATHS(pos) == 0) return true;

  flag_t *flag = game.flags[MAP_OBJ_INDEX(pos)];

  if (flag->get_player() != player->player_num) return false;

  return flag->can_demolish();
}

static int
demolish_flag(map_pos_t pos) {
  /* Handle any serf at pos. */
  if (MAP_SERF_INDEX(pos) != 0) {
    serf_t *serf = game_get_serf(MAP_SERF_INDEX(pos));
    switch (serf->state) {
    case SERF_STATE_READY_TO_LEAVE:
    case SERF_STATE_LEAVING_BUILDING:
      serf->s.leaving_building.next_state = SERF_STATE_LOST;
      break;
    case SERF_STATE_FINISHED_BUILDING:
    case SERF_STATE_WALKING:
      if (MAP_PATHS(pos) == 0) {
        serf_log_state_change(serf, SERF_STATE_LOST);
        serf->state = SERF_STATE_LOST;
      }
      break;
    default:
      break;
    }
  }

  flag_t *flag = game.flags[MAP_OBJ_INDEX(pos)];
  assert(!flag->has_building());

  flag_remove_player_refs(flag);

  /* Handle connected flag. */
  flag->merge_paths(pos);

  /* Update serfs with reference to this flag. */
  for (unsigned int i = 1; i < game.max_serf_index; i++) {
    if (SERF_ALLOCATED(i)) {
      serf_t *serf = game_get_serf(i);

      if (serf->state == SERF_STATE_READY_TO_LEAVE_INVENTORY &&
          serf->s.ready_to_leave_inventory.dest == flag->get_index()) {
        serf->s.ready_to_leave_inventory.dest = 0;
        serf->s.ready_to_leave_inventory.mode = -2;
      } else if (serf->state == SERF_STATE_WALKING &&
           serf->s.walking.dest == flag->get_index()) {
        serf->s.walking.dest = 0;
        serf->s.walking.res = -2;
      } else if (serf->state == SERF_STATE_IDLE_IN_STOCK && 1/*...*/) {
        /* TODO */
      } else if ((serf->state == SERF_STATE_LEAVING_BUILDING ||
            serf->state == SERF_STATE_READY_TO_LEAVE) &&
           serf->s.leaving_building.dest == flag->get_index() &&
           serf->s.leaving_building.next_state == SERF_STATE_WALKING) {
        serf->s.leaving_building.dest = 0;
        serf->s.leaving_building.field_B = -2;
      }
    }
  }

  map_set_object(pos, MAP_OBJ_NONE, 0);

  /* Remove resources from flag. */
  flag->remove_all_resources();

  game.flags.erase(flag->get_index());

  return 0;
}

/* Demolish flag at pos. */
int
game_demolish_flag(map_pos_t pos, player_t *player) {
  if (!game_can_demolish_flag(pos, player)) return -1;

  return demolish_flag(pos);
}

static int
demolish_building(map_pos_t pos) {
  building_t *building = game.buildings[MAP_OBJ_INDEX(pos)];

  if (building->is_burning()) return 0;

  building_remove_player_refs(building);

  player_t *player = game.player[building->get_player()];
  map_tile_t *tiles = game.map.tiles;

  building->burnup();

  /* Remove path to building. */
  tiles[pos].paths &= ~BIT(1);
  tiles[MAP_MOVE_DOWN_RIGHT(pos)].paths &= ~BIT(4);

  /* Disconnect flag. */
  flag_t *flag = game.flags[building->get_flag_index()];
  flag->unlink_building();
  flag_reset_transport(flag);

  /* Remove lost gold stock from total count. */
  if (building->is_done() &&
      (building->get_type() == BUILDING_HUT ||
       building->get_type() == BUILDING_TOWER ||
       building->get_type() == BUILDING_FORTRESS ||
       building->get_type() == BUILDING_GOLDSMELTER)) {
    int gold_stock = building->get_res_count_in_stock(1);
    game.map_gold_deposit -= gold_stock;
  }

  /* Update land owner ship if the building is military. */
  if (building->is_done() &&
      building->is_active() &&
      building->is_military()) {
    game_update_land_ownership(building->get_position());
  }

  if (building->is_done() &&
      (building->get_type() == BUILDING_CASTLE ||
       building->get_type() == BUILDING_STOCK)) {
    /* Cancel resources in the out queue and remove gold
       from map total. */
    if (building->is_active()) {
      inventory_t *inventory = building->get_inventory();

      inventory->lose_queue();

      game.map_gold_deposit -= inventory->get_count_of(RESOURCE_GOLDBAR);
      game.map_gold_deposit -= inventory->get_count_of(RESOURCE_GOLDORE);

      game.inventories.erase(inventory->get_index());
    }

    /* Let some serfs escape while the building is burning. */
    int escaping_serfs = 0;
    for (unsigned int i = 1; i < game.max_serf_index; i++) {
      if (SERF_ALLOCATED(i)) {
        serf_t *serf = game_get_serf(i);

        if (serf->pos == building->get_position() &&
            (serf->state == SERF_STATE_IDLE_IN_STOCK ||
             serf->state == SERF_STATE_READY_TO_LEAVE_INVENTORY)) {
          if (escaping_serfs < 12) {
            /* Serf is escaping. */
            escaping_serfs += 1;
            serf->state = SERF_STATE_ESCAPE_BUILDING;
          } else {
            /* Kill this serf. */
            serf_set_type(serf, SERF_DEAD);
            game_free_serf(SERF_INDEX(serf));
          }
        }
      }
    }
  } else {
    building->stop_activity();
  }

  /* Remove stock from building. */
  building->remove_stock();

  building->stop_playing_sfx();

  int serf_index = building->get_burning_counter();
  building->set_burning_counter(2047);
  building->set_tick(game.tick);

  /* Update player fields. */
  if (building->is_done()) {
    player->total_building_score -=
                             building_get_score_from_type(building->get_type());

    if (building->get_type() != BUILDING_CASTLE) {
      player->completed_building_count[building->get_type()] -= 1;
    }
  } else {
    player->incomplete_building_count[building->get_type()] -= 1;
  }

  if (building->has_serf()) {
    building->serf_gone();

    if (building->is_done() &&
        building->get_type() == BUILDING_CASTLE) {
      player->build &= ~BIT(3);
      player->castle_score -= 1;

      building->set_burning_counter(8191);

      for (unsigned int i = 1; i < game.max_serf_index; i++) {
        if (SERF_ALLOCATED(i)) {
          serf_t *serf = game_get_serf(i);
          if (SERF_TYPE(serf) == SERF_TRANSPORTER_INVENTORY &&
              serf->pos == building->get_position()) {
            serf_set_type(serf, SERF_TRANSPORTER);
            serf->counter = 0;

            if (MAP_SERF_INDEX(serf->pos) == SERF_INDEX(serf)) {
              serf_log_state_change(serf, SERF_STATE_LOST);
              serf->state = SERF_STATE_LOST;
              serf->s.lost.field_B = 0;
            } else {
              serf_log_state_change(serf, SERF_STATE_ESCAPE_BUILDING);
              serf->state = SERF_STATE_ESCAPE_BUILDING;
            }
          }
        }
      }
    }

    if (building->is_done() &&
        building->is_military()) {
      while (serf_index != 0) {
        serf_t *serf = game_get_serf(serf_index);
        serf_index = serf->s.defending.next_knight;

        if (MAP_SERF_INDEX(serf->pos) == SERF_INDEX(serf)) {
          serf_log_state_change(serf, SERF_STATE_LOST);
          serf->state = SERF_STATE_LOST;
          serf->s.lost.field_B = 0;
        } else {
          serf_log_state_change(serf, SERF_STATE_ESCAPE_BUILDING);
          serf->state = SERF_STATE_ESCAPE_BUILDING;
        }
      }
    } else {
      serf_t *serf = game_get_serf(serf_index);
      if (SERF_TYPE(serf) == SERF_TRANSPORTER_INVENTORY) {
        serf_set_type(serf, SERF_TRANSPORTER);
      }

      serf->counter = 0;

      if (MAP_SERF_INDEX(serf->pos) == SERF_INDEX(serf)) {
        serf_log_state_change(serf, SERF_STATE_LOST);
        serf->state = SERF_STATE_LOST;
        serf->s.lost.field_B = 0;
      } else {
        serf_log_state_change(serf, SERF_STATE_ESCAPE_BUILDING);
        serf->state = SERF_STATE_ESCAPE_BUILDING;
      }
    }
  }

  map_pos_t flag_pos = MAP_MOVE_DOWN_RIGHT(pos);
  if (MAP_PATHS(flag_pos) == 0 &&
      MAP_OBJ(flag_pos) == MAP_OBJ_FLAG) {
    game_demolish_flag(flag_pos, player);
  }

  return 0;
}

/* Demolish building at pos. */
int
game_demolish_building(map_pos_t pos, player_t *player) {
  building_t *building = game.buildings[MAP_OBJ_INDEX(pos)];

  if (building->get_player() != player->player_num) return -1;
  if (building->is_burning()) return -1;

  return demolish_building(pos);
}

/* Calculate the flag state of military buildings (distance to enemy). */
void
game_calculate_military_flag_state(building_t *building) {
  const int border_check_offsets[] = {
    31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
    100, 101, 102, 103, 104, 105, 106, 107, 108,
    259, 260, 261, 262, 263, 264,
    241, 242, 243, 244, 245, 246,
    217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228,
    247, 248, 249, 250, 251, 252,
    -1,

    265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276,
    -1,

    277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288,
    289, 290, 291, 292, 293, 294,
    -1
  };

  int f, k;
  for (f = 3, k = 0; f > 0; f--) {
    int offset;
    while ((offset = border_check_offsets[k++]) >= 0) {
      map_pos_t check_pos = MAP_POS_ADD(building->get_position(),
                                        game.spiral_pos_pattern[offset]);
      if (MAP_HAS_OWNER(check_pos) &&
          MAP_OWNER(check_pos) != building->get_player()) {
        goto break_loops;
      }
    }
  }

break_loops:
  building->set_state(f);
}

/* Map pos is lost to the owner, demolish everything. */
static void
game_surrender_land(map_pos_t pos) {
  /* Remove building. */
  if (MAP_OBJ(pos) >= MAP_OBJ_SMALL_BUILDING &&
      MAP_OBJ(pos) <= MAP_OBJ_CASTLE) {
    demolish_building(pos);
  }

  if (!MAP_HAS_FLAG(pos) && MAP_PATHS(pos) != 0) {
    demolish_road(pos);
  }

  int remove_roads = MAP_HAS_FLAG(pos);

  /* Remove roads and building around pos. */
  for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
    map_pos_t p = MAP_MOVE(pos, d);

    if (MAP_OBJ(p) >= MAP_OBJ_SMALL_BUILDING &&
        MAP_OBJ(p) <= MAP_OBJ_CASTLE) {
      demolish_building(p);
    }

    if (remove_roads &&
        (MAP_PATHS(p) & BIT(DIR_REVERSE(d)))) {
      demolish_road(p);
    }
  }

  /* Remove flag. */
  if (MAP_OBJ(pos) == MAP_OBJ_FLAG) {
    demolish_flag(pos);
  }
}

/* Initialize land ownership for whole map. */
void
game_init_land_ownership() {
  for (buildings_t::iterator i = game.buildings.begin();
       i != game.buildings.end(); ++i) {
    building_t *building = *i;
    if (building->is_military()) {
      game_update_land_ownership(building->get_position());
    }
  }
}

/* Update land ownership around map position. */
void
game_update_land_ownership(map_pos_t init_pos) {
  /* Currently the below algorithm will only work when
     both influence_radius and calculate_radius are 8. */
  const int influence_radius = 8;
  const int influence_diameter = 1 + 2*influence_radius;

  int calculate_radius = influence_radius;
  int calculate_diameter = 1 + 2*calculate_radius;

  int *temp_arr = reinterpret_cast<int*>(calloc(GAME_MAX_PLAYER_COUNT*
                                                calculate_diameter*
                                                calculate_diameter,
                                                sizeof(int)));
  if (temp_arr == NULL) abort();

  const int military_influence[] = {
    0, 1, 2, 4, 7, 12, 18, 29, -1, -1,  /* hut */
    0, 3, 5, 8, 11, 15, 22, 30, -1, -1,  /* tower */
    0, 6, 10, 14, 19, 23, 27, 31, -1, -1  /* fortress */
  };

  const int map_closeness[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0,
    1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, 0, 0, 0, 0, 0,
    1, 2, 3, 4, 4, 4, 4, 4, 4, 3, 2, 1, 0, 0, 0, 0, 0,
    1, 2, 3, 4, 5, 5, 5, 5, 5, 4, 3, 2, 1, 0, 0, 0, 0,
    1, 2, 3, 4, 5, 6, 6, 6, 6, 5, 4, 3, 2, 1, 0, 0, 0,
    1, 2, 3, 4, 5, 6, 7, 7, 7, 6, 5, 4, 3, 2, 1, 0, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7, 6, 5, 4, 3, 2, 1,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 7, 6, 5, 4, 3, 2, 1,
    0, 0, 1, 2, 3, 4, 5, 6, 7, 7, 7, 6, 5, 4, 3, 2, 1,
    0, 0, 0, 1, 2, 3, 4, 5, 6, 6, 6, 6, 5, 4, 3, 2, 1,
    0, 0, 0, 0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 4, 3, 2, 1,
    0, 0, 0, 0, 0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 3, 2, 1,
    0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1,
    0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1
  };

  /* Find influence from buildings in 33*33 square
     around the center. */
  for (int i = -(influence_radius+calculate_radius);
       i <= influence_radius+calculate_radius; i++) {
    for (int j = -(influence_radius+calculate_radius);
         j <= influence_radius+calculate_radius; j++) {
      map_pos_t pos = MAP_POS_ADD(init_pos,
                MAP_POS(j & game.map.col_mask,
                  i & game.map.row_mask));

      if (MAP_OBJ(pos) >= MAP_OBJ_SMALL_BUILDING &&
          MAP_OBJ(pos) <= MAP_OBJ_CASTLE &&
          BIT_TEST(MAP_PATHS(pos), DIR_DOWN_RIGHT)) {  // TODO(_): Why wouldn't
                                                       //          this be set?
        building_t *building = game.buildings[MAP_OBJ_INDEX(pos)];
        int mil_type = -1;

        if (building->get_type() == BUILDING_CASTLE) {
          /* Castle has military influence even when not done. */
          mil_type = 2;
        } else if (building->is_done() &&
                   building->is_active()) {
          switch (building->get_type()) {
          case BUILDING_HUT: mil_type = 0; break;
          case BUILDING_TOWER: mil_type = 1; break;
          case BUILDING_FORTRESS: mil_type = 2; break;
          default: break;
          }
        }

        if (mil_type >= 0 &&
            !building->is_burning()) {
          const int *influence = military_influence + 10*mil_type;
          const int *closeness = map_closeness +
            influence_diameter*std::max(-i, 0) + std::max(-j, 0);
          int *arr = temp_arr +
            (building->get_player() * calculate_diameter*calculate_diameter) +
            calculate_diameter * std::max(i, 0) + std::max(j, 0);

          for (int k = 0; k < influence_diameter - abs(i); k++) {
            for (int l = 0; l < influence_diameter - abs(j); l++) {
              int inf = influence[*closeness];
              if (inf < 0) *arr = 128;
              else if (*arr < 128) *arr = std::min(*arr + inf, 127);

              closeness += 1;
              arr += 1;
            }
            closeness += abs(j);
            arr += abs(j);
          }
        }
      }
    }
  }

  map_tile_t *tiles = game.map.tiles;

  /* Update owner of 17*17 square. */
  for (int i = -calculate_radius; i <= calculate_radius; i++) {
    for (int j = -calculate_radius; j <= calculate_radius; j++) {
      int max_val = 0;
      int player = -1;
      for (int p = 0; p < GAME_MAX_PLAYER_COUNT; p++) {
        int *arr = temp_arr +
          p*calculate_diameter*calculate_diameter +
          calculate_diameter*(i+calculate_radius) + (j+calculate_radius);
        if (*arr > max_val) {
          max_val = *arr;
          player = p;
        }
      }

      map_pos_t pos = MAP_POS_ADD(init_pos,
                MAP_POS(j & game.map.col_mask,
                  i & game.map.row_mask));
      int old_player = -1;
      if (MAP_HAS_OWNER(pos)) old_player = MAP_OWNER(pos);

      if (old_player >= 0 && player != old_player) {
        game.player[old_player]->total_land_area -= 1;
        game_surrender_land(pos);
      }

      if (player >= 0) {
        if (player != old_player) {
          game.player[player]->total_land_area += 1;
          tiles[pos].height = (1 << 7) | (player << 5) | MAP_HEIGHT(pos);
        }
      } else {
        tiles[pos].height = (0 << 7) | (0 << 5) | MAP_HEIGHT(pos);
      }
    }
  }

  free(temp_arr);

  /* Update military building flag state. */
  for (int i = -25; i <= 25; i++) {
    for (int j = -25; j <= 25; j++) {
      map_pos_t pos = MAP_POS_ADD(init_pos,
                MAP_POS(i & game.map.col_mask,
                  j & game.map.row_mask));

      if (MAP_OBJ(pos) >= MAP_OBJ_SMALL_BUILDING &&
          MAP_OBJ(pos) <= MAP_OBJ_CASTLE &&
          BIT_TEST(MAP_PATHS(pos), DIR_DOWN_RIGHT)) {
        building_t *building = game.buildings[MAP_OBJ_INDEX(pos)];
        if (building->is_done() && building->is_military()) {
          game_calculate_military_flag_state(building);
        }
      }
    }
  }
}

static void
game_demolish_flag_and_roads(map_pos_t pos) {
  if (MAP_HAS_FLAG(pos)) {
    /* Remove roads around pos. */
    for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
      map_pos_t p = MAP_MOVE(pos, d);

      if (MAP_PATHS(p) & BIT(DIR_REVERSE(d))) {
        demolish_road(p);
      }
    }

    if (MAP_OBJ(pos) == MAP_OBJ_FLAG) {
      demolish_flag(pos);
    }
  } else if (MAP_PATHS(pos) != 0) {
    demolish_road(pos);
  }
}

/* The given building has been defeated and is being
   occupied by player. */
void
game_occupy_enemy_building(building_t *building, int player_num) {
  /* Take the building. */
  player_t *def_player = game.player[building->get_player()];
  player_add_notification(def_player, 2 + (player_num << 5),
                          building->get_position());

  player_t *player = game.player[player_num];
  player_add_notification(player, 3 + (player_num << 5),
                          building->get_position());

  if (building->get_type() == BUILDING_CASTLE) {
    player->castle_score += 1;
    demolish_building(building->get_position());
  } else {
    flag_t *flag = game.flags[building->get_flag_index()];
    flag_reset_transport(flag);

    /* Update player scores. */
    def_player->total_building_score -=
      building_get_score_from_type(building->get_type());
    def_player->total_land_area -= 7;
    def_player->completed_building_count[building->get_type()] -= 1;

    player->total_building_score +=
      building_get_score_from_type(building->get_type());
    player->total_land_area += 7;
    player->completed_building_count[building->get_type()] += 1;

    /* Demolish nearby buildings. */
    for (int i = 0; i < 12; i++) {
      map_pos_t pos = MAP_POS_ADD(building->get_position(),
                                  game.spiral_pos_pattern[7+i]);
      if (MAP_OBJ(pos) >= MAP_OBJ_SMALL_BUILDING &&
          MAP_OBJ(pos) <= MAP_OBJ_CASTLE) {
        demolish_building(pos);
      }
    }

    /* Change owner of land and remove roads and flags
       except the flag associated with the building. */
    map_tile_t *tiles = game.map.tiles;
    tiles[building->get_position()].height = (1 << 7) | (player_num << 5) |
      MAP_HEIGHT(building->get_position());

    for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
      map_pos_t pos = MAP_MOVE(building->get_position(), d);
      tiles[pos].height = (1 << 7) | (player_num << 5) | MAP_HEIGHT(pos);
      if (pos != flag->get_position()) {
        game_demolish_flag_and_roads(pos);
      }
    }

    /* Change owner of flag. */
    flag->set_player(player_num);

    /* Reset destination of stolen resources. */
    flag->reset_destination_of_stolen_resources();

    /* Remove paths from flag. */
    for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
      if (flag->has_path((dir_t)d)) {
        demolish_road(MAP_MOVE(flag->get_position(), d));
      }
    }

    /* Change owner of building */
    building->set_player(player_num);

    game_update_land_ownership(building->get_position());

    if (PLAYER_IS_AI(player)) {
      /* TODO AI */
    }
  }
}

/* mode: 0: IN, 1: STOP, 2: OUT */
void
game_set_inventory_resource_mode(inventory_t *inventory, int mode) {
  flag_t *flag = game.flags[inventory->get_flag_index()];

  if (mode == 0) {
    inventory->set_res_mode(mode_in);
  } else if (mode == 1) {
    inventory->set_res_mode(mode_stop);
  } else {
    inventory->set_res_mode(mode_out);
  }

  if (mode > 0) {
    flag->set_accepts_resources(false);

    /* Clear destination of serfs with resources destined
       for this inventory. */
    int dest = flag->get_index();
    for (unsigned int i = 1; i < game.max_serf_index; i++) {
      if (SERF_ALLOCATED(i)) {
        serf_t *serf = game_get_serf(i);

        switch (serf->state) {
        case SERF_STATE_TRANSPORTING:
          if (serf->s.walking.dest == dest) {
            serf->s.walking.dest = 0;
          }
          break;
        case SERF_STATE_DROP_RESOURCE_OUT:
          if (serf->s.move_resource_out.res_dest == dest) {
            serf->s.move_resource_out.res_dest = 0;
          }
          break;
        case SERF_STATE_LEAVING_BUILDING:
          if (serf->s.leaving_building.dest == dest &&
              serf->s.leaving_building.next_state ==
                SERF_STATE_DROP_RESOURCE_OUT) {
            serf->s.leaving_building.dest = 0;
          }
          break;
        case SERF_STATE_MOVE_RESOURCE_OUT:
          if (serf->s.move_resource_out.res_dest == dest &&
              serf->s.move_resource_out.next_state ==
                SERF_STATE_DROP_RESOURCE_OUT) {
            serf->s.move_resource_out.res_dest = 0;
          }
          break;
        default:
          break;
        }
      }
    }
  } else {
    flag->set_accepts_resources(true);
  }
}

/* mode: 0: IN, 1: STOP, 2: OUT */
void
game_set_inventory_serf_mode(inventory_t *inventory, int mode) {
  flag_t *flag = game.flags[inventory->get_flag_index()];

  if (mode == 0) {
    inventory->set_serf_mode(mode_in);
  } else if (mode == 1) {
    inventory->set_serf_mode(mode_stop);
  } else {
    inventory->set_serf_mode(mode_out);
  }

  if (mode > 0) {
    flag->set_accepts_serfs(false);

    /* Clear destination of serfs destined for this inventory. */
    int dest = flag->get_index();
    for (unsigned int i = 1; i < game.max_serf_index; i++) {
      if (SERF_ALLOCATED(i)) {
        serf_t *serf = game_get_serf(i);

        switch (serf->state) {
        case SERF_STATE_WALKING:
          if (serf->s.walking.dest == dest &&
              serf->s.walking.res < 0) {
            serf->s.walking.res = -2;
            serf->s.walking.dest = 0;
          }
          break;
        case SERF_STATE_READY_TO_LEAVE_INVENTORY:
          if (serf->s.ready_to_leave_inventory.dest == dest &&
              serf->s.ready_to_leave_inventory.mode < 0) {
            serf->s.ready_to_leave_inventory.mode = -2;
            serf->s.ready_to_leave_inventory.dest = 0;
          }
          break;
        case SERF_STATE_LEAVING_BUILDING:
        case SERF_STATE_READY_TO_LEAVE:
          if (serf->s.leaving_building.dest == dest &&
              serf->s.leaving_building.field_B < 0 &&
              serf->s.leaving_building.next_state == SERF_STATE_WALKING) {
            serf->s.leaving_building.field_B = -2;
            serf->s.leaving_building.dest = 0;
          }
          break;
        default:
          break;
        }
      }
    }
  } else {
    flag->set_accepts_serfs(true);
  }
}



/* Initialize AI parameters. */
static void
init_ai_values(player_t *player, int face) {
  const int ai_values_0[] = { 13, 10, 16, 9, 10, 8, 6, 10, 12, 5, 8 };
  const int ai_values_1[] = { 10000, 13000, 16000, 16000, 18000, 20000,
                              19000, 18000, 30000, 23000, 26000 };
  const int ai_values_2[] = { 10000, 35000, 20000, 27000, 37000, 25000,
                              40000, 30000, 50000, 35000, 40000 };
  const int ai_values_3[] = { 0, 36, 0, 31, 8, 480, 3, 16, 0, 193, 39 };
  const int ai_values_4[] = { 0, 30000, 5000, 40000, 50000, 20000, 45000,
                              35000, 65000, 25000, 30000 };
  const int ai_values_5[] = { 60000, 61000, 60000, 65400, 63000, 62000,
                              65000, 63000, 64000, 64000, 64000 };

  player->ai_value_0 = ai_values_0[face-1];
  player->ai_value_1 = ai_values_1[face-1];
  player->ai_value_2 = ai_values_2[face-1];
  player->ai_value_3 = ai_values_3[face-1];
  player->ai_value_4 = ai_values_4[face-1];
  player->ai_value_5 = ai_values_5[face-1];
}

static void
player_init(unsigned int number, unsigned int face, unsigned int color,
            unsigned int supplies, unsigned int reproduction,
            unsigned int intelligence) {
  assert(number < GAME_MAX_PLAYER_COUNT);

  player_t *player = game.player[number];
  player->flags = 0;

  if (face == 0) return;

  if (face < 12) { /* AI player */
    player->flags |= BIT(7); /* Set AI bit */
    /* TODO ... */
    /*game.max_next_index = 49;*/
  }

  player->player_num = number;
  player->color = color;
  player->face = face;
  player->build = 0;

  player->building = 0;
  player->castle_flag = 0;
  player->cont_search_after_non_optimal_find = 7;
  player->knights_to_spawn = 0;
  player->total_land_area = 0;
  player->total_military_score = 0;
  player->total_building_score = 0;

  player->last_tick = 0;

  player->serf_to_knight_rate = 20000;
  player->serf_to_knight_counter = 0x8000;

  player->knight_occupation[0] = 0x10;
  player->knight_occupation[1] = 0x21;
  player->knight_occupation[2] = 0x32;
  player->knight_occupation[3] = 0x43;

  player_reset_food_priority(player);
  player_reset_planks_priority(player);
  player_reset_steel_priority(player);
  player_reset_coal_priority(player);
  player_reset_wheat_priority(player);
  player_reset_tool_priority(player);

  player_reset_flag_priority(player);
  player_reset_inventory_priority(player);

  player->current_sett_5_item = 8;
  player->current_sett_6_item = 15;

  player->military_max_gold = 0;

  player->timers_count = 0;

  player->castle_knights_wanted = 3;
  player->castle_knights = 0;
  player->send_knight_delay = 0;
  player->send_generic_delay = 0;
  player->serf_index = 0;

  /* player->field_1b0 = 0; AI */
  /* player->field_1b2 = 0; AI */

  player->initial_supplies = supplies;
  player->reproduction_reset = (60 - reproduction) * 50;
  player->ai_intelligence = 1300*intelligence + 13535;

  if (PLAYER_IS_AI(player)) init_ai_values(player, face);

  player->reproduction_counter = player->reproduction_reset;
  player->castle_score = 0;

  for (int i = 0; i < 26; i++) {
    player->resource_count[i] = 0;
  }

  for (int i = 0; i < 24; i++) {
    player->completed_building_count[i] = 0;
    player->incomplete_building_count[i] = 0;
  }

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 112; j++) {
      player->player_stat_history[i][j] = 0;
    }
  }

  for (int i = 0; i < 26; i++) {
    for (int j = 0; j < 120; j++) {
      player->resource_count_history[i][j] = 0;
    }
  }

  for (int i = 0; i < 27; i++) {
    player->serf_count[i] = 0;
  }

  /* TODO AI: Set array field_402 of length 25 to -1. */
  /* TODO AI: Set array field_434 of length 280*2 to 0 */
  /* TODO AI: Set array field_1bc of length 8 to -1 */
}

/* Add new player to the game. Returns the player number
   or negative on error. */
int
game_add_player(unsigned int face, unsigned int color, unsigned int supplies,
                unsigned int reproduction, unsigned int intelligence) {
  int number = -1;
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    if (!PLAYER_IS_ACTIVE(game.player[i])) {
      number = i;
      break;
    }
  }

  if (number < 0) return -1;

  /* Allocate object */
  game.player[number] = reinterpret_cast<player_t*>(calloc(1,
                                                           sizeof(player_t)));
  if (game.player[number] == NULL) abort();

  player_init(number, face, color, supplies,
        reproduction, intelligence);

  /* Update map values dependent on player count */
  int active_players = 0;
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    if (PLAYER_IS_ACTIVE(game.player[i])) active_players += 1;
  }

  game.map_gold_morale_factor = 10 * 1024 * active_players;

  return number;
}

void
game_init() {
  /* Initialize global lookup tables */
  init_spiral_pattern();

  game.svga |= BIT(3); /* Game has started. */
  game.game_speed = DEFAULT_GAME_SPEED;

  game.map_water_level = 20;
  game.map_max_lake_area = 14;

  game.update_map_last_tick = 0;
  game.update_map_counter = 0;
  game.update_map_16_loop = 0;
  game.update_map_initial_pos = 0;
  game.next_index = 0;

  /* Clear player objects */
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    if (game.player[i] != NULL) free(game.player[i]);
    game.player[i] = NULL;
  }

  memset(game.player_history_index, '\0', sizeof(game.player_history_index));
  memset(game.player_history_counter, '\0',
         sizeof(game.player_history_counter));
  game.resource_history_index = 0;

  game.tick = 0;
  game.const_tick = 0;
  game.tick_diff = 0;

  game.game_stats_counter = 0;
  game.history_counter = 0;

  game.knight_morale_counter = 0;
  game.inventory_schedule_counter = 0;
}

/* Initialize spiral_pos_pattern from spiral_pattern. */
static void
init_spiral_pos_pattern() {
  int *pattern = game.spiral_pattern;

  if (game.spiral_pos_pattern == NULL) {
    game.spiral_pos_pattern =
                    reinterpret_cast<map_pos_t*>(malloc(295*sizeof(map_pos_t)));
    if (game.spiral_pos_pattern == NULL) abort();
  }

  for (int i = 0; i < 295; i++) {
    int x = pattern[2*i] & game.map.col_mask;
    int y = pattern[2*i+1] & game.map.row_mask;

    game.spiral_pos_pattern[i] = MAP_POS(x, y);
  }
}

static void
game_init_map() {
  game.map.col_size = 5 + game.map_size/2;
  game.map.row_size = 5 + (game.map_size - 1)/2;
  game.map.cols = 1 << game.map.col_size;
  game.map.rows = 1 << game.map.row_size;

  /* game.split |= BIT(3); */

  if (game.map.cols < 64 || game.map.rows < 64) {
    /* game.split &= ~BIT(3); */
  }

  map_init_dimensions(&game.map);

  game.map_regions = (game.map.cols >> 5) * (game.map.rows >> 5);

  game.serf_limit = 500 * game.map_regions;

  /* Reserve half the serfs equally among players,
     and the rest according to the amount of land controlled. */
  game.max_serfs_per_player = (game.serf_limit/2)/GAME_MAX_PLAYER_COUNT;
  game.max_serfs_from_land = game.serf_limit/2;

  game.map_gold_morale_factor = 0;

  init_spiral_pos_pattern();
  map_init();
  map_init_minimap();

  game.winning_player = -1;
  /* game.show_game_end = 0; */
  game.max_next_index = 33;
}

void
game_allocate_objects() {
  /* Serfs */
  if (game.serfs != NULL) free(game.serfs);
  game.serfs = reinterpret_cast<serf_t*>(malloc(game.serf_limit *
                                                sizeof(serf_t)));
  if (game.serfs == NULL) abort();

  if (game.serf_bitmap != NULL) free(game.serf_bitmap);
  game.serf_bitmap = reinterpret_cast<uint8_t*>(calloc(((game.serf_limit-1) /
                                                        8) + 1, 1));
  if (game.serf_bitmap == NULL) abort();

  game.max_serf_index = 0;

  /* Create NULL-serf */
  serf_t *serf;
  game_alloc_serf(&serf, NULL);
  serf->state = SERF_STATE_NULL;
  serf->type = 0;
  serf->animation = 0;
  serf->counter = 0;
  serf->pos = -1;

  /* Create NULL-building (index 0 is undefined) */
  game.buildings.allocate(NULL, NULL);
}

int
game_load_mission_map(int level) {
  const unsigned int default_player_colors[] = {
    64, 72, 68, 76
  };

  memcpy(&game.init_map_rnd, &mission[level].rnd,
         sizeof(random_state_t));

  game.mission_level = level;
  game.map_size = 3;
  game.map_preserve_bugs = 1;

  game.init_map_rnd.state[0] ^= 0x5a5a;
  game.init_map_rnd.state[1] ^= 0xa5a5;
  game.init_map_rnd.state[2] ^= 0xc3c3;

  game_init_map();
  game_allocate_objects();

  /* Initialize player and build initial castle */
  for (int i = 0; i < GAME_MAX_PLAYER_COUNT; i++) {
    const mission_t *m = &mission[level];
    unsigned int face = (i == 0) ? 12 : m->player[i].face;
    if (face == 0) continue;

    int n = game_add_player(face, default_player_colors[i],
          m->player[i].supplies,
          m->player[i].reproduction,
          m->player[i].intelligence);
    if (n < 0) return -1;

    if (m->player[n].castle.col > -1 &&
        m->player[n].castle.row > -1) {
      map_pos_t pos = MAP_POS(m->player[n].castle.col,
            m->player[n].castle.row);
      game_build_castle(pos, game.player[n]);
    }
  }

  return 0;
}

int
game_load_random_map(int size, const random_state_t *rnd) {
  if (size < 3 || size > 10) return -1;

  game.map_size = size;
  game.map_preserve_bugs = 0;

  memcpy(&game.init_map_rnd, rnd, sizeof(random_state_t));

  game_init_map();
  game_allocate_objects();

  return 0;
}

int
game_load_save_game(const char *path) {
  int r = load_state(path);
  if (r < 0) return -1;

  init_spiral_pos_pattern();
  game_init_land_ownership();
  map_init_minimap();

  return 0;
}

/* Cancel a resource being transported to destination. This
   ensures that the destination can request a new resource. */
void
game_cancel_transported_resource(resource_type_t res, unsigned int dest) {
  if (dest == 0) return;

  flag_t *flag = game.flags[dest];
  assert(flag->has_building());
  building_t *building = flag->get_building();
  building->cancel_transported_resource(res);
}

/* Called when a resource is lost forever from the game. This will
   update any global state keeping track of that resource. */
void
game_lose_resource(resource_type_t res) {
  if (res == RESOURCE_GOLDORE ||
      res == RESOURCE_GOLDBAR) {
    game.map_gold_deposit -= 1;
  }
}

uint16_t
game_random_int() {
  return random_int(&game.rnd);
}
