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

Thanks to https://github.com/alessandrocuda/noise_generator for the reference
