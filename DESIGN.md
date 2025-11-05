# The Plan

```graphviz
strict digraph ThePlan {
      
  subgraph HighPrio
  {
    node [color=orange, style=filled]
  }

  subgraph InProgress
  {
    node [color=green, style=filled]
    "Extract player map"
  }

  subgraph Done
  {
    node [color=darkgreen, style=filled, fontcolor=white]
    "Describe all levels"
    "Introduce tile properties"
    "Extract dungeon map"
    "Update dungeon map from view"
  }

  // Tasks
  "Introduce tile properties" -> "Extract dungeon map" -> "Extract player map"
  "Extract dungeon map" -> "Update dungeon map from view" -> "Update player map from dungeon map"
  "Extract player map" -> "Update player map from view"
  "Extract player map" -> "Update player map from dungeon map"
  "Describe all levels" -> "Figure out state machine"
  { "Figure out state machine" "Update player map from view" "Update player map from view" } -> "Extract discoverer" -> "Extract Solver"
  "Extract discoverer" -> "Parallel discoverers"
  "Update player map from dungeon map" -> "Parallel discoverers"
}
```

# Experiments

* Drop boulder on enemy
* How far can one carry a boulder?

# The levels

0. A single room
1. A maze
2. Exit hidden behind a door
3. Need to pass two doors in sequence - all doors are visible - only one key accessible
4. Need to pass three doors in sequence - all doors are visible - only one key accessible
5. Need to pass three doors in sequence - one door is hidden - two keys accessible
6. A boulder blocks the exit
7. Place a boulder on a pressureplate (lots of boulders in this level)
8. Avoid being caught by the enemy (some boulders)
9. Stand on pressureplate. Drop the door on the enemy. The enemy will drop the key. (no boulders)
10. Use sword to kill the enemy. The enemy will drop the key
11. Two enemies, one of which behind a door. Both enemies have keys. Be sure to collect sufficient hearts
12. One enemy which has the key to unlock my colleague. Huge distance between sword and heart. Danger ;-)

# Ways to make progress

* Go to the exit (as a pair)
* Open a door with a key
* Put a boulder on a pressureplate
* Moving a "bad" boulder

# Things to try

* Explore
* Stand on a pressureplate
  * Pressureplate in sight of a door. Drop the door on the enemy
* Kill an enemy
  (So far (L12), only one enemy was accessible at any one time.)

# "Game" state
```plantuml
[*] --> Idle
Idle --> Exploring
Exploring --> ReconsideringUncheckedBoulders 
ReconsideringUncheckedBoulders --> MovingBoulder
ReconsideringUncheckedBoulders --> Exploring
MovingBoulder --> Exploring
Exploring --> OpeningDoor
OpeningDoor --> Exploring
Exploring --> MovingToExit
Exploring --> [*]
```
