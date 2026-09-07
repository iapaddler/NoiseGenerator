#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

// Get the name of the Bluetooth speaker. In the example case, a Bose speaker
// $ pactl list sinks short
#define SPEAKER_NAME "bluez_output.08_DF_1F_00_1E_49.1"
#define SAMPLE_RATE 48000
//#define SAMPLE_RATE 44100
#define CHANNELS 2
#define FREQUENCY 440.0
#define VOLUME  0.25f
//#define VOLUME 0.15f
#define START_SEED (uint32_t)589765974UL

struct app {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    double phase;
};

// Variable for "quick and dirt" PRNG
static uint32_t rqd_seed = 0UL;

// low pass filter
static float filtered = 0.0f;

/**
 * Init the First Seed
 *
 * \start_seed      Started seed
 */
void srandqd(uint32_t start_seed)
{
    rqd_seed = start_seed;
}

/**
 * Return a random sample in the range [0, 2^32 - 1].
 * look in  "Numerical Recipes in C" Second Edition 
 * for the numbers explanation
 *
 * \return      Random number between [0, 2^32 - 1]
 */
int16_t
randqd_int16()
{    
    rqd_seed = (uint32_t) (1664525UL * rqd_seed + 1013904223UL);
    return (int16_t)(rqd_seed >> 16);
}

float 
get_wnoise()
{
    return (float)randqd_int16() / 32768.0f;
}

/*
  Digital filter:
  0.01  → very soft / slowly changing
  0.05  → softer noise
  0.10  → moderately filtered
  0.20  → still fairly bright
  1.00  → original white noise

  desired cutoff at a 48hz sampling rate
  -------------------------
   100 Hz           0.013
   500 Hz           0.063
  1000 Hz           0.123
  2000 Hz           0.230
  5000 Hz           0.486
 10000 Hz           0.730
*/
float
get_soft_noise(void)
{
    float input = get_wnoise();

    filtered += 0.05f * (input - filtered);

    return filtered;
}

static
void process(void *userdata)
{
    struct app *app = userdata;
    struct pw_buffer *buffer;
    struct spa_buffer *spa_buffer;

    int16_t *dst;
    uint32_t n_frames;
    uint32_t stride;

    buffer = pw_stream_dequeue_buffer(app->stream);
    if (!buffer)
        return;

    spa_buffer = buffer->buffer;

    dst = spa_buffer->datas[0].data;
    if (!dst)
        return;

    stride = sizeof(int16_t) * CHANNELS;

    n_frames = spa_buffer->datas[0].maxsize / stride;

    if (buffer->requested)
        n_frames = SPA_MIN(buffer->requested, n_frames);

    for (uint32_t i = 0; i < n_frames; i++) {
        float noise = get_soft_noise();
        int16_t sample = (int16_t)(noise * VOLUME * 32767.0f);
        app->phase += 2.0 * M_PI * FREQUENCY / SAMPLE_RATE;

        if (app->phase >= 2.0 * M_PI)
            app->phase -= 2.0 * M_PI;

        /* stereo */
        *dst++ = sample;
        *dst++ = sample;
    }

    spa_buffer->datas[0].chunk->offset = 0;
    spa_buffer->datas[0].chunk->stride = stride;
    spa_buffer->datas[0].chunk->size = n_frames * stride;

    pw_stream_queue_buffer(app->stream, buffer);
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = process,
};

int
main(int argc, char *argv[])
{
    struct app app = { 0 };
    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct spa_pod_builder builder =
        SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    pw_init(&argc, &argv);

    app.loop = pw_main_loop_new(NULL);

    app.stream = pw_stream_new_simple(
        pw_main_loop_get_loop(app.loop),
        "c-pipewire-test",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Playback",
            PW_KEY_MEDIA_ROLE, "Music",
            PW_KEY_TARGET_OBJECT,
            SPEAKER_NAME,
            NULL),
        &stream_events,
        &app
        );

    params[0] = spa_format_audio_raw_build(
        &builder,
        SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT(
            .format = SPA_AUDIO_FORMAT_S16,
            .channels = CHANNELS,
            .rate = SAMPLE_RATE
        )
    );

    pw_stream_connect(
        app.stream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT |
        PW_STREAM_FLAG_MAP_BUFFERS |
        PW_STREAM_FLAG_RT_PROCESS,
        params,
        1
    );

    srandqd(START_SEED);

    printf("Playing 440 Hz...\n");
    printf("Press Ctrl-C to stop.\n");

    pw_main_loop_run(app.loop);

    pw_stream_destroy(app.stream);
    pw_main_loop_destroy(app.loop);

    return 0;
}
