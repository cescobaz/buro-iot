# Nodered

```sh
ansible-playbook \
  --vault-password-file .vault-password \
  -i inventory.yml \
  nodered/playbook/docker-run.yml
```

Settings file `/data/settings.js`

To create password hash use `node-red admin hash-pw`
