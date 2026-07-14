/* This code is currently configured for beagleplay's gpio inorder to make it for beagleboneblack 
get more detail about this in implementation section 7

for beagleboneblack version of this code on the deployment changes the GPIOCHIP to /dev/gpiochip0
and GPIO_PIN to 28
*/

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <linux/gpio.h>


#define GPIO_CHIP   "/dev/gpiochip3"
#define GPIO_PIN    12
#define PULSE_NS    30000000L

static void print_elapsed_time(void)
{
    int                     secs, nsecs;
    static int              first_call = 1;
    struct timespec         curr;
    static struct timespec  start;

    if (first_call) {
        first_call = 0;
        if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
            err(EXIT_FAILURE, "clock_gettime");
    }

    if (clock_gettime(CLOCK_MONOTONIC, &curr) == -1)
        err(EXIT_FAILURE, "clock_gettime");

    secs = curr.tv_sec - start.tv_sec;
    nsecs = curr.tv_nsec - start.tv_nsec;
    if (nsecs < 0) {
        secs--;
        nsecs += 1000000000;
    }
    printf("%d.%09d:\t", secs, nsecs);
}

static int setup_gpio(void)
{
    int chip_fd;
    int pin_fd;
    static struct gpiohandle_request req;

    chip_fd = open(GPIO_CHIP,O_WRONLY|O_CLOEXEC);
    usleep(10000);
    if (chip_fd < 0)
    {
	printf("cant open the gpio %s \n",strerror(errno));
	return -1;
    }
   else
   {
	memset(&req , 0 , sizeof(req));
	req.lines=1;
	snprintf(req.consumer_label, sizeof(req.consumer_label), "pps-pulser");
	req.default_values[0]=0;
	req.flags=GPIOHANDLE_REQUEST_OUTPUT;
	req.lineoffsets[0]=GPIO_PIN;
	
	if(ioctl(chip_fd,GPIO_GET_LINEHANDLE_IOCTL,&req)<0)
	{
		printf("cant get the GPIO_GET_LINEHANDLE_IOCTL %s \n",strerror(errno));
		close(chip_fd);
		return -1;
	}
	else
	{
		close(chip_fd);
		pin_fd=req.fd;
		return pin_fd;
	}
   }
}

static void gpio_set(int pin_fd, int value)
{
  struct gpiohandle_data data;
  data.values[0]=value;
  if(ioctl(pin_fd,GPIOHANDLE_SET_LINE_VALUES_IOCTL,&data)<0)
  {
	  printf("cant set pin %s",strerror(errno));
	  return ;
  } 
  return ;
}

int main(int argc, char *argv[])
{
    int                 fd, line_fd;
    ssize_t             s;
    uint64_t            expir, tot_expir, max_expir;
    struct timespec     now;
    struct itimerspec   new_value;
    struct timespec     pulse_width;

    pulse_width.tv_sec  = 0;
    pulse_width.tv_nsec = PULSE_NS;

    if (argc != 2 && argc != 4) {
        fprintf(stderr, "Usage: %s init-secs [interval-secs max-num-expir]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    line_fd=setup_gpio();
    if(line_fd<0)
    {
		exit(EXIT_FAILURE);
    }
	
    if (clock_gettime(CLOCK_REALTIME, &now) == -1)
        err(EXIT_FAILURE, "clock_gettime");

    new_value.it_value.tv_sec  = now.tv_sec + atoi(argv[1]);
    new_value.it_value.tv_nsec = 0;

    if (argc == 2) {
        new_value.it_interval.tv_sec = 0;
        max_expir = 1;
    } else {
        new_value.it_interval.tv_sec = atoi(argv[2]);
        max_expir = atoi(argv[3]);
    }
    new_value.it_interval.tv_nsec = 0;

    fd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC);
    if (fd == -1)
     {
	   err(EXIT_FAILURE, "timerfd_create");
     }

    if (timerfd_settime(fd, TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &new_value, NULL) == -1)
    {
        err(EXIT_FAILURE, "timerfd_settime");
    }

    print_elapsed_time();
    printf("timer armed\n");

    for (tot_expir = 0; tot_expir < max_expir;) {
        
	s = read(fd, &expir, sizeof(uint64_t));
        if (s < 0) {
            if (errno == ECANCELED) {
                if (clock_gettime(CLOCK_REALTIME, &now) == -1)
                {
		   err(EXIT_FAILURE, "clock_gettime rearm");
		}
                new_value.it_value.tv_sec  = now.tv_sec + 1;
                new_value.it_value.tv_nsec = 0;
                if (timerfd_settime(fd, TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET, &new_value, NULL) == -1)
                {
		    err(EXIT_FAILURE, "timerfd_settime rearm");
                }
		continue;
            }
            err(EXIT_FAILURE, "read timerfd");
        }

        if (s != sizeof(uint64_t))
        {
	    err(EXIT_FAILURE, "read size mismatch");
	}

        gpio_set(line_fd, 1);
        nanosleep(&pulse_width, NULL);
        gpio_set(line_fd, 0);

        tot_expir += expir;
        print_elapsed_time();
        printf("read: %llu; total=%llu\n",
               (unsigned long long)expir,
               (unsigned long long)tot_expir);
    }

    close(fd);
    close(line_fd);
    exit(EXIT_SUCCESS);
}
