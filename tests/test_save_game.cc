/*
 * test_map.cc - TAP test for loading/saving game
 *
 * Copyright (C) 2016  Jon Lund Steffensen <jonlst@gmail.com>
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

#include <iostream>
#include <sstream>
#include <memory>

#include "src/game.h"
#include "src/random.h"
#include "src/savegame.h"

int
main(int argc, char *argv[]) {
  // Print number of tests for TAP
  std::cout << "1..7" << "\n";

  // Create random map game
  std::unique_ptr<Game> game(new Game());
  game->init();
  game->load_random_map(3, Random("8667715887436237"));

  // Add player to game
  unsigned int p_0 = game->add_player(12, 64, 35, 30, 40);
  Player *player_0 = game->get_player(p_0);
  if (player_0 == NULL) {
    std::cout << "not ok 1 - Player not added to game\n";
    return 0;
  } else {
    std::cout << "ok 1 - Player added to game\n";
  }

  // Build castle
  if (game->build_castle(game->get_map()->pos(6, 6), player_0)) {
    std::cout << "ok 2 - Player built castle\n";
  } else {
    std::cout << "not ok 2 - Player was not able to build castle\n";
    return 0;
  }

  // Run game for a number of ticks
  for (int i = 0; i < 500; i++) game->update();

  std::cout << "ok 3 - Random map game started and running\n";

  // Save the game state
  std::stringstream str;
  bool saved = save_text_state(&str, game.get());
  str.flush();

  if (saved && str.good()) {
    std::cout << "ok 4 - Saved game state\n";
  } else {
    std::cout << "not ok 4 - Failed to save game state; returned " <<
      saved << "\n";
    return 0;
  }

  // Load the game state into a new game
  str.seekg(0, std::ios::beg);
  std::unique_ptr<Game> loaded_game(new Game());
  bool loaded = load_text_state(&str, loaded_game.get());

  if (loaded) {
    std::cout << "ok 5 - Loaded game state\n";
  } else {
    std::cout << "not ok 5 - Failed to load save game state; returned " <<
      loaded << "\n";
    return 0;
  }

  // Restore state that is not stored in the save game
  loaded_game->init_land_ownership();

  // Check map
  if (*game->get_map() == *loaded_game->get_map()) {
    std::cout << "ok 6 - Maps are equal\n";
  } else {
    std::cout << "not ok 6 - Map equality test failed\n";
  }

  // Check gold deposit
  if (game->get_gold_total() == loaded_game->get_gold_total()) {
    std::cout << "ok 7 - Total gold count is identical\n";
  } else {
    std::cout << "not ok 7 - Total gold count is not identical\n";
  }
}
