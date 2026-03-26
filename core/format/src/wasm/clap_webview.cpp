// CLAP webview extension — auto-generated parameter UI
// Generates HTML/JS that creates a responsive parameter editor
// communicating with the plugin via postMessage.

#include <pulp/format/web/clap_webview.hpp>
#include <sstream>

namespace pulp::format::wclap {

std::string generate_webview_html(const std::string& plugin_name,
                                   const std::string& params_json) {
    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>)" << plugin_name << R"(</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', system-ui, sans-serif;
    background: #1e1e2e; color: #cdd6f4;
    display: flex; flex-direction: column; min-height: 100vh;
    padding: 16px;
  }
  h1 { font-size: 18px; color: #89b4fa; margin-bottom: 16px; }
  .params { display: flex; flex-wrap: wrap; gap: 12px; }
  .param {
    background: #313244; border-radius: 8px; padding: 12px;
    min-width: 140px; flex: 1;
  }
  .param label { display: block; font-size: 11px; color: #a6adc8; margin-bottom: 4px; }
  .param .value { font-size: 14px; font-weight: 600; color: #cdd6f4; margin-bottom: 8px; }
  .param input[type=range] {
    width: 100%; accent-color: #89b4fa; cursor: pointer;
  }
  .param.boolean input[type=checkbox] {
    width: 20px; height: 20px; accent-color: #89b4fa; cursor: pointer;
  }
  .footer { margin-top: auto; padding-top: 16px; font-size: 11px; color: #585b70; }
</style>
</head>
<body>
<h1>)" << plugin_name << R"(</h1>
<div class="params" id="params"></div>
<div class="footer">Pulp WebView UI — CLAP webview extension</div>
<script>
const params = )" << params_json << R"(;

function sendParam(id, value) {
  // postMessage to plugin host (WCLAP bridge or WAMv2 host)
  if (window.parent !== window) {
    window.parent.postMessage({ type: 'param', id, value }, '*');
  }
  if (window.clapHost) {
    window.clapHost.setParamValue(id, value);
  }
}

function createParamUI(param) {
  const div = document.createElement('div');
  div.className = 'param' + (param.type === 'boolean' ? ' boolean' : '');

  const label = document.createElement('label');
  label.textContent = param.label + (param.unit ? ' (' + param.unit + ')' : '');

  const valueDisplay = document.createElement('div');
  valueDisplay.className = 'value';

  if (param.type === 'boolean') {
    const cb = document.createElement('input');
    cb.type = 'checkbox';
    cb.checked = param.defaultValue >= 0.5;
    valueDisplay.textContent = cb.checked ? 'ON' : 'OFF';
    cb.addEventListener('change', () => {
      const v = cb.checked ? 1.0 : 0.0;
      valueDisplay.textContent = cb.checked ? 'ON' : 'OFF';
      sendParam(param.id, v);
    });
    div.appendChild(label);
    div.appendChild(valueDisplay);
    div.appendChild(cb);
  } else {
    const slider = document.createElement('input');
    slider.type = 'range';
    slider.min = param.minValue;
    slider.max = param.maxValue;
    slider.step = param.step || 0.01;
    slider.value = param.defaultValue;
    valueDisplay.textContent = Number(param.defaultValue).toFixed(2);
    slider.addEventListener('input', () => {
      const v = parseFloat(slider.value);
      valueDisplay.textContent = v.toFixed(2);
      sendParam(param.id, v);
    });
    slider.addEventListener('dblclick', () => {
      slider.value = param.defaultValue;
      valueDisplay.textContent = Number(param.defaultValue).toFixed(2);
      sendParam(param.id, param.defaultValue);
    });
    div.appendChild(label);
    div.appendChild(valueDisplay);
    div.appendChild(slider);
  }

  return div;
}

// Receive param updates from host
window.addEventListener('message', (e) => {
  if (e.data?.type === 'paramUpdate') {
    // Update slider/checkbox to reflect host-side changes
  }
});

const container = document.getElementById('params');
params.forEach(p => container.appendChild(createParamUI(p)));
</script>
</body>
</html>)";
    return html.str();
}

} // namespace pulp::format::wclap
