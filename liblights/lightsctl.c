#include <stdio.h>
#include <linux/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cutils/properties.h>

typedef uint8_t  u8;
#include "steelhead_avr.h"

struct avr_led_rgb_vals prepare_leds(unsigned int rgb) {
    struct avr_led_rgb_vals reg;
    int red, green, blue;

    reg.rgb[0] = (rgb >> 16) & 0xFF;
    reg.rgb[1] = (rgb >> 8) & 0xFF;
    reg.rgb[2] = rgb & 0xFF;

    return reg;
}

int main(int argc, char *argv[]) {

    int fd = open("/dev/leds",O_RDWR);
    int res = 0;
    unsigned int maxled = 0;
    char property[PROPERTY_VALUE_MAX];
    unsigned int rgb = 14428;   // 14428 is our default color
    unsigned int colorstrip = maxled;
    unsigned int ledcount = 0;
    unsigned int padding = 0;
    unsigned int ledpos = 0;
    unsigned int mutecolor = 0;
    struct avr_led_rgb_vals color;
    struct avr_led_set_range_vals *reg;

    ioctl(fd,AVR_LED_GET_MODE,&maxled);
    if (maxled != 0x1) {
        maxled = 0x1; //AVR_LED_MODE_HOST_AUTO_COMMIT
        ioctl(fd,AVR_LED_SET_MODE,&maxled);
    }

    res = ioctl(fd,AVR_LED_GET_COUNT,&maxled);

    // make an array AFTER we know the LED count
    unsigned int colors[maxled];

    if (!res) {
        printf("This device has %d LEDs\n",maxled);
    } else {
        printf("Failed to get LED count\n");
        return 1;
    }

    if (argc==1) {
        char *nexttok;
        /* use default */
        property_get("persist.sys.ringcolor", property, "14428");
        nexttok=strtok(property," ");
        ledcount=0;
        while (nexttok && ledcount<=maxled) {
            colors[ledcount] = atoi(nexttok);
            //printf("Set %d to %d\n",ledcount,colors[ledcount]);
            ledcount++;
            nexttok = strtok(NULL," ");
        }
    } else if (argc >= 3)
    {
        u8 start = atoi(argv[1]);
        u8 count = atoi(argv[2]);

        if ( start > maxled )
        {
            printf("Start LED [%d] out of range [0, %d]\n",
                    start, maxled);
            return 1;
        }
        if ( start + count > maxled )
        {
            printf("Operation will overflow maxled. (%d > %d)\n",
                    start+count, maxled);
            return 1;
        }

        // This struct can get kinda large
        // We allocate space for the struct itself and all of the RGB
        // structs that it may contain.
        // See http://gcc.gnu.org/onlinedocs/gcc/Zero-Length.html

        reg = (struct avr_led_set_range_vals *) 
            malloc( sizeof(struct avr_led_set_range_vals) +
                    sizeof(struct avr_led_rgb_vals) * count );

        reg->start = start;
        reg->count = count;
        reg->rgb_triples = count;

        // Let's make a handy-dandy pointer to where our colors should be
        char ** vptr = argv+3;
        if ( start == 0 )
        {
            mutecolor = atoi(*vptr);
            rgb = mutecolor;
            vptr++;
        }
        int i;
        for ( i = 0; i < count; i++ )
        {
            if( *vptr != 0 )
            {
                rgb = atoi(*vptr);
                vptr++;
            }
            // else: just repeat the last RGB value we had.
            // If we aren't passed any, just use the default
            reg->rgb_vals[i] = prepare_leds(rgb);
        }

        if ( start == 0 )
        {
            color = prepare_leds( mutecolor );
            ioctl(fd, AVR_LED_SET_MUTE, &color);
        }

        res = ioctl(fd, AVR_LED_SET_RANGE_VALS, reg);
        free(reg);

        close(fd);

        return !!res;

    }
    else {
        colors[0]=atoi(argv[1]);
        ledcount=1;
        while (ledcount < (unsigned int)argc-1 && ledcount<=maxled) {
            colors[ledcount]=atoi(argv[ledcount+1]);
            ledcount++;
        }
    }


    reg = malloc(sizeof(reg));

    colorstrip = (unsigned int)(maxled / ledcount);
    padding = maxled - (ledcount * colorstrip);

    /* Set the mute LED to the first listed color */
    color = prepare_leds(colors[0]);
    ioctl(fd,AVR_LED_SET_MUTE,&color);

    for (ledpos=0; ledpos < ledcount; ledpos++) {
        unsigned int i = 0;
        rgb = (colors[ledpos]);
        color = prepare_leds(rgb);
        for (i=0; i<colorstrip; i++) {
            //printf("Setting %d to 0x%.6x\n",((ledpos*colorstrip)+i),rgb);
            reg->rgb_vals[(ledpos*colorstrip)+i] = color;
        }
    }

    /* Fill in the remainder, if any */
    for (ledpos=padding; ledpos > 0; ledpos--) {
        reg->rgb_vals[maxled-ledpos] = color;
        //printf("Setting %d to 0x%.6x\n",maxled-ledpos,rgb);
    }

    reg->start = 0;
    reg->count = maxled;
    reg->rgb_triples = maxled;
    res = ioctl(fd,AVR_LED_SET_RANGE_VALS,reg);
    free(reg);

    close(fd);
    return (res ? 1 : 0);
}

