// turtle draw sample: a colourful house + sun
// run:  xl run projects\dev\turtle\turtle.xl draw projects\dev\turtle\sample.cmd sample.svg

COLOR blue
FD 150
RT 90
FD 150
RT 90
FD 150
RT 90
FD 150
RT 90

// roof
COLOR red
FD 150
RT 30
FD 150
RT 120
FD 150
RT 30
RT 90

// door
COLOR brown
RT 90
FD 50
LT 90
FD 60
LT 90
FD 50

// move to sun (pen up)
PU
RT 90
FD 150
PD

COLOR orange
RT 90
FD 30
RT 150
FD 30
RT 60
FD 30
RT 150
FD 30
RT 60
FD 30
RT 150
FD 30
RT 60
FD 30
RT 150
FD 30
RT 60
FD 30
RT 150
FD 30
