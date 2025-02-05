/*
 * building.h - Building related functions.
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


#ifndef SRC_BUILDING_H_
#define SRC_BUILDING_H_

#include "src/map.h"
#include "src/resource.h"
#include "src/misc.h"
#include "src/serf.h"
#include "src/objects.h"

// adding support for requested resource timeouts
//  this number represents the number of seconds to allow for a requested
//   resource to travel one game tile at default game speed (2)
//   It needs to account for steepness and reasonable traffic
//  A quick test shows that it takes about nine seconds and about 1000 ticks
//   for a serf to travel one tile up a road of the steepest (red) category
// timeouts from 100-1000 might be reasonable, the higher end mainly to account
//  for serf and resource traffic on roads, terrain probably matters less
#define TIMEOUT_SECS_PER_TILE  350
// also copying these here from freeserf.h as it is not included but is needed for
//  the request resource timeouts
// The length between game updates in miliseconds
//   this is controlled by SDL_Timer ever TICK_LENGTH which triggers and SDL Event
//   and is handled by the SDL_WaitEvent inside event_loop-sdl.cc EventLoopSDL::run()
//   which also handles other SDL Events such as user input immediately
//#define TICK_LENGTH  20
// 1000/20 is 50, but it seems the actual timing is about 100 per second at game speed 2
//  maybe it is intended for game speed 1?
//#define TICKS_PER_SEC  (1000/TICK_LENGTH)


class Inventory;
class Serf;
class SaveReaderBinary;
class SaveReaderText;
class SaveWriterText;

class Building : public GameObject {
 public:
  // Max number of different types of resources accepted by buildings.
  static const unsigned int kMaxStock = 3;

 public:
  typedef enum Type {
    TypeNone = 0,
    TypeFisher,
    TypeLumberjack,
    TypeBoatbuilder,
    TypeStonecutter,
    TypeStoneMine,
    TypeCoalMine,
    TypeIronMine,
    TypeGoldMine,
    TypeForester,
    TypeStock,
    TypeHut,
    TypeFarm,
    TypeButcher,
    TypePigFarm,
    TypeMill,
    TypeBaker,
    TypeSawmill,
    TypeSteelSmelter,
    TypeToolMaker,
    TypeWeaponSmith,
    TypeTower,
    TypeFortress,
    TypeGoldSmelter,
    TypeCastle
  } Type;

 protected:
  typedef struct Stock {
    Resource::Type type;
    int prio;
    int available;
    int requested;
    int maximum;
    // adding support for resource request timeouts
    int req_timeout_tick[8];
  } Stock;

  /* Map position of building */
  MapPos pos;
  /* Type of building */
  Type type;
  /* Building owner */
  unsigned int owner;
  /* Building under construction */
  bool constructing;
  //bool pending_demolition; // for option_AdvancedDemolition    /* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180*/
  /* Flags */
  size_t threat_level;  // 0 is safest/white flag, 3 is highest threat, thick cross
                        // THIS IS ALSO BEING RE-USED for tracking if a mine has ever mined a resource, to distinguish between a new mine and a totally depleted one
                        //  in this case, a new mine starts at "threat_level" 0 and increases by one each time a mining attempt is made, capping out at 15 as that is as far back as the tracking goes
  bool playing_sfx;
  bool serf_request_failed;
  bool serf_requested;
  //int serf_req_timeout_tick;  // adding support for serf request timeouts/retries
  bool burning;
  bool active;
  bool holder;
  /* Index of flag connected to this building */
  unsigned int flag;
  /* Stock of this building */
  Stock stock[kMaxStock];
  unsigned int holder_or_first_knight;
  int burning_counter;
  int progress;
  union u {
    unsigned int tick; /* Used for burning building. */
    unsigned int level;
  } u;
  Inventory *inventory;

 public:
  Building(Game *game, unsigned int index);

  MapPos get_position() const { return pos; }
  void set_position(MapPos position) { pos = position; }

  unsigned int get_flag_index() const { return flag; }
  void link_flag(unsigned int flag_index) { flag = flag_index; }
  void unlink_flag() { flag = 0; }

  bool has_knight() const { return (holder_or_first_knight != 0); }  // this doesn't actually check if the holder is a knight!!
  unsigned int get_holder_or_first_knight() const { return holder_or_first_knight; }
  void set_holder_or_first_knight(unsigned int serf);

  int get_burning_counter() const { return burning_counter; }
  void set_burning_counter(int counter) { burning_counter = counter; }
  void decrease_burning_counter(int delta) { burning_counter -= delta; }

  /* Type of building. */
  Type get_type() const { return type; }
  bool is_military() const { return (type == TypeHut) ||
                                    (type == TypeTower) ||
                                    (type == TypeFortress) ||
                                    (type == TypeCastle); }
  /* Owning player of the building. */
  unsigned int get_owner() const { return owner; }
  void set_owner(unsigned int new_owner) { owner = new_owner; }
  /* Whether construction of the building is finished. */
  bool is_done() const { return !constructing; }
  bool is_leveling() const { return (!is_done() && progress == 0); }
  void done_leveling();
  Map::Object start_building(Type type);
  int get_progress() const { return progress; }
  bool build_progress();
  void increase_mining(int res);
  void set_under_attack() { progress |= BIT(0); }
  bool is_under_attack() const { return BIT_TEST(progress, 0); }

  /* The threat level of the building. Higher values mean that
   the building is closer to the enemy. */
  // ALSO RE-USED TO DETERMINE IF A MINE HAS EVER BEEN ACTIVE
  //  in this case, a new mine starts at "threat_level" 0 and increases by one each time a mining attempt is made, capping out at 15 as that is as far back as the tracking goes
  size_t get_threat_level() const { return threat_level; }  // 0 is safest/white flag, 3 is highest threat, thick cross  
  /* Building is currently playing back a sound effect. */
  //
  // THESE FUNCTION NAMES ARE MISLEADING:
  // the start/stop functions don't actually start or stop anything,
  //  they are used as a locking mechanism to prevent sounds from
  //  playing too often and/or overlapping.  Because the random function
  //  returns the SAME VALUE OVER MULTIPLE xxx::update cycles, these functions
  //  are used to prevent constant looping of the sound effects.  A call to
  //  "stop_playing_sfx" means "unlock" (i.e. allow the sound to play again this update)
  //
  bool is_playing_sfx() const { return playing_sfx; }
  void start_playing_sfx() { playing_sfx = true; }
  void stop_playing_sfx() { playing_sfx = false; }
  /* Building is active (specifics depend on building type). */
  bool is_active() const { return active; }
  void start_activity() { active = true; }
  void stop_activity() { active = false; }
  /* Building is burning. */
  bool is_burning() const { return burning; }
  bool burnup();
  
    /* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180
  void call_for_demolition() { pending_demolition = true; }  // for option_AdvancedDemolition
  //void cancel_demolition() { pending_demolition = false; }  // for option_AdvancedDemolition  NOT IMPLEMENTED YET
  bool is_pending_demolition() { return pending_demolition; }  // for option_AdvancedDemolition
  */
  /* Building has an associated serf. */
  bool has_serf() const { return holder; }
    /* removing AdvancedDemolition for now, see https://github.com/forkserf/forkserf/issues/180
  void evict_holder(); // used for option_AdvancedDemolition
  void evict_knights(); // used for option_AdvancedDemolition
  */
  /* Building has succesfully requested a serf. */
  void serf_request_granted() { serf_requested = true; }
  void requested_serf_lost();
  void clear_holder();
  void requested_serf_reached(Serf *serf);
  /* Building has requested a serf but none was available. */
  void clear_serf_request_failure() { serf_request_failed = false; }
  void knight_request_granted();

  /* Building has inventory and the inventory pointer is valid. */
  bool has_inventory() const { return (inventory != nullptr); }
  Inventory *get_inventory() { return inventory; }
  void set_inventory(Inventory *inventory_) { inventory = inventory_; }

  unsigned int get_level() const { return u.level; }
  void set_level(unsigned int level) { u.level = level; }

  unsigned int get_tick() const { return u.tick; }
  void set_tick(unsigned int tick) { u.tick = tick; }

  unsigned int get_knight_count() const { return waiting_planks(); }  // wtf is this???  it seems this function does what it says, but not sure why planks

  unsigned int waiting_stone() const {
    return stock[1].available; }  // Stone always in stock #1
  unsigned int waiting_planks() const {
    return stock[0].available; }  // Planks always in stock #0
  unsigned int military_gold_count() const;

  void cancel_transported_resource(Resource::Type res);

  Serf *call_defender_out();
  Serf *call_attacker_out(int knight_index);

  // adding support for requested resource timeouts
  //bool add_requested_resource(Resource::Type res, bool fix_priority);
  bool add_requested_resource(Resource::Type res, bool fix_priority, int dist_from_inv);
  bool is_stock_active(int stock_num) const {
    return (stock[stock_num].type > 0); }
  unsigned int get_res_count_in_stock(int stock_num) const {
    return stock[stock_num].available; }
  Resource::Type get_res_type_in_stock(int stock_num) const {
    return stock[stock_num].type; }
  void stock_init(unsigned int stock_num, Resource::Type type,
                  unsigned int maximum);
  void remove_stock();
  int get_max_priority_for_resource(
    Resource::Type resource, int minimum = 0) const;
  int get_maximum_in_stock(int stock_num) const {
    return stock[stock_num].maximum; }
  int get_requested_in_stock(int stock_num) const {
    return stock[stock_num].requested; }
  void set_priority_in_stock(int stock_num, int priority) {
    stock[stock_num].prio = priority; }
  void set_initial_res_in_stock(int stock_num, int count) {
    stock[stock_num].available = count; }
  //void requested_resource_delivered(Resource::Type resource);
  bool requested_resource_delivered(Resource::Type resource);  // adding bool return to indicate if res should be sent back, for resource req timeouts
  void plank_used_for_build() {
    stock[0].available -= 1; stock[0].maximum -= 1; }
  void stone_used_for_build() {
    stock[1].available -= 1; stock[1].maximum -= 1; }
  bool use_resource_in_stock(int stock_num);
  bool use_resources_in_stocks();
  void decrease_requested_for_stock(int stock_num) {
    stock[stock_num].requested -= 1; }

  int pigs_count() const { return stock[1].available; }
  void send_pig_to_butcher() { stock[1].available -= 1; }
  void place_new_pig() { stock[1].available += 1; }

  void boat_clear() { stock[1].available = 0; }
  void boat_do() { stock[1].available++; }

  void requested_knight_arrived();
  void requested_knight_attacking_on_walk() { stock[0].requested -= 1; }
  void requested_knight_defeat_on_walk() {
    if (!has_inventory()) stock[0].requested -= 1; }
  bool is_enough_place_for_knight() const;
  bool knight_come_back_from_fight(Serf *knight);
  void knight_occupy();

  void update_military_flag_state();

  void update(unsigned int tick);

  friend SaveReaderBinary&
    operator >> (SaveReaderBinary &reader, Building &building);
  friend SaveReaderText&
    operator >> (SaveReaderText &reader, Building &building);
  friend SaveWriterText&
    operator << (SaveWriterText &writer, Building &building);

 private:
  void update();
  void update_unfinished();
  void update_unfinished_adv();
  void update_castle();
  void update_military();

  void request_serf_if_needed();

  bool send_serf_to_building(Serf::Type type, Resource::Type res1, Resource::Type res2);
};


int building_get_score_from_type(Building::Type type);


#endif  // SRC_BUILDING_H_
