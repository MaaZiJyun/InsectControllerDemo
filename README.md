# InsectControllerDemo
Based on XIAO electronic board control

## LAN API

When the board joins the same Wi-Fi network as Exp.-Recorder, it exposes:

- `GET http://<board-ip>/config` for controller limits and defaults.
- `POST http://<board-ip>/command?...` for stimulation commands.
- `GET/POST http://<board-ip>/camera/settings` for camera configuration.
- `GET http://<board-ip>:81/stream` for the MJPEG camera stream.
- `GET http://<board-ip>/camera/capture` for a maximum-resolution JPEG photo.
- The default live-video profile is `240x240 @ 30 FPS` for smooth OV3660 motion;
  still photos continue to use the sensor's maximum supported resolution.
- UDP port `37020` for the `EXP_RECORDER_DISCOVER_V1` discovery handshake.

The controller page at `/` is served only in access-point fallback mode. In
station mode, the controller UI is provided by Exp.-Recorder on the client
device. See [Exp.-Recorder LAN integration](docs/EXP_RECORDER_API.md) for the
complete API contract and frontend examples.

Example command:

```text
POST http://<board-ip>/command?name=forward&chnl1=7&chnl2=8&chnl3=9&chnl4=10&durationMs=500&frequency=50
```

Valid command names are `forward`, `back`, `left`, `right`, and `stop`. Channel
values are the XIAO labels `D0` through `D10` represented by indices `0` through
`10`; all four values must be different. The control API permits CORS so that an
HTTP-hosted browser client on the same LAN can call it directly.
