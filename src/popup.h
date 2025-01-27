/*
 * popup.h - Popup GUI component
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

#ifndef SRC_POPUP_H_
#define SRC_POPUP_H_

#include <string>
#include <vector>
#include <memory>

#include "src/gui.h"
#include "src/resource.h"

#include "src/minimap.h" // for refreshing minimap when FogOfWar toggled
#include "src/text-input.h" // for TextInput text_input_filter for filtering bad chars from savegame name

class Interface;
class MinimapGame;
class ListSavedFiles;
class TextInput;

class PopupBox : public GuiObject {
 public:
  typedef enum Type {
    TypeNone = 0,
    TypeMap = 1,
    TypeMapOverlay, /* UNUSED */
    TypeMineBuilding,
    TypeBasicBld,
    TypeBasicBldFlip,
    TypeAdv1Bld,
    TypeAdv2Bld,
    TypeStatSelect,
    TypeStat4,
    TypeStatBld1,
    TypeStatBld2,
    TypeStatBld3,
    TypeStatBld4,
    TypeStat8,
    TypeStat7,
    TypeStat1,
    TypeStat2,
    TypeStat6,
    TypeStat3,
    TypeStartAttack,
    TypeStartAttackRedraw,
    TypeGroundAnalysis,
    TypeLoadArchive,  // I don't think this is used
    TypeLoadSave,    // this is tied to the SettSelect popup  (no, the issue is that popups are re-used!)
    Type25,
    TypeDiskMsg,
    TypeSettSelect,  // this is tied to the save game popup LoadSave  (no, the issue is that popups are re-used!)
    TypeSett1,
    TypeSett2,
    TypeSett3,
    TypeKnightLevel,
    TypeSett4,
    TypeSett5,
    TypeQuitConfirm,
    TypeNoSaveQuitConfirm,
    TypeSettSelectFile, /* UNUSED */
    TypeOptions,
    TypeCastleRes,
    TypeMineOutput,
    TypeOrderedBld,
    TypeDefenders,
    TypeTransportInfo,
    TypeCastleSerf,
    TypeResDir,
    TypeInventoryQueues,
    TypeSett8,
    TypeSett6,
    TypeBld1,
    TypeBld2,
    TypeBld3,
    TypeBld4,
    TypeMessage,
    TypeBldStock,
    TypePlayerFaces,
    TypeGameEnd,
    TypeDemolish,
    TypeJsCalib,
    TypeJsCalibUpLeft,
    TypeJsCalibDownRight,
    TypeJsCalibCenter,
    TypeCtrlsInfo,
    TypeGameOptions,
    TypeGameOptions2,
    TypeGameOptions3,
    TypeGameOptions4,
    TypeEditMapGenerator,
    TypeEditMapGenerator2,
    TypeDebug,
    TypePleaseWaitSaving,  // added for option_EnableAutoSave
    TypePleaseWaitFogOfWar // added for option_FogOfWar
  } Type;

  typedef enum BackgroundPattern {
    PatternStripedGreen = 129,   // \\\.
    PatternDiagonalGreen = 310,  // xxx
    PatternCheckerdDiagonalBrown = 311,  // xxx
    PatternPlaidAlongGreen = 312,  // ###
    PatternStaresGreen = 313,       // * *
    PatternSquaresGreen = 314,
    PatternConstruction = 131,  // many dots
    PatternOverallComparison = 132,  // sward + building + land
    PatternRuralProperties = 133,  // land
    PatternBuildings = 134,  // buildings
    PatternCombatPower = 135,  // sward
    PatternFish = 138,
    PatternPig = 139,
    PatternMeat = 140,
    PatternEheat = 141,
    PatternFlour = 142,
    PatternBread = 143,
    PatternLumber = 144,
    PatternPlank = 145,
    PatternBoat = 146,
    PatternStone = 147,
    PatternIronore = 148,
    PatternSteel = 149,
    PatternCoal = 150,
    PatternGoldore = 151,
    PatternGoldbar = 152,
    PatternShovel = 153,
    PatternHammer = 154,
    PatternRod = 155,
    PatternCleaver = 156,
    PatternScythe = 157,
    PatternAxe = 158,
    PatternSaw = 159,
    PatternPick = 160,
    PatternPincer = 161,
    PatternSword = 162,
    PatternShield = 163
  } BackgroundPattern;

 protected:
  Interface *interface;
  std::unique_ptr<MinimapGame> minimap;
  std::unique_ptr<ListSavedFiles> file_list;
  std::unique_ptr<TextInput> file_field;
  // tlongstretch debug popup
  std::unique_ptr<TextInput> debug_pos_text_input_field;

  bool lifted = false; // if this popup was ever dragged anywhere, it becomes lifted and eligible for pinning
  //bool pinned = false; // if pinned the popup will not move
  int target_obj_index;  // usually a Building, but sometimes a Flag, index of target object of many popups


  Type box;

  int current_sett_5_item;
  int current_sett_6_item;
  int current_stat_8_mode;

  // function-as-parameter style copied from RandomText::TextInput in game-init.cc
  static bool savegame_text_input_filter(const char key, TextInput *text_input) {
    // allow only a-z, 0-9, dash, and dot. 
    // there is no uppercase, it seems all letters are evaluated as lowercase
    // there are also sprites for german special chars, %, and :, but these are bad in filenames and so excluded
    if (!((key >= 'a' && key <= 'z') ||
          (key >= '0' && key <= '9') ||
          (key == '-' || key == '.'))) {
      return false;
    }
    if (text_input->get_text().length() > 30) {
      return false;
    }
    return true;
  }

  static bool numeric_text_input_filter(const char key, TextInput *text_input) {
    // allow only 0-9
    if (!(key >= '0' && key <= '9')) {
      return false;
    }
    if (text_input->get_text().length() > 30) {
      return false;
    }
    return true;
  }

 public:
  explicit PopupBox(Interface *interface);
  virtual ~PopupBox();

  Type get_box() const { return box; }
  MinimapGame *get_minimap() { return minimap.get(); }

  void show(Type box);
  void hide();

  int get_target_obj_index() { return target_obj_index; }
  void set_target_obj_index(int index) { target_obj_index = index; }

  bool handle_drag(int x, int y); // testing movable popup  // moved from public Viewport::layout can call it when screen is resized

 protected:
  void draw_popup_box_frame();
  void draw_large_popup_box_frame();
  void draw_popup_icon(int x, int y, int sprite);
  void draw_popup_building(int x, int y, int sprite);
  void draw_box_background(BackgroundPattern sprite);
  void draw_large_box_background(BackgroundPattern sprite);
  void draw_box_row(int sprite, int y);
  void draw_green_string(int x, int y, const std::string &str);
  void draw_green_number(int x, int y, int n);
  void draw_green_large_number(int x, int y, int n);
  void draw_additional_number(int x, int y, int n);
  unsigned int get_player_face_sprite(size_t face);
  void draw_player_face(int x, int y, int player);
  void draw_custom_bld_box(const int sprites[]);
  void draw_custom_icon_box(const int sprites[]);
  const std::string prepare_res_amount_text(int amount) const;
  void draw_map_box(); // minimap popup
  void draw_mine_building_box();
  void draw_basic_building_box(int flip);
  void draw_adv_1_building_box();
  void draw_adv_2_building_box();
  void draw_resources_box(const ResourceMap &resources);
  void draw_serfs_box(const int serfs[], int total);
  void draw_stat_select_box();
  void draw_stat_4_box();
  void draw_building_count(int x, int y, int type);
  void draw_stat_bld_1_box();
  void draw_stat_bld_2_box();
  void draw_stat_bld_3_box();
  void draw_stat_bld_4_box();
  void draw_player_stat_chart(const int *data, int index, const Color &color);
  void draw_stat_8_box();
  void draw_stat_7_box();
  void draw_gauge_balance(int x, int y, unsigned int value, unsigned int count);
  void draw_gauge_full(int x, int y, unsigned int value, unsigned int count);
  void draw_stat_1_box();
  void draw_stat_2_box();
  void draw_stat_6_box();
  void draw_stat_3_meter(int x, int y, int value);
  void draw_stat_3_box();
  void draw_start_attack_redraw_box();
  void draw_start_attack_box();
  void draw_ground_analysis_box();
  void draw_sett_select_box();
  void draw_slide_bar(int x, int y, int value);
  void draw_colored_slide_bar(int x, int y, int value, Color color);
  void draw_sett_1_box();
  void draw_sett_2_box();
  void draw_sett_3_box();
  void draw_knight_level_box();  // knight staffing level min/max based on threat_level (i.e. how close to enemy)
  void draw_sett_4_box();  // toolmaker build priority sliders
  void draw_popup_resource_stairs(int order[]);
  void draw_sett_5_box();
  void draw_quit_confirm_box();
  void draw_no_save_quit_confirm_box();
  void draw_options_box();
  void draw_game_options_box();
  void draw_game_options2_box();
  void draw_game_options3_box();
  void draw_game_options4_box();
  void draw_edit_map_generator_box();
  void draw_edit_map_generator2_box();
  void draw_debug_box();
  void draw_castle_res_box();
  void draw_mine_output_box();
  void draw_ordered_building_box();
  void draw_defenders_box();
  void draw_transport_info_box();
  void draw_castle_serf_box();
  void draw_resdir_box();
  void draw_inv_queues_box();  // new
  void draw_sett_8_box();  // civ<>knight slider, create-knights button, gold-morale %, switch knights, button.  "Knight stomping" icon
  void draw_sett_6_box();  // "INVENTORY-priority" order for resources, only used in Evacuation mode for Castle/Stocks (very rarely used)
  void draw_bld_1_box();
  void draw_bld_2_box();
  void draw_bld_3_box();
  void draw_bld_4_box();
  void draw_building_stock_box();
  void draw_player_faces_box();
  void draw_demolish_box();
  void draw_please_wait_saving_box();  // added for option_EnableAutoSave
  void draw_please_wait_FogOfWar_box();  // added for option_FogOfWar
  void draw_save_box();
  void activate_sett_5_6_item(int index);
  void move_sett_5_6_item(int up, int to_end);
  void handle_send_geologist();
  void sett_8_train(int number);
  void set_inventory_resource_mode(int mode);
  void set_inventory_serf_mode(int mode);

  void handle_action(int action, int x, int y);
  int handle_clickmap(int x, int y, const int clkmap[]);
  //bool handle_drag(int x, int y); // testing movable popup  // moved to public Viewport::layout can call it when screen is resized
  bool handle_mouse_button_down(int x, int y, Event::Button button); // testing movable popup

  void handle_box_close_clk(int x, int y);
  void handle_debug_clk(int x, int y);
  void handle_box_options_clk(int x, int y);
  void handle_box_game_options_clk(int x, int y);
  void handle_box_game_options2_clk(int x, int y);
  void handle_box_game_options3_clk(int x, int y);
  void handle_box_game_options4_clk(int x, int y);
  void handle_box_edit_map_generator_clk(int x, int y);
  void handle_box_edit_map_generator2_clk(int x, int y);
  void handle_mine_building_clk(int x, int y);
  void handle_basic_building_clk(int x, int y, int flip);
  void handle_adv_1_building_clk(int x, int y);
  void handle_adv_2_building_clk(int x, int y);
  void handle_stat_select_click(int x, int y);
  void handle_stat_bld_click(int x, int y);
  void handle_stat_8_click(int x, int y);
  void handle_stat_7_click(int x, int y);
  void handle_stat_1_2_3_4_6_click(int x, int y);
  void handle_start_attack_click(int x, int y);
  void handle_ground_analysis_clk(int x, int y);
  void handle_sett_select_clk(int x, int y);
  void handle_sett_1_click(int x, int y);
  void handle_sett_2_click(int x, int y);
  void handle_sett_3_click(int x, int y);
  void handle_knight_level_click(int x, int y);
  void handle_sett_4_click(int x, int y);
  void handle_sett_5_6_click(int x, int y);
  void handle_quit_confirm_click(int x, int y);
  void handle_no_save_quit_confirm_click(int x, int y);
  void handle_castle_res_clk(int x, int y);
  void handle_transport_info_clk(int x, int y);
  void handle_castle_serf_clk(int x, int y);
  void handle_resdir_clk(int x, int y);
  void handle_inv_queues_clk(int x, int y);
  void handle_sett_8_click(int x, int y);
  void handle_message_clk(int x, int y);
  void handle_player_faces_click(int x, int y);
  void handle_box_demolish_clk(int x, int y);
  void handle_minimap_clk(int x, int y);
  void handle_box_bld_1(int x, int y);
  void handle_box_bld_2(int x, int y);
  void handle_box_bld_3(int x, int y);
  void handle_box_bld_4(int x, int y);
  void handle_save_clk(int x, int y);

  void set_box(Type box);

  // THESE FUNCTIONS BELOW ARE ALSO DEFINED IN map-generator.h and interface.h !!!!
  // 65500 (not 65535) / 2 = 32750
  double slider_uint16_to_double(uint16_t val){ return double(double(val) / double(32750)); }
  uint16_t slider_double_to_uint16(double val){ return uint16_t(val * 32750); }
  /* the default mineral amounts are already really high, leave the defaults as the max reasonable values
  // 65500 (not 65535) / 17 = 3852.94  (trying 3852)
  double slider_mineral_uint16_to_int_to_double(uint16_t val){ return double(int(val / 3852)); }  // convert to int midway so there are no fractional values
  uint16_t slider_mineral_double_to_uint16(double val){ return uint16_t(val * 3853); }
  */
  // 65500 (not 65535) / 9 = 7277.77, trying 7277 and 7278
  double slider_mineral_uint16_to_int_to_double(uint16_t val){ return double(int(val / 7277)); }  // convert to int midway so there are no fractional values
  uint16_t slider_mineral_double_to_uint16(double val){ return uint16_t(val * 7278); }

  virtual void internal_draw();
  //virtual bool handle_left_click(int x, int y);
  virtual bool handle_left_click(int x, int y, int modifier);
};

#endif  // SRC_POPUP_H_
