#include <pigpio.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <mosquitto.h>
#include <assert.h>

#define EXIT_CODE_GPIO_INITIALISATION_FAILED 1
#define EXIT_CODE_MOSQUITTO_INITIALISATION_FAILED 2

typedef struct {
  int argc;
  char **argv;
  char *hostname;
  struct mosquitto *mosquitto;
} gpio2mosquitto_t;

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

void eventFuncEx(int event, int level, uint32_t tick, void *userdata) {
  printf("event: %u, level: %d, tick: %lu\n", event, level, tick);
  gpio2mosquitto_t *data = userdata;
  char payload[100];
  snprintf(payload, 100, "gpio=%u&value=%d", event, level);
  int result = mosquitto_publish(data->mosquitto, NULL, "buro/raspi-1/things\0", strlen(payload), payload, 0, false);
  if (result != MOSQ_ERR_SUCCESS) {
    fprintf(stderr, "mosquitto_publish result = %d\n", result);
  }
}

int main(int argc, char *argv[]) {
  init();
  gpio2mosquitto_t data;
  data.argc = argc;
  data.argv = argv;
  char hostname[100];
  gethostname(hostname, 100);
  data.hostname = hostname;
  int result = 0;

  struct mosquitto *mosquitto = mosquitto_new("da-codice-c", true, NULL);
  data.mosquitto = mosquitto;
  result = mosquitto_username_pw_set(mosquitto, "buro", "ciao");
  assert(result == MOSQ_ERR_SUCCESS);
  result = mosquitto_connect(mosquitto, "192.168.1.102", 1883, 30);
  assert(result == MOSQ_ERR_SUCCESS);

  for (int i = 1; i < argc; i++) {
    uint8_t gpio = atoi(argv[i]);
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
  terminate();
  return 0;
}
