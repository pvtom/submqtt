## Docker

### Environment Variables

| Variable | Remarks | Values |
| --- | --- | --- |
| MQTT_HOST | Hostname of the MQTT Broker | "localhost" |
| MQTT_PORT | Port of the MQTT Broker | 0 - 65535 |
| MQTT_USER | MQTT User | "tom" |
| MQTT_PASSWORD | MQTT Password | "secret" |
| MQTT_CLIENT_ID | MQTT CID  | "subm" |
| MQTT_QOS | QOS | 0 - 2 |
| TLS | Use TLS | true/false |
| TLS_CAFILE | TLS configuration | tls/ca.crt |
| TLS_CAPATH | TLS configuration | tls/capath |
| TLS_CERTFILE | TLS configuration  | tls/client.crt |
| TLS_KEYFILE | TLS configuration | tls/client.key |
| TLS_PASSWORD | TLS configuration | "tlssecret" |
| TZ | Local Time Zone | "Europe/Berlin" |
| SUB | Show sub column  | true/false |
| UNSORTED | unsorted data | true/false |
| CLEANUP | replace non-printable characters in payload | true/false |
| HEAT | highlight the newest updated topics | true/false |
| HIGHLIGHT | topic to be highlighted (regex) | "power" |
| OUTDATE | Number of seconds to drop old data sets | 86400 |
| UNDERLINE | underline highlighted lines | true/false |
| LOCK_Q | disable stopping the program with key q | true/false |
| TOPIC* | Subscribe topic | "pv/+/power" |
| FILTER* | Filter out (regex) | |
| PAYLOAD* | Filter by payload (regex) | |
| TIMESTAMP* | Filter by timestamp (regex) | |
| SEARCH* | search string (regex) in topic and replace by the following REPLACE | |
| REPLACE* | replace the matching SEARCH by this string | |
| COLOR | Color Set | white, blue, green, cyan, magenta, red, yellow, blue-screen |
| COLOR | Color Set | terminal-white, terminal-cyan, terminal-green, terminal-blue |
| COLOR | Color Set | terminal-redm terminal-yellowm terminal-magentam terminal |

*) multiple variables are allowed like TOPIC_01, TOPIC_02, ...

### Examples

```
# Example 1
docker run --rm -it -e TZ=Europe/Berlin -e MQTT_HOST=your_mqtt_broker -e COLOR=blue-screen pvtom/submqtt:latest

# Example 2
docker run --rm -it -e TZ=Europe/Berlin -e MQTT_HOST=your_mqtt_broker -e TOPIC_01=pv/+/power -e TOPIC_02=smarthome/# -e SUB=true -e HIGHLIGHT="power|temperature" -e COLOR=cyan pvtom/submqtt:latest
```
