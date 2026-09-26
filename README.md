# buro-iot

## Ansible

All playbooks are run **from the repository root**: each service keeps its own
playbook (`mosquitto/docker-run.yml`, `nodered/playbook/docker-run.yml`,
`devices/playbook/*.yml`, ...) and reaches sibling directories with relative
`src:` paths, so `./ansible.cfg` and `./inventory.yml` are the single source of
truth. There is no per-directory ansible config.

```bash
ansible-playbook --vault-password-file .vault-password mosquitto/docker-run.yml
ansible-playbook --vault-password-file .vault-password devices/playbook/install-raspberry-mqtt-io.yml --limit raspi-2
```

### Create encrypted var

```bash
ansible-vault encrypt_string --vault-password-file .vault-password --stdin-name 'the_secret'
```

You will get the encrypted var to use in template and playbook.

## Full reinstall

Redeploys every service and device. All the docker playbooks already set
`recreate: true`, so containers are force-recreated even when the image tag has
not changed. Persistent data lives in bind mounts on the host and survives
recreation:

| service        | data on the VPS                 | notes                                    |
| -------------- | ------------------------------- | ---------------------------------------- |
| mosquitto      | `/root/prod/mosquitto`          | config + passfile, re-templated from repo |
| home-assistant | `/root/prod/homeassistant/config` | preserved untouched                     |
| nodered        | `/root/buro-iot/nodered/data`   | flows preserved, `settings.js` overwritten from repo |

**Order matters.** `mosquitto.conf` does not enable persistence, so recreating
the broker discards every retained message — including the Home Assistant MQTT
discovery configs. The devices republish theirs on start, so deploy the broker
first and the devices last, otherwise Home Assistant shows the entities as
unavailable until the next device restart.

```bash
# 1. broker first
ansible-playbook --vault-password-file .vault-password mosquitto/docker-run.yml

# 2. consumers
ansible-playbook --vault-password-file .vault-password homeassistant/docker-run.yml
ansible-playbook --vault-password-file .vault-password nodered/playbook/docker-run.yml

# 3. devices last - they republish the retained discovery configs on start
ansible-playbook --vault-password-file .vault-password devices/playbook/install-raspberry-mqtt-io.yml
ansible-playbook --vault-password-file .vault-password devices/playbook/install-raspberry-mqtt-camera.yml
ansible-playbook --vault-password-file .vault-password devices/playbook/install-raspberry-ir-receiver.yml
```

`raspi-3` is still in the inventory but is not currently deployed. Playbooks
that target the whole `raspberry` group need to skip it:

```bash
ansible-playbook --vault-password-file .vault-password devices/playbook/apt-update.yml --limit raspi-1,raspi-2
```

### Verify

```bash
ansible all -m ping                        # reachability
ansible raspi-1,raspi-2 -b -m shell -a 'systemctl is-active mqtt-io mqtt-camera lircd-to-mqtt'
ansible ocean -m shell -a 'docker ps'      # containers up, RestartCount 0
ansible raspi-1 -b -m shell -a 'journalctl -u mqtt-camera --since "-5min" --no-pager | tail'
```

Expected service layout: `mqtt-io` on both Pis, `mqtt-camera` on raspi-1 only
(group `raspi_camera`), `lircd-to-mqtt` on raspi-2 only.

### rtsp-server / live streaming

mediamtx runs as `bluenviron/mediamtx:1.21.1`. `rtsp-server/etc/mediamtx.yml`
is a minimal config holding only the settings that differ from upstream
defaults; check the container actually picked it up with:

```bash
ansible ocean -m shell -a 'docker logs --tail 5 rtsp-server'
# expect: "configuration loaded from /mediamtx.yml"
```

The file **must** be mounted at `/mediamtx.yml`. mediamtx only searches
`./mediamtx.yml`, `/usr/local/etc`, `/usr/etc` and `/etc/mediamtx/`, so a file
mounted at `/etc/mediamtx.yml` is silently ignored and the server comes up with
default settings and no authentication.

HLS is reachable over HTTPS through traefik at `https://rtsp.burelli.xyz`
(routed to container port 8888).

#### Firewall

Publishing reaches the droplet on **TCP 8554**, which is open in the
DigitalOcean cloud firewall. The droplet itself does not filter
(`iptables -P INPUT ACCEPT`, no ufw), so when a publish fails the cloud
firewall is the first thing to check - the public ports are 22, 80, 443, 8883
and 8554, and everything else is filtered there rather than on the host:

```bash
ansible ocean -m shell -a 'ss -lntp | grep 8554'   # listening on the droplet
timeout 5 bash -c 'exec 3<>/dev/tcp/burelli.xyz/8554' && echo open || echo filtered
```

**TCP only - do not open UDP 8554.** `mediamtx.yml` sets
`rtspTransports: [tcp]`, so no UDP RTP/RTCP listener is ever opened (the
startup log says `[RTSP] started with listeners on :8554 (TCP/RTSP)`), the
container publishes `8554/tcp` only, and `camera/rtsp-stream.sh` publishes with
`-rtsp_transport tcp`. An RTSP control channel is always TCP regardless, and a
UDP rule would forward to nothing.

8888 stays closed: HLS is served through traefik on 443. Home Assistant needs
no open port at all - it reads from the container over the internal `main`
docker network (`rtsp://rtsp-server:8554/<path>`).

### Test the stream with ffmpeg

The stream path is the camera's `object_id`, i.e. `<iot_area>-camera-mqtt`
(`living-camera-mqtt` for raspi-1). Reading requires the `homeassistant`
credentials from `rtsp-server/etc/mediamtx.yml`; publishing is anonymous.

```bash
# 1. publish a synthetic stream
ffmpeg -re -f lavfi -i testsrc=size=640x480:rate=15 \
  -c:v libx264 -preset ultrafast -tune zerolatency \
  -f rtsp -rtsp_transport tcp rtsp://rtsp.burelli.xyz:8554/test

# 2. play it back over RTSP
ffplay -rtsp_transport tcp 'rtsp://homeassistant:<password>@rtsp.burelli.xyz:8554/test'

# 3. or play it back over HLS through traefik on 443
ffplay 'https://homeassistant:<password>@rtsp.burelli.xyz/test/index.m3u8'
```

To test the real camera instead of a synthetic source, ask it to start
streaming over MQTT and then play its path:

```bash
mosquitto_pub --url "mqtt://<user>:<pass>@mqtt.burelli.xyz:8883/raspi-1/select/living-camera-mqtt-state/commands" -m streaming
ffplay -rtsp_transport tcp 'rtsp://homeassistant:<password>@rtsp.burelli.xyz:8554/living-camera-mqtt'
mosquitto_pub --url "mqtt://<user>:<pass>@mqtt.burelli.xyz:8883/raspi-1/select/living-camera-mqtt-state/commands" -m ready
```

Check the server side while testing:

```bash
ansible ocean -m shell -a 'docker logs --tail 20 rtsp-server'
# a successful publish logs: "is publishing to path 'test'"
```

## Hardware

# IR receiver

GPIO11 (pin 23)

Open /boot/config.txt and edit line dtoverlay=gpio-ir,gpio_pin=11
then

```bash
sudo reboot now
sudo apt install ir-keytable
sudo ir-keytable -c -p all -t
```

IR database
https://lirc-remotes.sourceforge.net/remotes-table.html

# Setup Raspberry

## Setup SD boot partition

Add the following wpa_supplicant.conf file
```
# wpa_supplicant.conf
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
country=IT
update_config=1

network={
 ssid="GIARGIANA"
 psk="<Password for your wireless LAN>"
}
```
Touch `ssh` file ti enable sshd
```bash
touch ssh
```

Create a user (raspi-2)
```
# userconf.txt
pi:$6$HdPBlG7bDoFzk/S9$XKG974gMGuEmGzzRPBXzbMWENWmEb1la1Q.8gkXRR.4fIiFeONKrvUmE4Bx9p8OFGDp.jleCp.lLB.1GPDckg0
```

## Add user to mosquitto

In the remote machine use
```
mosquitto_passwd
```

```
scp root@burelli.xyz:prod/mosquitto/passfile ./mosquitto/
```

```
ansible-vault encrypt --vault-password-file .vault-password mosquitto/passfile
```

Update the passfile then trigger mosquitto by:

```bash
ps aux | grep mosquitto | grep -v grep | awk '{ print $2 }' | \
  xargs kill -HUP
```
