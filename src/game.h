/*
 * game.h - Gameplay related functions
 *
 * Copyright (C) 2013-2017  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef SRC_GAME_H_
#define SRC_GAME_H_

#include <vector>
#include <map>
#include <string>
#include <list>
#include <memory>
#include <mutex>   //NOLINT (build/c++11) this is a Google Chromium req, not relevant to general C++.  // for AI thread locking

#include "src/player.h"
#include "src/flag.h"
#include "src/serf.h"
#include "src/inventory.h"
#include "src/map.h"
#include "src/random.h"
#include "src/objects.h"
#include "src/lookup.h"

#define DEFAULT_GAME_SPEED  2
#define DEFAULT_TICK_LENGTH  20
#define GAME_MAX_PLAYER_COUNT  4

// these were originally private, inside Game class
typedef Collection<Flag, 5000> Flags;

class SaveReaderBinary;
class SaveReaderText;
class SaveWriterText;

class Game {
 public:
  std::mutex mutex;
  typedef std::list<Serf*> ListSerfs;
  typedef std::list<Building*> ListBuildings;
  typedef std::list<Inventory*> ListInventories;

 protected:
  // moved to outside of Game class so AI can use the Flags typedef
  //typedef Collection<Flag, 5000> Flags;
  typedef Collection<Inventory, 100> Inventories;
  typedef Collection<Building, 1000> Buildings;
  typedef Collection<Serf, 5000> Serfs;
  typedef Collection<Player, 5> Players;

  PMap map;

  typedef std::map<unsigned int, unsigned int> Values;
  int map_gold_morale_factor;
  unsigned int gold_total;

  Players players;

  Flags flags;
  Inventories inventories;  // castle and warehouse/stocks.  NOT other buildings
  Buildings buildings;
  Serfs serfs;

  Random init_map_rnd;
  unsigned int game_speed_save;
  unsigned int game_speed;
  unsigned int tick;
  unsigned int last_tick;
  // what is the diff between const_tick and last_tick??
  //  it looks like const_tick is not game-speed adjusted,
  //  while tick is game-speed adjusted
  unsigned int const_tick;
  unsigned int game_stats_counter;
  unsigned int history_counter;
  Random rnd;
  uint16_t next_index;
  uint16_t flag_search_counter;

  uint16_t update_map_last_tick;
  int16_t update_map_counter;
  MapPos update_map_initial_pos;
  int tick_diff;
  uint16_t max_next_index;
  int16_t update_map_16_loop;
  int player_history_index[4];
  int player_history_counter[3];
  int resource_history_index;
  int ticks_since_last_corruption_detection; // for debugging
  uint16_t field_340;
  uint16_t field_342;
  Inventory *field_344;
  int game_type;
  int tutorial_level;
  int mission_level;
  int map_preserve_bugs;
  int player_score_leader;

  int knight_morale_counter;
  int inventory_schedule_counter;

  bool ai_locked;
  bool signal_ai_exit;
  unsigned int ai_threads_remaining;
  ColorDotMap debug_mark_pos;  // list of positions for LayerDebug to mark
  std::vector<int> debug_mark_serf;    // used to mark serfs on map with status text.  For debugging, when debug overlay is on
  //Road *debug_mark_road = (new Road);  // a road or pseudo-road to mark
  Road debug_mark_road;  // a road or pseudo-road to mark
  std::string mutex_message;  // used for logging why mutex being locked/unlocked
  clock_t mutex_timer_start;  // used for logging how much time spent with mutex lock
  bool must_redraw_frame;  // part of hack for option_FogOfWar to allow Serf/Building to trigger frame redraw
  // I think this has to be defined here and not in Game constructor because the getter is being called before Game constructor?? not sure
  MapPos desired_cursor_pos = bad_map_pos;  // to allow Game to set the Interface/Viewport player cursor pos (during Interface::update)

 public:
  Game();
  virtual ~Game();

  PMap get_map() { return map; }

  // for AI to use random functions
  Random * get_rand() { return &rnd; }

  // tell ai to exit when a game ends
  void stop_ai_threads() { signal_ai_exit = true; }
  // ai checks this every loop and exits if true
  bool should_ai_stop() { return signal_ai_exit; }
  // ai begins locked, and is unlocked by game init close
  void unlock_ai() { ai_locked = false; }
  // for debugging
  void lock_ai() { ai_locked = true; }
  // ai remains locked while the game init_box is shown because they are not actually playing yet
  bool is_ai_locked() { return ai_locked; }
  // used to keep track of running ai threads, there are examples using std::future and std::async but I couldn't understand them
  void ai_thread_exiting() { ai_threads_remaining--; Log::Debug["game"] << "ai_thread_exiting, " << ai_threads_remaining << " remain"; }
  void ai_thread_starting() { ai_threads_remaining++; Log::Debug["game"] << "ai_thread_starting, " << ai_threads_remaining << " started"; }
  unsigned int get_ai_thread_count() { return ai_threads_remaining; }
  std::mutex * get_mutex() { return &mutex; }  // AI uses this to call game mutex lock/unlock while writing log messages to its own AI log
  // used by AI to check if game is paused
  unsigned int get_game_speed() const { return game_speed; }
  // used for Debug overlay LayerDebug to mark MapPos on screen
  ColorDotMap * get_debug_mark_pos() { return &debug_mark_pos; }
  std::vector<int> * get_debug_mark_serf() { return &debug_mark_serf; }
  void set_debug_mark_pos(MapPos pos, std::string color){ 
    debug_mark_pos.erase(pos);
    debug_mark_pos.insert(ColorDot(pos, color));
  }
  void clear_debug_mark_pos(){ debug_mark_pos.clear(); }
  Road * get_debug_mark_road() { return &debug_mark_road; }
  void set_debug_mark_road(Road road) {
    Log::Debug["game.h"] << "inside Game::set_debug_mark_road, provided new road has length " << road.get_length();
     debug_mark_road = road;
  }
  //void clear_debug_mark_road() {  // is this needed?
  void reset_game_options_defaults();

  unsigned int get_tick() const { return tick; }
  unsigned int get_const_tick() const { return const_tick; }
  unsigned int get_gold_morale_factor() const { return map_gold_morale_factor; }
  unsigned int get_gold_total() const { return gold_total; }
  void add_gold_total(int delta);

  Building *get_building_at_pos(MapPos pos);
  Flag *get_flag_at_pos(MapPos pos);
  Serf *get_serf_at_pos(MapPos pos);

  /* External interface */
  unsigned int add_player(unsigned int intelligence, unsigned int supplies,
                          unsigned int reproduction);
  //bool init(unsigned int map_size, const Random &random);
  bool init(unsigned int map_size, const Random &random, const CustomMapGeneratorOptions custom_map_generator_options);

  void update();
  void pause();
  void speed_increase();
  void speed_decrease();
  void speed_reset();

  // hack function to allow Serf, Building to trigger game to flush the frame
  //  so for option_FogOfWar so FoW cna be updated only when borders change
  void set_must_redraw_frame() { must_redraw_frame = true; }
  bool get_must_redraw_frame() { return must_redraw_frame; }
  void unset_must_redraw_frame() { must_redraw_frame = false; }

  // allow the Game to change the map clicked cursor pos
  //  Because the Game object cannot access the Interface or Viewport
  //  it is not possible to directly set any aspect of those, instead
  //  they make decisions based on Game state. So, new functions created
  //  to allow the Game to indicate to the Interface that it desires the
  //  player cursor/click pos to be set, and the Interface can do it here
  MapPos get_update_viewport_cursor_pos(){ 
    //Log::Debug["game.h"] << "inside Game::get_update_viewport_cursor_pos, returning pos " << desired_cursor_pos;
    return desired_cursor_pos;
  }
  void set_update_viewport_cursor_pos(MapPos pos){
    //Log::Debug["game.h"] << "inside Game::set_update_viewport_cursor_pos, old pos was " << desired_cursor_pos << ", setting new pos " << pos;
    desired_cursor_pos = pos;
  }
  void unset_update_viewport_cursor_pos(){ desired_cursor_pos = bad_map_pos;}

  void prepare_ground_analysis(MapPos pos, int estimates[5]);
  bool send_geologist(Flag *dest);

  int get_leveling_height(MapPos pos) const;

  bool can_build_military(MapPos pos) const;
  bool can_build_small(MapPos pos) const;  // NOTE - I think this misleading!  it does not consider blocking objects nor position ownership
  bool can_build_mine(MapPos pos) const;    // this too I bet
  bool can_build_large(MapPos pos) const;   // this too I bet
  bool can_build_building(MapPos pos, Building::Type type,  // this too I bet
                          const Player *player) const;
  bool can_build_castle(MapPos pos, const Player *player) const;
  bool can_build_flag(MapPos pos, const Player *player) const;   // this too I bet
  bool can_player_build(MapPos pos, const Player *player) const;
  bool can_build_field(MapPos pos) const;  // added to shorten FourSeasons/AdvancedFarming code

  int can_build_road(const Road &road, const Player *player,
                     MapPos *dest, bool *water) const;

  bool can_demolish_flag(MapPos pos, const Player *player) const;
  bool can_demolish_road(MapPos pos, const Player *player) const;

  bool build_road(const Road &road, const Player *player);

  bool build_flag(MapPos pos, Player *player);
  bool build_building(MapPos pos, Building::Type type, Player *player);
  bool build_castle(MapPos pos, Player *player);

  bool demolish_road(MapPos pos, Player *player);
  bool demolish_flag(MapPos pos, Player *player);
  bool demolish_building(MapPos pos, Player *player);
  bool mark_building_for_demolition(MapPos pos, Player *player);

  void set_inventory_resource_mode(Inventory *inventory, int mode);
  void set_inventory_serf_mode(Inventory *inventory, int mode);


  /* Internal interface */

  void init_land_ownership();
  void update_land_ownership(MapPos pos);
  void init_FogOfWar();  // option_FogOfWar
  void update_FogOfWar(MapPos pos);  // option_FogOfWar
  void occupy_enemy_building(Building *building, int player);

  void cancel_transported_resource(Resource::Type type, unsigned int dest);
  void lose_resource(Resource::Type type);

  uint16_t random_int();

  bool send_serf_to_flag(Flag *dest, Serf::Type type, Resource::Type res1, Resource::Type res2);

  int get_player_history_index(size_t scale) const {
    return player_history_index[scale]; }
  int get_resource_history_index() const { return resource_history_index; }
  //void set_resource_history_index(int res) { resource_history_index = res; }

  int next_search_id();

  Serf *create_serf(int index = -1);
  void delete_serf(Serf *serf);
  Flag *create_flag(int index = -1);
  Inventory *create_inventory(int index = -1);
  void delete_inventory(Inventory *inventory);
  Building *create_building(int index = -1);
  void delete_building(Building *building);

  Serf *get_serf(unsigned int index) { return serfs[index]; }
  Flag *get_flag(unsigned int index) { return flags[index]; }
  // got a segfault during flags_copy = game->get_flags... need to mutex wrap all AI game->get_flags calls?
  Flags *get_flags() { return &flags; }
  Inventory *get_inventory(unsigned int index) { return inventories[index]; }
  Building *get_building(unsigned int index) { return buildings[index]; }
  Player *get_player(unsigned int index) { return players[index]; }

  ListSerfs get_player_serfs(Player *player);
  ListBuildings get_player_buildings(Player *player);
  ListSerfs get_serfs_in_inventory(Inventory *inventory);
  ListSerfs get_serfs_in_inventory_out_queue(Inventory *inventory);
  ListSerfs get_serfs_related_to(unsigned int dest, Direction dir);
  ListInventories get_player_inventories(Player *player);

  ListSerfs get_serfs_at_pos(MapPos pos);

  Player *get_next_player(const Player *player);
  unsigned int get_enemy_score(const Player *player) const;
  // not only when captured from enemies, this runs when newly-built friendly military buildings first occupied also
  void building_captured(Building *building);
  void clear_search_id();
  void mutex_lock(const char* message);
  void mutex_unlock();



 protected:
  void clear_serf_request_failure();
  void update_knight_morale();
  static bool update_inventories_cb(Flag *flag, void *data);
  void update_inventories();
  void update_flags();
  static bool send_serf_to_flag_search_cb(Flag *flag, void *data);
  void update_buildings();
  void update_serfs();
  void record_player_history(int max_level, int aspect,
                             const int history_index[], const Values &values);
  int calculate_clear_winner(const Values &values);
  void update_game_stats();
  void get_resource_estimate(MapPos pos, int weight, int estimates[5]);
  bool road_segment_in_water(MapPos pos, Direction dir) const;
  void flag_reset_transport(Flag *flag);
  //void building_remove_player_refs(Building *building);  // this function should no longer be needed now that popup class objects store their target_obj_index
  bool path_serf_idle_to_wait_state(MapPos pos);
  void remove_road_forwards(MapPos pos, Direction dir);
  bool demolish_road_(MapPos pos);
  void build_flag_split_path(MapPos pos);
  bool map_types_within(MapPos pos, Map::Terrain low, Map::Terrain high) const;
  //void flag_remove_player_refs(Flag *flag);  // this function should no longer be needed now that popup class objects store their target_obj_index
  bool demolish_flag_(MapPos pos);
  bool demolish_building_(MapPos pos);
  void surrender_land(MapPos pos);
  void demolish_flag_and_roads(MapPos pos);

  MapPos auto_place_castle(Player *player);
  bool place_castle(MapPos center_pos, int player_index, unsigned int distance, unsigned int desperation);

 public:
  friend SaveReaderBinary&
    operator >> (SaveReaderBinary &reader, Game &game);
  friend SaveReaderText&
    operator >> (SaveReaderText &reader, Game &game);
  friend SaveWriterText&
    operator << (SaveWriterText &writer, Game &game);

 protected:
  bool load_serfs(SaveReaderBinary *reader, int max_serf_index);
  bool load_flags(SaveReaderBinary *reader, int max_flag_index);
  bool load_buildings(SaveReaderBinary *reader, int max_building_index);
  bool load_inventories(SaveReaderBinary *reader, int max_inventory_index);
};

typedef std::shared_ptr<Game> PGame;

#endif  // SRC_GAME_H_
