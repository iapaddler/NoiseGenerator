# NoiseGenerator
Creating white and brown noise from a Linux platform to a Bluetooth speaker

## Dependencies
### PipeWire
`sudo apt install libpipewire-0.3-dev`

## Approach
### White Noise / Audio / Bluetooth
#### Audio Source App
#### PipeWire (Handles the digital audio compression/codecs)
#### BlueZ (Handles the wireless transmission protocol)
#### Bluetooth Chip (Hardware broadcasts the wireless signal)

## Build
### Build the binary
`make all`

### Configure Bluetooth to connect to the speaker
Need to pair the speaker before doing this
TBD: get the name of the device dynamically for the makefile
`make config`

### Clean
`make clen`

### Run tests
`make test`

## Reference
### Get the name of the Bose speaker
$ pactl list sinks short

### Connect speaker
$ bluetoothctl connect 08:DF:1F:00:1E:49

### Get status of the speaker
$ bluetoothctl info 08:DF:1F:00:1E:49

