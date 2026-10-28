# InsectControllerDemo
Based on XIAO electronic board control

## LAN API

The same API is available through either the board's own access point or an
external Wi-Fi network:

- `GET http://<board-ip>/config` for controller limits and defaults.
- `POST http://<board-ip>/stimulation/start` to start generic bipolar stimulation.
- `POST http://<board-ip>/stimulation/stop` to stop stimulation safely.
- `GET/POST http://<board-ip>/camera/settings` for camera configuration.
- `GET http://<board-ip>:81/stream` for the MJPEG camera stream.
- `GET http://<board-ip>/stream/stats` for completed-frame, JPEG-byte, capture-time, and send-time counters.
- `GET http://<board-ip>/camera/capture` for a maximum-resolution JPEG photo.
- `GET/POST http://<board-ip>/wifi/settings` for Wi-Fi status and configuration.
- `GET/POST http://<board-ip>/wifi/access-point` for SoftAP channel settings.
- `GET http://<board-ip>/wifi/networks` to scan nearby Wi-Fi networks.
- The default live-video profile is `640x480 @ 30 FPS`;
  still photos continue to use the sensor's maximum supported resolution.
- UDP port `37020` for the `EXP_RECORDER_DISCOVER_V1` discovery handshake.

There is no onboard configuration page at `/`; Exp.-Recorder supplies the UI.
If the board cannot join a saved or built-in network, it creates the
`XIAOESP32S3` access point (password: `xiaoesp32s3`). A client connected to that
access point can use the complete stimulation, camera, video, discovery, and Wi-Fi APIs at
`192.168.4.1`. Wi-Fi changes run asynchronously while those services continue
working. After a successful external connection, the new credentials are saved
and the access point closes after five seconds. On failure it remains active.
See [Exp.-Recorder LAN integration](docs/EXP_RECORDER_API.md) for the complete
API contract and frontend examples.

Example request:

```http
POST http://<board-ip>/stimulation/start
Content-Type: application/json

{"pin1":1,"pin2":3,"frequencyHz":50,"durationMs":300}
```

`pin1` and `pin2` are XIAO labels `D0` through `D10`, represented by indices
`0` through `10`; they must be different. During the first phase, `pin1` is LOW
and `pin2` is HIGH. The second phase reverses that polarity. Every other
stimulation pin remains high impedance. Frequency must be `1` through `500` Hz,
and duration must be `1` through `4294967295` ms.

The firmware stops automatically after `durationMs`. A new start request safely
replaces the active stimulation. Send `POST /stimulation/stop` for immediate
stop; all D0–D10 pins then return to input/high-impedance mode. The control API
permits CORS so an HTTP-hosted browser client on the same LAN can call it.
