# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A personal home-IoT deployment repo. There is no application code to build or test: everything is Ansible playbooks, Jinja-templated config files, systemd units, and bash scripts that get pushed to a VPS (`burelli.xyz`) and to a handful of Raspberry Pis on the LAN. "Running" something means running a playbook.

## Commands

All playbooks are run **from the repo root** (relative `src:` paths inside playbooks are resolved against the playbook file, e.g. `../../camera/...`). `ansible.cfg` sets `inventory = ./inventory.yml` and `log_path = ./logs`.

```sh
# Deploy a server-side service (VPS, via docker)
ansible-playbook --vault-password-file .vault-password mosquitto/docker-run.yml
ansible-playbook --vault-password-file .vault-password homeassistant/docker-run.yml
ansible-playbook --vault-password-file .vault-password nodered/playbook/docker-run.yml
ansible-playbook --vault-password-file .vault-password rtsp-server/playbook/deploy-docker.yml

# Deploy / restart a Raspberry Pi device role
ansible-playbook --vault-password-file .vault-password devices/playbook/install-raspberry-mqtt-camera.yml
ansible-playbook --vault-password-file .vault-password devices/playbook/install-raspberry-ir-receiver.yml
ansible-playbook --vault-password-file .vault-password devices/playbook/restart-raspberry-mqtt-io-service.yml

# Limit to one host
ansible-playbook --vault-password-file .vault-password devices/playbook/apt-update.yml --limit raspi-2
```

`.vault-password` lives in the repo root and is gitignored. There is no per-directory ansible config: `./ansible.cfg` + `./inventory.yml` are the single source of truth, and every playbook is invoked from the root.

Secrets:

```sh
# Encrypt a value to paste into inventory.yml as a !vault block
ansible-vault encrypt_string --vault-password-file .vault-password --stdin-name 'the_secret'

# The mosquitto passfile is a vault-encrypted whole file
ansible-vault decrypt --vault-password-file .vault-password mosquitto/passfile   # edit
ansible-vault encrypt --vault-password-file .vault-password mosquitto/passfile   # re-encrypt before commit
```

Manual MQTT smoke tests: `mosquitto/test-publish.sh`, `mosquitto/test-subscribe.sh` (hardcoded broker + credentials, TLS on 8883 with `--insecure`).

Media pulled from the Node-RED snapshot directory: `./rsync-media.sh` (from the VPS) or `./download-media.sh` (from `arch-macbook`).

## Architecture

Everything talks MQTT; Home Assistant discovers devices automatically from retained config topics.

```
Raspberry Pis (raspi-1/2/3, LAN)          VPS burelli.xyz (docker network `main`, behind traefik)
  mqtt-io       ──┐                        ┌─ mosquitto  (mqtt.burelli.xyz:8883, TLS via traefik TCP+SNI)
  mqtt-camera   ──┼── MQTT over TLS ──────>│
  lircd-to-mqtt ──┘                        ├─ home-assistant (iot.burelli.xyz)
  libcamera-vid ──── RTSP ────────────────>├─ nodered  (nodered.burelli.xyz) → saves snapshots, Telegram alerts
                                           └─ mediamtx (rtsp.burelli.xyz)
```

- **`inventory.yml` (root) is the live inventory.** It defines the groups (`gateway`, `mosquitto`, `nodered`, `rtsp`, `raspberry`, plus `raspi_camera` as a child of `raspberry`), the public/docker hostnames used in traefik labels, and every per-host variable consumed by the templates: `device_name`, `iot_area`, `mqtt_client_id`, `mqtt_user`, `mqtt_password` (vault), `mqtt_host/port/tls_enabled`, `cpu_arc`.
- **Config layout**: playbooks live next to the service they deploy (`mosquitto/`, `nodered/playbook/`, `homeassistant/`, `rtsp-server/playbook/`, `devices/playbook/`) and reach siblings through relative `src:` paths. The `ansible/` directory was the pre-refactor home of the device playbooks (moved to `devices/` in `8e64682`) and should not come back.
- **Docker services** are all deployed the same way: `community.docker.docker_container` with `recreate: true`, `restart_policy: always`, joined to the external `main` network, config templated into `/root/{{ path_prefix }}/<service>/` (some older playbooks still use `/root/{{ env }}/...`), and exposed through **traefik labels** on the container rather than published ports. Upgrading a service = bumping the pinned `image:` tag and re-running its playbook.
- **Device roles** follow one pattern: apt-install deps → drop scripts under `/opt/<role>/` or `/usr/local/bin` → template a systemd unit that carries all configuration as `Environment=` lines → `systemd: state=restarted enabled=yes daemon_reload=yes`.
  - `mqtt-io` (raspi-1, raspi-2): pip-installed `mqtt-io` python package driving GPIO; the device's pin map lives in `devices/<host>/mqtt-io-config.yaml` and is templated to `/etc/mqtt-io/config.yaml`. This is where lamps, buttons, the motion sensor and the DHT sensor are declared, each with its own `ha_discovery` name.
  - `mqtt-camera` (group `raspi_camera`, currently just raspi-1 — both camera playbooks target the group, so adding a camera host means adding it there and nowhere else): pure bash. `camera/run.sh` is the entrypoint loop — it publishes two retained Home Assistant discovery configs (a `camera` image entity and a `select` entity for state), then blocks on `mosquitto_sub -C 1` on the command topic and dispatches `snapshot` / `streaming` / `ready`. Snapshots are published as the raw JPEG payload on the image topic; streaming shells out to `libcamera-vid | ffmpeg` into the RTSP server. All config comes from the env vars in `camera/mqtt-camera.service`.
  - `ir-receiver` (raspi-2): `irw` output piped through awk into `mosquitto_pub` on topic `ir-receiver`; remote keymap in `ir-receiver/A1156.lircd.conf`.
- **Topic conventions**: discovery configs go to `homeassistant/<component>/<object_id>/config`; device data is prefixed with `device_name` for both camera and mqtt-io. `object_id` is derived from `iot_area`, so the area var effectively names the entities.
- **`mosquitto.conf` sets no `persistence`**, so retained messages are in-memory only. Recreating the broker container discards every retained message, including the Home Assistant discovery configs — redeploy the devices afterwards to republish them (see the reinstall order in `README.md`).
- Broker auth is username/password only (`allow_anonymous false`, `password_file`); TLS is terminated by traefik, so `mosquitto.conf` itself only listens plaintext on 1883 inside the docker network. Adding a user means editing `mosquitto/passfile` (see root `README.md`: generate the hash with `mosquitto_passwd` on the server, re-encrypt with ansible-vault, redeploy, then `kill -HUP` the broker).

## Gotchas

- `camera/*.sh` are installed with `template`, not `copy`, so any `{{ ... }}` in them is rendered at deploy time. The vars must be in scope for the **`raspberry` group** — that is why `rtsp_host` / `rtsp_port` are declared there and not reused from the `rtsp` group (group vars don't cross groups).
- **Public ports are 22, 80, 443, 8883 and 8554 — and that list is enforced in DigitalOcean's cloud firewall, not on the droplet** (`iptables -P INPUT ACCEPT`, no ufw). So a service can be listening on `0.0.0.0` and still be unreachable, with no evidence on the host. Anything new needing a public port goes through traefik on 443, gets its own traefik entrypoint (as `mqtt` does on 8883), or needs a rule added in the DO console — which is outside this repo.
- `rtsp-server/etc/mediamtx.yml` must be mounted at **`/mediamtx.yml`**, not `/etc/mediamtx.yml` — mediamtx searches only `./mediamtx.yml`, `/usr/local/etc`, `/usr/etc` and `/etc/mediamtx/`, so a wrong path means it silently starts with defaults and **no authentication**. Confirm with `docker logs rtsp-server | head` → `configuration loaded from /mediamtx.yml`.
- That config is deliberately minimal (only non-default settings). It is mediamtx 1.x format: per-path `readUser`/`readPass` from the 0.x era no longer exist and live in the global `authInternalUsers` list instead.
- Every ansible run prints `Found both group and host with same name` twice, because `mosquitto` and `nodered` are each a group *and* the single host inside it. Harmless, but it means a var set on the host and on the group of the same name can shadow confusingly — keep new group names distinct from host names (and use `_`, not `-`, to avoid Ansible's invalid-group-character warning).
- Service config paths are inconsistent on the server: `mosquitto/` and `homeassistant/` template into `/root/{{ env }}/...` (`prod`) while `nodered/` and `rtsp-server/` use `/root/{{ path_prefix }}/...` (`buro-iot`). Unifying them relocates live config directories, so it needs a deliberate migration, not a find-and-replace.
- `mosquitto/mosquitto_docker_run.sh`, `mosquitto/generate-certs.sh` and `mosquitto/cp_certs.sh` predate the traefik/ansible setup (they mount a `certs/` dir and use the `burellixyz` network). The playbook is the source of truth.
- `raspi-1-to-mosquitto.sh` depends on a `pig2log` binary that is no longer in the repo; it was superseded by `mqtt-io`.
- `mosquitto/test-*.sh` contain a real broker password in plaintext — leave them out of anything that gets shared, and prefer vault vars for anything new.
- `media/` and `logs/` are untracked scratch output, not inputs.
