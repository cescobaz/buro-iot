#include "gpio2mosquitto.h"
#include <assert.h>
#include <errno.h>
#include <json-c/json.h>
#include <mosquitto.h>
#include <pigpio.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// https://stackoverflow.com/questions/3437404/min-and-max-in-c
#define MAX(a, b)                                                              \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a > _b ? _a : _b;                                                         \
  })

#define DEFAULT_STR_SIZE 200

#define EXIT_CODE_UNKNOWN_ARG 1
#define EXIT_CODE_GPIO_INITIALISATION_FAILED 2
#define EXIT_CODE_MOSQUITTO_INITIALISATION_FAILED 3

int write_gpio_modes(gpio2mosquitto_t *data) {
  FILE *file = fopen(data->gpio_modes_filepath, "w");
  if (file == NULL) {
    return 2;
  }
  size_t result = fwrite(data->gpio_modes, sizeof(char), GPIO_MODES_SIZE, file);
  fclose(file);
  return result == 0;
}

int read_gpio_modes(gpio2mosquitto_t *data) {
  FILE *file = fopen(data->gpio_modes_filepath, "r");
  if (file == NULL) {
    return 2;
  }
  size_t result = fread(data->gpio_modes, sizeof(char), GPIO_MODES_SIZE, file);
  fclose(file);
  return result == 0;
}

void cleanup_mosquitto(gpio2mosquitto_t *data) {
  if (!data->mosquitto) {
    return;
  }
  mosquitto_disconnect(data->mosquitto);
  mosquitto_loop_stop(data->mosquitto, false);
  printf("[info] mosquitto disconnected\n");
  mosquitto_destroy(data->mosquitto);
  data->mosquitto = NULL;
}

struct mosquitto *init_mosquitto(gpio2mosquitto_t *data) {
  cleanup_mosquitto(data);
  struct mosquitto *mosquitto =
      mosquitto_new(data->mosquitto_client_id, true, data);
  data->mosquitto = mosquitto;
  int result;
  if (data->mosquitto_username && data->mosquitto_password) {
    result = mosquitto_username_pw_set(mosquitto, data->mosquitto_username,
                                       data->mosquitto_password);
    assert(result == MOSQ_ERR_SUCCESS);
  }
  result =
      mosquitto_tls_set(mosquitto, NULL, "/etc/ssl/certs", NULL, NULL, NULL);
  assert(result == MOSQ_ERR_SUCCESS);
  result = mosquitto_connect(mosquitto, data->mosquitto_host,
                             data->mosquitto_port, 60);
  assert(result == MOSQ_ERR_SUCCESS);
  result = mosquitto_loop_start(mosquitto);
  assert(result == MOSQ_ERR_SUCCESS);
  printf("[info] mosquitto initialized %s %d\n", data->mosquitto_host,
         data->mosquitto_port);
  return mosquitto;
}

void publish_self_state(gpio2mosquitto_t *data) {
  time_t now = time(NULL);
  char topic[DEFAULT_STR_SIZE];
  snprintf(topic, DEFAULT_STR_SIZE, "%s/things/%s", data->username,
           data->device_name);

  char gpio[GPIO_MODES_SIZE + 1];
  memcpy(gpio, data->gpio_modes, GPIO_MODES_SIZE);
  gpio[GPIO_MODES_SIZE] = '\0';

  char payload[DEFAULT_STR_SIZE];
  snprintf(payload, DEFAULT_STR_SIZE, "{\"ut\":%ld,\"gpioModes\":\"%s\"}", now,
           gpio);
  mosquitto_publish(data->mosquitto, NULL, topic, strlen(payload), payload, 0,
                    false);
}

void on_message(struct mosquitto *mosquitto, void *data,
                const struct mosquitto_message *message) {
  struct json_tokener *tok;
  struct json_object *obj =
      json_tokener_parse_ex(tok, message->payload, message->payloadlen);
  if (!obj) {
    fprintf(stderr, "[error] on_message json_tokener_parse_ex error %d\n",
            json_tokener_get_error(tok));
    return;
  }
  struct json_object *gpio_modes_obj;
  if (!json_object_object_get_ex(obj, "gpioModes", &gpio_modes_obj)) {
    fprintf(stderr, "[error] on_message json_object_object_get_ex error: "
                    "missing gpioModes key\n");
    return;
  }
  const char *gpio_modes_s = json_object_get_string(gpio_modes_obj);
  if (!gpio_modes_s) {
    fprintf(stderr, "[error] on_message json_object_get_string is NULL\n");
    return;
  }

  json_object_put(obj);
  json_tokener_free(tok);
}

void eventFuncEx(int event, int level, uint32_t tick, void *userdata) {
  time_t now = time(NULL);
  printf("[info] event callback: event: %u, level: %d, tick: %u\n", event,
         level, tick);
  gpio2mosquitto_t *data = (gpio2mosquitto_t *)userdata;
  char device_name[DEFAULT_STR_SIZE];
  snprintf(device_name, DEFAULT_STR_SIZE, "%s.gpio.%u", data->device_name,
           event);
  char topic[DEFAULT_STR_SIZE];
  snprintf(topic, DEFAULT_STR_SIZE, "%s/things/%s", data->username,
           device_name);
  char payload[DEFAULT_STR_SIZE];
  snprintf(payload, DEFAULT_STR_SIZE,
           "{\"ut\":%ld,\"name\":\"%s\",\"gpio\":%u,\"value\":%d}", now,
           device_name, event, level);
  int result;
  int max_retry = 1;
  do {
    result = mosquitto_publish(data->mosquitto, NULL, topic, strlen(payload),
                               payload, 0, false);
    max_retry -= 1;
    if (result == MOSQ_ERR_SUCCESS) {
      printf("[info] published message: topic %s message %s\n", topic, payload);
      return;
    }
    if (result != MOSQ_ERR_SUCCESS) {
      fprintf(stderr, "mosquitto_publish result = %d, errno %u\n", result,
              errno);
      printf("[info] reconnecting to mqtt\n");
      result = mosquitto_reconnect(data->mosquitto);
      if (result != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "mosquitto_reconnect result = %d, errno %u\n", result,
                errno);
        return;
      }
    }
  } while (max_retry > 0);
}

void set_gpio_mode(gpio2mosquitto_t *data, int gpio, char mode) {
  if (mode != GPIO_MODE_INPUT && mode != GPIO_MODE_OUTPUT) {
    gpioSetAlertFuncEx(gpio, NULL, NULL);
    data->gpio_modes[gpio] = mode;
    printf("[info] unwatch gpio %u\n", gpio);
    return;
  }
  uint8_t pimode = mode == GPIO_MODE_INPUT ? PI_INPUT : PI_OUTPUT;
  int result = gpioSetMode(gpio, pimode);
  if (result == 0) {
    gpioSetAlertFuncEx(gpio, &eventFuncEx, data);
    data->gpio_modes[gpio] = mode;
    char *mode_s = mode == GPIO_MODE_INPUT ? "input" : "output";
    printf("[info] set and watch %s gpio %u\n", mode_s, gpio);
  } else {
    char *error_msg = result == PI_BAD_GPIO ? "PI_BAD_GPIO" : "PI_BAD_MODE";
    fprintf(stderr, "unable to gpioSetMode(%d, %d): %s\n", gpio, pimode,
            error_msg);
  }
}

void set_gpio_modes(gpio2mosquitto_t *data, char *gpio_modes) {
  int max = MAX(strlen(gpio_modes), GPIO_MODES_SIZE);
  for (int gpio = 0; gpio < max; gpio++) {
    char current_mode = data->gpio_modes[gpio];
    char new_mode = gpio_modes[gpio];
    if (current_mode == new_mode) {
      continue;
    }
    set_gpio_mode(data, gpio, new_mode);
  }
}

void apply_gpio_modes(gpio2mosquitto_t *data) {
  for (int gpio = 0; gpio < GPIO_MODES_SIZE; gpio++) {
    char mode = data->gpio_modes[gpio];
    set_gpio_mode(data, gpio, mode);
  }
}

void unwatch_all_gpio(gpio2mosquitto_t *data) {
  for (int gpio = 0; gpio < GPIO_MODES_SIZE; gpio++) {
    gpioSetAlertFuncEx(gpio, NULL, NULL);
    printf("[info] unwatch gpio %u\n", gpio);
  }
}

void gpio2mosquitto_init(gpio2mosquitto_t *data) {
  memset(data, 0, sizeof(gpio2mosquitto_t));
  data->mosquitto_port = 1883;
  memset(data->gpio_modes, '_', GPIO_MODES_SIZE);
}
void gpio2mosquitto_cleanup(gpio2mosquitto_t *data) {
  data->state = STATE_STOPPED;
  unwatch_all_gpio(data);
  cleanup_mosquitto(data);
  write_gpio_modes(data);
  memset(data, 0, sizeof(gpio2mosquitto_t));
}

void print_help() {
  printf("Usage: gpio2mosquitto OPTIONS\n"
         "--mqtt-client-id\n"
         "--mqtt-host\n"
         "--mqtt-port\n"
         "--mqtt-username\n"
         "--mqtt-password\n"
         "--mqtt-topics-prefix\n"
         "--gpio-modes-file\n");
}

void gpio2mosquitto_set_by_args(gpio2mosquitto_t *data, int argc,
                                char *argv[]) {
  data->argc = argc;
  data->argv = argv;

  uint8_t args_checks[] = {0, 0, 0, 0, 0, 0, 0};
  for (int i = 1; i < argc - 1; i += 2) {
    char *key = argv[i];
    char *value = argv[i + 1];
    if (strcmp(key, "--mqtt-client-id") == 0) {
      data->mosquitto_client_id = value;
    } else if (strcmp(key, "--mqtt-host") == 0) {
      data->mosquitto_host = value;
      args_checks[0] += 1;
    } else if (strcmp(key, "--mqtt-port") == 0) {
      data->mosquitto_port = atoi(value);
      args_checks[1] += 1;
    } else if (strcmp(key, "--mqtt-username") == 0) {
      data->mosquitto_username = value;
      args_checks[2] += 1;
    } else if (strcmp(key, "--mqtt-password") == 0) {
      data->mosquitto_password = value;
      args_checks[3] += 1;
    } else if (strcmp(key, "--gpio-modes-file") == 0) {
      data->gpio_modes_filepath = value;
      args_checks[4] += 1;
    } else if (strcmp(key, "--username") == 0) {
      data->username = value;
      args_checks[5] += 1;
    } else if (strcmp(key, "--device-name") == 0) {
      data->device_name = value;
      args_checks[6] += 1;
    } else {
      fprintf(stderr, "[error] unknown arg %s\n", key);
      print_help();
      exit(EXIT_CODE_UNKNOWN_ARG);
    }
  }

  for (int i = 0; i < 7; i++) {
    if (args_checks[i] != 1) {
      fprintf(stderr, "[error] missing required arg %d\n", i);
      print_help();
      exit(EXIT_CODE_UNKNOWN_ARG);
    }
  }
}

void gpio2mosquitto_start(gpio2mosquitto_t *data) {
  data->state = STATE_STARTED;
  init_mosquitto(data);
  read_gpio_modes(data);
  apply_gpio_modes(data);
  publish_self_state(data);
  mosquitto_message_callback_set(data->mosquitto, &on_message);
}
