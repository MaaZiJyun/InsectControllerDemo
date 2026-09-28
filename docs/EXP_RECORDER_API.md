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
  "durationMs": 500,
  "frequency": 50.0,
  "minDurationMs": 1,
  "maxDurationMs": 4294967295,
  "minFrequency": 1.0,
  "maxFrequency": 500.0,
  "defaultPins": [7, 8, 9, 10]
}
```

`networkMode` is `station` on a shared external LAN and `access-point` when the
board created its fallback Wi-Fi network.

## Sending control commands

```text
POST http://<board-ip>/command
```

All parameters are URL query parameters:

| Parameter | Required | Values | Meaning |
| --- | --- | --- | --- |
| `name` | yes | `forward`, `back`, `left`, `right`, `stop` | Command |
| `chnl1` | together | integer `0`–`10` | Common/ground channel |
| `chnl2` | together | integer `0`–`10` | Right channel |
| `chnl3` | together | integer `0`–`10` | Left channel |
| `chnl4` | together | integer `0`–`10` | Forward/back channel |
| `durationMs` | movement only | `1`–`4294967295` | Maximum stimulation duration |
| `frequency` | movement only | `1`–`500` | Bipolar waveform frequency in Hz |

The four channel parameters may all be omitted to use `[7,8,9,10]`. If any one
is supplied, all four must be supplied. They must be distinct. Their numeric
values refer to XIAO labels (`7` means `D7`), not raw ESP32 GPIO numbers.

The electrical channel pairs are:

| Command | Active pair | Other configured channels |
| --- | --- | --- |
| `forward` | Chnl1 ↔ Chnl4 | high impedance |
| `back` | Chnl1 ↔ Chnl4 | high impedance |
| `left` | Chnl1 ↔ Chnl3 | high impedance |
| `right` | Chnl1 ↔ Chnl2 | high impedance |
| `stop` | none | all D0–D10 set to input/high impedance |

Example:

```text
POST http://192.168.1.42/command?name=left&chnl1=7&chnl2=8&chnl3=9&chnl4=10&durationMs=500&frequency=50
```

Success response:

```json
{"ok":true,"durationMs":500,"phaseDurationMs":10}
```

Errors use HTTP `400` for invalid parameters and `503` when the command service
is unavailable. The response body contains `{"ok":false,"error":"..."}`.

## Stopping safely

Send the following request when a pointer/key is released, cancelled, leaves the
button, the browser loses focus, or the controller component unmounts:

```text
POST http://<board-ip>/command?name=stop
```

The firmware also stops automatically after `durationMs`. A newly received
command replaces the queued/current command.

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
    "96x96", "160x120", "128x128", "176x144", "240x176",
    "240x240", "320x240", "320x320", "400x296", "480x320",
    "640x480", "800x600", "1024x768", "1280x720",
    "1280x1024", "1600x1200", "1920x1080", "720x1280",
    "864x1536", "2048x1536"
  ],
  "maxFpsByResolution": {
    "320x240": 60,
    "640x480": 30,
    "800x600": 25,
    "1280x720": 20,
    "1600x1200": 15,
    "1920x1080": 12,
    "2048x1536": 10
  },
  "limits": {
    "fps": [1, 30],
    "brightness": [-2, 2],
    "contrast": [-2, 2],
    "sharpness": [-2, 2]
  }
}
```

Always populate the resolution selector from `supportedResolutions`. This list
is generated at runtime from the detected sensor PID and the largest frame
buffer allocated at boot. OV2640, OV3660, and OV5640 therefore expose different
lists. The firmware is built with OPI PSRAM, allocates buffers for the detected
sensor maximum, then lowers the default output to `640x480`.

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

Before replacing a stream URL in the frontend, clear the old image source and
then assign a cache-busted URL:

```ts
video.src = "";
video.src = `http://${boardIp}:81/stream?v=${Date.now()}`;
```

`fps` is a transmission ceiling, not a guaranteed sensor frame rate. Actual
rate depends on sensor model, exposure/light level, resolution, JPEG size, Wi-Fi
signal quality, and browser JPEG decoding. For low-latency control, prefer
`240x240` and keep only one `<img>` connected per board.

On the tested XIAO ESP32S3 Sense with OV3660, the measured rates were about
28 FPS at `240x240`, 27 FPS at `240x176`, 18 FPS at `320x240`, and 15–18 FPS
from `400x296` through `640x480`. The firmware therefore boots in the measured
smooth profile, `240x240` at a 30 FPS ceiling. Higher resolutions remain
available when spatial detail is more important than motion smoothness.

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
type Movement = "forward" | "back" | "left" | "right";

const channelPins = { chnl1: 7, chnl2: 8, chnl3: 9, chnl4: 10 };

export async function move(
  boardIp: string,
  name: Movement,
  durationMs: number,
  frequency: number,
) {
  const query = new URLSearchParams({
    name,
    durationMs: String(durationMs),
    frequency: String(frequency),
    ...Object.fromEntries(
      Object.entries(channelPins).map(([key, value]) => [key, String(value)]),
    ),
  });
  const response = await fetch(`http://${boardIp}/command?${query}`, {
    method: "POST",
  });
  if (!response.ok) throw new Error(await response.text());
  return response.json();
}

export function stop(boardIp: string) {
  return fetch(`http://${boardIp}/command?name=stop`, {
    method: "POST",
    keepalive: true,
  });
}
```

The board sends permissive CORS response headers. If Exp.-Recorder is served over
HTTPS, browsers will block direct HTTP board calls as mixed content; in that
deployment, proxy `/config` and `/command` through the Exp.-Recorder FastAPI
backend. The current local Next.js development server uses HTTP and can call the
board directly.
