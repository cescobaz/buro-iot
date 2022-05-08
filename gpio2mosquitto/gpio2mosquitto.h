#include <mosquitto.h>

#define STATE_STOPPED 0
#define STATE_STARTED 1

#define GPIO_MODES_SIZE 54
#define GPIO_MODE_INPUT 'i'
#define GPIO_MODE_OUTPUT 'o'
#define GPIO_MODE_NONE '_'

typedef struct {
  int state;
  int argc;
  char **argv;
  char *hostname;
  char *mosquitto_client_id;
  char *mosquitto_host;
  int mosquitto_port;
  char *mosquitto_username;
  char *mosquitto_password;
  struct mosquitto *mosquitto;
  char* gpio_modes_filepath;
  char gpio_modes[GPIO_MODES_SIZE];
} gpio2mosquitto_t;

void gpio2mosquitto_init(gpio2mosquitto_t* data);
void gpio2mosquitto_cleanup(gpio2mosquitto_t* data);

void gpio2mosquitto_set_by_args(gpio2mosquitto_t* data, int argc, char *argv[]);
void gpio2mosquitto_start(gpio2mosquitto_t* data);
