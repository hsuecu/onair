# Half Duplexing Acoustic Digital Communication

This project is about acoustic communication near ultrasonic frequencies.

## Features

- simple cli based operation
- single executable with both mode of transmission buildin


## Tech

This uses following dependencies:

- Linux environemnt
- ALSA sound library


## Installation

Build using the `makefile`

For initial state be sender
```sh
./main -m sender
```

For initial state be receiver
```sh
./main -m receiver
```

For help
```sh
./main -h
```

## Implementation Details

Using dual frequency diffirential (FSK) to transmit and receive custom morse encoding of hex's nibble.

There are two frequencies
- Q1 (19000hz)
- Q0 (18800hz)

>if at receiver side
power of Q1 > power of Q0
it is interpreted as 1
other wise as zero
>

Suggested sample rate
- 384000hz

Strcture of acoustic data that is being transmitted
```c
{1,0,0},               // 0 .
{1,1,0,0},             // 1 -
{1,0,1,0,0},           // 2 ..
{1,0,1,1,0,0},         // 3 .-
{1,1,0,1,0,0},         // 4 -.
{1,1,0,1,1,0,0},       // 5 --
{1,0,1,0,1,0,0},       // 6 ...
{1,0,1,0,1,1,0,0},     // 7 ..-
{1,0,1,1,0,1,0,0},     // 8 .-.
{1,0,1,1,0,1,1,0,0},   // 9 .--
{1,1,0,1,0,1,0,0},     // A -..
{1,1,0,1,0,1,1,0,0},   // B -.-
{1,1,0,1,1,0,1,0,0},   // C --.
{1,1,0,1,1,0,1,1,0,0}, // D ---
{1,0,1,0,1,0,1,0,0},   // E ....
{1,0,1,0,1,0,1,1,0,0}, // F ...-
{0,0,0,0,0,0,0,0,0,0}, // SYNC
{1,1,1,1,1,1,1,1,1,1}  // FIN
```

to transmit `0x12 0x13`
the structure of transmitted data that can be termed as `frame`
hence,
the frame will look like
|sync flag|data|dync flag|
|-|-|-|
|`0000000000 0000000000 0000000000`|`1100 10100 1100 101100`|`1111111111 1111111111 1111111111`|

the pulse duration is kept as `0.005 s` or `1920` samples out of `384000` samples. the receiver locks on when it 
start receiving zero which them self indicate start of transmission. It counts the number of zeros and divide
by 3 cause flags will always be repeated 3. Result is further divided by 10 because it is the number of pulse used.
This return the expected pulse on receiver side.

there are multiple sleep and usleep also sleep_estimator function in sender class. This is used to avoid
relaying audio data to ALSA api before the previous data is being transmitted.
