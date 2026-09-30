# InsectControllerDemo
Based on XIAO electronic board control

## LAN API

When the board joins the same Wi-Fi network as Exp.-Recorder, it exposes:

- `GET http://<board-ip>/config` for controller limits and defaults.
- `POST http://<board-ip>/command?...` for stimulation commands.
- `GET/POST http://<board-ip>/camera/settings` for camera configuration.
- `GET http://<board-ip>:81/stream` for the MJPEG camera stream.
- `GET http://<board-ip>/camera/capture` for a maximum-resolution JPEG photo.
- The default live-video profile is `640x480 @ 30 FPS`;
  still photos continue to use the sensor's maximum supported resolution.
- UDP port `37020` for the `EXP_RECORDER_DISCOVER_V1` discovery handshake.

The controller page at `/` is served only in access-point fallback mode. In
station mode, the controller UI is provided by Exp.-Recorder on the client
device. See [Exp.-Recorder LAN integration](docs/EXP_RECORDER_API.md) for the
complete API contract and frontend examples.

In access-point fallback mode, open the onboard controller's Settings panel to
scan for and join an existing Wi-Fi network. A successful connection is saved
for future boots; the response displays the board's new IP, then the XIAO access
point shuts down. If joining fails, the access point remains available.

Example command:

```text
POST http://<board-ip>/command?name=forward&chnl1=1&chnl2=2&chnl3=3&chnl4=4&durationMs=200&frequency=50
```

Valid command names are `forward`, `back`, `left`, `right`, and `stop`. Channel
values are the XIAO labels `D0` through `D10` represented by indices `0` through
`10`; all four values must be different. The control API permits CORS so that an
HTTP-hosted browser client on the same LAN can call it directly.

When waveform parameters are omitted, the default frequency is `50 Hz`.
`forward` defaults to `200 ms`; `back`, `left`, and `right` default to `400 ms`.
The default channel mapping is Chnl1–Chnl4 = D1–D4.
