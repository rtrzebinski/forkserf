/*
 * interface.cc - Top-level GUI interface
 *
 * Copyright (C) 2013-2018  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/interface.h"

#include <iostream>
#include <fstream>
#include <utility>
#include <thread>   //NOLINT (build/c++11) this is a Google Chromium req, not relevant to general C++.  // for AI threads
#include <vector>

#include "src/misc.h"
#include "src/debug.h"
#include "src/data.h"
#include "src/audio.h"
#include "src/freeserf_endian.h"
#include "src/freeserf.h"
#include "src/popup.h"
#include "src/game-init.h"
#include "src/viewport.h"
#include "src/notification.h"
#include "src/panel.h"
#include "src/savegame.h"
#include "src/lookup.h"
#include "src/ai.h"
#include "src/game-options.h"

#include "src/version.h" // for tick_length


// Interval between automatic save games
//#define AUTOSAVE_INTERVAL  (5*60*TICKS_PER_SEC)  // this is reasonable for normal play
//#define AUTOSAVE_INTERVAL  (1*60*TICKS_PER_SEC)  // much higher frequency, for debugging
#define AUTOSAVE_INTERVAL  (5*60*1000/tick_length)  // this is reasonable for normal play

Interface::Interface()
  : building_road_valid_dir(0)
  , sfx_queue{0}
  , trees_in_view(0)   // for bird sounds
  , water_in_view(0)   // for wave sounds
  , desert_in_view(0)  // for wind sounds
  , return_pos(0) {
  is_playing_birdsfx = false;   // for ambient bird sounds near trees
  is_playing_watersfx = false;  // for ambient wave sounds near water
  is_playing_desertsfx = false; // for ambient wind sounds near desert
  displayed = true;

  game = nullptr;

  map_cursor_pos = 0;
  map_cursor_type = (CursorType)0;
  build_possibility = BuildPossibilityNone;

  player = nullptr;

  /* Settings */
  config = 0x39;
  msg_flags = 0;
  return_timeout = 0;

  current_stat_8_mode = 0;
  current_stat_7_item = 7;

  map_cursor_sprites[0].sprite = 32;
  map_cursor_sprites[1].sprite = 33;
  map_cursor_sprites[2].sprite = 33;
  map_cursor_sprites[3].sprite = 33;
  map_cursor_sprites[4].sprite = 33;
  map_cursor_sprites[5].sprite = 33;
  map_cursor_sprites[6].sprite = 33;

  last_const_tick = 0;
  last_autosave_tick = 0;
  last_subseason_tick = 0;  // messing with weather/seasons/palette

  viewport = nullptr;
  panel = nullptr;
  popup = nullptr;
  init_box = nullptr;
  notification_box = nullptr;

  // THIS IS ALSO COPIED IN popup.cc clickmap case/switch
  // adding support for EditMapGeneratorOptions
  for (int x = 0; x < 23; x++){
    custom_map_generator_options.opt[x] = 1.00;
  }
    /* here is the original game ratio for mined resources (copied from ClassicMapGenerator)
    { 9, Map::MineralsCoal },
    { 4, Map::MineralsIron },
    { 2, Map::MineralsGold },
    { 2, Map::MineralsStone }
    */
  custom_map_generator_options.opt[CustomMapGeneratorOption::MountainGold] = 2.00;
  custom_map_generator_options.opt[CustomMapGeneratorOption::MountainIron] = 4.00;
  custom_map_generator_options.opt[CustomMapGeneratorOption::MountainCoal] = 9.00;
  custom_map_generator_options.opt[CustomMapGeneratorOption::MountainStone] = 2.00;

  GameManager::get_instance().add_handler(this);
  set_game(GameManager::get_instance().get_current_game());

}

Interface::~Interface() {
  GameManager::get_instance().del_handler(this);
  set_game(nullptr);

  delete viewport;
  delete panel;
  delete popup;
  delete init_box;
  delete notification_box;
}

Viewport *
Interface::get_viewport() {
  return viewport;
}

PanelBar *
Interface::get_panel_bar() {
  return panel;
}

PopupBox *
Interface::get_popup_box() {
  return popup;
}

/* Open popup box */
void
Interface::open_popup(int box) {
  if (popup == nullptr) {
    popup = new PopupBox(this);
    add_float(popup, 0, 0);
  }
  layout();
  if (box == PopupBox::TypeGameOptions || box == PopupBox::TypeGameOptions2 || box == PopupBox::TypeEditMapGenerator){
    // double wide, normal height
    popup->set_size(288, 160);
    // recenter the popup
    //   because the popup->get_position call requires providing pointers to ints,
    //   we must create those ints and pointers and then check them after the call
    int options_pos_x = 0;
    int options_pos_y = 0;
    int *ptoptions_pos_x = &options_pos_x;
    int *ptoptions_pos_y = &options_pos_y;
    popup->get_position(ptoptions_pos_x, ptoptions_pos_y);
    options_pos_x = *ptoptions_pos_x;
    // shift left half of one "normal width", keep same height
    options_pos_x -= 72;
    popup->move_to(options_pos_x, options_pos_y);
  }
  /* this doesn't work here, not exactly clear why
  if (box == PopupBox::TypeMap){
    // for option_FogOfWar
    //Minimap *minimap = popup->get_minimap();  // this gets invalid conversion error
    MinimapGame *minimap = popup->get_minimap();  // why does conversion to Minimap work in calling function in popup.cc, but not here?  maybe missing typedef access
    if (minimap != nullptr) {
      Log::Debug["panel.cc"] << "inside PanelBar::button_click, opening minimap D";
      minimap->set_player_index(get_player()->get_index());  // for option_FogOfWar
      MapPos pos = viewport->get_current_map_pos();
      minimap->move_to_map_pos(pos);
    }
  }*/
  popup->show((PopupBox::Type)box);
  if (panel != nullptr) {
    panel->update();
  }
}

/* Close the current popup. */
void
Interface::close_popup() {
  if (popup == nullptr) {
    return;
  }
  popup->hide();
  del_float(popup);
  delete popup;
  popup = nullptr;
  update_map_cursor_pos(map_cursor_pos);
  if (panel != nullptr) {
    panel->update();
  }
}

/* Open box for starting a new game */
void
Interface::open_game_init() {
  if (init_box == nullptr) {
    init_box = new GameInitBox(this);
    add_float(init_box, 0, 0);
  }
  init_box->set_displayed(true);
  init_box->set_enabled(true);
  if (panel != nullptr) {
    panel->set_displayed(false);
  }
  viewport->set_enabled(false);
  layout();

}

void
Interface::close_game_init() {
  if (init_box != nullptr) {
    init_box->set_displayed(false);
    del_float(init_box);
    delete init_box;
    init_box = nullptr;
  }
  if (panel != nullptr) {
    panel->set_displayed(true);
    panel->set_enabled(true);
  }
  viewport->set_enabled(true);
  layout();
  update_map_cursor_pos(map_cursor_pos);
  // start any AI threads
  initialize_AI();
}

/*
CustomMapGeneratorOptions &
Interface::get_custom_map_generator_options() {
  static CustomMapGeneratorOptions custom_map_generator_options;
  return custom_map_generator_options;
}
*/

CustomMapGeneratorOptions
Interface::get_custom_map_generator_options() {
  //Log::Info["interface.cc"] << "inside get_custom_map_generator_options";
  //  for (int x = 0; x < 23; x++){
  //  Log::Info["map-generator"] << "inside get_custom_map_generator_options, opt" << x << " = " << custom_map_generator_options.opt[x];
  //}
  return custom_map_generator_options;
}

// Initialize AI for non-human players
void
Interface::initialize_AI() {
  Player *player = game->get_player(0);
  unsigned int index = 0;
  do {
    // face: the face image that represents this player.
    //  1-11 is AI, 12-13 is human player.  0 is invalid
    if (player->get_face() >= 1 && player->get_face() <= 11) {
      Log::Info["freeserf"] << "Initializing AI for player #" << index;
      Log::Debug["ai"] << "MAIN GAME thread_id: " << std::this_thread::get_id();
      // set AI log file... I don't understand why this works the way it does
      //  the AI thread "takes" the file handle but the main game filehandle still works even though they all
      //    use the same static function???  and the class is a singleton??
      Log::set_file(new std::ofstream("ai_Player" + std::to_string(index) + ".txt"));
      AI *ai = new AI(game, index);
      game->ai_thread_starting();
      // store AI pointer in game so it can be fetched by other functions (viewport, at least, for AI overlay)
      set_ai_ptr(index, ai);
      std::thread ai_thread(&AI::start, ai);
      ai_thread.detach();
    }
    index++;
    player = game->get_player(index);
  } while (player != nullptr);
  // this locking mechanism probably isn't needed anymore now that AI startup is moved to this function instead of during 'mission' game initialization
  Log::Debug["interface"] << "unlocking AI from interface.cc";
  game->unlock_ai();
}

/* Open box for next message in the message queue */
void
Interface::open_message() {
  if (!player->has_notification()) {
    play_sound(Audio::TypeSfxClick);
    return;
  } else if (!BIT_TEST(msg_flags, 3)) {
    msg_flags |= BIT(4);
    msg_flags |= BIT(3);
    MapPos pos = viewport->get_current_map_pos();
    return_pos = pos;
  }

  Message message = player->pop_notification();

  if (message.type == Message::TypeCallToMenu) {
    /* TODO */
  }

  if (notification_box == nullptr) {
    notification_box = new NotificationBox(this);
    add_float(notification_box, 0, 0);
  }
  notification_box->show(message);
  layout();

  if (BIT_TEST(0x8f3fe, message.type)) {
    /* Move screen to new position */
    viewport->move_to_map_pos(message.pos);
    update_map_cursor_pos(message.pos);
  }

  msg_flags |= BIT(1);
  //return_timeout = 60*TICKS_PER_SEC;
  return_timeout = 60*1000/tick_length;
  play_sound(Audio::TypeSfxClick);
}

void
Interface::return_from_message() {
  if (BIT_TEST(msg_flags, 3)) { /* Return arrow present */
    msg_flags |= BIT(4);
    msg_flags &= ~BIT(3);

    return_timeout = 0;
    viewport->move_to_map_pos(return_pos);

    if ((popup != nullptr) && (popup->get_box() == PopupBox::TypeMessage)) {
      close_popup();
    }
    play_sound(Audio::TypeSfxClick);
  }
}

void
Interface::close_message() {
  if (notification_box == nullptr) {
    return;
  }

  notification_box->set_displayed(false);
  del_float(notification_box);
  delete notification_box;
  notification_box = nullptr;
  layout();
}

/* Return the cursor type and various related values of a MapPos. */
void
Interface::get_map_cursor_type(const Player *player_, MapPos pos,
                               BuildPossibility *bld_possibility,
                               CursorType *cursor_type) {
  PMap map = game->get_map();
  if (player_ == nullptr) {
    *bld_possibility = BuildPossibilityNone;
    *cursor_type = CursorTypeClear;
    return;
  }

  if (game->can_build_castle(pos, player_)) {
    *bld_possibility = BuildPossibilityCastle;
  } else if (game->can_player_build(pos, player_) &&
       Map::map_space_from_obj[map->get_obj(pos)] ==
         Map::SpaceOpen &&
       (game->can_build_flag(map->move_down_right(pos), player_) ||
         map->has_flag(map->move_down_right(pos)))) {
    if (game->can_build_mine(pos)) {
      *bld_possibility = BuildPossibilityMine;
    } else if (game->can_build_large(pos)) {
      *bld_possibility = BuildPossibilityLarge;
    } else if (game->can_build_small(pos)) {
      *bld_possibility = BuildPossibilitySmall;
    } else if (game->can_build_flag(pos, player_)) {
      *bld_possibility = BuildPossibilityFlag;
    } else {
      *bld_possibility = BuildPossibilityNone;
    }
  } else if (game->can_build_flag(pos, player_)) {
    *bld_possibility = BuildPossibilityFlag;
  } else {
    *bld_possibility = BuildPossibilityNone;
  }

  if (map->get_obj(pos) == Map::ObjectFlag &&
    map->get_owner(pos) == player_->get_index()) {
    if (game->can_demolish_flag(pos, player_)) {
      *cursor_type = CursorTypeRemovableFlag;
    } else {
      *cursor_type = CursorTypeFlag;
    }
  } else if (!map->has_building(pos) &&
             !map->has_flag(pos)) {
    int paths = map->paths(pos);
    if (paths == 0) {
      if (map->get_obj(map->move_down_right(pos)) ==
          Map::ObjectFlag) {
        *cursor_type = CursorTypeClearByFlag;
      } else if (map->paths(map->move_down_right(pos)) == 0) {
        *cursor_type = CursorTypeClear;
      } else {
        *cursor_type = CursorTypeClearByPath;
      }
    } else if (map->get_owner(pos) == player_->get_index()) {
      *cursor_type = CursorTypePath;
    } else {
      *cursor_type = CursorTypeNone;
    }
  } else if ((map->get_obj(pos) == Map::ObjectSmallBuilding ||
              map->get_obj(pos) == Map::ObjectLargeBuilding) &&
             map->get_owner(pos) == player_->get_index()) {
    Building *bld = game->get_building_at_pos(pos);
    if (!bld->is_burning()) {
      *cursor_type = CursorTypeBuilding;
    } else {
      *cursor_type = CursorTypeNone;
    }
  } else {
    *cursor_type = CursorTypeNone;
  }
}

/* Update the interface_t object with the information returned
   in get_map_cursor_type(). */
void
Interface::determine_map_cursor_type() {
  get_map_cursor_type(player, map_cursor_pos, &build_possibility,
                      &map_cursor_type);
}

/* Update the interface_t object with the information returned
   in get_map_cursor_type(). This is sets the appropriate values
   when the player interface is in road construction mode. */
void
Interface::determine_map_cursor_type_road() {
  PMap map = game->get_map();
  MapPos pos = map_cursor_pos;
  int h = map->get_height(pos);
  int valid_dir = 0;

  for (Direction d : cycle_directions_cw()) {
    int sprite = 0;

    if (building_road.is_undo(d)) {
      sprite = 45; /* undo */
      valid_dir |= BIT(d);
    } else if (map->is_road_segment_valid(pos, d)) {
      if (building_road.is_valid_extension(map.get(), d)) {
        int h_diff = map->get_height(map->move(pos, d)) - h;
        sprite = 39 + h_diff; /* height indicators */
        valid_dir |= BIT(d);
      } else {
        sprite = 44;
      }
    } else {
      sprite = 44; /* striped */
    }
    map_cursor_sprites[d+1].sprite = sprite;
  }

  building_road_valid_dir = valid_dir;
}

/* Set the appropriate sprites for the panel buttons and the map cursor. */
void
Interface::update_interface() {

/*
  Log::Info["interface"] << "option_EnableAutoSave is " << option_EnableAutoSave;
  //Log::Info["interface"] << "option_ImprovedPigFarms is " << option_ImprovedPigFarms;  // removing this as it turns out the default behavior for pig farms is to require almost no grain
  Log::Info["interface"] << "option_CanTransportSerfsInBoats is " << option_CanTransportSerfsInBoats;
  Log::Info["interface"] << "option_QuickDemoEmptyBuildSites is " << option_QuickDemoEmptyBuildSites;
  Log::Info["interface"] << "option_TreesReproduce is " << option_TreesReproduce;
  Log::Info["interface"] << "option_BabyTreesMatureSlowly is " << option_BabyTreesMatureSlowly;
  Log::Info["interface"] << "option_ResourceRequestsTimeOut is " << option_ResourceRequestsTimeOut;
  //PrioritizeUsableResources
  //AdvancedDemolition
  Log::Info["interface"] << "option_LostTransportersClearFaster is " << option_LostTransportersClearFaster;
  Log::Info["interface"] << "option_FourSeasons is " << option_FourSeasons;
  Log::Info["interface"] << "option_FishSpawnSlowly is " << option_FishSpawnSlowly;
  //option_FogOfWar
  */


  if (!building_road.is_valid()) {
    switch (map_cursor_type) {
    case CursorTypeNone:
      map_cursor_sprites[0].sprite = 32;
      map_cursor_sprites[2].sprite = 33;
      break;
    case CursorTypeFlag:
      map_cursor_sprites[0].sprite = 51;
      map_cursor_sprites[2].sprite = 33;
      break;
    case CursorTypeRemovableFlag:
      map_cursor_sprites[0].sprite = 51;
      map_cursor_sprites[2].sprite = 33;
      break;
    case CursorTypeBuilding:
      map_cursor_sprites[0].sprite = 32;
      map_cursor_sprites[2].sprite = 33;
      break;
    case CursorTypePath:
      map_cursor_sprites[0].sprite = 52;
      map_cursor_sprites[2].sprite = 33;
      if (build_possibility != BuildPossibilityNone) {
        map_cursor_sprites[0].sprite = 47;
      }
      break;
    case CursorTypeClearByFlag:
      if (build_possibility < BuildPossibilityMine) {
        map_cursor_sprites[0].sprite = 32;
        map_cursor_sprites[2].sprite = 33;
      } else {
        map_cursor_sprites[0].sprite = 46 + build_possibility;
        map_cursor_sprites[2].sprite = 33;
      }
      break;
    case CursorTypeClearByPath:
      if (build_possibility != BuildPossibilityNone) {
        map_cursor_sprites[0].sprite = 46 + build_possibility;
        if (build_possibility == BuildPossibilityFlag) {
          map_cursor_sprites[2].sprite = 33;
        } else {
          map_cursor_sprites[2].sprite = 47;
        }
      } else {
        map_cursor_sprites[0].sprite = 32;
        map_cursor_sprites[2].sprite = 33;
      }
      break;
    case CursorTypeClear:
      if (build_possibility) {
        if (build_possibility == BuildPossibilityCastle) {
          map_cursor_sprites[0].sprite = 50;
        } else {
          map_cursor_sprites[0].sprite = 46 + build_possibility;
        }
        if (build_possibility == BuildPossibilityFlag) {
          map_cursor_sprites[2].sprite = 33;
        } else {
          map_cursor_sprites[2].sprite = 47;
        }
      } else {
        map_cursor_sprites[0].sprite = 32;
        map_cursor_sprites[2].sprite = 33;
      }
      break;
    default:
      NOT_REACHED();
      break;
    }
  }

  if (panel != nullptr) {
    panel->update();
  }
}

void
Interface::set_game(PGame new_game) {
  if (viewport != nullptr) {
    del_float(viewport);
    delete viewport;
    viewport = nullptr;
  }

  game = std::move(new_game);

  if (game) {
    viewport = new Viewport(this, game->get_map());
    viewport->set_displayed(true);
    add_float(viewport, 0, 0);
  }

  layout();

  set_player(0);
}

void
Interface::set_player(unsigned int player_index) {
  if (panel != nullptr) {
    del_float(panel);
    delete panel;
    panel = nullptr;
  }

  if (!game) {
    player = nullptr;
    return;
  }

  if ((player != nullptr) && (player_index == player->get_index())) {
    return;
  }

  player = game->get_player(player_index);

  /* Move viewport to initial position */
  MapPos init_pos = game->get_map()->pos(0, 0);

  if (player != NULL) {
    panel = new PanelBar(this);
    panel->set_displayed(true);
    add_float(panel, 0, 0);
    layout();

    for (Building *building : game->get_player_buildings(player)) {
      if (building->get_type() == Building::TypeCastle) {
        init_pos = building->get_position();
      }
    }
  }

  update_map_cursor_pos(init_pos);
  viewport->move_to_map_pos(map_cursor_pos);
}

Color
Interface::get_player_color(unsigned int player_index) {
  if (game->get_player(player_index) == nullptr){
    Log::Error["interface.cc"] << "inside Interface::get_player_color, game->get_player(" << player_index << ") returned nullptr!  why is this invalid?  returning 'black'!";
    return Color::Color::black;
  }
  Player::Color player_color = game->get_player(player_index)->get_color();
  Color color(player_color.red, player_color.green, player_color.blue);
  return color;
}

void
Interface::update_map_cursor_pos(MapPos pos) {
  map_cursor_pos = pos;
  if (building_road.is_valid()) {
    determine_map_cursor_type_road();
  } else {
    determine_map_cursor_type();
  }
  update_interface();
}

/* Start road construction mode for player interface. */
void
Interface::build_road_begin() {
  determine_map_cursor_type();

  if (map_cursor_type != CursorTypeFlag &&
      map_cursor_type != CursorTypeRemovableFlag) {
    update_interface();
    return;
  }

  building_road.invalidate();
  building_road.start(map_cursor_pos);
  update_map_cursor_pos(map_cursor_pos);

  panel->update();
}

/* End road construction mode for player interface. */
void
Interface::build_road_end() {
  map_cursor_sprites[1].sprite = 33;
  map_cursor_sprites[2].sprite = 33;
  map_cursor_sprites[3].sprite = 33;
  map_cursor_sprites[4].sprite = 33;
  map_cursor_sprites[5].sprite = 33;
  map_cursor_sprites[6].sprite = 33;

  building_road.invalidate();
  update_map_cursor_pos(map_cursor_pos);

  panel->update();
}

/* Build a single road segment. Return -1 on fail, 0 on successful
   construction, and 1 if this segment completed the path. */
int
Interface::build_road_segment(Direction dir) {
  if (!building_road.is_extendable()) {
    /* Max length reached */
    return -1;
  }

  building_road.extend(dir);

  MapPos dest;
  int r = game->can_build_road(building_road, player, &dest, NULL);
  if (r <= 0) {
    /* Invalid construction, undo. */
    return remove_road_segment();
  }

  if (game->get_map()->get_obj(dest) == Map::ObjectFlag) {
    /* Existing flag at destination, try to connect. */
    if (!game->build_road(building_road, player)) {
      build_road_end();
      return -1;
    } else {
      build_road_end();
      update_map_cursor_pos(dest);
      return 1;
    }
  } else if (game->get_map()->paths(dest) == 0) {
    /* No existing paths at destination, build segment. */
    update_map_cursor_pos(dest);

    /* TODO Pathway scrolling */
  } else {
    /* TODO fast split path and connect on double click */
    return -1;
  }

  return 0;
}

int
Interface::remove_road_segment() {
  MapPos dest = building_road.get_source();
  int res = 0;
  building_road.undo();
  if (building_road.get_length() == 0 ||
      game->can_build_road(building_road, player, &dest, NULL) == 0) {
    /* Road construction is no longer valid, abort. */
    build_road_end();
    res = -1;
  }

  update_map_cursor_pos(dest);

  /* TODO Pathway scrolling */

  return res;
}

/* Extend currently constructed road with an array of directions. */
int
Interface::extend_road(const Road &road) {
  Road old_road = building_road;
  for (const Direction dir : road.get_dirs()) {
    int r = build_road_segment(dir);
    if (r < 0) {
      building_road = old_road;
      return -1;
    } else if (r == 1) {
      building_road.invalidate();
      return 1;
    }
  }

  return 0;
}

// the human player has clicked the "destroy building" panelbar button
//  which destroys a flag if possible, or burns a building
//   includes handling of option_AdvancedDemolition
void
Interface::demolish_object() {
  Log::Debug["interface.cc"] << "inside Interface::demolish_object";
  determine_map_cursor_type();

  if (map_cursor_type == CursorTypeRemovableFlag) {
    play_sound(Audio::TypeSfxClick);
    game->demolish_flag(map_cursor_pos, player);
  } else if (map_cursor_type == CursorTypeBuilding) {
    Building *building = game->get_building_at_pos(map_cursor_pos);
    if (building == nullptr){
      Log::Warn["interface.cc"] << "inside Interface::demolish_object, building at pos " << map_cursor_pos << " is nullptr!  returning early";
      return;
    }
  
    //if (false){
    if (option_AdvancedDemolition){
      Log::Debug["interface.cc"] << "inside Interface::demolish_object, option_AdvancedDemolition is on";
      // building must be fully completed to require serf for demolition
      // Exclude Stocks, let them be "self burnt".  For now.  I guess could check and see if
      //  stock has or can produce a Leveler that can be made to burn it
      // other serf Holder will be immediately sent back to nearest Inventory
      if (building->is_done() && building->get_type() != Building::TypeStock && building->get_type() != Building::TypeCastle){
        
        // then the digger arrives, this bool tells him to burn it instead of doing the normal leveling routine
        if (game->mark_building_for_demolition(map_cursor_pos, player)){
          Log::Info["interface.cc"] << "inside Interface::demolish_object, option_AdvancedDemolition is on, successfully marked building for deletion";
        }else{
          Log::Info["interface.cc"] << "inside Interface::demolish_object, option_AdvancedDemolition is on, failed marked building for deletion, is it already marked? is it not mine?? returning early";
          play_sound(Audio::TypeSfxNotAccepted);
          return;
        }

        // immediately evict the Holder serf
        //if (!building->is_military() && building->has_serf()){
        ///// NO, must evict knights immediately also, as the demo serf blocks the flag
        /////   knights in military buildings should not abandon their post until the building is actually on fire
        // buildings with no Holder have nobody to evict
        if (building->has_serf()){
          Log::Info["interface.cc"] << "inside Interface::demolish_object, option_AdvancedDemolition is on, evicting the Holder of this building";
          building->evict_holder();
        }


        // call a Digger to the building so he can burn it and clear the building site
        Flag *dest_flag = game->get_flag(building->get_flag_index());
        if (dest_flag == nullptr){
          Log::Error["interface.cc"] << "inside Interface::demolish_object, Flag* for building with pos " << map_cursor_pos << " is nullptr!";
          throw ExceptionFreeserf("inside Interface::demolish_object, Flag* for building is nullptr!  cannot call demolition");
        }
        Log::Info["interface.cc"] << "inside Interface::demolish_object, calling a Digger to demolish & clear building of type " << NameBuilding[building->get_type()] << " at pos " << map_cursor_pos;
        play_sound(Audio::TypeSfxAccepted);
        game->send_serf_to_flag(dest_flag, Serf::TypeDigger, Resource::TypeShovel, Resource::TypeNone);
        return;
      }
    }

    // handle normal demo (either option_AdvancedDemolition is not enabled, or
    //                      the building type or state is not eligible for it)
    play_sound(Audio::TypeSfxAhhh);
    game->demolish_building(map_cursor_pos, player);
  } else {
    play_sound(Audio::TypeSfxNotAccepted);
    update_interface();
  }
}

/* Build new flag. */
void
Interface::build_flag() {
  if (!game->build_flag(map_cursor_pos, player)) {
    play_sound(Audio::TypeSfxNotAccepted);
    return;
  }

  update_map_cursor_pos(map_cursor_pos);
}

/* Build a new building. */
void
Interface::build_building(Building::Type type) {
  if (!game->build_building(map_cursor_pos, type, player)) {
    play_sound(Audio::TypeSfxNotAccepted);
    return;
  }

  play_sound(Audio::TypeSfxAccepted);
  close_popup();

  /* Move cursor to flag. */
  MapPos flag_pos = game->get_map()->move_down_right(map_cursor_pos);
  update_map_cursor_pos(flag_pos);
}

/* Build castle. */
void
Interface::build_castle() {
  if (!game->build_castle(map_cursor_pos, player)) {
    play_sound(Audio::TypeSfxNotAccepted);
    return;
  }

  play_sound(Audio::TypeSfxAccepted);
  update_map_cursor_pos(map_cursor_pos);
}

void
Interface::build_road() {
  bool r = game->build_road(building_road, player);
  if (!r) {
    play_sound(Audio::TypeSfxNotAccepted);
    game->demolish_flag(map_cursor_pos, player);
  } else {
    play_sound(Audio::TypeSfxAccepted);
    build_road_end();
  }
}

void
Interface::update_map_height(MapPos pos, void *data) {
  Interface *interface = reinterpret_cast<Interface*>(data);
  interface->viewport->redraw_map_pos(pos);
}

void
Interface::internal_draw() {
}

void
Interface::layout() {
  int panel_x = 0;
  int panel_y = height;

  if (panel != nullptr) {
    int panel_width = 352;
    int panel_height = 40;
    panel_x = (width - panel_width) / 2;
    panel_y = height - panel_height;
    panel->move_to(panel_x, panel_y);
    panel->set_size(panel_width, panel_height);
  }

  if (popup != nullptr) {
    int popup_width = 144;
    int popup_height = 160;
    int popup_x = (width - popup_width) / 2;
    int popup_y = (height - popup_height) / 2;
    popup->move_to(popup_x, popup_y);
    popup->set_size(popup_width, popup_height);
  }

  if (init_box != nullptr) {
    int init_box_width = 360;
    int init_box_height = 256;
    int init_box_x = (width - init_box_width) / 2;
    int init_box_y = (height - init_box_height) / 2;
    init_box->move_to(init_box_x, init_box_y);
    init_box->set_size(init_box_width, init_box_height);
  }

  if (notification_box != nullptr) {
    int notification_box_width = 200;
    int notification_box_height = 88;
    int notification_box_x = panel_x + 40;
    int notification_box_y = panel_y - notification_box_height;
    notification_box->move_to(notification_box_x, notification_box_y);
    notification_box->set_size(notification_box_width, notification_box_height);
  }

  if (viewport != nullptr) {
    viewport->set_size(width, height);
  }

  set_redraw();
}

/* Called periodically when the game progresses. */
void
Interface::update() {
  //Log::Debug["interface.cc"] << "start of Interface::update()";
  if (!game) {
    return;
  }

  game->update();

  int tick_diff = game->get_const_tick() - last_const_tick;
  last_const_tick = game->get_const_tick();

  // hack to trigger redraw for option_FogOfWar
  //  any time borders change
  if (game->get_redraw_frame()){
    Log::Debug["interface.cc"] << "inside Interface::update(), game->get_redraw_frame is true";
    if (viewport != nullptr) {
      Log::Debug["interface.cc"] << "inside Interface::update(), game->get_redraw_frame is true and viewport is not a nullptr, calling viewport->set_size";
      viewport->set_size(width, height);  // this does the magic refresh without affecting popups (as Interface->layout() does)
    }
    game->unset_redraw_frame();
  }else{
    //Log::Debug["interface.cc"] << "inside Interface::update(), game->get_redraw_frame is false";
  }

  // allow the Game to change the map clicked cursor pos
  //  Because the Game object cannot access the Interface or Viewport
  //  it is not possible to directly set any aspect of those, instead
  //  they make decisions based on Game state. So, new functions created
  //  to allow the Game to indicate to the Interface that it desires the
  //  player cursor/click pos to be set, and the Interface can do it here
  if (game->get_update_viewport_cursor_pos() != bad_map_pos){
    Log::Debug["interface.cc"] << "inside Interface::update(), game->get_update_viewport_cursor_pos is not bad_map_pos";
    if (viewport != nullptr) {
      Log::Debug["interface.cc"] << "inside Interface::update(), game->get_update_viewport_cursor_pos is true and viewport is not a nullptr, calling viewport->move_to_map_pos() for pos " << game->get_update_viewport_cursor_pos();
      viewport->move_to_map_pos(game->get_update_viewport_cursor_pos());
    }
    game->unset_update_viewport_cursor_pos();
  }else{
    //Log::Debug["interface.cc"] << "inside Interface::update(), game->get_update_viewport_cursor_pos is false";
  }

  if (option_FourSeasons){
    // messing with weather/seasons/palette - increase subseason
    // in the game, it takes 100k ticks for a sown field to become harvestable (Seed5,Field0+)
    // and the field remains harvestable for 100k ticks until after Field5 it becomse FieldExpired
    // in real life, spring wheat is planted at start of spring and
    // harvested mid to late summer, at the latest right before fall starts, and crop declines soon after
    // so it should be 100k ticks from Seed0 to Seed5
    // so 100k ticks should be around 1.6 seasons
    // so one season should be about 62500 ticks
    // so one subseason (1/16th season) should be 3906.25
    // and one year of ticks is 250000

    // to make seasons persistent when game is saved/loaded, without modifying savegame format
    //  have the current season be an offset from starting tick

    int year = game->get_tick() / 250000;
    int year_offset = game->get_tick() % 250000;
    season = 1 + year_offset / 62500;  // increment by 1 to default to Summer
    if (season > 3){season = 0;} // ... but wrap back around as there is no fifth season
    int season_tick_offset = year_offset % 62500;
    subseason = season_tick_offset / 3906;
    int subseason_tick_offset = season_tick_offset % 3906;
    //Log::Debug["interface.cc"] << "FourSeasons calendar:  tick " << game->get_tick() << ", year " << year << ", year_offset " << year_offset << ", season " << season << ", season_tick_offset " << season_tick_offset << ", subseason " << subseason << ", subseason_tick_offset " << subseason_tick_offset;

    // IN THE FUTURE, ALLOW IT TO BE RANDOMIZED BY starting tick + random-seed offset up to 1yr

/* disable manual adjustment, instead it is tied to game tick

    if (game->get_tick() > 3906 + last_subseason_tick){
      Log::Debug["interface"] << "inside Interface::update, incrementing subseason, subseason is now " << subseason << ", last_subseason_tick " << last_subseason_tick << ", game tick " << game->get_tick();
      last_subseason_tick = game->get_tick();
      if (subseason < 15){
        subseason++;
      }else{
        Log::Debug["interface"] << "inside Interface::update, incrementing season, resetting subseason, tick " << game->get_tick();
        subseason = 0;
        if (season < 3){
          season++;
        }else{
          season = 0;
        }
        Log::Info["interface"] << "Changing Season to " << NameSeason[season] << " and clearing image cache";
      }
      clear_custom_graphics_cache();
      viewport->set_size(width, height);  // this does the magic refresh without affecting popups (as Interface->layout() does)
    }
*/

    if (last_subseason != subseason){
      if (last_season != season){
        Log::Info["interface.cc"] << "FourSeasons: changing season to " << NameSeason[season];
        last_season = season;
      }
      last_subseason = subseason;
      // handle fade at start of new seasons
      if (subseason <= 2 && season != 1){ // if fading into a new season (and not Summer which has no changes), flush tile cache
        Log::Debug["interface.cc"] << "FourSeasons: changing subseason to " << subseason << " and clearing tile cache because this season has tile changes";
        clear_custom_graphics_cache();
      }else{
        Log::Debug["interface.cc"] << "FourSeasons: changing subseason to " << subseason << " and NOT clearing tile cache, because this season/subseason has no tile changes";
      }
      viewport->set_size(width, height);  // this does the magic refresh without affecting popups (as Interface->layout() does)
    }

  }

  /* Clear return arrow after a timeout */
  if (return_timeout < tick_diff) {
    msg_flags |= BIT(4);
    msg_flags &= ~BIT(3);
    return_timeout = 0;
  } else {
    return_timeout -= tick_diff;
  }

  const int msg_category[] = {
    -1, 5, 5, 5, 4, 0, 4, 3, 4, 5,
    5, 5, 4, 4, 4, 4, 0, 0, 0, 0
  };

  /* Handle newly enqueued messages */
  if ((player != nullptr) && player->has_message()) {
    player->drop_message();
    while (player->has_notification()) {
      Message message = player->peek_notification();
      if (BIT_TEST(config, msg_category[message.type])) {
        play_sound(Audio::TypeSfxMessage);
        msg_flags |= BIT(0);
        break;
      }
      player->pop_notification();
    }
  }

  if ((player != nullptr) && BIT_TEST(msg_flags, 1)) {
    msg_flags &= ~BIT(1);
    while (1) {
      if (!player->has_notification()) {
        msg_flags &= ~BIT(0);
        break;
      }

      Message message = player->peek_notification();
      if (BIT_TEST(config, msg_category[message.type])) break;
      player->pop_notification();
    }
  }

  if (option_EnableAutoSave){
    // auto-save if interval reached
    //  note that this is const tick so should save at regular real time interval not game-tick interval (which increases with game speed)
    if (game->get_const_tick() >= AUTOSAVE_INTERVAL + last_autosave_tick){
      Log::Debug["interface"] << "auto-save interval reached, preparing to auto-save game...";
      game->mutex_lock("Interface::update auto-saving game");
      std::string savename = "AUTOSAVE.save";
      Log::Verbose["interface"] << "savegame name is '" << savename << " '";
      if (GameStore::get_instance().quick_save("autosave", game.get())){
        Log::Info["interface"] << "successfully auto-saved game";
      } else {
        Log::Warn["interface"] << "FAILED TO SAVE GAME!";
      }
      game->mutex_unlock();
      last_autosave_tick = game->get_const_tick();
    }
  }


  viewport->update();
  set_redraw();
}

bool
Interface::handle_key_pressed(char key, int modifier) {

  switch (key) {
    /* Interface control */

    /* disabling TAB key control for now because it triggers when
      alt-tabbing and I am wondering if it is responsible for frequent
      crashes seen when alt-tabbing and multitasking - when game is otherwise stable
    case '\t': {
      if (modifier & 2) {
        return_from_message();
      } else {
        open_message();
      }
      break;
    }
    */

    // what is this?   backspace or escape maybe? to cancel popup?
    case 27: {
      Log::Debug["interface"] << "BACKSPACE key pressed, closing any open popup / road build";
      if ((notification_box != nullptr) && notification_box->is_displayed()) {
        close_message();
      } else if ((popup != nullptr) && popup->is_displayed()) {
        close_popup();
      } else if (building_road.is_valid()) {
        build_road_end();
      }
      break;
    }

    /* Game speed */
    case '+': {
      game->speed_increase();
      break;
    }
    case '-': {
      game->speed_decrease();
      break;
    }
    case '0': {
      game->speed_reset();
      break;
    }
    case 'p': {
      game->pause();
      break;
    }

    // toggle sounds
    case 's': {
      Log::Info["interface"] << "'s' key pressed, toggling sounds";
      Audio &audio = Audio::get_instance();
      Audio::PPlayer splayer = audio.get_sound_player();
      if (splayer) {
        splayer->enable(!splayer->is_enabled());
      }
      break;
    }
    // toggle music
    case 'm': {
      Log::Info["interface"] << "'m' key pressed, toggling music";
      Audio &audio = Audio::get_instance();
      Audio::PPlayer splayer = audio.get_music_player();
      if (splayer) {
        Log::Debug["interface.cc"] << "audio splayer exists, state is " << splayer->is_enabled();
        splayer->enable(!splayer->is_enabled());
      }
      break;
    }

    // old Freeserf debug overlay, shows map grid and serf states
    case 'g': {
      Log::Info["interface"] << "'g' key pressed, toggling LayerGrid";
      viewport->switch_layer(Viewport::LayerGrid);
      break;
    }

    // show building placement possibilities on map
    case 'b': {
      Log::Info["interface"] << "'b' key pressed, toggling LayerBuilds";
      viewport->switch_layer(Viewport::LayerBuilds);
      break;
    }

    // AI overlay grid - colored dots showing AI searching positions, roads being build, AI status, serf status, etc.
    case 'y': {
      Log::Info["interface"] << "'y' key pressed, toggling LayerAI";
      viewport->switch_layer(Viewport::LayerAI);
      break;
    }

    // new Forkserf debug overlay, highlight items on map and provide misc debug info as needed
    case 'd': {
      Log::Info["interface"] << "'d' key pressed, toggling LayerDebug";
      viewport->switch_layer(Viewport::LayerDebug);
      if (!viewport->layer_active(Viewport::LayerDebug)){
        // clear the debug map pos any time debug layer turned off
        Log::Info["interface"] << "LayerDebug was turned off, clearing debug_mark_pos list";
        game->clear_debug_mark_pos();
      }
      break;
    }

    // Hidden Resources (Minerals/Fish) overlay grid - colored dots showing otherwise invisible map resoureces
    case 'h': {
      Log::Info["interface"] << "'h' key pressed, toggling LayerHiddenResources";
      viewport->switch_layer(Viewport::LayerHiddenResources);
      break;
    }

    // debugging function to "boot" clicked serf by making them Lost (only works for AI players currently)
    case 'l': {
      if (modifier & 1) {
        Log::Info["interface"] << "CTRL-L pressed, telling AI to make any selected serf StateLost, for debugging";
        AI *ai_ptr = get_ai_ptr(player->get_index());
        if (ai_ptr == nullptr){
          Log::Warn["interface"] << "got nullptr for get_ai_ptr(player->get_index()); for player index " << player->get_index();
          break;
        }else{
          ai_ptr->set_serf_lost();
        }
      }
    }

    case 'j': {
      //
      // NOTE - switching players should probably be made to close any open popup
      //  as the popups are not automatically refreshed and will at best show data
      //  from the player that opened it, and at worst may crash the game
      //
      // switch to next player and center view on their castle
      //  if only one player, play error noise but stll center view on castle
      unsigned int current_index = player->get_index();
      unsigned int next_index = game->get_next_player(player)->get_index();
      if (current_index == next_index) {
        //Log::Debug["interface"] << "next player index is current player index - i.e. there is only one player, not switching";
        play_sound(Audio::TypeSfxNotAccepted);
        // but also center on this player's castle at least
        for (Building *building : game->get_player_buildings(player)) {
          if (building->get_type() == Building::TypeCastle) {
            //update_map_cursor_pos(building->get_position());
            viewport->move_to_map_pos(building->get_position());
          }
        }
      }
      else {
        set_player(next_index);
        Log::Info["interface.cc"] << "Switched to player #" << next_index;
      }
      break;
    }
    case 'w':
      Log::Info["interface.cc"] << "'w' key pressed, toggling FourSeasons";
      if (option_FourSeasons){
        option_FourSeasons = false;
        Log::Info["interface.cc"] << "Disabling FourSeasons of Weather and clearing image cache";
      }else{
        option_FourSeasons = true;
        Log::Info["interface.cc"] << "Enabling FourSeasons of Weather and clearing image cache";
      }
      clear_custom_graphics_cache();
      viewport->set_size(width, height);  // this does the magic refresh without affecting popups (as Interface->layout() does)
      break;
    case 'f':
      Log::Info["interface.cc"] << "'f' key pressed, toggling FogOfWar";
      if (option_FogOfWar){
        option_FogOfWar = false;
        Log::Info["interface.cc"] << "Disabling option_FogOfWar clearing terrain tile cache";
      }else{
        option_FogOfWar = true;
        Log::Info["interface.cc"] << "Enabling option_FogOfWar clearing terrain tile cache, initializing FogOfWar for entire map";
      }
      game->init_FogOfWar();
      viewport->set_size(width, height);  // this does the magic refresh without affecting popups (as Interface->layout() does)
      break;
    case 'q':
      //disabled for now, season is now tied to tick so it works with save/load game
      play_sound(Audio::TypeSfxNotAccepted);
      /*
      if (season < 3){
        season++;
      }else{
        season = 0;
      }
      Log::Info["interface"] << "Changing Season to " << NameSeason[season] << " and clearing image cache";
      clear_custom_graphics_cache();
      viewport->set_size(width, height);  // this does the magic refresh without affecting popups (as Interface->layout() does)
      */
      break;
    case 'e':
      //disabled for now, season is now tied to tick so it works with save/load game
      play_sound(Audio::TypeSfxNotAccepted);
      /*
      if (subseason < 15){  // allow subseason to go to be  "tree + 1" so that it can have a 0 state with no change yet
        subseason++;
      }else{
        subseason = 0;
      }
      Log::Info["interface"] << "Changing Sub-Season to #" << subseason << " and clearing image cache";
      //// subseason does not require purging of tile cache because only ground tiles seem to be cached
      ////  and sub-seasons only affect map_objects (trees)
      // CHANGING TILES AS PART OF SUBSEASON, NOW MUST PURGE TILE CACHE
      clear_custom_graphics_cache();
      viewport->set_size(width, height);  // this does the magic refresh without affecting popups (as Interface->layout() does)
      */
      break;
    case 'z':
      Log::Info["interface"] << "'z' key pressed, quick-saving game'";
      if (modifier & 1) {
        GameStore::get_instance().quick_save("quicksave", game.get());
      }
      break;
    case 'n':
      Log::Info["interface"] << "'n' key pressed, opening new-game-init popup'";
      if (modifier & 1) {
        open_game_init();
      }
      break;
    case 'c':
      Log::Info["interface"] << "'c' key pressed, confirming quit'";
      // ALLOW ENTER KEY TO DO THIS!
      if (modifier & 1) {
        open_popup(PopupBox::TypeQuitConfirm);
      }
      break;

    default:
      return false;
  }

  return true;
}

bool
Interface::handle_event(const Event *event) {
  switch (event->type) {
    case Event::TypeResize:
      set_size(event->dx, event->dy);
      break;
    case Event::TypeUpdate:
      update();
      break;
    case Event::TypeDraw:
      draw(reinterpret_cast<Frame*>(event->object));
      break;

    default:
      return GuiObject::handle_event(event);
      break;
  }

  return true;
}

void
Interface::on_new_game(PGame new_game) {
  set_game(new_game);
}

void
Interface::on_end_game(PGame /*game*/) {
  set_game(nullptr);
}

// this allows the EditCustomMapGenerator popup to tell the
//  GameInitBox to refresh the map on popup
void
Interface::tell_gameinit_regen_map(){
  init_box->generate_map_preview();
  init_box->set_redraw();
}


// added to support messing with weather/seasons/palette
//  to allow clearing only the ground tile cache as the other 
//  images don't appear to actually be cached
void
Interface::clear_custom_graphics_cache() {
  //Image::clear_cache();  // this clears the entire cache

  std::set<uint64_t> to_purge = {};

  //uint64_t id = Data::Sprite::create_id(res, index, 0, 0, pc);
  // Data::Resource res, unsigned int index,
  // Data::AssetMapObject || res == Data::AssetMapShadow
  //std::set<uint64_t> to_purge = {};

  /*
  // this doesn't appear to actually be necessary!  it seems that map_object data is not cached
  //  for some reason.  Removing purge of this doesn't prevent trees graphics from changing with
  //  the seasons.
  for (int season_sprite_offset : {1200,1300,1400}){
    for (int tree_offset : {0,10,20,30,40,50,60,70}){
      for (int i=0; i<7; i++){
        to_purge.insert( Data::Sprite::create_id(Data::AssetMapObject, i + season_sprite_offset + tree_offset, 0, 0, {0,0,0,0}) );
      }
    }
  }
  // custom shadows not being used yet because the transparency isn't working, add those to purge list once that is fixed
  // for (xxx)
  //   to_purge.insert( Data::Sprite::create_id(Data::AssetMapShadow, i + xxx, 0, 0, {0,0,0,0}) );
  //
  */
  // purging terrain tiles IS necessary, though
  // Data::AssetMapGround , values 0 through 32
  // there are two mask types, each with 80 elements
  // Data::AssetMapMaskUp and Data::AssetMapMaskDown
  for (int mask_index=0; mask_index<81; mask_index++){
    for (int map_ground_index=0; map_ground_index<33; map_ground_index++){
      to_purge.insert( Data::Sprite::create_id(Data::AssetMapGround, map_ground_index, Data::AssetMapMaskUp, mask_index, {0,0,0,0}) );
      to_purge.insert( Data::Sprite::create_id(Data::AssetMapGround, map_ground_index, Data::AssetMapMaskDown, mask_index, {0,0,0,0}) );
    }
    // clear the option_FogOfWar darkened sprites also, as they stil change with the seasons
    for (int map_ground_index=100; map_ground_index<133; map_ground_index++){
      to_purge.insert( Data::Sprite::create_id(Data::AssetMapGround, map_ground_index, Data::AssetMapMaskUp, mask_index, {0,0,0,0}) );
      to_purge.insert( Data::Sprite::create_id(Data::AssetMapGround, map_ground_index, Data::AssetMapMaskDown, mask_index, {0,0,0,0}) );
    }
  }

  // for FourSeasons MapObject sprite manipulation (for Seeds Fields)
  //  try purging non-masked sprites, hopefully simpler...  yes this is easier
  // REMEMBER THAT
  //  'sprite' is reduced by 8 / Map::ObjectTree0 earlier in this function because 0-7 are some placeholders?
  //   so sprite for Seeds0 which is index 105 is actually sprite index 97 because 105-8=97
  //for (int unknown_index=Map::ObjectSeeds0 - 8; unknown_index <= Map::ObjectSeeds2 - 8; unknown_index++){
  //for (int unknown_index=Map::ObjectSeeds0 - 8; unknown_index <= Map::ObjectField5 - 8; unknown_index++){
    for (int unknown_index=Map::ObjectSeeds0 - 8; unknown_index <= Map::ObjectFieldExpired - 8; unknown_index++){
    to_purge.insert( Data::Sprite::create_id(Data::AssetMapObject, unknown_index, Data::AssetNone, 0, {0,0,0,0}) );
  }
    
  //for (uint64_t id : to_purge){
  //  Log::Debug["interface"] << "to_purge contains id " << id;
  //}
  Image::clear_cache_items(to_purge);

  //layout();  // THIS IS IT - this is the "fix viewport" function   // THIS IS CAUSING ISSUES WITH POPUP MENUS BEING CORRUPTED WHEN IT RUNS!
  //viewport->set_size(width, height);  // this does the magic refresh without affecting popups (as Interface->layout() does)
  //set_redraw(); // this is not enough!
}