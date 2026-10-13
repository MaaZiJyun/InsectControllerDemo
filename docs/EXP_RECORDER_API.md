# Exp.-Recorder LAN integration

## Runtime modes

The firmware has two distinct UI/network modes:

1. **Station mode**: the XIAO joins the same external Wi-Fi LAN as the device
   running Exp.-Recorder. Exp.-Recorder supplies the controller UI and calls the
   board's HTTP API. The board does not serve its onboard controller at `/`.
2. **Access-point fallback mode**: if joining the configured external Wi-Fi times
   out, the XIAO creates its own access point. In this mode only, opening `/`
   serves the controller page compiled into the firmware.

The control API and camera endpoints remain available in both modes.

## Discovering the board

Exp.-Recorder broadcasts the following exact payload on UDP port `37020`:

```text
EXP_RECORDER_DISCOVER_V1
```

The board replies to the sender with JSON:

```json
{
  "protocol": "EXP_RECORDER_BOARD_V1",
  "mac": "CC:D9:0B:69:70:90",
  "model": "XIAO ESP32S3 Sense",
  "control_port": 80,
  "video_port": 81
}
```

The response source address is the board IP. Exp.-Recorder already exposes its
scanner to the browser as:

```text
POST /backend/boards/discover
```

Use `discovered[].ip_address`; do not persist the DHCP address as permanent
hardware identity. Use `mac` for identity and scan again after reconnecting.

## Controller configuration

```text
GET http://<board-ip>/config
```

Example response:

```json
{
  "model": "XIAO ESP32S3 Sense",
  "networkMode": "station",
  "controlPort": 80,
  "videoPort": 81,
  "stimulation": {
    "startPath": "/stimulation/start",
    "stopPath": "/stimulation/stop",
    "supportedPins": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
    "minDurationMs": 1,
    "maxDurationMs": 4294967295,
    "minFrequencyHz": 1.0,
    "maxFrequencyHz": 500.0
  }
}
```

`networkMode` is `station` on a shared external LAN and `access-point` when the
board created its fallback Wi-Fi network.

## Onboard Wi-Fi setup

These endpoints are available only while the board is running its fallback
access point and are used by the dedicated onboard Wi-Fi provisioning page.
That page contains no controller, GPIO, stimulation, or camera functionality.
While provisioning is active, `/config`, `/stimulation/*`, and `/camera/*` return
`403`; port `81` streaming and UDP discovery are not started. Those services
start only after the board joins an existing Wi-Fi network.

Scan nearby networks:

```text
GET http://192.168.4.1/wifi/networks
```

Example response:

```json
{
  "ok": true,
  "networks": [
    { "ssid": "Lab Wi-Fi", "rssi": -48, "secure": true }
  ]
}
```

Join a network using an `application/x-www-form-urlencoded` POST body. Keep the
password out of the query string:

```text
POST http://192.168.4.1/wifi/connect
Content-Type: application/x-www-form-urlencoded

ssid=Lab+Wi-Fi&password=example-password
```

On success the response contains `ip` and `controlUrl`. The credentials are
saved, and the fallback access point closes about 2.5 seconds after the response
is sent. The client must join the same Wi-Fi and use UDP discovery or the
returned IP. If association fails, the endpoint returns `504` and the access
point remains active. Open networks may use an empty password.

## Starting stimulation

```http
POST http://<board-ip>/stimulation/start
Content-Type: application/json

{
  "pin1": 1,
  "pin2": 3,
  "frequencyHz": 50,
  "durationMs": 300
}
```

All four fields are required. Pin numbers refer to XIAO labels D0–D10 rather
than raw ESP32 GPIO numbers, and `pin1` must differ from `pin2`. Frequency must
be 1–500 Hz and duration must be 1–4294967295 ms.

The differential waveform has fixed ordering:

1. First phase: `pin1=LOW`, `pin2=HIGH`.
2. Second phase: `pin1=HIGH`, `pin2=LOW`.
3. Repeat until `durationMs` expires.

Every other stimulation pin is kept in input/high-impedance mode. The endpoint
returns `202 Accepted` after the request has been queued:

```json
{"ok":true,"pin1":1,"pin2":3,"frequencyHz":50.000,"durationMs":300}
```

Errors use HTTP `400` for invalid parameters and `503` when the command service
is unavailable. The response body contains `{"ok":false,"error":"..."}`.

## Stopping safely

Send the following request when a pointer/key is released, cancelled, leaves the
button, the browser loses focus, or the controller component unmounts:

```http
POST http://<board-ip>/stimulation/stop
```

The firmware also stops automatically after `durationMs`. A newly received
start request first returns the active pair to high impedance and then starts
the new pair. Stop also restores every D0–D10 stimulation pin to high impedance.

## Camera

### Camera settings

Read the current values, supported resolutions, and numeric limits:

```text
GET http://<board-ip>/camera/settings
```

Example response:

```json
{
  "ok": true,
  "sensorModel": "OV3660",
  "sensorPid": "0x3660",
  "psram": {
    "enabled": true,
    "totalBytes": 8388608,
    "freeBytes": 6900000
  },
  "allocationResolution": "2048x1536",
  "resolution": "640x480",
  "fps": 30,
  "brightness": 0,
  "contrast": 0,
  "sharpness": 0,
  "hmirror": false,
  "vflip": true,
  "streamGeneration": 1,
  "supportedResolutions": [
    "2048x1536", "1920x1080", "1280x720",
    "1024x768", "640x480", "320x240"
  ],
  "maxFpsByResolution": {
    "2048x1536": 15,
    "1920x1080": 20,
    "1280x720": 45,
    "1024x768": 45,
    "640x480": 60,
    "320x240": 120
  },
  "limits": {
    "fps": [1, 60],
    "brightness": [-2, 2],
    "contrast": [-2, 2],
    "sharpness": [-2, 2]
  }
}
```

Always populate the resolution selector from `supportedResolutions`. The stream
API has a strict six-profile whitelist and intersects it at runtime with the
detected sensor PID and largest frame buffer allocated at boot. Unsupported
profiles are omitted. The firmware is built with OPI PSRAM, allocates buffers
for the detected sensor maximum, then lowers the default output to `640x480`.

For OV3660 the exposed profiles are:

| Format | Resolution | FPS ceiling | Scaling method | Sensor system clock |
| --- | ---: | ---: | --- | ---: |
| 3 Megapixel | 2048×1536 | 15 | Full resolution | 54 MHz |
| 1080p | 1920×1080 | 20 | Crop from full resolution | 54 MHz |
| 720p | 1280×720 | 45 | Crop from full resolution | 54 MHz |
| XGA | 1024×768 | 45 | Horizontal and vertical subsampling | 54 MHz |
| VGA | 640×480 | 60 | Subsample from 1280×960 | 27 MHz |
| QVGA | 320×240 | 120 | Subsample from 1280×960 | 27 MHz |

These FPS and clock values describe the OV3660 sensor profiles. `fps` remains a
stream transmission ceiling; the ESP32-S3 JPEG/DVP path, exposure, Wi-Fi, and
browser can make the observed frame rate substantially lower.

Update one or more values using URL query parameters:

```text
POST http://<board-ip>/camera/settings?resolution=1280x720&fps=24&brightness=1&contrast=0&sharpness=1&hmirror=false&vflip=true
```

| Parameter | Values | Notes |
| --- | --- | --- |
| `resolution` | a value returned by `supportedResolutions` | Use ASCII `x`, for example `640x480` |
| `fps` | integer limited by `maxFpsByResolution[resolution]` | Target stream frame-rate ceiling |
| `brightness` | integer `-2`–`2` | Sensor brightness |
| `contrast` | integer `-2`–`2` | Sensor contrast |
| `sharpness` | integer `-2`–`2` | Sensor sharpness |
| `hmirror` | `true`/`false` or `1`/`0` | Horizontal mirror |
| `vflip` | `true`/`false` or `1`/`0` | Vertical flip |

This is a partial-update API: omitted values keep their current setting. A
successful update returns the same complete JSON shape as `GET`. Invalid values
return HTTP `400`; unavailable/rejected camera operations return `503`/`500`.

Before applying settings, the board pauses frame acquisition and waits until the
stream worker has returned its current camera buffer. The MJPEG TCP connection
stays open. It then discards queued frame buffers and verifies a fresh frame's
width, height, JPEG format, length, SOI, and EOI markers. A failed validation
rebuilds the camera with the previous settings; if that also fails, it falls
back to safe defaults.

After a successful update, the existing `/stream` request resumes with the new
settings and dimensions. Do not routinely replace the video URL after a camera
settings update; reconnect only after an actual network error.

`fps` throttles the MJPEG stream but cannot force the sensor, JPEG encoder, or
Wi-Fi link to produce that rate. The observed FPS can therefore be lower,
especially at high resolutions.

Frontend example:

```ts
type CameraSettings = {
  ok: true;
  sensorModel: "OV2640" | "OV3660" | "OV5640" | string;
  sensorPid: string;
  psram: { enabled: boolean; totalBytes: number; freeBytes: number };
  allocationResolution: string;
  resolution: string;
  fps: number;
  brightness: number;
  contrast: number;
  sharpness: number;
  hmirror: boolean;
  vflip: boolean;
  streamGeneration: number;
  supportedResolutions: string[];
  maxFpsByResolution: Record<string, number>;
  limits: {
    fps: [number, number];
    brightness: [number, number];
    contrast: [number, number];
    sharpness: [number, number];
  };
};

export async function updateCameraSettings(
  boardIp: string,
  patch: Partial<Pick<CameraSettings,
    "resolution" | "fps" | "brightness" | "contrast" | "sharpness" |
    "hmirror" | "vflip">>,
) {
  const query = new URLSearchParams();
  Object.entries(patch).forEach(([key, value]) => {
    if (value !== undefined) query.set(key, String(value));
  });
  const response = await fetch(
    `http://${boardIp}/camera/settings?${query}`,
    { method: "POST" },
  );
  if (!response.ok) throw new Error(await response.text());
  return response.json() as Promise<CameraSettings>;
}
```

Use the response to rebuild the resolution/FPS selectors. Keep the existing
live-video `<img>` request alive:

```ts
const next = await updateCameraSettings(boardIp, patch);
setCameraSettings(next);
```

The firmware starts with `hmirror=false` and `vflip=true`, preserving the
existing camera orientation. The external frontend can change either value.

### Video endpoints

MJPEG stream:

```text
GET http://<board-ip>:81/stream
```

The firmware keeps exactly one active MJPEG client. A new `/stream` request
replaces and closes the previous stream instead of waiting behind a stale
browser connection. Streaming runs in an asynchronous worker, uses the newest
camera buffer, disables TCP Nagle buffering, and applies a bounded socket send
timeout so a weak or disconnected client cannot freeze port 81.

Read board-side delivery statistics from the control port:

```text
GET http://<board-ip>/stream/stats
```

```json
{
  "active": true,
  "stream_id": 12,
  "frames_sent": 1842,
  "jpeg_bytes_sent": 23946000,
  "average_frame_bytes": 13000,
  "capture_time_ms": 9210,
  "send_time_ms": 48750
}
```

`frames_sent` increments only after the combined boundary/part header and the
complete JPEG payload have been successfully written to the active MJPEG
connection. `jpeg_bytes_sent` counts only those completed JPEG payloads, while
`average_frame_bytes` is `jpeg_bytes_sent / frames_sent`. `capture_time_ms` and
`send_time_ms` are cumulative stage timings for diagnosing camera and network
bottlenecks; a failed final send is included in `send_time_ms` even though it
does not increment `frames_sent`.
It remains available after the connection closes, while `active` becomes
`false`. Every accepted `/stream` connection receives a new `stream_id` and
starts from zero. The response uses `Cache-Control: no-store` and permits CORS.

To calculate board-side FPS, poll once per second and divide the
`frames_sent` delta by the real elapsed time. Reset the baseline when
`stream_id` changes, the counter decreases, or `active` is false. This measures
frames completely written by the board, not frames decoded or painted by the
browser.

Before replacing a stream URL in the frontend, clear the old image source and
then assign a cache-busted URL:

```ts
video.src = "";
video.src = `http://${boardIp}:81/stream?v=${Date.now()}`;
```

`fps` is a transmission ceiling, not a guaranteed sensor frame rate. Actual
rate depends on sensor model, exposure/light level, resolution, JPEG size, Wi-Fi
signal quality, and browser JPEG decoding. Keep only one `<img>` connected per
board and select `320x240` when low latency is more important than detail.

Single JPEG frame:

```text
GET http://<board-ip>/camera/capture
```

This endpoint pauses camera acquisition while keeping the active MJPEG TCP
connection open, captures a JPEG at the maximum resolution supported by the
detected sensor and allocated frame buffers, restores the previous streaming
settings, and resumes frames on the same `/stream` request. The frontend does
not normally need to reconnect after taking a photo.

The response header `X-Camera-Resolution` contains the actual still-image size.
`X-Stream-Reconnect-Required` is normally `false`; if it is `true`, the firmware
had to restart a blocked video socket and the frontend must reconnect using the
value returned in `X-Stream-Generation`.

Fetch the photo as a blob and display/download it using a separate element. Do
not assign `/camera/capture` to the live-video `<img>`, because doing so replaces
the browser's stream request even though the firmware kept it alive:

```ts
const response = await fetch(`http://${boardIp}/camera/capture`);
if (!response.ok) throw new Error(await response.text());

const photoUrl = URL.createObjectURL(await response.blob());
photoImage.src = photoUrl; // separate from the live-video element

if (response.headers.get("X-Stream-Reconnect-Required") === "true") {
  video.src = "";
  video.src = `http://${boardIp}:81/stream?v=${
    response.headers.get("X-Stream-Generation")
  }`;
}
```

Port 81 only provides `/stream`. The former `GET /capture` route on that port
has been removed because a long-running MJPEG request could block it. Always
use the control-port endpoint above for still photos.

For a React page, set the stream URL directly as the `<img src>` value.

## Browser example

```ts
export async function startStimulation(
  boardIp: string,
  pin1: number,
  pin2: number,
  durationMs: number,
  frequencyHz: number,
) {
  const response = await fetch(`http://${boardIp}/stimulation/start`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ pin1, pin2, frequencyHz, durationMs }),
  });
  if (!response.ok) throw new Error(await response.text());
  return response.json();
}

export function stop(boardIp: string) {
  return fetch(`http://${boardIp}/stimulation/stop`, {
    method: "POST",
    keepalive: true,
  });
}
```

The board sends permissive CORS response headers. If Exp.-Recorder is served over
HTTPS, browsers will block direct HTTP board calls as mixed content; in that
deployment, proxy `/config` and `/stimulation/*` through the Exp.-Recorder FastAPI
backend. The current local Next.js development server uses HTTP and can call the
board directly.
