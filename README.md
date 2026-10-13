# InsectControllerDemo
Based on XIAO electronic board control

## LAN API

When the board joins the same Wi-Fi network as Exp.-Recorder, it exposes:

- `GET http://<board-ip>/config` for controller limits and defaults.
- `POST http://<board-ip>/stimulation/start` to start generic bipolar stimulation.
- `POST http://<board-ip>/stimulation/stop` to stop stimulation safely.
- `GET/POST http://<board-ip>/camera/settings` for camera configuration.
- `GET http://<board-ip>:81/stream` for the MJPEG camera stream.
- `GET http://<board-ip>/stream/stats` for completed-frame, JPEG-byte, capture-time, and send-time counters.
- `GET http://<board-ip>/camera/capture` for a maximum-resolution JPEG photo.
- The default live-video profile is `640x480 @ 30 FPS`;
  still photos continue to use the sensor's maximum supported resolution.
- UDP port `37020` for the `EXP_RECORDER_DISCOVER_V1` discovery handshake.

The page at `/` is a Wi-Fi provisioning page served only in access-point
fallback mode. It contains no camera, stimulation, GPIO, or controller UI. In
station mode, the controller UI is provided by Exp.-Recorder on the client
device. See [Exp.-Recorder LAN integration](docs/EXP_RECORDER_API.md) for the
complete API contract and frontend examples.

In access-point fallback mode, open the onboard Wi-Fi setup page to scan for
and join an existing network. A successful connection is saved
for future boots; the response displays the board's new IP, then the XIAO access
point shuts down. If joining fails, the access point remains available.
While the fallback access point is active, controller/camera APIs return `403`,
and the MJPEG and UDP discovery services are not started.

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
