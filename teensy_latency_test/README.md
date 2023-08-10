The teensy_latency_test measures audio output latency using an additional Teensy
board with microphone by playing audio, triggering a timer on the Teensy board,
and stopping once the Teensy board records audio. Compared to loopback_latency,
we can get output latency results that are not affected by input latency.

Some commands

Serial port loopback latency
```
teensy_latency_test -s
```

Plug in 3.5mm heaset or speaker to teensy and toggle teensy playback on/off
from host through serial port by
```
teensy_latency_test -t
```

Speaker output latency test (ALSA path)
```
teensy_latency_test -a hw:0,0
```

Speaker output latency test (CRAS path)
```
teensy_latency_test -c
```

In the current SerialLoopBack/ code the measure output latency and set_level
command is missing. But we can add that back easily from the teensy side.
