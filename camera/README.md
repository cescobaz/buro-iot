# Camera "device"

Custom camera device created with bash scripts.

It uses mqtt for communications (state and still image). And rtsp server for video streaming.

The script(s) does the following:

1. present a camera-state device to homeassistant to auto-recognize it as state and commands
2. present a camera(image) device to homeassistant to auto-recognize it as image
3. update the state
4. listen for commands (take snapshot and start/stop streaming)

Nodered has an automation to store the images in `/data/media/` and send a message via Telegram
