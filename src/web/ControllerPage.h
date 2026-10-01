/**
 * @file ControllerPage.h
 * @brief SoftAP 模式下使用的最小 Wi-Fi 配网页。
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
  <title>XIAO Wi-Fi 配置</title>
  <style>
    :root { color-scheme: light; font-family: -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif }
    * { box-sizing: border-box }
    body {
      min-height: 100vh; margin: 0; padding: 24px; display: grid; place-items: center;
      color: #111; background: #ededed
    }
    main {
      width: min(420px,100%); padding: 28px; border: 1px solid #d2d2d2; border-radius: 18px;
      background: #fff; box-shadow: 0 16px 44px rgba(0,0,0,.09)
    }
    h1 { margin: 0 0 8px; font-size: 1.45rem }
    .intro { margin: 0 0 24px; color: #666; font-size: .9rem; line-height: 1.55 }
    label { display: grid; gap: 7px; margin-top: 15px; font-size: .82rem; font-weight: 650 }
    input, select {
      width: 100%; min-height: 46px; padding: 10px 12px; border: 1px solid #bdbdbd;
      border-radius: 10px; background: #fff; color: #111; font: inherit; outline: none
    }
    input:focus, select:focus { border-color: #222; box-shadow: 0 0 0 3px rgba(0,0,0,.08) }
    select:disabled { color: #888; background: #f3f3f3 }
    button {
      min-height: 46px; padding: 10px 15px; border: 1px solid #111; border-radius: 10px;
      background: #111; color: #fff; font: inherit; font-weight: 700; cursor: pointer
    }
    button.primary { width: 100%; margin-top: 20px }
    button:disabled { opacity: .55; cursor: wait }
    #status { min-height: 2.8em; margin: 16px 0 0; color: #777; font-size: .78rem; line-height: 1.45 }
    #status.error { color: #111; font-weight: 700 }
    #status.success { color: #111; font-weight: 700 }
  </style>
</head>
<body>
  <main>
    <h1>连接现有 Wi-Fi</h1>
    <p class="intro">选择网络并输入密码。列表每 30 秒自动刷新，连接成功后 XIAO 自建热点会自动关闭。</p>
    <label>Wi-Fi 名称（SSID）
      <select id="ssid" disabled>
        <option value="">正在扫描…</option>
      </select>
    </label>
    <label>Wi-Fi 密码
      <input id="password" type="password" maxlength="63" autocomplete="new-password" placeholder="开放网络可留空">
    </label>
    <button id="connect" class="primary" type="button">连接 Wi-Fi</button>
    <p id="status">正在扫描附近的 Wi-Fi…</p>
  </main>
  <script>
    const ssidSelect = document.getElementById('ssid');
    const passwordInput = document.getElementById('password');
    const connectButton = document.getElementById('connect');
    const status = document.getElementById('status');
    let scanning = false;
    let connecting = false;

    function setStatus(message, kind = '') {
      status.textContent = message;
      status.className = kind
    }

    async function scanNetworks() {
      if (scanning || connecting) return;
      scanning = true;
      const selectedSsid = ssidSelect.value;
      setStatus('正在扫描附近的 Wi-Fi…');
      try {
        const response = await fetch('/wifi/networks', { cache: 'no-store' });
        const result = await response.json();
        if (!response.ok) throw Error(result.error || '扫描失败');
        ssidSelect.replaceChildren();
        result.networks.forEach(network => {
          const option = document.createElement('option');
          option.value = network.ssid;
          option.textContent = network.ssid + '  (' + network.rssi + ' dBm' + (network.secure ? '，加密' : '，开放') + ')';
          ssidSelect.appendChild(option)
        });
        if (!result.networks.length) {
          const option = document.createElement('option');
          option.value = '';
          option.textContent = '没有发现 Wi-Fi';
          ssidSelect.appendChild(option)
        }
        ssidSelect.disabled = !result.networks.length;
        if (selectedSsid && result.networks.some(network => network.ssid === selectedSsid)) {
          ssidSelect.value = selectedSsid
        }
        setStatus(result.networks.length ? '已发现 ' + result.networks.length + ' 个网络；30 秒后自动刷新。' : '没有发现可用网络；30 秒后自动重试。')
      } catch (error) {
        setStatus(error.message, 'error')
      } finally {
        scanning = false
      }
    }

    async function connectWifi() {
      const ssid = ssidSelect.value;
      if (!ssid) {
        setStatus('请先从列表中选择 Wi-Fi。', 'error');
        ssidSelect.focus();
        return
      }
      connecting = true;
      ssidSelect.disabled = true;
      connectButton.disabled = true;
      setStatus('正在连接，请保持此页面打开，最多需要 10 秒…');
      try {
        const response = await fetch('/wifi/connect', {
          method: 'POST',
          cache: 'no-store',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
          body: new URLSearchParams({ ssid, password: passwordInput.value })
        });
        const result = await response.json();
        if (!response.ok) throw Error(result.error || '连接失败');
        setStatus('连接成功。板子的新 IP 是 ' + result.ip + '。热点即将关闭，请让控制设备加入同一个 Wi-Fi。', 'success')
      } catch (error) {
        setStatus(error.message, 'error');
        connecting = false;
        ssidSelect.disabled = false;
        connectButton.disabled = false;
        scanNetworks()
      }
    }

    connectButton.addEventListener('click', connectWifi);
    scanNetworks();
    setInterval(scanNetworks, 30000);
  </script>
</body>
</html>
)HTML";
}
