# Home Assistant

```sh
ansible-playbook --vault-password-file .vault-password \
  -i inventory.yml \
  homeassistant/docker-run.yml
```
