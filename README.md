# buro-iot

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

Update the passfile then trigger mosquitto by:

```bash
ps aux | grep mosquitto | grep -v grep | awk '{ print $2 }' | \
  xargs kill -HUP
```
