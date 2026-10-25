#ifndef AP_HTML_H
#define AP_HTML_H

#include <Arduino.h>

const char AP_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Matrix Clock</title>
  <style>
    :root { color-scheme: dark; }
    body { font-family: system-ui, sans-serif; margin: 0; background: #10131a; color: #e8edf5; }
    main { max-width: 620px; margin: 0 auto; padding: 24px; }
    h1 { margin: 0 0 8px; }
    .sub { color: #aab5c9; margin-bottom: 22px; }
    section { background: #1a202c; border: 1px solid #2b3648; border-radius: 12px; padding: 18px; margin: 14px 0; }
    label { display: block; font-weight: 650; margin-top: 12px; }
    input { width: 100%; box-sizing: border-box; padding: 11px; margin-top: 5px; border-radius: 7px; border: 1px solid #48566b; background: #111722; color: #fff; font: inherit; }
    input[type=checkbox] { width: auto; accent-color: #51a2ff; }
    input[type=color] { height: 46px; padding: 3px; }
    button { margin-top: 20px; background: #2879e0; border: 0; border-radius: 7px; padding: 12px 16px; color: white; font: inherit; font-weight: 700; cursor: pointer; }
    #status { padding: 11px; background: #17283c; border-radius: 7px; white-space: pre-line; }
    .notice { font-size: .9rem; color: #b8c3d5; }
  </style>
</head>
<body>
  <main>
    <h1>Matrix Clock Settings</h1>
    <p class="sub">Change any settings below. All fields are optional—leave any field empty to keep its current value.</p>
    <div id="status">Loading device status…</div>
    <form method="post" action="/save">
      <input type="hidden" name="blink_submitted" value="1">
      <section>
        <h2>Clock & Display</h2>
        <label>Date <input name="date" type="date" id="date"></label>
        <label>Time <input name="time" type="time" id="time"></label>
        <label>Brightness (0–255)<input name="brightness" type="number" min="0" max="255" id="brightness" placeholder="Leave empty to keep current"></label>
        <label>Clock color <input name="color" type="color" id="color"></label>
        <label>Weather display interval (1–30 min)<input name="weatherIntervalMin" type="number" min="1" max="30" id="weatherIntervalMin" placeholder="Default: 5 min"></label>
        <label><input name="blink" type="checkbox" id="blink"> Blink the colon each second</label>
      </section>
      <section>
        <h2>Access Point (Setup Wi-Fi)</h2>
        <p class="notice">Connect to this device's setup network at 192.168.4.1 or http://clock.local.</p>
        <label>Access-point name <input maxlength="32" name="apSsid" id="apSsid" placeholder="Leave empty to keep current"></label>
        <label>New access-point password <input minlength="8" maxlength="63" name="apPassword" type="password" placeholder="Leave blank to keep current password"></label>
      </section>
      <section>
        <h2>Home Wi-Fi Connection</h2>
        <p class="notice">Connects the clock to your home Wi-Fi for NTP time sync and http://clock.local access.</p>
        <label>Wi-Fi name <input maxlength="32" name="staSsid" id="staSsid" placeholder="Leave empty to keep current"></label>
        <label>Wi-Fi password <input maxlength="63" name="staPassword" type="password" placeholder="Leave blank to keep current password"></label>
      </section>
      <button type="submit">Save settings</button>
    </form>
    <form method="post" action="/restart">
      <button type="submit">Restart device</button>
    </form>
    <script>
      const $ = id => document.getElementById(id);
      fetch('/api/status').then(r => r.json()).then(s => {
        $('status').textContent = s.status + '\nAP: ' + s.apIp + (s.staIp ? '\nHome Wi-Fi: ' + s.staIp : '') + '\nHost: http://' + s.hostname;
        $('date').value = s.date;
        $('time').value = s.time;
        $('brightness').value = s.brightness;
        $('color').value = s.color;
        $('weatherIntervalMin').value = s.weatherIntervalMin || 5;
        $('blink').checked = s.blink;
        $('apSsid').value = s.apSsid;
        $('staSsid').value = s.staSsid;
      }).catch(() => $('status').textContent = 'Unable to read device status.');
    </script>
  </main>
</body>
</html>)HTML";

#endif // AP_HTML_H
