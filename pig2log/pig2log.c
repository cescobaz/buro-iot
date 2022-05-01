#include <pigpio.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define OUTPUT_SIZE 100

int main(int argc, char *argv[]) {
  uint32_t interested_mask = argc > 0 ? strtol(argv[1], NULL, 0) : 0xFFFFFFFF;
  fprintf(stderr, "%X\n", interested_mask);
  char output[OUTPUT_SIZE];
  gpioReport_t notification;
  size_t notification_size = sizeof(gpioReport_t);
  size_t buffer_size = notification_size * 1000;
  void *buffer = malloc(buffer_size);
  int data_start = 0;
  int data_end = 0;
  int r = 0;
  uint32_t previous_notification_level = 0;
  do {
    size_t free_buffer_size = buffer_size - data_end;
    if (free_buffer_size < notification_size) {
      memcpy(buffer, buffer + data_start, data_end - data_start);
      data_end -= data_start;
      data_start = 0;
      free_buffer_size = buffer_size - data_end;
    }
    r = read(0, buffer + data_end, free_buffer_size);
    data_end += r;
    while ((data_end - data_start) >= notification_size) {
      memcpy(&notification, buffer + data_start, notification_size);
      data_start += notification_size;
      fprintf(stderr, "%d %d %d %X\n", notification.seqno, notification.flags, notification.tick, notification.level);
      uint32_t notification_level = interested_mask & notification.level;
      uint32_t level_changed_mask = notification_level ^ previous_notification_level;
      uint32_t mask = 1;
      int gpio = 0;
      while (gpio < 32) {
	if ((level_changed_mask & mask) != 0) {
          int level = (notification_level & mask) == 0 ? 0 : 1;
          int len = snprintf(output, OUTPUT_SIZE, "%d %d\n\r", gpio, level);
	  write(1, output, len);
	  fsync(1);
	}
	gpio += 1;
	mask = mask << 1;
      }
      printf("\n");
      previous_notification_level = notification_level;
    }
  } while (r > 0);
  return 0;
}
