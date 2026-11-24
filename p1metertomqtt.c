// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

/**
 * P1 meter to MQTT: read a HomeWizard P1 meter (HWE-P1, the electricity meter's P1/HAN port) over its local API every interval,
 * and publish its data as it comes, with 'ts' (unix time it was read) added
 */

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MQTT_CONNECT_TIMEOUT 60
#define MQTT_PUBLISH_QOS     0
#define MQTT_PUBLISH_RETAIN  false
#include "include/mqtt_linux.h"

#include "include/config_linux.h"

#include "include/http_linux.h"

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

#define CONFIG_FILE_DEFAULT "p1metertomqtt.cfg"

#define P1METER_URL_DEFAULT "http://p1meter.local/api/v1/data"
#define INTERVAL_DEFAULT    30

#define MQTT_SERVER_DEFAULT "mqtt://localhost"
#define MQTT_CLIENT_DEFAULT "p1metertomqtt"
#define MQTT_TOPIC_DEFAULT  "sensors/p1meter"

#define RESPONSE_SIZE_MAX   8192

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

const struct option config_options[] = {
    { "config", required_argument, 0, 0 },      // config
    { "mqtt-server", required_argument, 0, 0 }, // mqtt
    { "mqtt-client", required_argument, 0, 0 }, // mqtt
    { "mqtt-topic", required_argument, 0, 0 },  // mqtt
    { "p1meter-url", required_argument, 0, 0 }, // p1meter
    { "interval", required_argument, 0, 0 },    // interval
    { "debug", required_argument, 0, 0 },       // debug
    { 0, 0, 0, 0 },
};

bool debug = false;
MqttConfig mqtt_config;
const char *mqtt_topic;
const char *p1meter_url;
int interval;

bool config(const int argc, const char *argv[]) {
    if (!config_load(CONFIG_FILE_DEFAULT, argc, argv, config_options))
        return false;
    debug = config_get_bool("debug", false);
    mqtt_config.server = config_get_string("mqtt-server", MQTT_SERVER_DEFAULT);
    mqtt_config.client = config_get_string("mqtt-client", MQTT_CLIENT_DEFAULT);
    mqtt_config.debug = debug;
    mqtt_topic = config_get_string("mqtt-topic", MQTT_TOPIC_DEFAULT);
    p1meter_url = config_get_string("p1meter-url", P1METER_URL_DEFAULT);
    interval = config_get_integer("interval", INTERVAL_DEFAULT);
    if (interval < 1)
        interval = 1;
    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

char p1meter_response[RESPONSE_SIZE_MAX];
char p1meter_message[RESPONSE_SIZE_MAX + 32];

// the meter's data, a JSON object: '{' ... '}' (with only whitespace around)
const char *json_object(char *text) {
    while (isspace((unsigned char)*text))
        text++;
    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1]))
        text[--length] = '\0';
    return length >= 2 && text[0] == '{' && text[length - 1] == '}' ? text : NULL;
}

// read the meter and publish; false if it can't be read
bool p1meter_publish(const time_t ts) {
    p1meter_response[0] = '\0';
    if (!http_get(p1meter_url, p1meter_response, sizeof(p1meter_response)))
        return false;
    const char *object = json_object(p1meter_response);
    if (object == NULL)
        return false;
    const char *rest = object + 1; // after the '{': empty object ('}') or members
    while (isspace((unsigned char)*rest))
        rest++;
    const int length = snprintf(p1meter_message, sizeof(p1meter_message), "{\"ts\":%lld%s%s", (long long)ts, *rest == '}' ? "" : ",", rest);
    if (length < 0 || (size_t)length >= sizeof(p1meter_message))
        return false;
    mqtt_send(mqtt_topic, p1meter_message, length);
    if (debug)
        printf("published '%s' (%d bytes)\n", mqtt_topic, length);
    return true;
}

// on the clock: interval 30 = at :00 and :30; a line when the meter stops answering, and when it answers again
void execute(volatile bool *running) {
    printf("executing (url='%s', interval=%d seconds, topic='%s')\n", p1meter_url, interval, mqtt_topic);
    time_t failing_since = 0;
    while (*running) {
        const time_t now = time(NULL), next = (now / interval + 1) * interval;
        while (*running && time(NULL) < next)
            sleep(1);
        if (!*running)
            break;
        if (p1meter_publish(next)) {
            if (failing_since != 0)
                printf("p1meter: answers again, after %lld seconds\n", (long long)(next - failing_since));
            failing_since = 0;
        } else if (failing_since == 0) {
            fprintf(stderr, "p1meter: can't be read ('%s'), will retry every %d seconds\n", p1meter_url, interval);
            failing_since = next;
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------

volatile bool running = true;

void signal_handler(int sig __attribute__((unused))) {
    if (running) {
        printf("stopping\n");
        running = false;
    }
}

int main(int argc, const char **argv) {
    setbuf(stdout, NULL);
    printf("starting\n");
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    if (!config(argc, argv)) {
        fprintf(stderr, "failed to load config\n");
        return EXIT_FAILURE;
    }
    if (!mqtt_begin(&mqtt_config)) {
        fprintf(stderr, "failed to connect mqtt\n");
        return EXIT_FAILURE;
    }
    http_begin();
    execute(&running);
    http_end();
    mqtt_end();
    printf("stopped\n");
    return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------------------------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------------------------
