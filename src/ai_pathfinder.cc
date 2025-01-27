/*
 * ai_pathfinder.cc - extra pathfinder functions used for AI
 *     Copyright 2019-2021 tlongstretch
 *
 *     FlagSearch function mostly copied from Pyrdacor's Freeserf.NET
 *        C# implementation
 *
 */

//
//  jan07 2021 - NOTE - it looks like the game stores the length of each path in the flag's length[] array.
//    This could be used instead of tracing and counting the tiles if the actual paths
//    are not needed, and it should be faster
//  NEVERMIND - the flag->length[dir] values are not true road lengths, though they start out that way,
//   they are banded into certain ranges, bit-shifted, and used for storing other state
//   I am thinking of modifying the Flag vars to store the true length and Dirs (i.e. the Road object)
//

#include "src/ai.h"

#include "src/pathfinder.h"  // for original tile SearchNode

class FlagSearchNode;

typedef std::shared_ptr<FlagSearchNode> PFlagSearchNode;

class FlagSearchNode {
 public:
  PFlagSearchNode parent;
  //unsigned int g_score;
  //unsigned int f_score;
  unsigned int flag_dist = 0;
  //unsigned int tile_dist;
  //Flag *flag = nullptr;   // try to make this work without using any Flag* objects, only MapPos where flag is
  MapPos pos = bad_map_pos;
  Direction dir = DirectionNone;  // this supports a search method I think is BROKEN!!! investigate and use the below way instead
  // the flag_pos of any path in each Direction from this fnode
  MapPos child_dir[6] = { bad_map_pos, bad_map_pos, bad_map_pos, bad_map_pos, bad_map_pos, bad_map_pos };
  // used for arterial_road flagsearch only, because it traces from first to last 
  //  when a solution is found, rather than last to first. This is so it can handle
  //   forks because it is not a single path but a network ending at an Inventory flag pos
  PFlagSearchNode child[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
};

static bool
flagsearch_node_less(const PFlagSearchNode &left, const PFlagSearchNode &right) {
  // A search node is considered less than the other if
  // it has a *lower* flag distance.  This means that in the max-heap
  // the shorter distance will go to the top
  //return left->flag_dist < right->flag_dist;
  // testing reversing this to see if it fixes depth-first issue
  // YES I think it does... so using above seems to do depth-first and using this below does breadth first
  return right->flag_dist < left->flag_dist;
  // hmm it seems flipping this breaks the iter_swap stuff in the find_flag_and_tile_dist code
  //  which rusults in a lot of problems.  I am reverting this and need to investigate if 
  //  find_flag_and_tile_dist are doing depth first or if it is only a problem with the new find_flag_path_between_flags
}




// plot a Road between two pos and return it
//  *In addition*, while plotting that road, use any existing flags and anyplace that new flags
//    could be built on existing roads along the way to populate a set of additional potential Roads.
//    Any valid existing or potential flag pos encountered "in the way" during pathfinding
//    will be included, but not positions that are not organically checked by the direct pathfinding logic.
//    The set will include the original-specified-end direct Road if it is valid
// is road_options actually used here??? or only by build_best_road??
//   seems that RoadOptions is not used, but I think I need it to be for HoldBuildingPos
// the road returned is the DIRECT road if found
//  however, if no direct road is found the calling function could still check for the new-flag splitting
//   options found


Road
// adding support for HoldBuildingPos
//AI::plot_road(PMap map, unsigned int player_index, MapPos start_pos, MapPos end_pos, Roads * const &potential_roads) {
//AI::plot_road(PMap map, unsigned int player_index, MapPos start_pos, MapPos end_pos, Roads * const &potential_roads, bool hold_building_pos) {
AI::plot_road(PMap map, unsigned int player_index, MapPos start_pos, MapPos end_pos, Roads * const &potential_roads, bool hold_building_pos, bool allow_passthru) {
  // time this function for debugging
  // THIS CLOCK IS CPU CYCLE BASED AND NOT REAL TIME!!!!
  std::clock_t start = std::clock();
  double duration;
  int slept_msec = 0;

  AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", inside plot_road for Player" << player_index << " with start " << start_pos << ", end " << end_pos;
  
  std::vector<PSearchNode> open;
  std::list<PSearchNode> closed;
  PSearchNode node(new SearchNode);

  if (use_plot_road_cache){
    // use cached open & closed nodes from previous search from same start_pos
    //  this cache must be cleared frequently!
    if (plot_road_closed_cache.count(start_pos) > 0){
      ////AILogDebug["plot_road"] << "DEBUG plot_roadplot_road_closed_cache_cache.at(start_pos) size is " << plot_road_closed_cache.at(start_pos).size();
      //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", inside plot_road for Player" << player_index << " with start " << start_pos << ", end " << end_pos << ", using cached closed node list from plot_road_cache";
      closed = plot_road_closed_cache.at(start_pos);
    }
    if (plot_road_open_cache.count(start_pos) > 0){
      ////AILogDebug["plot_road"] << "DEBUG plot_road_open_cache.at(start_pos) size is " << plot_road_open_cache.at(start_pos).size();
      //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", inside plot_road for Player" << player_index << " with start " << start_pos << ", end " << end_pos << ", using cached open node list from plot_road_cache";
      open = plot_road_open_cache.at(start_pos);
    }
  }else{
    //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", inside plot_road for Player" << player_index << " with start " << start_pos << ", end " << end_pos << ", NOT using use_plot_road_caches";
  }

  // prevent path in the potential building spot be inserting into the closed list!
  if (hold_building_pos == true){
    ////AILogDebug["plot_road"] << "debug - hold_building_pos is TRUE!";
    PSearchNode hold_building_pos_node(new SearchNode);
    hold_building_pos_node->pos = map->move_up_left(start_pos);
    closed.push_front(hold_building_pos_node);  // will this cause problems with cache???
    //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", hold_building_pos is TRUE, added hold_building_pos " << hold_building_pos_node->pos << " which is up-left of start_pos " << start_pos;
  }

  // allow pathfinding to *potential* new flags outside of the fake_flags process
  // if the specified end_pos has no flag
  //  oh wait this is already allowed, keep the sanity check and FYI note anyway though
  if (!map->has_flag(end_pos)){
    //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", the specified end_pos has no flag, allowing a Direct Road plot to this endpoint as if it had one";
    if (!game->can_build_flag(end_pos, player)){
      AILogWarn["plot_road"] << start_pos << " to " << end_pos << ", sanity check failed!  plot_road was requested to non-flag end_pos " << end_pos << ", but a flag cannot even be built there!  returning bad road";
      Road failed_road;
      return failed_road;
    }
  }

  //
  // pathfinding direction here is REVERSED from original code!  this function starts at the start_pos and ends at the end_pos, originally it was the opposite.
  //    As far as I can tell there is no downside to this, but flipping it makes fake flag solutions easy because when a potential new flag position
  //    is found on an existing road, the search path so far can be used to reach this new flag.  If the original (backwards) method is used, the
  //    search path "bread crumb trail" takes you back to the target_pos which isn't helpful - a path is needed between start_pos and new flag pos
  //
  node->pos = start_pos;
  node->g_score = 0;
  node->f_score = heuristic_cost(map.get(), end_pos, start_pos);
  open.push_back(node);
  Road direct_road;
  bool found_direct_road = false;
  //putting this here for now
  //unsigned int max_splitting_flags = 10;
  //unsigned int splitting_flags_count = 0;
  unsigned int max_alternate_road_solutions = 10;
  unsigned int found_flags_count = 0;
  unsigned int alternate_road_solutions = 0;
  //unsigned int new_closed_nodes = 0;  // I guess I added this but never used it
  //unsigned int new_open_nodes = 0;   // I guess I added this but never used it
  unsigned int total_pos_considered = 0;
  int flags_in_solution = 1;  // include one for the starting flag, as the while loop terminates once no parent node left and won't check the start pos for a flag

  ai_mark_pos.clear();

  //
  // this is a TILE search, finding open PATHs to build a single Road between two Flags
  //
  //  It may also allow connecting to non-existing (yet) new-splitting flags
  //
  //  And may also allow construction of multi-road passthru solutions that pass through
  //   one or more non-existing (yet) new-splitting flags and/or existing flags
  //
  while (!open.empty()) {
    std::pop_heap(open.begin(), open.end(), search_node_less);
    node = open.back();
    open.pop_back();

    //ai_mark_pos.insert(ColorDot(node->pos, "blue"));
    //std::this_thread::sleep_for(std::chrono::milliseconds(20));

    //-------------------------------------------------------------------
    // limit the search to a reasonable number of nodes and Road length
    //-------------------------------------------------------------------

    // limit the number of *positions* considered,
    //  though this is NOT the same as maximum possible length!!
    //  want to find a way to also check current road length to reject
    //  based on length without rejecting other, shorter roads as part of 
    //  same solution!
    if (total_pos_considered >= plot_road_max_pos_considered){
      //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: maximum MapPos POSITIONS-checked reached (plot_road_max_pos) " << total_pos_considered << ", ending search";
      break;
    }
    // PERFORMANCE - try pausing for a very brief time every thousand pos checked to give the CPU a break, see if it fixes frame rate lag
    if (total_pos_considered > 0 && total_pos_considered % 1000 == 0){
      //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: taking a quick sleep at " << total_pos_considered << " to give CPU a break";
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      slept_msec += 200;
    }
    total_pos_considered++;
    // limit the *length* considered
    unsigned int current_length = 0;
    PSearchNode tmp_length_check_node = node;
    // note that this checks every single node every time!
    //  though it seems to be very fast.  Removing it gives no noticeable perf gain
    while (tmp_length_check_node->parent) {
      current_length++;
      tmp_length_check_node = tmp_length_check_node->parent;
    }
    ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: current road-length so far for this solution is " << current_length;
    if (current_length >= plot_road_max_length){
      //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: maximum road-length reached (plot_road_max_length) " << current_length << ", ending search";
      break;
    }

    
    //------------------------------------------------------
    // if end_pos is reached, a Road solution is found
    //------------------------------------------------------
    if (node->pos == end_pos) {
      //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: solution found, node->pos is end_pos " << end_pos;
      Road breadcrumb_solution;
      breadcrumb_solution.start(end_pos);
      // retrace the "bread crumb trail" tile by tile from end to start
      //   this creates a Road with 'source' of end_pos and 'last' of start_pos
      //     which is backwards from the direction the pathfinding ran
      Direction last_dir = DirectionNone; // part of sanity check
      while (node->parent) {
        if (map->has_flag(node->pos)){
          //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: solution found, node->pos " << node->pos << " contains a flag";
          flags_in_solution++;
          //ai_mark_pos.insert(ColorDot(node->pos, "dk_yellow"));
        }
        Direction dir = node->dir;
        // sanity check, was seeing retracing on passthru solutions?
        if(last_dir == reverse_direction(dir)){
          AILogError["plot_road"] << start_pos << " to " << end_pos << ", plot_road: solution CONTAINS A RETRACING ROAD DIR!";
          AILogError["plot_road"] << start_pos << " to " << end_pos << ", plot_road: sleeping AI for a long time";
          std::this_thread::sleep_for(std::chrono::milliseconds(1000000));
          AILogError["plot_road"] << start_pos << " to " << end_pos << ", plot_road: done sleeping AI";
        }
        last_dir = dir;

        breadcrumb_solution.extend(reverse_direction(dir));
        // if ai_overlay is on, ai_mark_road will show the road being traced backwards from end_pos to start_pos
        //ai_mark_road->extend(reverse_direction(dir));
        //ai_mark_pos.insert(ColorDot(node->pos, "black"));
        //sleep_speed_adjusted(10);
        node = node->parent;
      }
      //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: solution found, solution contains " << flags_in_solution << " flags";
      unsigned int new_length = static_cast<unsigned int>(breadcrumb_solution.get_length());
      //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: solution found, new segment length is " << breadcrumb_solution.get_length();
      // now inverse the entire road so that it stats at start_pos and ends at end_pos, so the Road object is logical instead of backwards
      Road solution = reverse_road(map, breadcrumb_solution);
      // copy the solution to Road direct_road to be returned.  This isn't necessary but seems clearer,
      //   could just return 'solution' and move its declaration to the start of the function
      direct_road = solution;
      found_direct_road = true;  // is it really direct road if it is a passthru road??? 
      break;  // this breaks out of the while(open) search and ends the entire function
    }

    //---------------------------------------------------------------
    // handle alternate solutions, check passthru needs
    //---------------------------------------------------------------
    bool found_alternate_solution = false;
    //bool passthru = false;
    if (node->pos != start_pos){
      // if this node has a flag...
      if (map->get_obj(node->pos) == Map::ObjectFlag){
        if (road_options.test(RoadOption::Direct) == false){
          found_alternate_solution = true;
        }
        //if (road_options.test(RoadOption::AllowPassthru) == true) {
        //  passthru = true;
        //}
      }else{
      // if this node->pos does NOT contain a flag...
        // ...and has a blocking path...
        if (map->paths(node->pos) != 0){
          // ...but can create a new flag here...
          if (game->can_build_flag(node->pos, player) && road_options.test(RoadOption::SplitRoads)){
            found_alternate_solution = true;
          }
          //if (road_options.test(RoadOption::AllowPassthru) == true) {
          //  passthru = true;
          //}
        }
      }
    }

    if (found_alternate_solution){
      if (alternate_road_solutions >= max_alternate_road_solutions){
        //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: alternate existing-flag solution found while pathfinding, but max_alternate_road_solutions " << max_alternate_road_solutions << " reached, not including";
      }else{
        //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: alternate existing-flag solution found while pathfinding, a non-target flag exists, or a new splitting flag could be built, pos " << node->pos << ", adding the Road so far to potential_roads list";
        // retrace the "bread crumb trail" tile by tile from end to start
        //   this creates a Road with 'source' of end_pos and 'last' of start_pos
        //     which is backwards from the direction the pathfinding ran
        // start the road at node->pos where the existing flag is, and follow the 'node' path back to start_pos
        Road alternate_flag_solution;
        alternate_flag_solution.start(node->pos);
        //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: alternate existing-flag solution found while pathfinding, retracing nodes backwards from current node->pos " << node->pos;
        PSearchNode alternate_node = node;  // create an copy of current node to avoid disrupting the original search
        Direction alternate_node_last_dir = DirectionNone; // part of retrace sanity check
        int passthru_flags = 0;  // part of impossibly-close new flag sanity check
        bool reject_solution = 0;  // part of impossibly-close new flag sanity check
        MapPos last_new_flag_pos = bad_map_pos;  // part of impossibly-close new flag sanity check
        while (alternate_node->parent) {
          // if ai_overlay is on, ai_mark_road will show the road being traced backwards from end_pos to start_pos
          Direction dir = alternate_node->dir;

          // sanity check, was seeing retracing on passthru solutions?  I THINK THIS IS FIXED NOW
          //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: dir " << reverse_direction(dir) << ", last_dir " << reverse_direction(alternate_node_last_dir);
          if(alternate_node_last_dir == reverse_direction(dir)){
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: alternate existing-flag solution CONTAINS A RETRACING ROAD DIR!";
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: alternate existing-flag solution sleeping AI for a long time";
            std::this_thread::sleep_for(std::chrono::milliseconds(1000000));
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: alternate existing-flag solution done sleeping AI";
            break;  // reject this solution
          }

          // sanity check, does this solution depend on creating a new flag impossibly close to another flag?
          if ( !map->is_road_segment_valid(alternate_node->parent->pos, alternate_node->parent->dir) && game->can_build_flag(alternate_node->pos, player) && map->has_any_path(alternate_node->pos) && alternate_node->pos != end_pos){
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: alternate existing-flag solution sanity check, pos " << alternate_node->pos << " in new-split-flag passthru sanity check, this is a new-split-flag solution";
            passthru_flags++;  // ... it is a passthru new splitting flag
            // Flags cannot be created right next to each other, so reject any solution that depends on this
            // ALSO, if the end of the road ALSO requires a new flag, reject if the end of the road is one pos away
            if (passthru_flags > 1){
              //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: alternate existing-flag solution sanity check, pos " << alternate_node->pos << " in new-split-flag passthru sanity check, sanity-checking pos surrounding " << alternate_node->pos << " to see if any are the last_new_flag_pos " << last_new_flag_pos;
              for (Direction check_dir : cycle_directions_cw()){
                if (map->move(alternate_node->pos, check_dir) == last_new_flag_pos
                || map->move(alternate_node->pos, check_dir) == end_pos){
                  //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: alternate existing-flag solution sanity check, pos " << alternate_node->pos << " in new-split-flag passthru sanity check, rejecting this new-split-flag solution because it depends on multiple new flags impossibly close to each other";
                  reject_solution = true;
                  break;
                }
              }
              if (reject_solution){ break; }
            }
            if (passthru_flags > max_passthru_flags_per_solution){
              //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: breached max_passthru_flags_per_solution of " << max_passthru_flags_per_solution << ", rejecting this solution";
              reject_solution = true;
              break;
            }
            last_new_flag_pos = alternate_node->pos;
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: alternate existing-flag solution sanity check, pos " << alternate_node->pos << " in new-split-flag passthru sanity check, no illegal flag placement found, allowing this new-split-flag node";
          }

          alternate_node_last_dir = dir;
          //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: alternate existing-flag solution found while pathfinding, retracing nodes backwards, node->pos has a parent node with Dir" << reverse_direction(dir);
          alternate_flag_solution.extend(reverse_direction(dir));
          //ai_mark_road->extend(reverse_direction(dir));
          //ai_mark_pos.insert(ColorDot(node->pos, "black"));
          //sleep_speed_adjusted(10);
          alternate_node = alternate_node->parent;
        }
        if (reject_solution){
          //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: alternate_flag_solution failed a sanity check, not adding";
        }else{
          unsigned int new_length = static_cast<unsigned int>(alternate_flag_solution.get_length());
          //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: alternate_flag_solution found, new segment length is " << alternate_flag_solution.get_length();
          // now inverse the entire road so that it stats at start_pos and ends at end_pos, so the Road object is logical instead of backwards
          alternate_flag_solution = reverse_road(map, alternate_flag_solution);
          //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: inserting alternate/fake flag Road solution to PotentialRoads with segment start_pos " << alternate_flag_solution.get_source() << ", segment end_pos " << alternate_flag_solution.get_end(map.get()) << ", new segment length would be " << alternate_flag_solution.get_length();
          potential_roads->push_back(alternate_flag_solution);
          alternate_road_solutions++;
        }
        //
        // NOTE - for passthru solutions, the entire multi-road solution will be returned as a single Road, and it will be up
        //  to the build_best_road function to break it up into the chain of roads within and update the solution flag_score
        //
      }
    }

    //-------------------------------------------------
    // ---------------close this node unless it is a Passthru solution
    //   WAIT!!! I this this is bad logic
    //   closed means it definitely isn't the SOLUTION
    //   and a new node shouldn't be created for it again
    //   but it should still have its adjacent pos checked
    //   so passthru should have nothing to do with it!
    //-------------------------------------------------
    //if (passthru){
    //  //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: NOT closing this node->pos " << node->pos << " because it is part of a passthru solution";
    //}else{
    //  //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: closing this node->pos " << node->pos << " as impassable for this solution";
    //  closed.push_front(node);  // note this can create a duplicate cache entry but I don't think it matters
    //  //new_closed_nodes++;
    //}

    //------------------------------------------------------------------------------------------------
    // close this node, meaning it cannot be the last pos in any solution and should not be re-checked
    //------------------------------------------------------------------------------------------------
    ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", is solution? plot_road: closing this node->pos " << node->pos << " as it is NOT the end_pos / solution";
    closed.push_front(node);  // note this can create a duplicate cache entry but I don't think it matters

    //-------------------------------------------------
    // check surrounding pos in each Direction
    //-------------------------------------------------
    // oct21 2020 - changing to this use RANDOM start direction to avoid issue where a road
    //   is never built because of an obstacle in one major direction, but a road could have been built if
    //   a different direction was chosen.  This means if many nearby spots are tried one should eventually succeed
    //for (Direction d : cycle_directions_cw()) {
    for (Direction d : cycle_directions_rand_cw()) {
      //
      // for each Direction around this pos
      //  either 'continue' to reject new_pos as impassable
      //  or do nothing to accept new_pos as passable
      //
      MapPos new_pos = map->move(node->pos, d);

      // Check if the new_pos in this Direction d is valid
      //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << ", checking if this new_pos is valid";

      //-------------------------------------------------------------
      // check Dirs - First, handle conditions that always result in rejection
      //-------------------------------------------------------------

      // reject if pos contains an impassable object
      if (Map::map_space_from_obj[map->get_obj(new_pos)] >= Map::SpaceSemipassable){
        //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " cannot be valid because it contains an impassable map object";
        continue;  // reject this solution
      }

      // reject if pos is outside this player's borders
      if (!map->has_owner(new_pos) || map->get_owner(new_pos) != map->get_owner(node->pos)) {
        //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " cannot be valid because it is not owned by this player";
        continue;  // reject this solution
      }

      /* I saw an instance where a path was plotted right across a body of water, not sure how it passed this check
      //  for now simply excluding any solution that includes water

      // reject if pos crosses water without a flag (AI pathfinder will NOT mix water & land roads in a single solution)
      if (map->is_in_water(node->pos) != map->is_in_water(new_pos) && !(map->has_flag(node->pos) || map->has_flag(new_pos))) {
        ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " cannot be valid because it cross between land & water with no flag in either pos";
        continue;  // reject this solution
      }

      */
      // reject if pos crosses water at all, totally disallowing water roads for AI!

      // actually I think this is a bad idea, it may prevent legitimate roads
      //if (map->is_EITHER_TRIANGLE_water_tile(node->pos) || map->is_EITHER_TRIANGLE_water_tile(new_pos)) {
      // going back to normal way
      if (map->is_water_tile(node->pos) || map->is_water_tile(new_pos)) {
        //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " cannot be valid because it cross between land & water with no flag in either pos";
        continue;  // reject this solution
      }

      //
      // IMPORTANT - NEED TO REJECT if the dir from prev pos to current pos has a path ALONG THE WAY, that is, if we are following along a road
      //    it is ALWAYS invalid because no passthru can happen along a road, only intersecting one!
      //
      // here is how serfs determine the next path dir along a road
      /* Serf is not at a flag. Just follow the road. 
      int paths = game->get_map()->paths(pos) & ~BIT(s.walking.dir);
      Direction dir = DirectionNone;
      for (Direction d : cycle_directions_cw()) {
        if (paths == BIT(d)) {
          dir = d;
          break;
        }
      }
      */

      // check if we are FOLLOWING ALONG a path in this dir, which is always illegal even with passthru
      // WAIT - it should be possible for passthru solutions to re-use existing roads as segments of a new
      //  multi-segment solution!  To do this, once a node begins tracing an existing path, it must stay on the
      //  path until the end flag is reached, so this check must enforce that

      // NOTE - this check does not care at all if a path is found that is NOT IN THE DIRECTION OF TRAVEL
      //   if there is some other blocking path a later check will handle it.

      // check if we are ALREADY following a path, by checking if the parent pos -> current pos has a path
      bool must_follow_path = false;

      // debug
      if (node->parent){
        //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? node has a parent, parent pos " << node->parent->pos << " has dir-to-child " << node->parent->dir;
      }

      if (node->parent){
        //// if there is a flag at the parent pos, we can exit any followed path here so set the bool to false to disable enforcement of path following
        //if (map->has_flag(new_pos)){
        //  //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? there is a flag at new_pos " << new_pos << ", so not checking to see if we are following a path because we could exit";
        //}else{
        // no flag here, need to check if we are already following a path
          if(map->has_path(node->parent->pos, node->parent->dir)){
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? we are already following an existing path from parent node, parent dir is " << node->parent->dir << ", setting must_follow_path bool to true";
            must_follow_path = true;
          }
        //}
      }

      // there is a path between current and new pos
      //  if we are already following a path, we must stay on it
      //  if we are not yet following a path, we can start following one if conditions are right
      if (map->has_path(node->pos, d)){
        //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is along a path";
        if (must_follow_path){
          //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is along a path that we are already following, will continue along the path";
        }else{
          // if the current node->pos leading to this new_pos/dir has a flag or can build one, we can start following this new path
          //  NOTE that the impossibly-close-new-splitting-flag check later should catch an illegal follow... actually I don't think an illegally close
          //   new split flag could even happen here as if this is a path it already prevents illegal flags I think
          if ((map->has_flag(node->pos) && road_options.test(RoadOption::AllowPassthru) == true)
           || (game->can_build_flag(node->pos, player) && road_options.test(RoadOption::AllowPassthru) == true && road_options.test(RoadOption::SplitRoads) == true)){
            // start following this path from the flag
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is along a path from a flag at the current node->pos, will start following this new existing path.  NOT setting must_follow_path to true because we could still exit this path if this dir doesn't work out for other reasons";
            //must_follow_path = true;  // NO!  do not set this here, let it be set at the next node.  This being set here forces following the Direction of a first path found from a node->pos, which might not be desirable
          }else{
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is along a path that we cannot follow, either because previous pos has no flag and can't build one, or RoadOptions prevent it";
            continue; // reject this solution
          }
        }
      }else{
      // there is no path DIRECTLY BETWEEN current and new pos (there could be a blocking path in some other dir)
        // NEED TO CHECK IF THERE IS ANY PATH AT ALL, other than along the current direction, to see if we are straying off it while already following a path
        //  if we are already following a path, can we exit it here?
        //  if we are not following a path, and there is no blocking path, continue as usual (this is the MOST COMMON case, simply a non-path pos)
        if (must_follow_path){
          //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is straying from the path that we are already following, checking to see if there exists or we can build a flag to exit from";
          // check if there is a other blocking path
          if (map->paths(new_pos) != 0){
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is rejected because we are already following a path and new_pos contains a blocking path (not in the direction of travel)";
            continue; // reject this solution
          }
          // check to see if we can exit the road here, either at an existing flag or by building one
          if (map->has_flag(node->pos)
           || (game->can_build_flag(node->pos, player) && road_options.test(RoadOption::AllowPassthru) == true && road_options.test(RoadOption::SplitRoads) == true)){
            // stop following the current path
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " can exit this path here, because either a flag exists or we can build one, setting must_follow_path bool to false";
            must_follow_path = false;
          }else{
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " cannot stray from this path as no flag exists or we cannot build one or roadoptions forbid it";
            continue; // reject this solution
          }
        }else{
        // we are not currently following any path
          // check to see if there is any blocking path
          if (map->paths(new_pos) != 0){
            // there is a blocking path, defer to later check to see if it must be rejected
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " has a blocking path, not rejecting it yet, will let later passthru & splitting flags logic handle it";
            // DO NOT REJECT IT YET, LET LATER LOGIC HANDLE
          }else{
            // no blocking path and not following road, most common scenario!
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is not along any path and has no blocking paths, and so is normally valid";
          }
        }
      } // end if there is a path ALONG this way


      // reject if both previous and new_pos within "forbidden zone" immediately surrounding an Inventory flag pos
      // Castle or any Stock flag must have a surrounding "forbidden zone" to prevent creation of paths blocking excessively
      //            22
      //            --\2
      //     castle_   \2
      //  2/\2 /\cc/\   \2 
      // 2/_2\/cc\/cc\   \2
      // 2\--2\cc/\cc/\1  \2
      //  2\  2\/__\/__\1  \2  castle/stock flag area, double lines are forbidden paths
      //   2\  1\  /\  /1  /2            NOTE that ring1 pos can connect to ring2 pos, 
      //    2\  1\/__\/1  /2                   but ring1-to-ring1 and ring2-to-ring2 disallowed
      //     2\   1111   /2
      //      2\________/2
      //        22222222
      //
      // and path segment with both ends in same ring forbidden list must be rejected
      if (node->parent){
        uint8_t forbidden_ends = 0;
        for (MapPos forbidden_pos : stock_building_counts.at(inventory_pos).forbidden_paths_ring1){
          if (node->pos == forbidden_pos){
            ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", node->pos " << node->pos << " is on the stock_building_counts.at(inventory_pos).forbidden_paths_ring1 list";
            forbidden_ends++;
          }else if(node->parent->pos == forbidden_pos){
            ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", node->parent->pos " << node->pos << " is on the stock_building_counts.at(inventory_pos).forbidden_paths_ring1 list";
            forbidden_ends++;
          }
        }
        if (forbidden_ends > 1){
          ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", is valid? rejecting direction " << d << " / " << NameDirection[d] << " because it crosses the forbidden partial-hexagon-perimeter of castle flag ring1";
          continue;  // reject this solution
        }
        forbidden_ends = 0;
        for (MapPos forbidden_pos : stock_building_counts.at(inventory_pos).forbidden_paths_ring2){
          if (node->pos == forbidden_pos){
            ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", node->pos " << node->pos << " is on the stock_building_counts.at(inventory_pos).forbidden_paths_ring2 list";
            forbidden_ends++;
          }else if(node->parent->pos == forbidden_pos){
            ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", node->parent->pos " << node->pos << " is on the stock_building_counts.at(inventory_pos).forbidden_paths_ring2 list";
            forbidden_ends++;
          }
        }
        if (forbidden_ends > 1){
          ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", is valid? rejecting direction " << d << " / " << NameDirection[d] << " because it crosses the forbidden partial-hexagon-perimeter of castle flag ring2";
          continue;  // reject this solution
        }
      }

      //-------------------------------------------------------------
      // check Dirs - Next, handle conditions with various exceptions
      //-------------------------------------------------------------
      bool found_existing_flag = false;
      bool found_potential_flag = false;
      if (map->get_obj(new_pos) == Map::ObjectFlag){
      // new_pos already contains a flag
        //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " contains a flag, setting found_existing_flag to true";
        found_existing_flag = true;
        if (new_pos == end_pos){
          //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " contains a flag and is the target end_pos, solution found but not handling it yet";
        }else if (new_pos == start_pos){
          //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is rejected because it is the start_pos, which is not helpful";
          continue;  // reject this solution
        }else{
          if (road_options.test(RoadOption::AllowPassthru) == true || road_options.test(RoadOption::Direct) == false) {
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " contains a flag but it is not the target end_pos, and either AllowPassthru=true or Direct=false, will add it to found_flags list if this is found to be a new node";
          }else{
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is rejected because either AllowPassthru=false or Direct=true";
            continue;  // reject this solution
          }
        }
      }else{
      // new_pos does NOT already contain a flag
        // if current pos to new_pos has ANY path...
        if (map->paths(new_pos) != 0){
          //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is NOT normally valid because it contains an existing path but has no flag";
          if (!must_follow_path && !road_options.test(RoadOption::SplitRoads)) {
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is rejected because because it contains an existing path but has no flag and RoadOption::SplitRoads is false";
            continue;  // reject this solution
          }
          // see if a flag can be built here
          if (game->can_build_flag(new_pos, player)){
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is NOT normally valid because it contains an existing path but has no flag, but could build flag here to make it valid, setting found_potential_flag to true, will add it to found_flags list if this is found to be a new node";
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is NOT normally valid because it contains an existing path but has no flag, but could build flag here to make valid.  Must perform sanity check to ensure no adjacent pos is ALSO marked for new-flag, which would be illegal";

//----------------------------------------------------

            int passthru_flags = 0;
            MapPos last_new_flag_pos = new_pos;
            bool reject_solution = false;
            PSearchNode split_flag_sanity_check_node = node;  // create a copy of node to avoid affecting existing solution
            ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " starting passthru sanity-check for this new-splitting-flag solution to see if it depends on impossibly close flags";
            while (split_flag_sanity_check_node->parent) {
              if (reject_solution){ break; }
              MapPos parent_pos = split_flag_sanity_check_node->parent->pos;
              Direction parent_dir_to_child = split_flag_sanity_check_node->parent->dir;
              MapPos pos = split_flag_sanity_check_node->pos;
              ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " in new-split-flag passthru sanity check, parent_pos " << parent_pos << ", parent_dir_to_child " << parent_dir_to_child << ", pos " << pos;
              // if this pos has no flag and is blocked by an existing path, but a flag could be built here AND this is not the end pos, this is a new-splitting passthru flag
              if ( !map->is_road_segment_valid(parent_pos, parent_dir_to_child) && game->can_build_flag(pos, player) && map->has_any_path(pos) && pos != end_pos){
                //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " in new-split-flag passthru sanity check, this is a new-split-flag solution";
                passthru_flags++;  // ... it is a passthru new splitting flag
                // Flags cannot be created right next to each other, so reject any solution that depends on this
                // ALSO, if the end of the road ALSO requires a new flag, reject if the end of the road is one pos away
                if (passthru_flags > 1){
                  //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " in new-split-flag passthru sanity check, sanity-checking pos surrounding " << pos << " to see if any are the last_new_flag_pos " << last_new_flag_pos;
                  for (Direction check_dir : cycle_directions_cw()){
                    if (map->move(pos, check_dir) == last_new_flag_pos
                    || map->move(pos, check_dir) == end_pos){
                      //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " in new-split-flag passthru sanity check, rejecting this new-split-flag solution because it depends on multiple new flags impossibly close to each other";
                      reject_solution = true;
                      break;
                    }
                  }
                  if (reject_solution){ break; }
                }
                last_new_flag_pos = pos;
                //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " in new-split-flag passthru sanity check, no illegal flag placement found, allowing this new-split-flag node";
      //        }else if (pos == end_pos) {
      //          ai_mark_pos.insert(ColorDot(pos, "dk_blue"));  // end pos
      //          AILogDebug["util_build_best_road"] << "" << calling_function << " passthru check, this pos " << pos << " is the end pos flag";
      //        }else{
      //          ai_mark_pos.insert(ColorDot(pos, "white"));  // normal pos
      //          AILogDebug["util_build_best_road"] << "" << calling_function << " passthru check, this pos " << pos << " contains nothing blocking";
              }
              split_flag_sanity_check_node = split_flag_sanity_check_node->parent;
            }
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " done new-split-flag passthru sanity check, found " << passthru_flags << " passthru_flags";

//-------------------------------------

            if (reject_solution){
              //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is rejected because it failed impossibly-close-new-flag passthru sanity check";
            }else{
              found_potential_flag = true;
              //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " passed passthru sanity check, if it is actually a new pos a new node will be created";
            }
          }else{
            // this new_pos has ANY path, and no flag, and a flag cannot be built here
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " HAS ANY path, and no flag, and a flag cannot be built here, checking to see if this is a path we are following";
            // check to see if this is a path in the direction of travel, and if we are following it as part of a passthru solution
            if (map->has_path(node->pos, d)){
              //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " has a path in the direction of travel, checking to see if are already following it";
              if (must_follow_path){
                //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " has a path in the direction of travel that we are already following";
              }else{
                //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " has a path in the direction of travel that we are not already following, let this play out and allow it to be rejected if needed";
                /*
                AILogError["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " has a path in the direction of travel that we are NOT ALREADY FOLLOWING!  this should not be possible due to earlier check, crashing";
                AILogError["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " marking start_pos in black, next pos in white";
                ai_mark_pos.insert(ColorDot(node->pos, "black"));
                ai_mark_pos.insert(ColorDot(new_pos, "white"));
                AILogWarn["util_build_best_road"] << "SLEEPING AI FOR LONG TIME";
                std::this_thread::sleep_for(std::chrono::milliseconds(1000000));
                AILogWarn["util_build_best_road"] << "DONE SLEEPING";
                //throw ExceptionFreeserf("sanity check failed, this new_pos contains a path we are not following, earlier check should have caught this!");
                */
              }
            }else{
              // this is a blocking path
              //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is rejected because it contains an existing path NOT in the direction of tavel, no flag, and a new flag cannot be built here";
              continue;  // reject this solution
            }
          }
        }else{
          //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " has no paths";
          /* moved this check logic to earlier, removing for now
          // this new_pos has no paths at all, check to make sure we aren't straying off a path we are already following
          if (must_follow_path){
            //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " has no paths, but we are following a path, cannot exit here";
          }else{
            AILogError["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " has a path in the direction of travel that we are NOT ALREADY FOLLOWING!  this should not be possible due to earlier check, crashing";
            AILogError["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " marking start_pos in black, next pos in white";
            ai_mark_pos.insert(ColorDot(node->pos, "black"));
            ai_mark_pos.insert(ColorDot(new_pos, "white"));
            AILogWarn["util_build_best_road"] << "SLEEPING AI FOR LONG TIME";
            std::this_thread::sleep_for(std::chrono::milliseconds(1000000));
            AILogWarn["util_build_best_road"] << "DONE SLEEPING";
            //throw ExceptionFreeserf("sanity check failed, this new_pos contains a path we are not following, earlier check should have caught this!");
          }
          */
          //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is normally valid with no complications";
        }
      }

      //-------------------------------------------------------------------------------------
      // check Dirs - if this point reached, new_pos might be passible if conditions are met
      //-------------------------------------------------------------------------------------
      ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is valid? new_pos " << new_pos << " is potentially valid";

      // this is used to sort the heap to determine the optimal solution, it cannot be removed!
      unsigned int cost = actual_cost(map.get(), node->pos, d);

      //------------------------------------------------------------------------
      // check Dirs - check if new_pos is already marked as closed (impassable)
      //------------------------------------------------------------------------
      //   I believe this check is usually slower than the previous validity checks
      //   and that is why it is done after them instead of before
      //
      bool in_closed = false;
      for (PSearchNode closed_node : closed) {
        if (closed_node->pos == new_pos) {
          // to avoid missing the end_pos because it was marked
          // as closed a previous search from the same start_pos:
          //  if this is the end_pos, do NOT set in_closed to true
          //  so it will continue and find solution on next loop
            //ai_mark_pos.erase(new_pos);
            //ai_mark_pos.insert(ColorDot(new_pos, "red"));
            //sleep_speed_adjusted(10);
          if (use_plot_road_cache && closed_node->pos == end_pos){
            ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", in closed? plot_road: use_plot_road_cache is true and cache has end_pos " << end_pos << " marked as already closed, bypassing this, solution coming";
          }else{
            in_closed = true;
            //ai_mark_pos.erase(new_pos);
            //ai_mark_pos.insert(ColorDot(new_pos, "magenta"));
            //sleep_speed_adjusted(10);
            // PERFORMANCE - try pausing for a very brief time every thousand pos checked to give the CPU a break, see if it fixes frame rate lag
            if (total_pos_considered > 0 && total_pos_considered % 1000 == 0){
              //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", plot_road: taking a quick sleep at " << total_pos_considered << " to give CPU a break";
              std::this_thread::sleep_for(std::chrono::milliseconds(200));
              slept_msec += 200;
            }
            total_pos_considered++;
          }
          break;
        }
      }
      // check Dirs - reject new_pos if it was found to be already in closed list
      if (in_closed){
        ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", in closed? new_pos " << new_pos << " is rejected because already in the closed list";
        continue;
      }

      //ai_mark_pos.erase(new_pos);
      //ai_mark_pos.insert(ColorDot(new_pos, "green"));
      //sleep_speed_adjusted(10);

      //--------------------------------------------------------------------------------------
      // check Dirs - if new_pos node already on open list, heapify (re-sort entire solution)
      //--------------------------------------------------------------------------------------
      bool in_open = false;
      for (std::vector<PSearchNode>::iterator it = open.begin();
        it != open.end(); ++it) {
        PSearchNode n = *it;
        if (n->pos == new_pos) {
          in_open = true;
          ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", in open? new_pos " << new_pos << " is already in the open list, heapifying (re-sorting)";
          //ai_mark_pos.erase(new_pos);
          //ai_mark_pos.insert(ColorDot(new_pos, "seafoam"));
          //sleep_speed_adjusted(10);
          if (n->g_score >= node->g_score + cost) {
            n->g_score = node->g_score + cost;
            n->f_score = n->g_score + heuristic_cost(map.get(), new_pos, end_pos);
            n->parent = node;
            n->dir = d;

            // Move element to the back and heapify (re-sort all nodes by f_score)
            iter_swap(it, open.rbegin());
            std::make_heap(open.begin(), open.end(), search_node_less);
          }
          break;
        }
      }

      //-------------------------------------------------------------
      // check Dirs - if not already in open list, create a new node
      //-------------------------------------------------------------
      if (!in_open) {
        PSearchNode new_node(new SearchNode);

        new_node->pos = new_pos;
        new_node->g_score = node->g_score + cost;
        new_node->f_score = new_node->g_score + heuristic_cost(map.get(), new_pos, end_pos);
        new_node->parent = node;
        new_node->dir = d;
        ////AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", " << node->pos << "->dir" << d << ", is new? new_pos " << new_pos << " is a new node, calling push_heap";
        open.push_back(new_node);
        std::push_heap(open.begin(), open.end(), search_node_less);

        //new_open_nodes++;
      }

    } // foreach direction / new_pos  
  } // while nodes left to search

  //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", done plot_road, found_direct_road: " << std::to_string(found_direct_road) << ". additional potential_roads: " << alternate_road_solutions << ", flags_in_solution " << flags_in_solution;
  if (direct_road.get_source() == bad_map_pos) {
    //AILogDebug["plot_road"] << start_pos << " to " << end_pos << ", NO DIRECT SOLUTION FOUND for start_pos " << start_pos << " to end_pos " << end_pos;
  }

  // for debugging, keep track of the largest road built to get an idea of what is a reasonable limit to set for performance reasons
  //  also, consider making the limit an argument to this function, so that high limits can be allowed for certian roads but low limits
  //  for others
  // WAIT - this highlights the longest road *plotted* not the longest actually selected and built
  //  instead, moving this to the build_best_road call
  //if (direct_road.get_length() > longest_road_so_far.get_length()){
  //  //AILogDebug["plot_road"] << "this road is the new longest road built by this AI so far, with length " << direct_road.get_length() << ", highlighting it as ai_mark_road";
  //  longest_road_so_far = direct_road; 
  //  ai_mark_road = &longest_road_so_far;
  //}

  // update the closed node cache for this start_pos
  if (use_plot_road_cache){
    //AILogDebug["plot_road"] << "caching (posibly-updated) plot_road cached-node lists for start_pos " << start_pos;
    plot_road_closed_cache.erase(start_pos);
    plot_road_closed_cache.insert(std::make_pair(start_pos, closed));
    plot_road_open_cache.erase(start_pos);
    plot_road_open_cache.insert(std::make_pair(start_pos, open));
  }

  //
  // ALL ATTEMPTS TO CALCULATE CACHE HIT RATE AND pos per second with cache vs no-cache
  //   have failed beacuse it is difficult to identify a cache hit without doing 
  //   additional work that will slow the request down.  Each pos may be visited multiple 
  //   times so can't use "new pos created vs total pos visited" and it isn't easy to tell
  //   how many positions were not considered at all because they were bypassed by resuming
  //   the open cache
  //
  //  THE BEST WAY to prove the cache works is to run repeat calls of the exact same
  //   plot_road request and seeing that the second call is much faster and considers far
  //   fewer positions
  //
  //unsigned int cache_hits = total_pos_considered - new_closed_nodes;
  //
  //int cache_hit_ratio = 0;
  ////if (cache_hits > 0){cache_hit_ratio = ( pos_considered / cache_hits) * 100;}
  //if (cache_hits > 0){cache_hit_ratio = ( double(cache_hits) / double(total_pos_considered - 1) ) * double(100.00);}

  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);

  //int pos_per_second = total_pos_considered / duration / 1000; // in thousands
  //if (duration == 0){pos_per_second = 999;}

  ////AILogDebug["plot_road"] << "plot_road call considered " << pos_considered << " MapPos, took " << duration;
  ////AILogDebug["plot_road"] << "plot_road call with start_pos " << start_pos << ", end_pos " << end_pos << ", considered " << total_pos_considered << " MapPos, took " << duration << ", cache_hits " << cache_hits << ", cache_hit_ratio " << cache_hit_ratio << "%";
  ////AILogDebug["plot_road"] << "plot_road call with start_pos " << start_pos << ", end_pos " << end_pos << ", considered " << total_pos_considered << " MapPos, took " << duration << ", pos_per_sec " << pos_per_second << "k/sec";
  //AILogDebug["plot_road"] << "plot_road call with start_pos " << start_pos << ", end_pos " << end_pos << ", considered " << total_pos_considered << " MapPos, took " << duration << ", " << slept_msec << "ms was spent sleeping";

  //sleep_speed_adjusted(2000);
  return direct_road;
}

//
// MAKE THIS A GENERAL GAME FUNCTION AND NOT AI-SPECIFIC!
//
// count how many tiles apart two MapPos are
int
AI::get_straightline_tile_dist(PMap map, MapPos start_pos, MapPos end_pos) {
  //AILogDebug["get_straightline_tile_dist"] << "inside Pathfinder::tile_dist with start_pos " << start_pos << ", end_pos " << end_pos;
  // function copied from heuristic_cost but ignores height diff and walking_cost
  int dist_col = map->dist_x(start_pos, end_pos);
  int dist_row = map->dist_y(start_pos, end_pos);
  int tile_dist = 0;
  if ((dist_col > 0 && dist_row > 0) || (dist_col < 0 && dist_row < 0)) {
    tile_dist = std::max(abs(dist_col), abs(dist_row));
  }
  else {
    tile_dist = abs(dist_col) + abs(dist_row);
  }
  //AILogDebug["get_straightline_tile_dist"] << "returning tile_dist: " << tile_dist;
  return tile_dist;
}


// find best flag-path from flag_pos to target_pos and store flag and tile count in roadbuilder
//  return true if solution found, false if couldn't be found
// this function will accept "fake flags" / splitting-flags that do not actually exist
//  and score them based on the best score of their adjacent flags
bool
AI::score_flag(PMap map, unsigned int player_index, RoadBuilder *rb, RoadOptions road_options, MapPos flag_pos, MapPos castle_flag_pos, ColorDotMap *ai_mark_pos) {
  // time this function for debugging
  std::clock_t start = std::clock();
  double duration;

  AILogDebug["score_flag"] << "inside score_flag";
  MapPos target_pos = rb->get_target_pos();
  AILogDebug["score_flag"] << "preparing to score_flag for Player" << player_index << " for flag at flag_pos " << flag_pos << " to target_pos " << target_pos;

  // handle split_road / fake flag solutions
  if (!map->has_flag(flag_pos)) {
    AILogDebug["score_flag"] << "flag not found at flag_pos " << flag_pos << ", assuming this is a fake flag/split road solution and using closest adjacent flag + tile dist from fake flag";
    // fake_flags can't be scored normally because their SerfPathData isn't complete so the flag->get_other_end_dir used in flag-path finding doesn't work
    // instead, of the two flags adjacent to the fake flag, the best-scoring one will be used and then add 1 flag dist and actual traced tile dist between fake flag and that flag
    //    to get the correct score for evaluation
    // renaming the MapPos here to reduce confusion about what they mean
    MapPos splitting_flag_pos = flag_pos;
    MapPos adjacent_flag_pos;
    //unsigned int tile_dist = bad_score;
    //unsigned int lowest_tile_dist = bad_score;
    //unsigned int flag_dist = bad_score;
    //unsigned int lowest_flag_dist = bad_score;
    unsigned int best_adjacent_flag_adjusted_score = bad_score;
    MapPos best_adjacent_flag_pos = bad_map_pos;
    for (Direction dir : cycle_directions_cw()) {
      //AILogDebug["score_flag"] << "looking for a path in dir " << NameDirection[dir];
      if (!map->has_path_IMPROVED(splitting_flag_pos, dir)) {
        continue;
      }
      AILogDebug["score_flag"] << "found a path in dir " << NameDirection[dir] << ", tracing road";
      Road split_road = trace_existing_road(map, splitting_flag_pos, dir);
      adjacent_flag_pos = split_road.get_end(map.get());
      unsigned int tiles_to_adjacent_flag = static_cast<unsigned int>(split_road.get_length());
      AILogDebug["score_flag"] << "split road in dir " << NameDirection[dir] << " has length " << tiles_to_adjacent_flag << " to adjacent flag at pos " << adjacent_flag_pos;
      unsigned int tile_dist = bad_score;
      unsigned int flag_dist = bad_score;
      unsigned int adjusted_score = bad_score;
      bool contains_castle_flag = false;
      // go score the adjacent flags if they aren't already known (sometimes they will already have been checked)
      if (rb->has_score(adjacent_flag_pos)) {
        // simply record that a cache hit was found, to verify this functionality works
        AILogDebug["score_flag"] << "score_flag, found cached score for splitting-flag's adjacent_flag_pos " << adjacent_flag_pos;
        // this definitely works, I see it using the cache
      }else{
        AILogDebug["score_flag"] << "score_flag, rb doesn't have score yet for adjacent flag_pos " << adjacent_flag_pos << ", will try to score it";
        // USING NEW FUNCTION
        //if (find_flag_and_tile_dist(map, player_index, rb, adjacent_flag_pos, castle_flag_pos, ai_mark_pos)) {
        MapPosVector found_flags = {};
        unsigned int tile_dist = 0;
        if(find_flag_path_and_tile_dist_between_flags(map, adjacent_flag_pos, target_pos, &found_flags, &tile_dist, ai_mark_pos)){
          AILogDebug["score_flag"] << "score_flag, splitting_flag find_flag_path_and_tile_dist_between_flags() returned true from flag_pos " << adjacent_flag_pos << " to target_pos " << target_pos;
          // manually set rb stuff because the new function doesn't use it
          unsigned int flag_dist = found_flags.size();
          for(MapPos flag_pos : found_flags){
            if (flag_pos == castle_flag_pos){
              AILogDebug["score_flag"] << "this solution contains the castle flag!";
              contains_castle_flag = true;
              break;
            }
          }
          AILogDebug["score_flag"] << "score_flag, splitting_flag's adjacent_flag_pos " << adjacent_flag_pos << " has flag_dist " << flag_dist << ", tile_dist " << tile_dist << ".  setting this score in rb";
          rb->set_score(adjacent_flag_pos, flag_dist, tile_dist, contains_castle_flag);
        }
        else{
          AILogDebug["score_flag"] << "score_flag, splitting_flag find_flag_path_and_tile_dist_between_flags() returned false, cannot find flag-path solution from flag_pos " << adjacent_flag_pos << " to target_pos " << target_pos << ".  Returning false";
          // if not found just let rb->get_score call fail and it wlil use the default bad_scores given.
          AILogDebug["score_flag"] << "score_flag returned false for adjacent flag at pos " << adjacent_flag_pos;
          AILogDebug["score_flag"] << "for now, leaving default bogus super-high score for adjacent flag";
          //std::this_thread::sleep_for(std::chrono::milliseconds(0));
        }
      }
      FlagScore score = rb->get_score(adjacent_flag_pos);
      tile_dist = score.get_tile_dist();
      flag_dist = score.get_flag_dist();
      contains_castle_flag = score.get_contains_castle_flag();   // keep whatever value the adjacent flag has, it's flag-path could very well contain the castle flag
      AILogDebug["score_flag"] << "adjacent flag at " << adjacent_flag_pos << " has tile_dist " << tile_dist << ", flag dist " << flag_dist << ", contains_castle_flag " << contains_castle_flag;
      tile_dist += tiles_to_adjacent_flag;
      flag_dist += 1;
      AILogDebug["score_flag"] << "splitting_flag has tile_dist: " << tile_dist << ", flag_dist: " << flag_dist;
      adjusted_score = tile_dist + tiles_to_adjacent_flag + flag_dist;
      if (road_options.test(RoadOption::PenalizeCastleFlag) && contains_castle_flag == true) {
        if (target_pos == castle_flag_pos){
          AILogDebug["score_flag"] << "not applying contains_castle_flag penalty because the target *is* the castle flag pos!";
        }else{
          adjusted_score += contains_castle_flag_penalty;
          AILogDebug["score_flag"] << "applying contains_castle_flag penalty of " << contains_castle_flag_penalty;
        }
      }
      AILogDebug["score_flag"] << "adjacent flag at " << adjacent_flag_pos << " has adjusted_score " << adjusted_score;
      //rb->set_score(best_adjacent_flag_pos, tile_dist, flag_dist, contains_castle_flag);
      // use the score from whichever adjacent flag is better (because the game will use the best route serfs/resources)
      if (adjusted_score < best_adjacent_flag_adjusted_score) {
        AILogDebug["score_flag"] << "this score " << adjusted_score << " is better than best_adjacent_flag_adjusted_score of " << best_adjacent_flag_adjusted_score << ", setting splitting_flag_pos " << splitting_flag_pos << "'s score to that";
        best_adjacent_flag_pos = adjacent_flag_pos;
        best_adjacent_flag_adjusted_score = adjusted_score;
        rb->set_score(splitting_flag_pos, tile_dist, flag_dist, contains_castle_flag);
      }
      AILogDebug["score_flag"] << "best adjacent flag right now is best_adjacent_flag_pos " << best_adjacent_flag_pos << " with adjusted_score " << best_adjacent_flag_adjusted_score;
    }
    AILogDebug["score_flag"] << "done with flag flag/split road solution at flag_pos " << flag_pos << "'s adjacent_flag scoring";
    duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    AILogDebug["score_flag"] << "done score_flag, found solution, returning true, call took " << duration;
    return true;
  } // if split road / fake flag

  // handle direct road to target_pos - perfect score
  if (target_pos == flag_pos) {
    // this is a *direct route* to the target flag with no intermediate flags
    //    so the only scoring factor will be the new segment's tile length
    //     ??  need to add penalties??
    AILogDebug["score_flag"] << "score_flag, flag_pos *IS* target_pos, setting values 0,0";
    //ai_mark_pos->erase(flag_pos);
    //ai_mark_pos->insert(ColorDot(flag_pos, "coral"));
    //std::this_thread::sleep_for(std::chrono::milliseconds(0));
    // note that this blindly ignores if castle flag / area part of solution, FIX!
    AILogDebug["score_flag"] << "inserting perfect score 0,0 for target_pos flag at " << flag_pos << " into rb";
    rb->set_score(flag_pos, 0, 0, false);
    AILogDebug["score_flag"] << "score_flag, flag_pos *IS* target_pos, returning true";
    duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    AILogDebug["score_flag"] << "done score_flag, found solution, returning true, call took " << duration;
    return true;
  }

  // handle most common case, regular scoring
  // USING NEW FUNCTION
  //if (!find_flag_and_tile_dist(map, player_index, rb, flag_pos, castle_flag_pos, ai_mark_pos)) {
  //  AILogDebug["score_flag"] << "score_flag, find_flag_and_tile_dist() returned false, cannot find flag-path solution from flag_pos " << flag_pos << " to target_pos " << target_pos << ".  Returning false";
  //  return false;
  //}
  MapPosVector found_flags = {};
  unsigned int tile_dist = 0;
  bool contains_castle_flag = false;
  if(find_flag_path_and_tile_dist_between_flags(map, flag_pos, target_pos, &found_flags, &tile_dist, ai_mark_pos)){
    AILogDebug["score_flag"] << "score_flag, find_flag_path_and_tile_dist_between_flags() returned true from flag_pos " << flag_pos << " to target_pos " << target_pos;
    // manually set rb stuff because the new function doesn't use it
    unsigned int flag_dist = found_flags.size();
    for(MapPos flag_pos : found_flags){
      if (flag_pos == castle_flag_pos){
        AILogDebug["score_flag"] << "this solution contains the castle flag!";
        contains_castle_flag = true;
        break;
      }
    }
    AILogDebug["score_flag"] << "score_flag, setting score for flag_pos " << flag_pos << " to flag_dist " << flag_dist << ", tile_dist " << tile_dist;
    rb->set_score(flag_pos, flag_dist, tile_dist, contains_castle_flag);
  }else{
    AILogDebug["score_flag"] << "score_flag, find_flag_path_and_tile_dist_between_flags() returned false, cannot find flag-path solution from flag_pos " << flag_pos << " to target_pos " << target_pos << ".  Returning false";
    duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
    AILogDebug["score_flag"] << "done score_flag, did not find solution, returning false, call took " << duration;
    return false;
  }



  // debug only
  FlagScore score = rb->get_score(flag_pos);
  unsigned int debug_flag_dist = score.get_flag_dist();
  unsigned int debug_tile_dist = score.get_tile_dist();
  AILogDebug["score_flag"] << "score_flag, find_flag_path_and_tile_dist_between_flags() from flag_pos " << flag_pos << " to target_pos " << target_pos << " found flag_dist " << debug_flag_dist << ", tile_dist " << debug_tile_dist;

  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["score_flag"] << "done score_flag, found solution, returning true, call took " << duration;
  return true;
}


// build a RoadEnds object from a Road object by checking the first and
//    last dir, to determine the Direction leading back to each end
//    until another flag is found.  The start pos doesn't have to be a real flag,
//     which means fake flags will work
RoadEnds
AI::get_roadends(PMap map, Road road) {
  //AILogDebug["get_roadends"] << "inside get_roadends";
  std::list<Direction> dirs = road.get_dirs();
  std::list<Direction>::iterator i;
  //for (i = dirs.begin(); i != dirs.end(); i++) {
  //  Direction dir = *i;
  //  AILogVerbose["get_roadends"] << "get_roadends - Direction " << dir << " / " << NameDirection[dir];
  //}
  MapPos start_pos = road.get_source();
  MapPos end_pos = road.get_end(map.get());  // this function just traces the road along the existing path anyway
  Direction start_dir = road.get_dirs().front();
  // the Direction of the path leading back to the start is the reverse of the last dir in the road (the dir that leads to the end flag)
  Direction end_dir = reverse_direction(road.get_dirs().back());
  //AILogDebug["get_roadends"] << "inside get_roadends, start_pos " << start_pos << ", start_dir: " << start_dir << ", end_pos " << end_pos << ", end_dir: " << end_dir;
  //AILogDebug["get_roadends"] << "inside get_roadends, start_pos " << start_pos << ", start_dir: " << NameDirection[start_dir] << ", end_pos " << end_pos << ", end_dir: " << NameDirection[end_dir];
  RoadEnds ends = std::make_tuple(start_pos, start_dir, end_pos, end_dir);
  return ends;
}


// reverse a Road object - end_pos becomes start_pos, vice versa, and all directions inside reversed to match
Road
AI::reverse_road(PMap map, Road road) {
  //AILogDebug["reverse_road"] << "inside reverse_road, for a road with source " << road.get_source() << ", end " << road.get_end(map.get()) << ", and length " << road.get_length();
  Road reversed_road;
  reversed_road.start(road.get_end(map.get()));
  std::list<Direction> dirs = road.get_dirs();
  std::list<Direction>::reverse_iterator r;
  for (r = dirs.rbegin(); r != dirs.rend(); r++) {
    reversed_road.extend(reverse_direction(*r));
  }
  //AILogDebug["reverse_road"] << "returning reversed_road which has source " << reversed_road.get_source() << ", end " << reversed_road.get_end(map.get()) << ", and length " << reversed_road.get_length();
  return reversed_road;
}

// *********************************************
//  WAIT AGAIN!  I think flagsearch_node_less is correct, but something else is wrong
//  it is not using flag->length it is using node->flag_length which should be correct
// flag dist being set by me in the function
// *********************************************
//
// NOTE - understand why flagsearch_node_less does!  it chooses the flag with the lower flag->length value
//   BUT, this function actually does a more accurate check than flag->length, it counts exact tile dist
//  MAKE SURE THAT the flagsearch_node_less is replaced with a function that compares TILE DIST, and one
//   that correctly compares FLAG DIST in case it is being messed up by that
// here's the flagsearch_node_less function which returns a bool for the one that is higher/lower than the other:
//
// ALSO, it seems to be corrupting things... switching flags?   after it runs I am seeing flag->get_building->get_type return
//  the wrong building for the pos it is associated with!!!!!
//
/*
static bool
flagsearch_node_less(const PFlagSearchNode &left, const PFlagSearchNode &right) {
  // A search node is considered less than the other if
  // it has a *lower* flag distance.  This means that in the max-heap
// the shorter distance will go to the top
  return left->flag_dist < right->flag_dist;
}
*/
//


// return MapPosVector of the Inventory buildings nearest to the specified MapPos,
//  including castle, by straightline distance.  This allows "sharing" of a building by 
//  multiple Inventories if the building is not significantly closer to one (by some threshold)
// the purpose of this function is to allow tracking of military buildings that hold the
//  borders around a given Inventory (castle, warehouse/stock) so that they can be iterated
//  over.  Because it is not imperative that they map 1-to-1, overlap is allowed
MapPosVector
AI::find_nearest_inventories_to_military_building(MapPos pos) {
	// hardcoding this here for now, put it in some tuning vars later?
	unsigned int overlap_threshold = 8;  // 8 tiles
	AILogDebug["find_nearest_inventories_to_military_building"] << "inside find_nearest_inventories_to_military_building to pos " << pos << ", overlap_threshold " << overlap_threshold << ", currently selected inventory_pos is " << inventory_pos;
	MapPosVector closest_inventories = {};
	// get inventory distances by straight-line map distance only, ignoring roads, flags, obstacles, etc.
	unsigned int best_dist = bad_score;
	MapPosSet inventory_dists = {};
	for (MapPos inv_pos : stocks_pos) {
		unsigned int dist = AI::get_straightline_tile_dist(map, pos, inv_pos);
		AILogDebug["find_nearest_inventories_to_military_building"] << "straightline tile dist from pos " << pos << " to inventory_pos " << inv_pos << " is " << dist;
		inventory_dists.insert(std::make_pair(inv_pos, dist));
		if (dist < best_dist){
			AILogDebug["find_nearest_inventories_to_military_building"] << "inventory at inventory_pos " << inv_pos << " is the closest so far to pos " << pos << " , with dist " << dist;
			best_dist = dist;
		}
	}
	if (inventory_dists.size() == 0 || best_dist == bad_score) {
		AILogDebug["find_nearest_inventories_to_military_building"] << "no inventories found!  returning empty MapPosVector";
		return closest_inventories;
	}
	// create a vector of all inventories within the threshold of the best_dist
	for (std::pair<MapPos, unsigned int> pair : inventory_dists) {
    unsigned int inv_pos = pair.first;
		unsigned int dist = pair.second;
		if (dist < best_dist + overlap_threshold) {
			AILogDebug["find_nearest_inventories_to_military_building"] << "inventory at " << inv_pos << " has dist " << dist << ", which is within " << best_dist + overlap_threshold << " of pos " << pos << ", including in list";
			closest_inventories.push_back(inv_pos);
		}
	}
	AILogDebug["find_nearest_inventories_to_military_building"] << "done, closest_inventories list has " << std::to_string(closest_inventories.size()) << " items, best_dist is " << best_dist;
	return closest_inventories;
}



// replacing the original AI find_nearest_inventory() function with this one that has the same input/output of MapPos
//  instead of having to change all the calling code to use Flag* type or having all of the ugly
//  conversion code being done in the calling code
//
// return the MapPos of the Flag* of the Inventory (castle, stock/warehouse) that is the shortest
//  number of Flags away from the MapPos of the requested Flag or Building
//
//  this function accepts EITHER a Flag pos, OR a Building pos (for which it will check the flag)
//
//  - the Inventory must be accepting Resources
//  - no consideration for if the Inventory is accepting Serfs
//  ----- CHANGED ---------- no consideration of tile/straightline distance, only Flag distance matters-------
//      as of jan31 2021, now only returning true if Inventory is closest by BOTH flag and straightline dist
//       NEED TO ENSURE THIS DOESN'T BREAK THINGS THAT REQUIRE THIS FUNCTION TO FIND AN INVENTORY!
//       It is likely to fail a lot as both conditions might not be true for many Flags
//  - no check to see if transporters/sailors are already in place along the path
//
// the intention of this function is to help "pair" most buildings in the AI player's realm to a
// given Inventory so that each Inventory can have accurate counts and limits of the buildings
// "paired" with that Inventory. The reason that straightline distance cannot be used (as it 
//  originally was) is that only Flag distance determines which Inventory a resource-producing
//  -building will send non-directly-routable resources to (i.e. ones piling up in storage)
//  If straightline-dist is used when determining nearest Inventory, but flag distance is used when
//   serfs transport resources to Inventories, the Inventory that receives the resouce may not be 
//   the Inventory "paired" with the building, and so the "stop XXX when resource limit reached"
//   checks would never be triggered because the check is running against one Inventory while
//   some other Inventory is actually piling up resources and may have already passed its limit.
//
//  the AI does not currently change these stock accept/reject/purge resource/serf settings so
//   I do not think these should introduce any issues, unless a human player modifies the settings
//    of an AI player's game?  Not sure if it matters much
//
//
// this will return bad_map_pos if there is no Flag at the requested pos
//  AND no Building at the requested pos

// this is now a stub that calls the two underlying functions depending on what DistType is set
MapPos
AI::find_nearest_inventory(PMap map, unsigned int player_index, MapPos pos, DistType dist_type, ColorDotMap *ai_mark_pos) {
  AILogDebug["util_find_nearest_inventory"] << "inside find_nearest_inventory to pos " << pos << " with dist_type " << NameDistType[dist_type];
  if (pos == bad_map_pos){
    AILogWarn["util_find_nearest_inventory"] << "the called pos is a bad_map_pos!  returning bad_map_pos";
    return bad_map_pos;
  }
  if (!map->has_flag(pos) && !map->has_building(pos)) {
		AILogWarn["util_find_nearest_inventory"] << "no flag or building found at pos " << pos << " as expected!  Cannot run search, returning bad_map_pos";
		return bad_map_pos;
	}
	MapPos flag_pos = bad_map_pos;
	if (map->has_building(pos)) {
		flag_pos = map->move_down_right(pos);
		AILogDebug["util_find_nearest_inventory"] << "request pos " << pos << " is a building pos, using its flag pos " << flag_pos << " instead";
	}
	else {
		flag_pos = pos;
	}
	Flag *flag = game->get_flag_at_pos(flag_pos);
	if (flag == nullptr) {
		AILogWarn["util_find_nearest_inventory"] << "got nullptr for Flag at flag_pos " << flag_pos << "!  Cannot run search, returning bad_map_pos";
		return bad_map_pos;
	}

  MapPos by_straightline_dist = bad_map_pos;
  if (dist_type == DistType::StraightLineOnly || dist_type == DistType::FlagAndStraightLine){
    by_straightline_dist = find_nearest_inventory_by_straightline(map, player_index, flag_pos, ai_mark_pos);
    if (by_straightline_dist == bad_map_pos){
      AILogDebug["util_find_nearest_inventory"] << "got bad_map_pos for by_straightline_dist, can't continue, returning bad_map_pos";
      return bad_map_pos;
    }
  }

  MapPos by_flag_dist = bad_map_pos;
  if (dist_type == DistType::FlagOnly || dist_type == DistType::FlagAndStraightLine){
    by_flag_dist = find_nearest_inventory_by_flag(map, player_index, flag_pos, ai_mark_pos);
    if (by_flag_dist == bad_map_pos){
      AILogDebug["util_find_nearest_inventory"] << "got bad_map_pos for by_flag_dist, can't continue, returning bad_map_pos";
      return bad_map_pos;
    }
  }

  if (dist_type == DistType::StraightLineOnly){
    AILogDebug["util_find_nearest_inventory"] << "returning nearest Inventory pos by_straightline_dist: " << by_straightline_dist;
    return by_straightline_dist;
  }

  if (dist_type == DistType::FlagOnly){
    AILogDebug["util_find_nearest_inventory"] << "returning nearest Inventory pos by_flag_dist: " << by_flag_dist;
    return by_flag_dist;
  }

  // what purpose does this have???  I can no longer remember why this combination is important
  // maybe it was to try to avoid buildings being "stolen" or moved between inventory "owners" 
  // because of straight-line stuff and/or road changes?  
  // I changed some places that used this to FlagOnly, see if any remain and reconsider if
  // this is really needed, I might be making a mistake by not using it now
  //
  // so the only place this seems appropriate is for helping ensure buildings being placed
  //  are located near their Inv so they are less likely to be "stolen" by other Invs.  I only
  //  see this being used by the food buildings function, not sure if it is redundant or if
  //  the other do_build functions should be using it too
  if (dist_type == DistType::FlagAndStraightLine){
    if (by_flag_dist != by_straightline_dist){
      AILogDebug["util_find_nearest_inventory"] << "nearest Inventory to flag_pos " << flag_pos << " by_flag_dist is " << by_flag_dist << ", but by_straightline_dist is " << by_straightline_dist << ", returning bad_map_pos";
      return bad_map_pos;
    }else{
      AILogDebug["util_find_nearest_inventory"] << "returning both matching pos " << by_straightline_dist;
      return by_straightline_dist;  // or could return by_flag_dist, same result
    }
  }
  AILogError["util_find_nearest_inventory"] << "this should never happen, returning bad_map_pos";
  return bad_map_pos;
}

MapPos
AI::find_nearest_inventory_by_straightline(PMap map, unsigned int player_index, MapPos flag_pos, ColorDotMap *ai_mark_pos) {
  AILogDebug["find_nearest_inventory_by_straightline"] << "inside find_nearest_inventory_by_straightline for flag_pos " << flag_pos;
  unsigned int shortest_dist = bad_score;
  MapPos closest_inv = bad_map_pos;
  Game::ListBuildings buildings = game->get_player_buildings(player);
  for (Building *building : buildings) {
    if (building == nullptr)
      continue;
    if (building->is_burning())
      continue;
    if (building->get_type() != Building::TypeCastle && building->get_type() != Building::TypeStock)
      continue;
    Flag *building_flag = game->get_flag(building->get_flag_index());
    if (building_flag == nullptr)
      continue;
    if (!building_flag->accepts_resources())
      continue;
    MapPos building_flag_pos = building_flag->get_position();
    unsigned int dist = (unsigned)get_straightline_tile_dist(map, flag_pos, building_flag_pos);
    if (dist >= shortest_dist)
      continue;
    AILogDebug["find_nearest_inventory_by_straightline"] << "SO FAR, the closest Inventory building to flag_pos " << flag_pos << " found at " << building_flag_pos;
    shortest_dist = dist;
    closest_inv = building_flag_pos;
    continue;
  }
  if (closest_inv != bad_map_pos){
    AILogDebug["find_nearest_inventory_by_straightline"] << "closest Inventory building to flag_pos " << flag_pos << " found at " << closest_inv;
  }else{
    AILogWarn["find_nearest_inventory_by_straightline"] << "closest Inventory building to flag_pos " << flag_pos << " did not find ANY valid Inventory building - was Castle and all Stocks destroyed???";
  }
  return closest_inv;
}


MapPos
AI::find_nearest_inventory_by_flag(PMap map, unsigned int player_index, MapPos flag_pos, ColorDotMap *ai_mark_pos) {
	AILogDebug["find_nearest_inventory_by_flag"] << "inside find_nearest_inventory_by_flag to flag_pos " << flag_pos;
	// now that this function uses the FlagSearchNode search instead of "internal" flag search
	//  it should probably be switched to operate on Flag* objects instead of MapPos
	//  to avoid so many MapPos<>Flag* conversions
	std::vector<PFlagSearchNode> open;
	std::list<PFlagSearchNode> closed;
	PFlagSearchNode fnode(new FlagSearchNode);
	fnode->pos = flag_pos;
	unsigned int flag_dist = 0;
	unsigned int tile_dist = 0;
	open.push_back(fnode);
	//AILogVerbose["find_nearest_inventory_by_flag"] << "fsearchnode - starting fnode search for flag_pos " << flag_pos;
	while (!open.empty()) {
		std::pop_heap(open.begin(), open.end(), flagsearch_node_less);
		fnode = open.back();
		open.pop_back();
    //AILogDebug["find_nearest_inventory_by_flag"] << "fsearchnode - inside fnode search for flag_pos " << flag_pos << ", inside while-open-list-not-empty loop";

    if (game->get_flag_at_pos(fnode->pos) == nullptr){
      AILogWarn["find_nearest_inventory_by_flag"] << "fnode's flag at fnode->pos is nullptr!  skipping";
      continue;
    }
    // got a segfault here for the first time jan04 2022, adding nullptr check (above)
    if (game->get_flag_at_pos(fnode->pos)->accepts_resources()) {
      // to avoid crashes, handle discovering a newly built warehouse that just now became active
      //  after the most recent update_stocks run, and doesn't exist in stocks_pos yet
      if (stock_building_counts.count(fnode->pos) == 0){
        //update_stocks_pos();
        // hmm this seems like a bad place to put this.. for now just skip this Inventory
        //  and let the next AI loop find it
        AILogDebug["find_nearest_inventory_by_flag"] << "found a newly active Inventory building at " << fnode->pos << " that is not tracked yet, skipping it for now.";
      }else{
        // an Inventory building's flag reached, solution found
        AILogDebug["find_nearest_inventory_by_flag"] << "flagsearch complete, found solution from flag_pos " << flag_pos << " to an Inventory building's flag";

        // NONE of this backtracking is required for this type of search because all that matters is the dest pos, NOT the path to reach it
        // HOWEVER, if we want to track the flag_dist to the Inventory all that needs to be done is:
        // ***********************************
        //		while (fnode->parent){
        //			fnode = fnode->parent;
        //			flag_dist++;
        //		}
        // ***********************************
        //while (fnode->parent) {
          //MapPos end1 = fnode->parent->pos;
          //MapPos end2 = bad_map_pos;
          //Direction dir1 = fnode->parent->dir;  // DONT USE ->dir, instead use child_dir[d] !!!
          //Direction dir2 = DirectionNone;
          // is this needed?
          //existing_road = trace_existing_road(map, end1, dir1);
          //end2 = existing_road.get_end(map.get());
          //fnode = fnode->parent;
          //flag_dist++;
        //}

        AILogDebug["find_nearest_inventory_by_flag"] << "done find_nearest_inventory_by_flag, found solution, returning Inventory flag pos " << fnode->pos;
        //return true;
        // this needs to return the MapPos of the inventory flag (update it later to get rid of all these conversions)
        return fnode->pos;
      }
		}

    //AILogDebug["find_nearest_inventory_by_flag"] << "fsearchnode - fnode->pos " << fnode->pos << " is not at an Inventory building flag yet, adding fnode to closed list";
		closed.push_front(fnode);

		// for each direction that has a path, trace the path until a flag is reached
		for (Direction d : cycle_directions_cw()) {
			if (map->has_path_IMPROVED(fnode->pos, d)) {
				// couldn't this use the internal flag->other_end_dir stuff instead of tracing it?
				// maybe...  try that after this is stable
				Road fsearch_road = trace_existing_road(map, fnode->pos, d);
				MapPos new_pos = fsearch_road.get_end(map.get());
				//AILogDebug["find_nearest_inventory_by_flag"] << "fsearchnode - fsearch from fnode->pos " << fnode->pos << " and dir " << NameDirection[d] << " found flag at pos " << new_pos << " with return dir " << reverse_direction(fsearch_road.get_last());
				// check if this flag is already in closed list
				bool in_closed = false;
				for (PFlagSearchNode closed_node : closed) {
					if (closed_node->pos == new_pos) {
						in_closed = true;
            //AILogDebug["find_nearest_inventory_by_flag"] << "fsearchnode - fnode at new_pos " << new_pos << ", breaking because in fnode already in_closed";
						break;
					}
				}
				if (in_closed) continue;

        //AILogDebug["find_nearest_inventory_by_flag"] << "fsearchnode - fnode at new_pos " << new_pos << ", continuing because fnode NOT already in_closed";

				// check if this flag is already in open list
				bool in_open = false;
				for (std::vector<PFlagSearchNode>::iterator it = open.begin(); it != open.end(); ++it) {
					PFlagSearchNode n = *it;
					if (n->pos == new_pos) {
						in_open = true;
            //AILogDebug["find_nearest_inventory_by_flag"] << "fnodesearch - fnode at new_pos " << new_pos << " is already in_open ";
						if (n->flag_dist >= fnode->flag_dist + 1) {
              //AILogDebug["find_nearest_inventory_by_flag"] << "fnodesearch - new_pos " << new_pos << "'s flag_dist is >= fnode->flag_dist + 1";
							n->flag_dist += 1;
							n->parent = fnode;
							iter_swap(it, open.rbegin());
							std::make_heap(open.begin(), open.end(), flagsearch_node_less);
						}
						break;
					}
				}
				// this pos has not been seen before, create a new fnode for it
        //
        // SHOULD WE NOT CHECK FOR THE SUCCESS CONDITION (found inventory) AND QUIT IF SO???  OR AT LEAST BREAK EARLY
        //   maybe, but that could screw up the whole in_open  and flagsearch_node_less compare stuff?
				if (!in_open) {
          //AILogDebug["find_nearest_inventory_by_flag"] << "fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, creating a new fnode ";
					PFlagSearchNode new_fnode(new FlagSearchNode);
					new_fnode->pos = new_pos;
					new_fnode->parent = fnode;
					new_fnode->flag_dist = new_fnode->parent->flag_dist + 1;
					// when doing a flag search, the parent node's direction *cannot* be inferred by reversing its child node dir
					//   like when doing tile pathfinding, so it must be set explicitly set. it will be reused and reset into each explored direction
					//   but that should be okay because once an end solution is found the other directions no longer matter
					//fnode->dir = d;
          fnode->child_dir[d] = new_pos;
					open.push_back(new_fnode);
					std::push_heap(open.begin(), open.end(), flagsearch_node_less);
					//AILogVerbose["find_nearest_inventory_by_flag"] << "fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, DONE CREATING new fnode ";
				} // if !in_open
				//AILogVerbose["find_nearest_inventory_by_flag"] << "fnodesearch end of if(map->has_path_IMPROVED)";
			} // if map->has_path_IMPROVED(node->pos, d)
			//AILogVerbose["find_nearest_inventory_by_flag"] << "fnodesearch end of if dir has path Direction - did I find it??";
		} // foreach direction
		//AILogVerbose["find_nearest_inventory_by_flag"] << "fnodesearch end of foreach Direction cycle_dirs";
	} // while open flag-nodes remain to be checked
	//AILogVerbose["find_nearest_inventory_by_flag"] << "fnodesearch end of while open flag-nodes remain";

	// if the search ended it means nothing was found, return bad_map_pos
	AILogDebug["find_nearest_inventory_by_flag"] << "no flag-path solution found from flag_pos " << flag_pos << " to an Inventory building's flag.  returning false";
	return bad_map_pos;
}


// For every flag in this realm
//  - do a flagsearch starting from the flag to the nearest Inventory
//  - trace the flag-path between the starting flag and Inventory flag
//     and note the last Direction into the flag
//     and increment the "found" counter for each flag pos in the path
//  - keep a separate flag score count for each Dir into the Inventory flag
//  the resulting numbers should indicate the flags that are seen most often, meaning
//  they are part of the aterial road for that Inventory Flag-Direction
// Also
//  - store this overall Road (even though it passes through multiple flags - a Road is just an ordered array of Dirs
//    and maybe also store the Flag path taken?
//  - build a list of separate road networks to identify
//     road segments that are disconnected from rest of the road system[s]
//
// would it make more sense to have each solution just walk backwards and add + to the flag as it retraces?
//  rather than just adding +1 to every flag touched?
//  yes I think so! trying that instead
void
AI::identify_arterial_roads(PMap map){
  /*

  IMPORTANT - I suspect there is a significant bug in all of my Flagsearch logic
   When the flagsearch_node_less re-orders the list, it breaks the relationship
   between a fnode->dir parent and its child fnode, because the fnode->dir is
   re-used constantly.  At first I was thinking the child should store the dir
   that the parent reached it from, but I think that could also change(?)
   to be safe and clear, 
    //  MAYBE - faster?? change the flagsearch node stuff to use actual Flag
     objects, and the flag->get_other_end_dir //
   store the flag_pos in each valid Direction on a parent fnode, instead
   of only a single direction

  Once this is solved... go back and see if this bug exists on the existing code I wrote

  nov 2021 - might have fixed this when doing two things, 1) reversed the flagsearch_node_less
    and 2) added assigning fnode->dir = d earlier in those functions in case it quits early 
    there won't be child nodes whose parent dir is -1 DirectionNone (i.e. null default)

  */

  AILogDebug["util_identify_arterial_roads"] << "inside AI::identify_arterial_roads";
  // time this function for debugging
  std::clock_t start = std::clock();
  double duration;

  // store the number of times each flag appears in the best path to the
  //  Inventory approach in this Direction
  // key MapPos           - of Inventory flag
  //  key Direction       - of path into Inventory flag
  //   key MapPos         - of Flag being counted
  //    val unsigned int  - number of times Flag appears in this InvFlag-Path combination
  std::map<MapPos,std::map<Direction,std::map<MapPos, unsigned int>>> flag_counts = {};

  Flags flags_copy = *(game->get_flags());  // create a copy so we don't conflict with the game thread, and don't want to mutex lock for a long function
  for (Flag *flag : flags_copy) {
    if (flag == nullptr)
      continue;
    if (flag->get_owner() != player_index)
      continue;
    if (!flag->is_connected())
      continue;
    // don't do searches on Inventory flags (castle, warehouse/stocks)
    if (flag->accepts_resources())
      continue;
    MapPos flag_pos = flag->get_position();
    //AILogDebug["util_identify_arterial_roads"] << "checking flag at pos " << flag_pos;

    std::vector<PFlagSearchNode> open = {};
    std::list<PFlagSearchNode> closed = {};
    PFlagSearchNode fnode(new FlagSearchNode);
    
    fnode->pos = flag_pos;
    open.push_back(fnode);
    std::push_heap(open.begin(), open.end(), flagsearch_node_less);
    int flag_dist = 0;
    bool found_inv = false;

    while (!open.empty()) {
      std::pop_heap(open.begin(), open.end(), flagsearch_node_less);
      fnode = open.back();
      open.pop_back();
      //AILogDebug["util_identify_arterial_roads"] << "fsearchnode - inside fnode search for flag_pos " << flag_pos << ", inside while-open-list-not-empty loop";

      if (game->get_flag_at_pos(fnode->pos)->accepts_resources()) {
        // to avoid crashes, handle discovering a newly built warehouse that just now became active
        //  after the most recent update_stocks run, and doesn't exist in stocks_pos yet
        if (stock_building_counts.count(fnode->pos) == 0){
          //update_stocks_pos();
          // hmm this seems like a bad place to put this.. for now just skip this Inventory
          //  and let the next AI loop find it
          AILogDebug["util_identify_arterial_roads"] << "found a newly active Inventory building at " << fnode->pos << " that is not tracked yet, skipping it for now.";
        }else{
          //AILogDebug["util_identify_arterial_roads"] << "flagsearch solution found from flag_pos " << flag_pos << " to an Inventory building's flag";
          found_inv = true;
          // uniquely identify the connection point of each artery to the Flag-Dir it is coming from
          // this is the dir of 2nd last node, which leads to the last node (which has no dir)
          if (fnode->parent == nullptr){
            AILogDebug["util_identify_arterial_roads"] << "no parent flag node found!  this must be the Inventory pos, breaking";
            break;
          }
          MapPos inv_flag_pos = fnode->pos;
          //AILogDebug["util_identify_arterial_roads"] << "debug:::::::::   inv_flag_pos = " << inv_flag_pos << " when recording flag dirs with paths from inv";
          /* wait, this doesn't even matter
            at this point we don't actually need to track the dirs between flags
            as we can just check the dir between the last two nodes
            THOUGH I guess it is possible that there could be multiple direct
            connections from the second to last flag to the end Inv flag, but
            if so the end result would be pretty much the same, right? so what
            if the very last flag segment is wrong or random*/
          // NEVERMIND, keeping it this way for now
          Direction inv_flag_conn_dir = DirectionNone;
          for (Direction d : cycle_directions_ccw()){
            if (fnode->parent->child_dir[d] == fnode->pos){
              // this is the dir from the Flag of 2nd-to-last to Inv pos, 
              //  NOT the dir at the very last tile/path to Inv pos!
              //inv_flag_conn_dir = d;
              // get the actual connection dir on the Inv flag side
              //  by trusting that the Flag->get_other_end_dir works
              //AILogDebug["util_identify_arterial_roads"] << "the 2nd-to-last flag at pos " << fnode->parent->pos << " has path TO inv_flag_pos " << inv_flag_pos << " in dir " << NameDirection[d] << " / " << d;
              inv_flag_conn_dir = game->get_flag_at_pos(fnode->parent->pos)->get_other_end_dir(d);
            }
          }
          //
          // NOTE - I started seeing this error appear after I "fixed" the flagsearch_node_less function
          //  where I reversed the ordering so it does breadth-frist instead of depth-first
          //  there is probably a bug in this code, or in the flagsearch logic, but because I am not actually using
          //  arterial roads for anything yet I will simply disable it for now and figure the bug out later
          // MIGHT BE FIXED NOW - renabled and testing
          // nope still broken, trying same fix as used elsewhere, setting fnode->dir = d earlier
          // wait that isn't even done here, this function is written differently.  look into why, one of them is probably wrong
          //
          // UPDATE Dec04 2022, I haven't seen this in a long time, disabling sleep
          if (inv_flag_conn_dir == DirectionNone){
            AILogError["util_identify_arterial_roads"] << "could not find the Dir from fnode->parent->pos " << fnode->parent->pos << " to child fnode->pos " << fnode->pos << "! throwing exception";
            //std::this_thread::sleep_for(std::chrono::milliseconds(120000));
            throw ExceptionFreeserf("in AI::util_identify_arterial_roads, could not find the Dir from fnode->parent->pos to child !fnode->pos!  it should be known");
          }
          // actually, I take it back, tracking it the entire way is fine
          //  if the alternative is checking 6 dirs and potentially buggy
          //Direction inv_flag_conn_dir = trace...
          //AILogDebug["util_identify_arterial_roads"] << "reached an Inventory building's flag at pos " << inv_flag_pos << ", connecting from dir " << NameDirection[inv_flag_conn_dir] << " / " << inv_flag_conn_dir;
          while (fnode->parent){
            //AILogDebug["util_identify_arterial_roads"] << "fnode->pos = " << fnode->pos << ", fnode->parent->pos " << fnode->parent->pos << ", fnode->parent->dir = " << NameDirection[fnode->parent->dir] << " / " << fnode->parent->dir;
            fnode = fnode->parent;
            flag_counts[inv_flag_pos][inv_flag_conn_dir][fnode->pos]++;
            flag_dist++;
          }
          //AILogDebug["util_identify_arterial_roads"] << "flag_dist from flag_pos " << flag_pos << " to nearest Inventory pos " << inv_flag_pos << " is " << flag_dist;
          break;
        }
      }

      //AILogDebug["util_identify_arterial_roads"] << "fsearchnode - fnode->pos " << fnode->pos << " is not at an Inventory building flag yet, adding fnode to closed list";
      closed.push_front(fnode);

      // for each direction that has a path, trace the path until a flag is reached
      for (Direction d : cycle_directions_cw()) {
        if (map->has_path_IMPROVED(fnode->pos, d)) {
          // couldn't this use the internal flag->other_end_dir stuff instead of tracing it?
          // maybe...  try that after this is stable
          Road fsearch_road = trace_existing_road(map, fnode->pos, d);
          MapPos new_pos = fsearch_road.get_end(map.get());
          //AILogDebug["util_identify_arterial_roads"] << "fsearchnode - fsearch from fnode->pos " << fnode->pos << " and dir " << NameDirection[d] << " found flag at pos " << new_pos << " with return dir " << reverse_direction(fsearch_road.get_last());
          // check if this flag is already in closed list
          bool in_closed = false;
          for (PFlagSearchNode closed_node : closed) {
            if (closed_node->pos == new_pos) {
              in_closed = true;
              //AILogDebug["util_identify_arterial_roads"] << "fsearchnode - fnode at new_pos " << new_pos << ", breaking because in fnode already in_closed";
              break;
            }
          }
          if (in_closed) continue;

          //AILogDebug["util_identify_arterial_roads"] << "fsearchnode - fnode at new_pos " << new_pos << ", continuing because fnode NOT already in_closed";

          // check if this flag is already in open list
          bool in_open = false;
          for (std::vector<PFlagSearchNode>::iterator it = open.begin(); it != open.end(); ++it) {
            PFlagSearchNode n = *it;
            if (n->pos == new_pos) {
              in_open = true;
              //AILogDebug["util_identify_arterial_roads"] << "fnodesearch - fnode at new_pos " << new_pos << " is already in_open ";
              if (n->flag_dist >= fnode->flag_dist + 1) {
                AILogDebug["util_identify_arterial_roads"] << "fnodesearch - new_pos " << new_pos << "'s flag_dist is >= fnode->flag_dist + 1";
                n->flag_dist += 1;
                n->parent = fnode;
                iter_swap(it, open.rbegin());
                std::make_heap(open.begin(), open.end(), flagsearch_node_less);
              }
              break;
            }
          }
          // this pos has not been seen before, create a new fnode for it
          //
          // SHOULD WE NOT CHECK FOR THE SUCCESS CONDITION (found inventory) AND QUIT IF SO???  OR AT LEAST BREAK EARLY
          //   maybe, but that could screw up the whole in_open  and flagsearch_node_less compare stuff?
          if (!in_open) {
            //AILogDebug["util_identify_arterial_roads"] << "fnodesearch - fnode at new_pos " << new_pos << " is NOT already in_open, creating a new fnode ";
            PFlagSearchNode new_fnode(new FlagSearchNode);
            new_fnode->pos = new_pos;
            new_fnode->parent = fnode;
            new_fnode->flag_dist = new_fnode->parent->flag_dist + 1;
            // when doing a flag search, the parent node's direction *cannot* be inferred by reversing its child node dir
            //   like when doing tile pathfinding, so it must be set explicitly set
            fnode->child_dir[d] = new_pos; // wait WHY IS THIS DONE DIFFERENTLY THAN IN MY OTHER SEARCHES?  one is likely wrong!
            open.push_back(new_fnode);
            std::push_heap(open.begin(), open.end(), flagsearch_node_less);
          } // if !in_open
        } // if map->has_path_IMPROVED(node->pos, d)
      } // foreach direction
    } // while open flag-nodes remain to be checked

    // if the search ended it means no other Inventory building was found, so this road is likely not connected to the main
    //  road system.  That might be okay
    if (!found_inv)
      AILogDebug["util_identify_arterial_roads"] << "flagsearch never completed, flag at pos " << flag_pos << " is not connected to any valid Inventory!";

  } // foreach Flag pos

  // dump the entire search results
  for (std::pair<MapPos,std::map<Direction,std::map<MapPos, unsigned int>>>  inv_pair : flag_counts){
    MapPos inv_flag_pos = inv_pair.first;
    AILogDebug["util_identify_arterial_roads"] << "DUMP inv_pos " << inv_flag_pos;
    for (std::pair<Direction,std::map<MapPos, unsigned int>> dir_pair : inv_pair.second){
      Direction dir = dir_pair.first;
      AILogDebug["util_identify_arterial_roads"] << "DUMP         dir " << NameDirection[dir] << " / " << dir;
      for (std::pair<MapPos, unsigned int> flag_count_pair : dir_pair.second){
        MapPos flag_pos = flag_count_pair.first;
        unsigned int count = flag_count_pair.second;
        AILogDebug["util_identify_arterial_roads"] << "DUMP                 flag_pos " << flag_pos << " seen " << count << " times";
      }
    }
  }

  // record the arterial flags
  for (std::pair<MapPos,std::map<Direction,std::map<MapPos, unsigned int>>>  inv_pair : flag_counts){
    MapPos inv_flag_pos = inv_pair.first;
    AILogDebug["util_identify_arterial_roads"] << "MEDIAN inv_flag_pos " << inv_flag_pos;
    for (std::pair<Direction,std::map<MapPos, unsigned int>> dir_pair : inv_pair.second){
      Direction dir = dir_pair.first;
      AILogDebug["util_identify_arterial_roads"] << "MEDIAN         dir " << NameDirection[dir] << " / " << dir;

      // find the median "number of times a flag appears in the Inventory flag's road network in this direction"
      //  and use this as the cutoff for a flag to be "arterial"
      std::vector<unsigned int> counts = {};
      for (std::pair<MapPos, unsigned int> flag_count_pair : dir_pair.second){
        MapPos flag_pos = flag_count_pair.first;
        unsigned int count = flag_count_pair.second;
        counts.push_back(count);
        AILogDebug["util_identify_arterial_roads"] << "MEDIAN                 flag_pos " << flag_pos << " count " << count;
      }
      sort(counts.begin(), counts.end());
      size_t size = counts.size();
      size_t median = 0;
      // middle record is median value
      //median = counts[size / 2];
      // changing this to 70th percentile
      median = counts[size * 0.7];
      AILogDebug["util_identify_arterial_roads"] << "the median count " << median;

      // build MapPosVector of all flags that are > median, this is the arterial flag-path 
      //  in this direction from this Inventory
      MapPosVector art_flags = {};
      for (std::pair<MapPos, unsigned int> flag_count_pair : dir_pair.second){
        MapPos flag_pos = flag_count_pair.first;
        unsigned int count = flag_count_pair.second;
        if (count > median){
          AILogDebug["util_identify_arterial_roads"] << "MEDIAN                 flag_pos " << flag_pos << " count " << count << " is above median, including";
          art_flags.push_back(flag_pos);
        }
      }
      //    key pos/dir -> val unordered(?) list of flags, assume starting at the Inventory
      //    typedef std::map<std::pair<MapPos, Direction>, MapPosVector> FlagDirToFlagVectorMap;
      ai_mark_arterial_road_flags->insert(std::make_pair(std::make_pair(inv_flag_pos, dir), art_flags));
      //    key pos/dir -> val ordered list of flag-dir pairs, assume starting at the Inventory
      //    typedef std::map<std::pair<MapPos, Direction>, std::vector<std::pair<MapPos,Direction>>> FlagDirToFlagDirVectorMap;
      //ai_mark_arterial_road_pairs->insert(std::make_pair(std::make_pair(inv_flag_pos, dir), new std::vector<std::pair<MapPos,Direction>>));
      std::vector<std::pair<MapPos,Direction>> foo = {};
      ai_mark_arterial_road_pairs->insert(std::make_pair(std::make_pair(inv_flag_pos, dir), foo));

    } // foreach dir_pair : inv_pair.second
  } // foreach inv_pair : flag_counts

  // record the flag->Dir combinations so they can be traced along tile-paths
  //  and highlighted inside Viewport
  // to do this, run a special depth-first flagsearch from the inv_pos until
  //  all arterial flags reached and mark the Dir from each parent to child
  // WHAT ABOUT CIRCULAR PATHS??  I dunno, let's see what happens
  for (std::pair<std::pair<MapPos, Direction>, MapPosVector> record : *(ai_mark_arterial_road_flags)){
    std::pair<MapPos, Direction> flag_dir = record.first;
    MapPos inv_flag_pos = flag_dir.first;
    Direction dir = flag_dir.second;
    AILogDebug["util_identify_arterial_roads"] << "begin retracing solutions - Inventory at pos " << inv_flag_pos << " has a path in Dir " << NameDirection[dir] << " / " << dir;

    Flag *inv_flag = game->get_flag_at_pos(inv_flag_pos);
    if (inv_flag == nullptr)
      continue;
    if (inv_flag->get_owner() != player_index)
      continue;
    if (!inv_flag->is_connected())
      continue;

    AILogDebug["util_identify_arterial_roads"] << "retracing solutions for inv_flag_pos " << inv_flag_pos << " dir " << NameDirection[dir] << " / " << dir << " - starting flagsearch from inv_flag_pos " << inv_flag_pos;
    MapPosVector closed = {};
    MapPosVector *closed_ptr = &closed;
    int depth = 0;
    int *depth_ptr = &depth;
    arterial_road_depth_first_recursive_flagsearch(inv_flag_pos, flag_dir, closed_ptr, depth_ptr);

    AILogDebug["util_identify_arterial_roads"] << "done retracing solutions - Inventory at pos " << inv_flag_pos << " has a path in Dir " << NameDirection[dir] << " / " << dir;
  }

  duration = (std::clock() - start) / static_cast<double>(CLOCKS_PER_SEC);
  AILogDebug["util_identify_arterial_roads"] << "done AI::identify_arterial_roads, call took " << duration;
}

//  instead of retracing backwards from last node to parent node as is usual,
//    start at oldest node and trace downwards to each child until it dead ends
//    using a depth-first recursive walk
//     and record the Set as parent->child flag pos  so each line only appears once
//   otherwise, if we did something like this:
//  for each arterial flag (start at Inventory) 
//    foreach cycle_dirs
//      if flag has path in dir that ends at another arterial flag
//         draw the line
//  .. this would result in drawing lines in both directions for each flag
void
AI::arterial_road_depth_first_recursive_flagsearch(MapPos flag_pos, std::pair<MapPos,Direction> inv_dir, MapPosVector *closed, int *depth){
  (*depth)++;
  //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "starting with flag_pos " << flag_pos << ", recusion depth " << *(depth);
  MapPosVector arterial_flags = ai_mark_arterial_road_flags->at(inv_dir);
  //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo0";
  // find the Dir from this flag to the next flag and see if it is an arterial flag
  for (Direction dir : cycle_directions_cw()){
    //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo1, dir " << dir;
    if (!map->has_path_IMPROVED(flag_pos, dir))
      continue;
    if (!map->has_flag(flag_pos)){
      AILogError["arterial_road_depth_first_recursive_flagsearch"] << "expecting that start_pos provided to this function is a flag pos, but game->get_flag_at_pos(" << flag_pos << ") returned nullptr!  throwing exception";
      throw ExceptionFreeserf("expecting that start_pos provided to this function is a flag pos, but game->get_flag_at_pos(flag_pos)!");
    }
    Flag *flag = game->get_flag_at_pos(flag_pos);
    Flag *other_end_flag = flag->get_other_end_flag(dir);
    if (other_end_flag == nullptr){
      AILogWarn["arterial_road_depth_first_recursive_flagsearch"] << "got nullptr for game->get_other_end_flag(" << NameDirection[dir] << ") from flag at pos " << flag_pos << " skipping this dir";
      continue;
    }
    //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo2";
    MapPos other_end_flag_pos = other_end_flag->get_position();
    // if other_end_flag_pos is on the arterial flag list for this Inventory-Dir
    //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo3";
    if (std::find(arterial_flags.begin(), arterial_flags.end(), other_end_flag_pos) != arterial_flags.end()){
      //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo4";
      if (std::find(closed->begin(), closed->end(), other_end_flag_pos) == closed->end()){
        //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo5+ closing pos";
        closed->push_back(other_end_flag_pos);
        //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo5+ pushing pair to vector";
        ai_mark_arterial_road_pairs->at(inv_dir).push_back(std::make_pair(flag_pos,dir));
        // recursively search this new flag_pos the same way
        //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo6 entering recurse call";
        arterial_road_depth_first_recursive_flagsearch(other_end_flag_pos, inv_dir, closed, depth);
      }else{
        //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "foo5- pos is already on closed list";
      }
    } 
  }
  //AILogDebug["arterial_road_depth_first_recursive_flagsearch"] << "done with node pos " << flag_pos;
}



//
// perform breadth-first FlagSearch to find best flag-path from start_pos 
//  to target_pos and determine flag path along the way.
//
// return true if solution found, false if not
//
// ALSO SET:
//
// MapPosVector *solution_flags
//  vector of the MapPos of all of the Flags in the solution, in *reverse* order,
//  including both start and end flags
// unsigned int *tile_dist
//  total TILE length of the complete solution, which may pass through many flags
//
//  this function will NOT work for fake flags / splitting flags
//
// this function is NOT optimal and is written for understanding and visualizing
//  the search.  A more common search would nodes to open before checking them
//  but when visualizing a search it appears that the search is going past the
//  target.  Instead, this search checks every new node as it is first found
//  so that it can quit and return solution immediately
//
// also, note that this is not a "A*" priority queue search like the Pathfinder.cc, 
//  there is no node comparisons because flags all have equal priority aside from their
//  distance from start_pos, and there is no heuristic because general direction towards
//  goal does not matter, only flag dist.  The distance from start pas is effectively handled in
//  correct priority order because it does breadth-first and if(closed) checking should prevent looping
//
//
// OPTIMIZATION - because this call is repeated many times in quick succession for a single
//  build_best_road call, especially for ReconnectNetworks, it makes sense to cache
//  the results for the some duration?
//
bool
AI::find_flag_path_and_tile_dist_between_flags(PMap map, MapPos start_pos, MapPos target_pos, 
              MapPosVector *solution_flags, unsigned int *tile_dist, ColorDotMap *ai_mark_pos){

  //AILogDebug["find_flag_path_and_tile_dist_between_flags"] << "start_pos " << start_pos << ", target_pos " << target_pos;

  // sanity check - this excludes fake flag solution starts (could change that later though)
  if (!map->has_flag(start_pos)){
    /*
    AILogError["find_flag_path_and_tile_dist_between_flags"] << "expecting that start_pos " << start_pos << " provided to this function is a flag pos, marking start_pos blue and throwing exception";
    ai_mark_pos->erase(start_pos);
    ai_mark_pos->insert(ColorDot(start_pos, "blue"));
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    throw ExceptionFreeserf("expecting that start_pos provided to this function is a flag pos");
    */
    // I think this could simply be a result of lost territory, not an exception.  Only started seeing it with multiple players
    AILogWarn["find_flag_path_and_tile_dist_between_flags"] << "expecting that start_pos " << start_pos << " provided to this function is a flag pos, maybe it was removed?  returng bad_score";
    return false;
  }

  // handle start=end
  if (start_pos == target_pos){
    //AILogWarn["find_flag_path_and_tile_dist_between_flags"] << "start_pos " << start_pos << " IS target_pos " << target_pos << ", why even make this call?";
    return true;
  }

  std::list<PFlagSearchNode> open;   // flags discovered but not checked for adjacent flags.  List is of Nodes instead of MapPos because needs parent, dist, etc.
  std::list<MapPos> closed;          // flags already checked in all Dirs (by MapPos).  List of MapPos is fine because it only exists to avoid re-checking
  
  // set the first node of the search to the start_pos
  //  and put it in the open list
  PFlagSearchNode fnode(new FlagSearchNode);
  fnode->pos = start_pos;
  open.push_back(fnode);

  while (!open.empty()) {

    fnode = open.front(); // read the first element
    open.pop_front(); // remove the first element that was just read and put into fnode

    // get this Flag
    Flag *flag = game->get_flag_at_pos(fnode->pos);
    if (flag == nullptr){
      AILogWarn["find_flag_path_and_tile_dist_between_flags"] << "got nullptr for game->get_flag_at_pos " << fnode->pos << " skipping this Flag (what happens next?)";
      // saw this trigger once, jan02 2022.  Single AI player
      //throw ExceptionFreeserf("got nullptr for game->get_flag_at_pos");
      continue;
    }
    
    // find adjacent flags to check
    for (Direction dir : cycle_directions_cw()){

      // skip dir if no path
      if (!map->has_path_IMPROVED(fnode->pos, dir))
        continue;

      // avoid "building that acccepts resources appears to have a path UpLeft" issue
      // could check if building accepts resources, but does it matter? reject any building
      if (dir == DirectionUpLeft && flag->has_building())
        continue;

      // get the other Flag in this dir
      Flag *other_end_flag = flag->get_other_end_flag(dir);
      if (other_end_flag == nullptr){
        AILogError["find_flag_path_and_tile_dist_between_flags"] << "got nullptr for game->get_other_end_flag(" << NameDirection[dir] << ") from flag at pos " << fnode->pos << ", returning false instead of crashing";
        //ai_mark_pos->erase(map->move(fnode->pos, dir));
        //ai_mark_pos->insert(ColorDot(map->move(fnode->pos, dir), "coral"));
        // update Dec04 2022 - haven't seen this in a long time, removing sleep
        //std::this_thread::sleep_for(std::chrono::milliseconds(30000));
        // saw this again jan 2023, changing to not crash
        //throw ExceptionFreeserf("got nullptr for game->get_other_end_flag");
        return false;
      }
      MapPos other_end_flag_pos = other_end_flag->get_position();
      //AILogDebug["find_flag_path_and_tile_dist_between_flags"] << "other_end_flag_pos is " << other_end_flag_pos;

      // skip dir if adjacent flag pos is already in the closed list
      if (std::find(closed.begin(), closed.end(), other_end_flag_pos) != closed.end())
        continue;

      // skip dir if adjacent flag pos is already in the open list
      bool already_in_open = false;
      for (PFlagSearchNode tmp_fnode : open){
        if (tmp_fnode->pos == other_end_flag_pos){
          already_in_open = true;
          break;
        }
      }
      if (already_in_open)
        continue;

      // this flag pos is not yet known, create a new node
      PFlagSearchNode new_fnode(new FlagSearchNode);
      new_fnode->pos = other_end_flag_pos;
      new_fnode->parent = fnode;
      // this is the dir from the PARENT to this child fnode
      //  but it shouldn't be set in the parent because the parent can have multiple children
      //  so it would require storing multiple dirs which is more complex.  So, again, the
      //  CHILD->dir represents the dir that the child's PARENT connects to the CHILD node
      // so far this is only used for trace_existing_road call, if road tracing is removed
      //  we don't even need this dir anymore
      new_fnode->dir = dir;  

      // stop here to check if the new pos is the target_pos
      //  and if so retrace it and record the solution
      if (new_fnode->pos == target_pos){
        AILogDebug["find_flag_path_and_tile_dist_between_flags"] << "found solution, reached " << target_pos;
        PFlagSearchNode solution_node = new_fnode;
        while(solution_node->parent){
          //AILogDebug["find_flag_path_and_tile_dist_between_flags"] << "solution contains " << solution_node->pos;
          solution_flags->push_back(solution_node->pos);

          // also trace the road to determine length in tiles of the solution
          Road tmp_road = trace_existing_road(map, solution_node->parent->pos, solution_node->dir);
          *(tile_dist) += tmp_road.get_length();
          //AILogDebug["find_flag_path_and_tile_dist_between_flags"] << "solution tile_dist so far is " << *(tile_dist);

          solution_node = solution_node->parent;
          
        }
        // push the last node too, and return
        //AILogDebug["find_flag_path_and_tile_dist_between_flags"] << "solution last node " << solution_node->pos;
        solution_flags->push_back(solution_node->pos);
        return true;
      }

      // this new node is not the target_pos, add to open list
      //  to continue the search
      open.push_back(new_fnode);

    } //end foreach Direction

    // all dirs checked for this node, put this pos on closed list
    closed.push_back(fnode->pos);

  } //end while(!open.empty)

  AILogDebug["find_flag_path_and_tile_dist_between_flags"] << "search completed with no solution found, returning false";
  return false;
} //end function
