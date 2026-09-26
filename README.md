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
