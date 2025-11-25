# SC3 Camera Connector

# Requirements

Need installed the following:

`libk4a1.4`
`libk4a1.4-dev` (Not for prod)

(both of which can be gotten from the microsoft repos, which is provided in the .deb package in scripts/ or can build and use the docker image for all deps).

`libopencv-dev`
( Probably install only what is linked in prod, i.e. libopencv4-xxxxx )

`libcurl4-openssl-dev` (libcurl4t64 for prod)

`libudev-dev`(libudev1 for prod)

Also build tools such as 
`make`
`g++`
`cmake`

# How to Build
`mkdir build && cd build`
`cmake ..`
`make`

# How to run
Need to run as root, or with sufficient permissions to access Azure kinect usb devices.

`sudo ./SC3-Camera-Connector`

# Troubleshooting
## Failed loading depth engine
The `libdepthengine.so` Microsoft provides relies on OpenGL to create framebuffers, for legacy reasons this requires a running display X-Server, i.e. for $DISPLAY to point to a valid X-Server.

If you are running over ssh, -X can be used as an argument to ssh, with a valid X-Server running on the host.

If you are running headless an X-Server needs to be mocked, you can install `xvfb` and then run a "fake" display with `Xvfb :99 -screen 0 640x480x24`. Then set $DISPLAY to :99, so `export DISPLAY=:99`. Then rereun and no issues.

## Lots of dropped color frame
A mix of the depth engine and the qr code detector running on cpu, can be quite slow, at high frame-rates this seems to collapse to the point nearly every frame is dropped due to the automatic queue management K4A side. In the code I have set to 5 fps, which is plenty sufficient for this application, possibility to optimise in the future here.