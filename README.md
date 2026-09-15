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

Thanks to https://github.com/alessandrocuda/noise_generator for the reference

## UnoQ Specifics
Arduino creates a nasty scenario with the UnoQ. It creates a libwire config for a system user.
This is intended for use by their App Lab, as I understand it.
This conflicts with the login user being able to connect to a bluetooth speaker.

### The Fix
Arduino gives us the exact fix:
UNO Q instructions say to create masks under:
`/var/lib/lightdm/.config/systemd/user/`

for:
pipewire.service
pipewire.socket
wireplumber.service
pipewire-pulse.service
pipewire-pulse.socket

They specifically emphasize masking both services and sockets, because socket activation can otherwise
restart PipeWire when a client connects.


### System wide
`
sudo mkdir -p /var/lib/lightdm/.config/systemd/user

for u in pipewire.service pipewire.socket wireplumber.service \
         pipewire-pulse.service pipewire-pulse.socket
do
    sudo ln -sf /dev/null "/var/lib/lightdm/.config/systemd/user/$u"
done

sudo chown -R lightdm:lightdm /var/lib/lightdm/.config
`

`sudo ls -l /var/lib/lightdm/.config/systemd/user/`

These five entries should be pointing to /dev/null:
pipewire.service -> /dev/null
pipewire.socket -> /dev/null
wireplumber.service -> /dev/null
pipewire-pulse.service -> /dev/null
pipewire-pulse.socket -> /dev/null
