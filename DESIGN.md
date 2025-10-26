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
    "Extract dungeon map"
  }

  subgraph Done
  {
    node [color=darkgreen, style=filled, fontcolor=white]
    "Describe all levels"
    "Introduce tile properties"
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
10. Use sword to kill the enemy. The enemy will drop the key?


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
