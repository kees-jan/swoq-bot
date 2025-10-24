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
    "Introduce tile properties"
    "Describe all levels"
  }

  subgraph Done
  {
    node [color=darkgreen, style=filled, fontcolor=white]
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
