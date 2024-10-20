#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <string.h>

/*
 * event3 -> wireless keyboard
 * event4 -> laptop keyboard
*/

int main() {
    const char *device = "/dev/input/event1"; // Change this to your keyboard's event file
    int fd = open(device, O_WRONLY);
    
    if (fd == -1) {
        perror("Failed to open input device");
        return 1;
    }

    struct input_event event;
    memset(&event, 0, sizeof(struct input_event));
    
    event.type = EV_LED;
    event.code = LED_CAPSL;
    event.value = 1; // 1 to turn on, 0 to turn off

    // Turn on the Caps Lock LED
    if (write(fd, &event, sizeof(struct input_event)) == -1) {
        perror("Failed to turn on Caps Lock LED");
        close(fd);
        return 1;
    }

    // Sleep for 2 seconds
    sleep(2);

    // Turn off the Caps Lock LED
    event.value = 0;
    if (write(fd, &event, sizeof(struct input_event)) == -1) {
        perror("Failed to turn off Caps Lock LED");
        close(fd);
        return 1;
    }

    printf("Caps Lock LED toggled successfully!\n");
    close(fd);
    return 0;
}
