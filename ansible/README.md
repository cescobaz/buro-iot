## Create encrypted var

```bash
ansible-vault encrypt_string --vault-password-file .vault-password --stdin-name 'the_secret'
```

You will get the encrypted var to use in template and playbook.

## Run playbook with vault

```bash
ansible-playbook --vault-password-file .vault-password playbook/set-up-mqtt-io.yml
```
