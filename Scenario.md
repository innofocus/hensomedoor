samplepers = 10
samplenb = 5
sampledur = samplepers / samplenb
stddevmax = 10
speed_slow = 3
speed_fast = 8
w_zero = 10
w_precision = 30
w_delta = 100

# Initialisation
drectioned = 0
tared = 0

## Tare
### setup : door not wooked, no weight
tare = tare
tared = 1

## direction
## setup : door not hooked
## small weight arround 20g (< w_delta) or pinched with fingers
motor_up
while weight not increasing:
directioned = 1

## door weight
motor down 
wait 3
motor stop
w_door = mean(4s)
w_stdd = stdd(4s)

# door weight down
# door time to close
t0
motor down 
while weight == w_door +- w_precision
    mean update
    stdd_update
w_door_down = mean
w_door_down_stdd = stdd
while weight > w_zero
motor stop
t_door_closing = now - t0

# door weight up
# door time to open
t0
motor up
wait 3
while weight == w_door +- w_precision
   mean update
   stdd update
w_door_up = mean
w_door_up_stdd = stdd
while weight < w_door + w_precision + w_delta
    wait
motor stop
t_door_opening = now - t0





