#include <pigpio.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <mosquitto.h>
#include "gpio2mosquitto.h"

#define EXIT_CODE_UNKNOWN_ARG 1
#define EXIT_CODE_GPIO_INITIALISATION_FAILED 2
#define EXIT_CODE_MOSQUITTO_INITIALISATION_FAILED 3

int wait_for_any_signal() {
  sigset_t set;
  sigfillset(&set);
  int sig;
  sigwait(&set, &sig);
  printf("terminate because signal %d\n", sig);
  return sig;
}

int main(int argc, char *argv[]) {
  if (gpioInitialise() < 0) {
    return EXIT_CODE_GPIO_INITIALISATION_FAILED;
  }
  int result = mosquitto_lib_init();
  if (result != MOSQ_ERR_SUCCESS) {
    return EXIT_CODE_MOSQUITTO_INITIALISATION_FAILED;
  }
  gpio2mosquitto_t data;
  gpio2mosquitto_init(&data);
  gpio2mosquitto_set_by_args(&data, argc, argv);

  gpio2mosquitto_start(&data);

  wait_for_any_signal();

  gpio2mosquitto_cleanup(&data);
  mosquitto_lib_cleanup();
  gpioTerminate();
  return 0;
}
