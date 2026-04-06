/**
 * WAMv2 Plugin Runtime — wraps a Pulp WASM module as a Web Audio Module.
 *
 * Usage by a WAMv2 host:
 *   const wam = await PulpWAM.createInstance(audioContext);
 *   audioContext.destination.connect(wam.audioNode);
 *
 * Conforms to @webaudiomodules/api WebAudioModule interface.
 */

// ── WamProcessor (AudioWorkletProcessor) ────────────────────────────────

const WamProcessorCode = `
class PulpWamProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    this._module = options.processorOptions?.wasmModule;
    this._initialized = false;
    this.port.onmessage = (e) => this._handleMessage(e.data);
  }

  async _init(sampleRate) {
    if (this._module) {
      const instance = await WebAssembly.instantiate(this._module);
      this._wasm = instance.exports;
      this._wasm.wam_initialize(sampleRate, 128);
      this._initialized = true;
    }
  }

  _handleMessage(msg) {
    switch (msg.type) {
      case 'init':
        this._init(msg.sampleRate);
        break;
      case 'param':
        if (this._wasm) this._wasm.wam_set_param(msg.id, msg.value);
        break;
      case 'midi':
        if (this._wasm) this._wasm.wam_schedule_midi(msg.status, msg.data1, msg.data2, 0);
        break;
      case 'getState':
        // TODO: serialize state from WASM and post back
        this.port.postMessage({ type: 'state', data: null });
        break;
      case 'setState':
        // TODO: deserialize state into WASM
        break;
    }
  }

  process(inputs, outputs, parameters) {
    if (!this._initialized || !this._wasm) return true;

    const input = inputs[0];
    const output = outputs[0];
    if (!output || output.length === 0) return true;

    const numFrames = output[0].length;
    const numChannels = output.length;

    // Call WASM processor
    // For now, process channel-by-channel (simplified interop)
    for (let ch = 0; ch < numChannels; ch++) {
      const inData = (input && input[ch]) ? input[ch] : new Float32Array(numFrames);
      this._wasm.wam_process(inData, output[ch], 1, numFrames);
    }

    return true;
  }

  static get parameterDescriptors() {
    return []; // WAMv2 manages params via message port, not AudioParam
  }
}

registerProcessor('pulp-wam-processor', PulpWamProcessor);
`;

// ── WamNode (AudioWorkletNode) ──────────────────────────────────────────

class PulpWamNode extends AudioWorkletNode {
  constructor(module, options) {
    super(module.audioContext, 'pulp-wam-processor', {
      numberOfInputs: module.descriptor.hasAudioInput ? 1 : 0,
      numberOfOutputs: module.descriptor.hasAudioOutput ? 1 : 0,
      outputChannelCount: [2],
      processorOptions: { wasmModule: options?.wasmModule }
    });
    this._module = module;
  }

  get module() { return this._module; }
  get groupId() { return this._module.groupId; }
  get moduleId() { return this._module.moduleId; }
  get instanceId() { return this._module.instanceId; }

  async getParameterInfo() { return this._module.getParameterInfo(); }
  async getParameterValues() { return this._module.getParameterValues(); }
  async setParameterValues(values) { return this._module.setParameterValues(values); }

  scheduleEvents(events) {
    for (const event of events) {
      if (event.type === 'wam-midi') {
        this.port.postMessage({
          type: 'midi',
          status: event.data.bytes[0],
          data1: event.data.bytes[1],
          data2: event.data.bytes[2]
        });
      } else if (event.type === 'wam-automation') {
        this.port.postMessage({
          type: 'param',
          id: event.data.id,
          value: event.data.value
        });
      }
    }
  }

  clearEvents() { /* TODO */ }
  connectEvents(target) { /* TODO: WAMv2 inter-plugin event routing */ }
  disconnectEvents(target) { /* TODO */ }

  async getState() {
    return new Promise((resolve) => {
      this.port.onmessage = (e) => {
        if (e.data.type === 'state') resolve(e.data.data);
      };
      this.port.postMessage({ type: 'getState' });
    });
  }

  async setState(state) {
    this.port.postMessage({ type: 'setState', data: state });
  }

  destroy() {
    this.disconnect();
  }
}

// ── WebAudioModule (main thread) ────────────────────────────────────────

let instanceCounter = 0;

export default class PulpWAM {
  static get isWebAudioModuleConstructor() { return true; }

  static async createInstance(audioContext, initialState) {
    const wam = new PulpWAM(audioContext);
    await wam.initialize(initialState);
    return wam;
  }

  constructor(audioContext) {
    this._audioContext = audioContext;
    this._audioNode = null;
    this._initialized = false;
    this._instanceId = `pulp-wam-${++instanceCounter}`;
    this._descriptor = null;
    this._paramInfo = {};
  }

  get isWebAudioModule() { return true; }
  get audioContext() { return this._audioContext; }
  get audioNode() { return this._audioNode; }
  get initialized() { return this._initialized; }
  get groupId() { return 'pulp'; }
  get moduleId() { return 'com.pulp.wam'; }
  get instanceId() { return this._instanceId; }
  get descriptor() { return this._descriptor; }
  get name() { return this._descriptor?.name || 'Pulp Plugin'; }
  get vendor() { return this._descriptor?.vendor || 'Pulp'; }

  async initialize(state) {
    // Register AudioWorklet processor
    const blob = new Blob([WamProcessorCode], { type: 'application/javascript' });
    const url = URL.createObjectURL(blob);
    await this._audioContext.audioWorklet.addModule(url);
    URL.revokeObjectURL(url);

    // Fetch descriptor from WASM
    // In production, this would load the WASM module and call wam_get_descriptor_json()
    this._descriptor = {
      name: 'Pulp Plugin',
      vendor: 'Pulp',
      version: '1.0.0',
      apiVersion: '2.0.0',
      isInstrument: false,
      hasAudioInput: true,
      hasAudioOutput: true,
      hasMidiInput: false,
      hasMidiOutput: false,
      hasAutomationInput: true,
      hasAutomationOutput: false,
    };

    this._audioNode = await this.createAudioNode();
    this._initialized = true;

    if (state) await this.setState(state);
  }

  async createAudioNode() {
    const node = new PulpWamNode(this, {});
    node.port.postMessage({
      type: 'init',
      sampleRate: this._audioContext.sampleRate
    });
    return node;
  }

  async createGui() {
    // WAMv2 GUI: return an HTMLElement
    const el = document.createElement('div');
    el.innerHTML = `<div style="padding:16px;background:#1e1e2e;color:#cdd6f4;font-family:system-ui;">
      <h3>${this.name}</h3>
      <p>WAMv2 Plugin by ${this.vendor}</p>
    </div>`;
    return el;
  }

  destroyGui(element) {
    if (element?.parentNode) element.parentNode.removeChild(element);
  }

  async getParameterInfo() { return this._paramInfo; }
  async getParameterValues() { return {}; /* TODO */ }
  async setParameterValues(values) {
    if (this._audioNode) {
      for (const [id, { value }] of Object.entries(values)) {
        this._audioNode.port.postMessage({ type: 'param', id, value });
      }
    }
  }

  async getState() { return this._audioNode?.getState(); }
  async setState(state) { return this._audioNode?.setState(state); }

  scheduleEvents(events) { this._audioNode?.scheduleEvents(events); }
  clearEvents() { this._audioNode?.clearEvents(); }
  connectEvents(target) { this._audioNode?.connectEvents(target); }
  disconnectEvents(target) { this._audioNode?.disconnectEvents(target); }
}
