# MQTT Subscription Tool
[![GitHub sourcecode](https://img.shields.io/badge/Source-GitHub-green)](https://github.com/pvtom/submqtt/)
[![GitHub last commit](https://img.shields.io/github/last-commit/pvtom/submqtt)](https://github.com/pvtom/submqtt/commits)
[![GitHub issues](https://img.shields.io/github/issues/pvtom/submqtt)](https://github.com/pvtom/submqtt/issues)
[![GitHub pull requests](https://img.shields.io/github/issues-pr/pvtom/submqtt)](https://github.com/pvtom/submqtt/pulls)
[![GitHub](https://img.shields.io/github/license/pvtom/submqtt)](https://github.com/pvtom/submqtt/blob/main/LICENSE)

![Image](submqtt.png)

submqtt is a command-line tool that acts as an MQTT client and displays MQTT data in a table format.
You can subscribe to multiple topics. Topics, payloads and timestamps can be filtered.

The display can be customized in terms of color scheme. New and changed topics are highlighted in color. In addition, specific topics can be further highlighted. 

## Start the docker container

For further help visit [Docker](DOCKER.md).
 
```
# Example 1
docker run --rm -it -e TZ=Europe/Berlin -e MQTT_HOST=your_mqtt_broker -e COLOR=blue-screen pvtom/submqtt:latest

# Example 2
docker run --rm -it -e TZ=Europe/Berlin -e MQTT_HOST=your_mqtt_broker -e TOPIC_01=pv/+/power -e TOPIC_02=smarthome/# -e SUB=true -e HIGHLIGHT="power|temperature" -e COLOR=cyan pvtom/submqtt:latest
```

## Start submqtt locally

```
submqtt --help
```

```
usage: submqtt

	MQTT broker:
		-h <host> (default: localhost)
		-p <port> (default: 1883)
		--client_id <client_id>
		--user <user>
		--password <password>
		-q <0..2> QOS (default: 0)

	TLS mode:
		--tls use TLS
		--cafile <ca.crt>
		--capath <capath>
		--certfile <client.crt>
		--keyfile <client.key>
		--tls_password <password>

	Filters: (can occur multiple times and in every combination)
		-t --topic <topic> will be subscribed
		-f --filter <regex> matching topics will be filtered out
		-l --payload <regex> matching payloads will be printed
		-ts --timestamp <regex> matching timestamps will be printed
		-s --search <regex> term to search in the topic (needs a corresponding -r value, can occur multiple times)
		-r --replace <string> which replaces the term in the topic (needs a corresponding -s value, can occur multiple times)

	Output:
		--heat shows changes highlighted
		--highlight highlight topics by regex
		--underline underline highlighted topics
		--outdate <number of seconds> clear buffered data
		--sub add the matching subscribed topic to the output
		--unsorted output of the buffered table

	Color Sets:
		--white --blue --cyan --red --green --magenta --yellow --blue-screen
		--terminal-white --terminal-blue --terminal-cyan --terminal-red
		--terminal-green --terminal-magenta --terminal-yellow --terminal

	Control:
		--lock_q lock 'q' key

	Interactive Controls:
		Press the 'h' key for an overview of the interactive commands
```

## Examples

```
# Colored table
submqtt -h localhost --topic "pv/+/power" --topic "pv/battery/soc" --sub --cyan

# Colored table, delete one day old entries
submqtt -h localhost --topic "pv/day/#" --green --outdate 86400

# Using TLS
submqtt -h 192.168.178.123 -p 8883 --tls --cafile ~/ca.crt --certfile ~/client.crt --keyfile ~/client.key
```

## Used external libraries
- Eclipse Mosquitto (https://github.com/eclipse/mosquitto)
- Ncurses (https://invisible-island.net/ncurses)
