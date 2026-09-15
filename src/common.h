#ifndef __COMMON_H__
#define __COMMON_H__

#ifndef bool
typedef unsigned int bool;
#endif

#define TRUE 1
#define FALSE 0

// Get the name of the Bluetooth speaker. In the example case, a Bose speaker
// $ pactl list sinks short
//#define SPEAKER_NAME "bluez_output.08_DF_1F_00_1E_49.1"
#define RPI_SPEAKER_NAME "bluez_output.08_DF_1F_00_1E_49.1"
#define UNOQ_SPEAKER_NAME "bluez_output_08_DF_1F_7E_B2_64.1" 
#define SAMPLE_RATE 48000
//#define SAMPLE_RATE 44100
#define CHANNELS 2
#define FREQUENCY 440.0
//#define VOLUME  0.25f
//#define VOLUME 0.15f
#define VOLUME 0.05f
#define VOLUME_CHANGE 0.01f
#define TONE 0.05f
#define TONE_CHANGE 0.01f
#define START_SEED (uint32_t)589765974UL
#define PORT 9000
#define BUFFER_SIZE 2048
#define RESPONSE_SIZE 1024
#define PWIRE_THREAD 1
#define HTTPD_THREAD 2
#define MAXFDS 2
#define API_VOL_UP "/api/vup"
#define API_VOL_DOWN "/api/vdown"
#define API_TONE_UP "/api/tup"
#define API_TONE_DOWN "/api/tdown"

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

extern char speaker_name[64];

extern void *pwire_start(void *arg);
extern void *httpd_start(void *arg);
extern int report_error(int, int, int);
extern float pw_vol_inc(int);
extern float pw_vol_dec(int);
extern float pw_get_vol(void);
extern float pw_tone_inc(int);
extern float pw_tone_dec(int);
extern float pw_get_tone(void);

#endif // __COMMON_H__
