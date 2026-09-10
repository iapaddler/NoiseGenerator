#ifndef __COMMON_H__
#define __COMMON_H__

// Get the name of the Bluetooth speaker. In the example case, a Bose speaker
// $ pactl list sinks short
#define SPEAKER_NAME "bluez_output.08_DF_1F_00_1E_49.1"
#define SAMPLE_RATE 48000
//#define SAMPLE_RATE 44100
#define CHANNELS 2
#define FREQUENCY 440.0
//#define VOLUME  0.25f
#define VOLUME 0.15f
#define START_SEED (uint32_t)589765974UL
#define PORT 9000
#define BUFFER_SIZE 2048
#define PWIRE_THREAD 1
#define HTTPD_THREAD 2
#define MAXFDS 2

struct app {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    double phase;
};

struct cmd_args {
    int *argc;
    char ***argv;
    int efd;
};

struct error_event {
    int thread_id;
    int error;
};

extern void *pwire_start(void *arg);
extern void *httpd_start(void *arg);
extern int report_error(int, int, int);

#endif // __COMMON_H__
