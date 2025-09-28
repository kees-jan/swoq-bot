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
