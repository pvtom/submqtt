## Local installation
### Prerequisites

- An existing MQTT broker in your environment, e.g. a Mosquitto (https://mosquitto.org)
- submqtt needs the libraries ncurses and libmosquitto. To install it on a Raspberry Pi enter:

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
