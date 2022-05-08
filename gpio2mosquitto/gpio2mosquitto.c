#include <pigpio.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <mosquitto.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include "gpio2mosquitto.h"

#define DEFAULT_STR_SIZE 200

#define EXIT_CODE_UNKNOWN_ARG 1
#define EXIT_CODE_GPIO_INITIALISATION_FAILED 2
#define EXIT_CODE_MOSQUITTO_INITIALISATION_FAILED 3

int write_gpio_modes(gpio2mosquitto_t* data) {
  FILE *file = fopen(data->gpio_modes_filepath, "w");
  if (file == NULL) {
    return 2;
  }
  size_t result = fwrite(data->gpio_modes, sizeof(char), GPIO_MODES_SIZE, file);
  fclose(file);
  return result == 0;
}

int read_gpio_modes(gpio2mosquitto_t* data) {
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

struct mosquitto* init_mosquitto(gpio2mosquitto_t *data) {
  cleanup_mosquitto(data);
  struct mosquitto *mosquitto = mosquitto_new(data->mosquitto_client_id, true, data);
  data->mosquitto = mosquitto;
  int result;
  if (data->mosquitto_username && data->mosquitto_password) {
    result = mosquitto_username_pw_set(mosquitto, data->mosquitto_username, data->mosquitto_password);
    assert(result == MOSQ_ERR_SUCCESS);
  }
  result = mosquitto_tls_set(mosquitto, NULL, "/etc/ssl/certs", NULL, NULL, NULL);
  assert(result == MOSQ_ERR_SUCCESS);
  result = mosquitto_connect(mosquitto, data->mosquitto_host, data->mosquitto_port, 60);
  assert(result == MOSQ_ERR_SUCCESS);
  result = mosquitto_loop_start(mosquitto);
  assert(result == MOSQ_ERR_SUCCESS);
  printf("[info] mosquitto initialized %s %d\n", data->mosquitto_host, data->mosquitto_port);
  return mosquitto;
}


void eventFuncEx(int event, int level, uint32_t tick, void *userdata) {
  time_t now = time(NULL);
  printf("[info] event callback: event: %u, level: %d, tick: %u\n", event, level, tick);
  gpio2mosquitto_t *data = (gpio2mosquitto_t *)userdata;
  char topic[DEFAULT_STR_SIZE];
  snprintf(topic, DEFAULT_STR_SIZE, "buro/things/%s.gpio.%u", data->hostname, event);
  char payload[DEFAULT_STR_SIZE];
  snprintf(payload, DEFAULT_STR_SIZE, "{\"ut\":%ld,\"gpio\":%u,\"value\":%d}", now, event, level);
  int result;
  int max_retry = 1;
  do {
    result = mosquitto_publish(data->mosquitto, NULL, topic, strlen(payload), payload, 0, false);
    max_retry -= 1;
    if (result == MOSQ_ERR_SUCCESS) {
      printf("[info] published message: topic %s message %s\n", topic, payload);
      return;
    }
    if (result != MOSQ_ERR_SUCCESS) {
      fprintf(stderr, "mosquitto_publish result = %d, errno %u\n", result, errno);
      printf("[info] reconnecting to mqtt\n");
      result = mosquitto_reconnect(data->mosquitto);
      if (result != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "mosquitto_reconnect result = %d, errno %u\n", result, errno);
        return;
      }
    }
  }
  while (max_retry > 0);
}

void set_gpio(gpio2mosquitto_t *data) {
  for (int gpio = 0; gpio < GPIO_MODES_SIZE; gpio++) {
    char mode = data->gpio_modes[gpio];
    if (mode != GPIO_MODE_INPUT && mode != GPIO_MODE_OUTPUT) {
      gpioSetAlertFuncEx(gpio, NULL, NULL);
      printf("[info] unwatch gpio %u\n", gpio);
      continue;
    }
    uint8_t pimode = mode == GPIO_MODE_INPUT ? PI_INPUT : PI_OUTPUT;
    gpioSetAlertFuncEx(gpio, NULL, NULL);
    int result = gpioSetMode(gpio, pimode);
    if (result == 0) {
      gpioSetAlertFuncEx(gpio, &eventFuncEx, data);
      char* mode_s = mode == GPIO_MODE_INPUT ? "input" : "output";
      printf("[info] set and watch %s gpio %u\n", mode_s, gpio);
    } else {
      char* error_msg = result == PI_BAD_GPIO ? "PI_BAD_GPIO" : "PI_BAD_MODE";
      fprintf(stderr, "unable to gpioSetMode(%d, %d): %s\n", gpio, pimode, error_msg);
    }
  }
}

void unwatch_all_gpio(gpio2mosquitto_t* data) {
  for (int gpio = 0; gpio < GPIO_MODES_SIZE; gpio++) {
    gpioSetAlertFuncEx(gpio, NULL, NULL);
    printf("[info] unwatch gpio %u\n", gpio);
  }
}

void gpio2mosquitto_init(gpio2mosquitto_t *data) {
  memset(data, 0, sizeof(gpio2mosquitto_t));
  data->hostname = malloc(DEFAULT_STR_SIZE);
  gethostname(data->hostname, DEFAULT_STR_SIZE);
  data->mosquitto_port = 1883;
  memset(data->gpio_modes, '_', GPIO_MODES_SIZE);
}
void gpio2mosquitto_cleanup(gpio2mosquitto_t *data) {
  data->state = STATE_STOPPED;
  unwatch_all_gpio(data);
  cleanup_mosquitto(data);
  write_gpio_modes(data);
  free(data->hostname);
  memset(data, 0, sizeof(gpio2mosquitto_t));
}

void gpio2mosquitto_set_by_args(gpio2mosquitto_t *data, int argc, char *argv[]) {
  data->argc = argc;
  data->argv = argv;
  for (int i = 1; i < argc - 1; i += 2) {
    char *key = argv[i];
    char *value = argv[i+1];
    if (strcmp(key, "--client-id") == 0) {
      data->mosquitto_client_id = value;
    } else if (strcmp(key, "--host") == 0) {
      data->mosquitto_host = value;
    } else if (strcmp(key, "--port") == 0) {
      data->mosquitto_port = atoi(value);
    } else if (strcmp(key, "--username") == 0) {
      data->mosquitto_username = value;
    } else if (strcmp(key, "--password") == 0) {
      data->mosquitto_password = value;
    } else if (strcmp(key, "--gpio-file") == 0) {
      data->gpio_modes_filepath = value;
    } else if (strcmp(key, "--gpio-input") == 0) {
      uint8_t gpio = atoi(value);
      data->gpio_modes[gpio] = GPIO_MODE_INPUT;
    } else if (strcmp(key, "--gpio-output") == 0) {
      uint8_t gpio = atoi(value);
      data->gpio_modes[gpio] = GPIO_MODE_OUTPUT;
    } else {
      fprintf(stderr, "[error] unknown arg %s\n", key);
      exit(EXIT_CODE_UNKNOWN_ARG);
    }
  }
  write_gpio_modes(data);
}

void gpio2mosquitto_start(gpio2mosquitto_t* data) {
  data->state = STATE_STARTED;
  init_mosquitto(data);
  read_gpio_modes(data);
  set_gpio(data);

  time_t now = time(NULL);
  char topic[DEFAULT_STR_SIZE];
  snprintf(topic, DEFAULT_STR_SIZE, "buro/things/%s", data->hostname);

  char gpio[GPIO_MODES_SIZE + 1];
  memcpy(gpio, data->gpio_modes, GPIO_MODES_SIZE);
  gpio[GPIO_MODES_SIZE] = '\0';

  char payload[DEFAULT_STR_SIZE];
  snprintf(payload, DEFAULT_STR_SIZE, "{\"ut\":%ld,\"hostname\":%s,\"gpio\":\"%s\"}", now, data->hostname, gpio);
  mosquitto_publish(data->mosquitto, NULL, topic, strlen(payload), payload, 0, false);
}
