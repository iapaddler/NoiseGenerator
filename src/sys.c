#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>

#include "common.h"

struct thermal {
    char name[NAME_MAX];
    char path[PATH_MAX];
    double temp;
};

static struct thermal *g_therm;
static uint32_t g_num_thermals;

struct memory {
    double mtotal;
    double mfree;
};

static char *
sys_get_type(char *path)
{
    int zfd, rc;
    char *type;

    if ((type = calloc(1, NAME_MAX)) == NULL) {
        syslog(LOG_ERR, "calloc: %s", strerror(errno));
        return NULL;
    }


    zfd = open(path, O_RDONLY);
    if (zfd < 0) {
        syslog(LOG_ERR, "open: %s", strerror(errno));
        return NULL;
    }

    if ((rc = read(zfd, type, NAME_MAX)) <= 0) {
        syslog(LOG_ERR, "read: %s", strerror(errno));
        close(zfd);
        return NULL;
    }

    type[strlen(type) - 1] = '\0';
    close(zfd);

    return type;
}

static double
sys_get_temp(char *path)
{
    int zfd, rc;
    double temp;
    char *end;
    char tstr[NAME_MAX];

    zfd = open(path, O_RDONLY);
    if (zfd < 0) {
        syslog(LOG_ERR, "open: %s", strerror(errno));
        return (double)0.0;
    }

    if ((rc = read(zfd, tstr, NAME_MAX)) <= 0) {
        syslog(LOG_ERR, "read: %s", strerror(errno));
        close(zfd);
        return (double)0.0;
    }

    temp =  strtod((const char *)tstr, &end);
    if (end == tstr || errno == ERANGE) {
        syslog(LOG_ERR, "strtod: %s", strerror(errno));
        close(zfd);
        return (double)0.0;
    }

    close(zfd);

    //Convert from micro C to F
    temp = ((temp/1000) * 1.8) + 32;

    return temp;
}

static bool
__attribute__((unused)) sys_list_thermals(void)
{
    for (uint32_t i = 0; i < g_num_thermals; i++) {
        printf("Thermal(%d): %s is %.2f from %s\n", i, g_therm[i].name, g_therm[i].temp, g_therm[i].path);
    }

    return TRUE;
}

/*
 * Thermals: /sys/class/thermal/thermal_zone*
 * multiple zones each with a dir name type and a dir name temp
 * [{\"type\":\"cpu-thermal\",\"temp\":48.2},{\"type\":\"gpu-thermal\",\"temp\":45.6}]
 */
static char *
sys_thermals()
{
    double temp;
    char *therms;

    if ((therms = calloc(1, THERMAL_SIZE * g_num_thermals)) == NULL) {
        syslog(LOG_ERR, "calloc: %s", strerror(errno));
        return NULL;
    }

    therms[0] = '[';

    for (uint32_t i = 0; i < g_num_thermals; i++) {
        char entry[THERMAL_SIZE];

        // Read the current temp value
        temp = sys_get_temp(g_therm[i].path);

        snprintf(entry, THERMAL_SIZE, "{\"type\":\"%s\",\"temp\":%.2f},", g_therm[i].name, temp);
        strncat(therms, entry, THERMAL_SIZE);
    }

    size_t len = strnlen(therms, RESPONSE_SYS_SIZE);
    if (len > THERMAL_SIZE * g_num_thermals) {
        syslog(LOG_ERR, "strnlen: %s", strerror(errno));
        return NULL;
    } else {
        therms[len - 1] = ']';
        therms[len] = '\0';
    }

    return therms;
}

/*
 * Get an entry from a proc meminfo object.
 */
static double
sys_get_mentry(char *mentry, const char *instance)
{
    double mtotal;
    char *token, *entry, *value, *saveptr;
    const char *newline;
    char this_entry[BUFFER_SIZE];

    if ((entry = strcasestr((const char *)mentry, instance)) == NULL) {
        syslog(LOG_ERR, "strcasestr: %s", strerror(errno));
        return FALSE;
    }

    newline = strchr(entry, '\n');
    size_t elen = newline ? (size_t)(newline - entry) : strlen(entry);
    strncpy(this_entry, entry, elen);

    if ((value = strstr(this_entry, "kB")) == NULL) {
        syslog(LOG_ERR, "strstr: %s", strerror(errno));
        return FALSE;
    }
    *value = '\0';

    token = this_entry + strlen(instance);

    mtotal = strtod(token, &saveptr);
    return (double) (mtotal / (double)1000000);
}

/*
 * Get memory values: /proc/meminfo
 * Memory Free, Memory Total
 */
static bool
sys_mem(struct memory *mem)
{
    int zfd, rc;
    char mentry[BUFFER_SIZE];

    zfd = open(MEM_FILE, O_RDONLY);
    if (zfd < 0) {
        syslog(LOG_ERR, "open: %s", strerror(errno));
        return FALSE;
    }

    if ((rc = read(zfd, mentry, BUFFER_SIZE)) <= 0) {
        syslog(LOG_ERR, "read: %s", strerror(errno));
        close(zfd);
        return FALSE;
    }

    close(zfd);

    mem->mtotal = sys_get_mentry(mentry, "MemTotal:");

    mem->mfree = sys_get_mentry(mentry, "MemFree:");
    return TRUE;
}

/*
 * CPU: /proc/stat
 * cpu  user nice system idle iowait irq softirq steal guest guest_nice
 */
static double
sys_cpu()
{
    long hz, num_cores, interval;
    FILE *fs;
    char *cpu_line, *cpu_value, *saveptr, *end;
    double cpu, this_cpu, percentage;
    static double prev_cpu = 0.0;
    char cpu_entry[BUFFER_SIZE];

    hz = sysconf(_SC_CLK_TCK);
    num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    if ((fs = fopen(CPU_FILE, "r")) == NULL) {
        syslog(LOG_ERR, "fopen: %s", strerror(errno));
        return 0.0;
    }

    if ((cpu_line = fgets(cpu_entry, sizeof(cpu_entry), fs)) == NULL) {
        syslog(LOG_ERR, "fgets: %s", strerror(errno));
        return 0.0;
    }

    fclose(fs);

    cpu_value = strtok_r(cpu_line, "cpu ", &saveptr);
    cpu = 0.0;
    interval = hz * num_cores * UI_SAMPLE_RATE;

    // getting user, nice, system -> 3 entries
    for (int i = 0; i < 3; i++) {
        this_cpu =  strtod((const char *)cpu_value, &end);
        if (end == cpu_value || errno == ERANGE) {
            syslog(LOG_ERR, "strtod: %s", strerror(errno));
            continue;
        }

        cpu += this_cpu;
        cpu_value = strtok_r(NULL, " ", &saveptr);
    }

    if (prev_cpu != 0.0) {
        // (delta ticks / (ticks per sec * num_cores * sample interval)) * 100.0
        percentage = (((cpu - prev_cpu) / interval) * 100.0);
    } else {
        percentage = 0.0;
    }

    prev_cpu = cpu;
    return percentage;
}

char *
sys_api(char *buffer, ssize_t bytes_recvd)
{
    char *api, *therms;
    size_t slen = strlen("GET ");
    double cpu;
    struct memory mem;

    // If not a get method, ignore
    if (strncmp("GET ", buffer, slen) == 0) {
        api = buffer + slen;

        if (strncmp(API_SYSTEM, api, strlen(API_SYSTEM)) == 0) {
            char *response;

            if ((response = calloc(1, RESPONSE_SYS_SIZE)) == NULL) {
                syslog(LOG_ERR, "calloc: %s", strerror(errno));
                return NULL;
            }

            cpu = sys_cpu();

            if (sys_mem(&mem) == FALSE) {
                syslog(LOG_ERR, "ERROR:sys_mem");
                return NULL;
            }

            therms = sys_thermals();

            if (snprintf(response, (size_t)RESPONSE_SYS_SIZE,
                         "{\"cpu_percent\":%f, \"memory_free\":%.2f, \"memory_total\":%.2f, \"thermals\":%s}",
                         cpu, mem.mfree, mem.mtotal, therms) < 0) {
                syslog(LOG_ERR, "snprintf: %s", strerror(errno));
                free(response);
                return NULL;
            }

            free(therms);
            //printf("%s:%d response: %s\n", __FUNCTION__, __LINE__, response);
            return response;
        } 
    }
    return NULL;
}

/*
 * Read the thermal sources and size accordingly. These entries are static. Getting them once works.
 * Hence, the use of a dynamically sized array.
 */
bool
sys_init(void)
{
    int i;
    DIR *tdir;
    struct dirent *dent;

    g_num_thermals = 0;

    if ((tdir = opendir(THERMAL_DIR)) == NULL) {
        syslog(LOG_ERR, "opendir: %s", strerror(errno));
        return FALSE;
    }

    // Count the numer of thermal zone entries
    while ((dent = readdir(tdir)) != NULL) {
        if (strncmp(dent->d_name, THERMAL_ZONE, strlen(THERMAL_ZONE)) == 0) {
            g_num_thermals++;
        }
    }

    if (closedir(tdir) != 0) {
        syslog(LOG_ERR, "closedir: %s", strerror(errno));
        return FALSE;
    }

    // Alloc for the existing zones
    if ((g_therm = calloc(g_num_thermals, sizeof(struct thermal))) == NULL) {
        syslog(LOG_ERR, "calloc: %s", strerror(errno));
        return FALSE;
    }

    // Now that we have a count and space, populate
    if ((tdir = opendir(THERMAL_DIR)) == NULL) {
        syslog(LOG_ERR, "opendir: %s", strerror(errno));
        return FALSE;
    }

    i = 0;
    while ((dent = readdir(tdir)) != NULL) {
        if ((strncmp(dent->d_name, THERMAL_ZONE, strlen(THERMAL_ZONE)) == 0) && (i <= g_num_thermals)) {
            int nc;
            char *type;
            char zone_entry[PATH_MAX];

            // Get the zone type
            nc = snprintf(zone_entry, PATH_MAX, "%s%s/type", THERMAL_DIR, dent->d_name);
            if (nc < 0 || (size_t)nc >= sizeof zone_entry) {
                syslog(LOG_ERR, "snprintf: %s", strerror(errno));
                continue;
            }

            type = sys_get_type(zone_entry);
            strncpy(g_therm[i].name, type, NAME_MAX);
            free(type);

            // Get the zone temp
            snprintf(zone_entry, PATH_MAX, "%s%s/temp", THERMAL_DIR, dent->d_name);
            double temp = sys_get_temp(zone_entry);

            // Save the current temp and path values
            g_therm[i].temp =  temp;
            strncpy(g_therm[i].path, zone_entry, PATH_MAX);

            i++;
        }
    }

    if (closedir(tdir) != 0) {
        syslog(LOG_ERR, "closedir: %s", strerror(errno));
        return FALSE;
    }

    //sys_list_thermals();
    sys_thermals();
    return TRUE;
}
