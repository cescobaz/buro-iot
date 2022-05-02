#include <pigpio.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <mosquitto.h>
#include <assert.h>
#include <errno.h>

#define DEFAULT_STR_SIZE 100

#define EXIT_CODE_UNKNOWN_ARG 1
#define EXIT_CODE_GPIO_INITIALISATION_FAILED 2
#define EXIT_CODE_MOSQUITTO_INITIALISATION_FAILED 3

typedef struct {
  int argc;
  char **argv;
  char *hostname;
  char *mosquitto_client_id;
  char *mosquitto_host;
  int mosquitto_port;
  char *mosquitto_username;
  char *mosquitto_password;
  struct mosquitto *mosquitto;
  int gpio_in_count;
  uint8_t *gpio_in;
  int gpio_out_count;
  uint8_t *gpio_out;
} gpio2mosquitto_t;

void gpio2mosquitto_init(gpio2mosquitto_t *data) {
  memset(data, 0, sizeof(gpio2mosquitto_t));
  data->hostname = malloc(DEFAULT_STR_SIZE);
  data->mosquitto_port = 1883;
  data->gpio_in = malloc(sizeof(uint8_t) * 32);
  data->gpio_out = malloc(sizeof(uint8_t) * 32);
}
void gpio2mosquitto_cleanup(gpio2mosquitto_t *data) {
  free(data->hostname);
  free(data->gpio_in);
  free(data->gpio_out);
  memset(data, 0, sizeof(gpio2mosquitto_t));
}

void init() {
  if (gpioInitialise() < 0) {
    exit(EXIT_CODE_GPIO_INITIALISATION_FAILED);
  }
  int result = mosquitto_lib_init();
  if (result != MOSQ_ERR_SUCCESS) {
    exit(EXIT_CODE_MOSQUITTO_INITIALISATION_FAILED);
  }
}

void terminate() {
  mosquitto_lib_cleanup();
  gpioTerminate();
}

void read_args(gpio2mosquitto_t *data, int argc, char *argv[]) {
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
    } else if (strcmp(key, "--gpio-input") == 0) {
      uint8_t gpio = atoi(value);
      data->gpio_in[data->gpio_in_count] = gpio;
      data->gpio_in_count += 1;
    } else if (strcmp(key, "--gpio-output") == 0) {
      uint8_t gpio = atoi(value);
      data->gpio_out[data->gpio_out_count] = gpio;
      data->gpio_out_count += 1;
    } else {
      fprintf(stderr, "[error] unknown arg %s\n", key);
      exit(EXIT_CODE_UNKNOWN_ARG);
    }
  }
}

void eventFuncEx(int event, int level, uint32_t tick, void *userdata) {
  printf("[info] event callback: event: %u, level: %d, tick: %lu\n", event, level, tick);
  gpio2mosquitto_t *data = userdata;
  char payload[DEFAULT_STR_SIZE];
  snprintf(payload, DEFAULT_STR_SIZE, "gpio=%u&value=%d", event, level);
  char topic[DEFAULT_STR_SIZE];
  snprintf(topic, DEFAULT_STR_SIZE, "buro/%s/things/gpio-%u", data->hostname, event);
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

int main(int argc, char *argv[]) {
  init();
  gpio2mosquitto_t data;
  gpio2mosquitto_init(&data);
  read_args(&data, argc, argv);
  gethostname(data.hostname, DEFAULT_STR_SIZE);
  int result = 0;

  struct mosquitto *mosquitto = mosquitto_new(data.mosquitto_client_id, true, &data);
  data.mosquitto = mosquitto;
  if (data.mosquitto_username && data.mosquitto_password) {
    result = mosquitto_username_pw_set(mosquitto, data.mosquitto_username, data.mosquitto_password);
    assert(result == MOSQ_ERR_SUCCESS);
  }
  result = mosquitto_tls_set(mosquitto, NULL, "/etc/ssl/certs", NULL, NULL, NULL);
  result = mosquitto_connect(mosquitto, data.mosquitto_host, data.mosquitto_port, 29);
  assert(result == MOSQ_ERR_SUCCESS);

  for (int i = 0; i < data.gpio_in_count; i++) {
    uint8_t gpio = data.gpio_in[i];
    printf("setting gpio %u\n", gpio);
    gpioSetMode(gpio, PI_INPUT);
    gpioSetAlertFuncEx(gpio, &eventFuncEx, &data);
  }

  sigset_t set;
  sigfillset(&set);
  int sig;
  sigwait(&set, &sig);
  printf("terminate because signal %d\n", sig);

  mosquitto_destroy(mosquitto);
  gpio2mosquitto_cleanup(&data);
  terminate();
  return 0;
}
