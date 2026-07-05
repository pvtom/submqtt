## Local Installation
### Prerequisites

submqtt needs the libraries ncurses and libmosquitto.

```
sudo apt-get install -y build-essential git libmosquitto-dev ncurses-dev
```

### Cloning the Repository

```
git clone https://github.com/pvtom/submqtt.git
cd submqtt
```

### Compilation

```
make
```

## Usage

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
