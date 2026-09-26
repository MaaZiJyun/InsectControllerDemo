/**
 * @file ControllerPage.h
 * @brief Controller 单页前端。
 */
#pragma once

#include <Arduino.h>

namespace ControllerPage
{
  static const char HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-CN">

<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
    <title>Insect Controller</title>
    <style>
        :root {
            color-scheme: dark;
            --glass: rgba(255, 255, 255, .20);
            --glass-active: rgba(255, 255, 255, .72);
            --stop: #e53935;
            --safe-top: env(safe-area-inset-top, 0px);
            --safe-bottom: env(safe-area-inset-bottom, 0px)
        }

        * {
            box-sizing: border-box;
            -webkit-tap-highlight-color: transparent
        }

        html,
        body {
            width: 100%;
            height: 100%;
            margin: 0;
            overflow: hidden;
            background: #050505;
            color: #fff;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif
        }

        .camera {
            position: fixed;
            inset: 0;
            width: 100vw;
            height: 100dvh;
            overflow: hidden;
            background: radial-gradient(circle at center, #242424, #050505 70%)
        }

        .video {
            position: absolute;
            inset: 0;
            width: 100%;
            height: 100%;
            display: block;
            object-fit: cover;
            background: transparent
        }

        .shade {
            position: absolute;
            inset: 0;
            pointer-events: none;
            background: linear-gradient(to bottom, rgba(0, 0, 0, .55), transparent 25%, transparent 62%, rgba(0, 0, 0, .58))
        }

        .topbar {
            position: absolute;
            z-index: 2;
            top: 0;
            left: 0;
            right: 0;
            padding: calc(14px + var(--safe-top)) 18px 14px;
            display: flex;
            align-items: flex-start;
            justify-content: space-between;
            gap: 16px;
        }

        .camera-state {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: .92rem;
            font-weight: 650
        }

        .dot {
            width: 9px;
            height: 9px;
            border-radius: 50%;
            background: #f6b73c;
            box-shadow: 0 0 0 3px rgba(246, 183, 60, .18)
        }

        .dot.online {
            background: #49d17d;
            box-shadow: 0 0 0 3px rgba(73, 209, 125, .18)
        }

        .dot.offline {
            background: #ef5350;
            box-shadow: 0 0 0 3px rgba(239, 83, 80, .18)
        }

        .clock {
            text-align: left
        }

        .time {
            font-size: 1.15rem;
            font-weight: 720;
            letter-spacing: .02em
        }

        .date {
            margin-top: 2px;
            font-size: .78rem;
            opacity: .82
        }

        [hidden] {
            display: none !important
        }

        .settings-toggle {
            z-index: 3;
            padding: 8px 13px;
            border-radius: 999px;
            font-size: .78rem;
        }

        .settings-modal {
            position: absolute;
            z-index: 10;
            inset: 0;
            display: grid;
            place-items: center;
            padding: 20px
        }

        .settings-backdrop {
            position: absolute;
            inset: 0;
            background: rgba(0, 0, 0, .20);
            backdrop-filter: blur(5px);
            -webkit-backdrop-filter: blur(5px)
        }

        .settings {
            position: relative;
            z-index: 1;
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 14px 16px;
            width: min(440px, 100%);
            max-height: calc(100dvh - 40px);
            overflow-y: auto;
            padding: 20px;
            border: 1px solid rgba(255, 255, 255, .23);
            border-radius: 22px;
            background: var(--glass);
        }

        .settings-title {
            grid-column: 1/-1;
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: 2px
        }

        .settings-title h2 {
            margin: 0;
            font-size: 1.05rem
        }

        .close-settings {
            min-height: auto;
            padding: 7px 11px;
            border-radius: 10px;
            font-size: .75rem;
            background: rgba(255, 255, 255, .12)
        }

        .setting {
            display: grid;
            gap: 6px;
            font-size: .78rem
        }

        .setting input,
        .pin-grid select {
            width: 100%;
            padding: 9px;
            border: 1px solid rgba(255, 255, 255, .32);
            border-radius: 10px;
            background: rgba(255, 255, 255, .12);
            color: #fff;
            font: inherit;
            text-align: center;
            outline: none
        }

        .setting input:focus,
        .pin-grid select:focus {
            border-color: rgba(255, 255, 255, .8)
        }

        .pin-settings {
            grid-column: 1/-1;
            display: grid;
            gap: 9px
        }

        .pin-settings h3 {
            margin: 2px 0 0;
            font-size: .86rem
        }

        .pin-grid {
            display: grid;
            grid-template-columns: minmax(62px, .8fr) minmax(90px, 1.4fr) minmax(64px, 1fr);
            align-items: center;
            gap: 7px;
            font-size: .72rem;
            text-align: center
        }

        .pin-grid .pin-name,
        .pin-grid .pin-role {
            text-align: left;
            font-weight: 650
        }

        .pin-grid .pin-role {
            font-weight: 450;
            opacity: .78
        }

        .pin-grid select {
            padding: 7px 4px;
            text-align: center
        }

        .pin-grid select.invalid {
            border-color: #ff8b87
        }

        .pin-help {
            font-size: .66rem;
            opacity: .74
        }

        .wave-info {
            grid-column: 1/-1;
            text-align: center;
            font-size: .7rem;
            opacity: .82
        }

        .wave-info.invalid {
            color: #ff8b87;
            opacity: 1
        }

        .controls {
            position: absolute;
            z-index: 3;
            left: 50%;
            bottom: calc(18px + var(--safe-bottom));
            transform: translateX(-50%);
            display: grid;
            grid-template-columns: repeat(3, 64px);
            grid-template-rows: repeat(3, 56px);
            grid-template-areas: ". forward ." "left stop right" ". back .";
            gap: 9px;
            padding: 12px;
        }

        button {
            border: 1px solid rgba(255, 255, 255, .42);
            border-radius: 18px;
            background: var(--glass);
            color: #fff;
            font-size: 1.45rem;
            font-weight: 750;
            touch-action: none;
            user-select: none;
            cursor: pointer;
            transition: transform .08s ease, background .08s ease
        }

        button:active,
        button.active {
            transform: scale(.93);
            background: var(--glass-active);
            color: #151515;
            text-shadow: none
        }

        .forward {
            grid-area: forward
        }

        .back {
            grid-area: back
        }

        .left {
            grid-area: left
        }

        .right {
            grid-area: right
        }

        .stop {
            grid-area: stop;
            background: rgba(229, 57, 53, .88);
            border-color: rgba(255, 255, 255, .58);
            font-size: .72rem;
            letter-spacing: .05em;
            text-transform: uppercase
        }

        .stop:active,
        .stop.active {
            background: #ff504b;
            color: #fff
        }

        @media(max-height:560px) and (orientation:landscape) {
            .controls {
                left: auto;
                right: calc(18px + var(--safe-bottom));
                bottom: 50%;
                transform: translateY(50%);
                grid-template-columns: repeat(3, 58px);
                grid-template-rows: repeat(3, 50px)
            }

            .topbar {
                padding-right: 220px
            }
        }
    </style>
</head>

<body>
    <main class="camera">
        <img id="video" class="video" alt="Camera stream">
        <div class="shade"></div>
        <header class="topbar">
            <div>
                <div class="camera-state">
                    <span id="cameraDot" class="dot"></span>
                    <span id="cameraStatus">Camera Connecting</span>
                </div>
                <div class="clock">
                    <div id="time" class="time">--:--:--</div>
                    <div id="date" class="date">----</div>
                </div>
            </div>

            <button id="openSettings" class="settings-toggle" type="button">
                Settings
            </button>
        </header>

        <section class="controls">
            <button class="forward" data-command="forward" aria-label="Forward">▲</button>
            <button class="back" data-command="back" aria-label="Back">▼</button>
            <button class="left" data-command="left" aria-label="Left">◀</button>
            <button class="right" data-command="right" aria-label="Right">▶</button>
            <button class="stop" data-command="stop" aria-label="Stop">STOP</button>
        </section>
        <section id="settingsModal" class="settings-modal" hidden>
            <div id="settingsBackdrop" class="settings-backdrop"></div>
            <div class="settings" role="dialog" aria-modal="true" aria-labelledby="settingsTitle">
                <div class="settings-title">
                    <h2 id="settingsTitle">Settings</h2><button id="closeSettings" class="close-settings" type="button">Close</button>
                </div>
                <label class="setting">Duration (ms)<input id="durationMs" type="number" inputmode="numeric"></label>
                <label class="setting">Frequency (Hz)<input id="frequency" type="number" inputmode="decimal"></label>
                <div class="pin-settings">
                    <h3>GPIO mapping</h3>
                    <div id="pinGrid" class="pin-grid"></div>
                    <div class="pin-help">Forward: Chnl1 + Chnl4 · Right: Chnl1 + Chnl2 · Left: Chnl1 + Chnl3 · Stop: high impedance</div>
                </div>
                <div id="waveInfo" class="wave-info">Loading settings…</div>
            </div>
        </section>
    </main>
    <script>
        const video = document.getElementById('video'),
            cameraDot = document.getElementById('cameraDot'),
            cameraStatus = document.getElementById('cameraStatus');
        const durationInput = document.getElementById('durationMs'),
            frequencyInput = document.getElementById('frequency'),
            waveInfo = document.getElementById('waveInfo');
        const settingsModal = document.getElementById('settingsModal');
        const pinGrid = document.getElementById('pinGrid');
        const pinVariables = [
            ['Chnl1', 'Ground'],
            ['Chnl2', 'Right'],
            ['Chnl3', 'Left'],
            ['Chnl4', 'Forward']
        ];
        const pinStorageKey = 'insect-controller-channel-pins-v1';
        let maxDurationMs = 4294967295;

        ['Variable', 'Role', 'Output pin'].forEach(label => {
            const heading = document.createElement('span');
            heading.textContent = label;
            pinGrid.appendChild(heading)
        });
        pinVariables.forEach(([name, role], channel) => {
            const variable = document.createElement('span');
            variable.className = 'pin-name';
            variable.textContent = name;
            pinGrid.appendChild(variable);
            const description = document.createElement('span');
            description.className = 'pin-role';
            description.textContent = role;
            pinGrid.appendChild(description);
            const select = document.createElement('select');
            select.dataset.channel = channel;
            select.setAttribute('aria-label', name + ' ' + role);
            for (let pin = 0; pin <= 10; pin++) {
                const option = document.createElement('option');
                option.value = pin;
                option.textContent = 'D' + pin;
                select.appendChild(option)
            }
            pinGrid.appendChild(select)
        });

        function setCameraState(state, text) {
            cameraDot.className = 'dot ' + state;
            cameraStatus.textContent = text
        }

        function updateClock() {
            const now = new Date();
            document.getElementById('time').textContent = now.toLocaleTimeString('en-US', {
                hour12: false
            });
            document.getElementById('date').textContent = now.toLocaleDateString('en-US', {
                year: 'numeric',
                month: '2-digit',
                day: '2-digit',
                weekday: 'short'
            })
        }
        updateClock();
        setInterval(updateClock, 1000);
        video.addEventListener('load', () => setCameraState('online', 'Camera Online'));
        video.addEventListener('error', () => setCameraState('offline', 'Camera Connection Failed'));

        function stimulation() {
            const durationMs = Number(durationInput.value),
                frequency = Number(frequencyInput.value);
            const phaseMs = Math.max(1, Math.round(500 / frequency));
            const valid = Number.isInteger(durationMs) && durationMs >= Number(durationInput.min) && durationMs <= maxDurationMs && Number.isFinite(frequency) && frequency >= Number(frequencyInput.min) && frequency <= Number(frequencyInput.max);
            waveInfo.textContent = valid ? 'Phase ' + phaseMs + ' ms · Duration ' + durationMs + ' ms' : 'Invalid duration or frequency';
            waveInfo.classList.toggle('invalid', !valid);
            return {
                durationMs,
                frequency,
                valid
            }
        }
        function pinValues() {
            return Array.from(pinGrid.querySelectorAll('select[data-channel]')).map(select => Number(select.value))
        }

        function updateSettingsInfo() {
            const selects = Array.from(pinGrid.querySelectorAll('select[data-channel]'));
            const pins = pinValues();
            const valid = pins.length === 4 && new Set(pins).size === 4 && pins.every(pin => Number.isInteger(pin) && pin >= 0 && pin <= 10);
            selects.forEach(select => select.classList.toggle('invalid', !valid));
            if (!valid) {
                waveInfo.textContent = 'Invalid: Chnl1–Chnl4 must use four different pins';
                waveInfo.classList.add('invalid');
                return false
            }
            return stimulation().valid
        }

        function savePinMapping() {
            try {
                localStorage.setItem(pinStorageKey, JSON.stringify(pinValues()))
            } catch (_) {}
        }

        function loadPinMapping(defaultPins) {
            let saved = [];
            try {
                saved = JSON.parse(localStorage.getItem(pinStorageKey) || '[]')
            } catch (_) {}
            const pins = Array.isArray(saved) && saved.length === 4 ? saved : defaultPins;
            pinGrid.querySelectorAll('select[data-channel]').forEach((select, channel) => {
                const pin = Number(pins[channel]);
                select.value = Number.isInteger(pin) && pin >= 0 && pin <= 10 ? pin : defaultPins[channel]
            });
            updateSettingsInfo()
        }

        durationInput.addEventListener('input', updateSettingsInfo);
        frequencyInput.addEventListener('input', updateSettingsInfo);
        pinGrid.addEventListener('change', () => {
            savePinMapping();
            updateSettingsInfo()
        });

        function openSettings() {
            settingsModal.hidden = false;
            durationInput.focus()
        }

        function closeSettings() {
            settingsModal.hidden = true
        }
        document.getElementById('openSettings').addEventListener('click', openSettings);
        document.getElementById('closeSettings').addEventListener('click', closeSettings);
        document.getElementById('settingsBackdrop').addEventListener('click', closeSettings);
        document.addEventListener('keydown', event => {
            if (event.key === 'Escape') closeSettings()
        });
        const send = name => {
            let url = '/command?name=' + encodeURIComponent(name);
            const pins = pinValues();
            if (pins.length !== 4 || new Set(pins).size !== 4) {
                updateSettingsInfo();
                return Promise.resolve()
            }
            pins.forEach((pin, index) => url += '&chnl' + (index + 1) + '=' + pin);
            if (name !== 'stop') {
                const w = stimulation();
                if (!w.valid) return Promise.resolve();
                url += '&durationMs=' + w.durationMs + '&frequency=' + w.frequency
            }
            return fetch(url, {
                method: 'POST',
                cache: 'no-store'
            }).then(async r => {
                if (!r.ok) throw Error((await r.json()).error || r.status)
            }).catch(error => {
                waveInfo.textContent = error.message;
                waveInfo.classList.add('invalid')
            })
        };
        fetch('/config', {
            cache: 'no-store'
        }).then(r => r.json()).then(c => {
            video.src = 'http://' + location.hostname + ':' + c.videoPort + '/stream';
            durationInput.min = c.minDurationMs;
            durationInput.max = c.maxDurationMs;
            durationInput.value = c.durationMs;
            frequencyInput.min = c.minFrequency;
            frequencyInput.max = c.maxFrequency;
            frequencyInput.step = '0.1';
            frequencyInput.value = c.frequency;
            maxDurationMs = c.maxDurationMs;
            loadPinMapping(c.defaultPins || [7, 8, 9, 10])
        }).catch(() => setCameraState('offline', 'Config Read Failed'));
        let active = null;

        function clearActive() {
            if (active) {
                active.classList.remove('active');
                active = null
            }
        }
        document.querySelectorAll('button[data-command]').forEach(button => {
            button.addEventListener('pointerdown', event => {
                event.preventDefault();
                clearActive();
                active = button;
                button.classList.add('active');
                send(button.dataset.command)
            });
            button.addEventListener('pointerup', clearActive);
            button.addEventListener('pointercancel', clearActive);
            button.addEventListener('pointerleave', clearActive);
        });
        window.addEventListener('blur', clearActive);
    </script>
</body>

</html>
)HTML";
}
