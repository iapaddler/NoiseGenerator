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
 * CPU: /proc/stat
 * cpu  user nice system idle iowait irq softirq steal guest guest_nice
 */
static double
sys_cpu()
{
    return(double)12.2;
}

/*
 * Memory Total: /proc/meminfo
 * MemTotal
 */
static double
sys_mem_total()
{
    return(double)15620184 / 1000000;
}

/*
 * Memory Free: /proc/meminfo
 * MemFree
 */
static double
sys_mem_free()
{
    return(double)5875700 / 1000000;
}

char *
sys_api(char *buffer, ssize_t bytes_recvd)
{
    char *api, *therms;
    size_t slen = strlen("GET ");
    double cpu, mem_free, mem_tot;

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
            mem_tot = sys_mem_total();
            mem_free = sys_mem_free();
            therms = sys_thermals();

            if (snprintf(response, (size_t)RESPONSE_SYS_SIZE,
                         "{\"cpu_percent\":%f, \"memory_free\":%.2f, \"memory_total\":%.2f, \"thermals\":%s}", cpu, mem_free, mem_tot, therms) < 0) {
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
