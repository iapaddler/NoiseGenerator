# Noise Generator Notes

## References
### Get the name of the Bose speaker
$ `pactl list sinks short`

### Connect to the speaker
$ `bluetoothctl connect 08:DF:1F:00:1E:49`

### Get status
$ `bluetoothctl info 08:DF:1F:00:1E:49`
$ `wpctl status`
$ `systemctl --user status noise-generator.service --no-pager`
$ `journalctl --user -u noise-generator.service -f`

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
`sudo mkdir -p /var/lib/lightdm/.config/systemd/user`

`for u in pipewire.service pipewire.socket wireplumber.service \`
`         pipewire-pulse.service pipewire-pulse.socket`
`do`
`    sudo ln -sf /dev/null "/var/lib/lightdm/.config/systemd/user/$u"`
`done`

`sudo chown -R lightdm:lightdm /var/lib/lightdm/.config`

`sudo ls -l /var/lib/lightdm/.config/systemd/user/`

These five entries should be pointing to /dev/null:
      pipewire.service -> /dev/null
      pipewire.socket -> /dev/null
      wireplumber.service -> /dev/null
      pipewire-pulse.service -> /dev/null
      pipewire-pulse.socket -> /dev/null

### Uno Q Access
`ssh arduino@Name-UnoQ.home.local`
`http://name-unoq.home.local:9000/`

## Service Details
The service config file from service/noise-generator.service is located at:
`~/.config/systemd/user/noise-generator.service`

### Install the service
$ `systemctl --user daemon-reload`
$ `systemctl --user enable noise-generator.service`

### Check service status and app output
$ `systemctl --user status noise-generator.service --no-pager`
$ `journalctl --user -u noise-generator.service -f`
