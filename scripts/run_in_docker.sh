#!/bin/bash

# start socat in the background
socat TCP-LISTEN:8080,fork UNIX-CONNECT:/run/sc3/ui.socket &

# Start SC3 camera connector in simulation mode
/usr/bin/SC3-Camera-Connector -c /etc/sc3/config.json -s