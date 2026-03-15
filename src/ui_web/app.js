const ROUTING_LABELS = {
  forward: "Forward",
  backward: "Backward",
  random: "Random",
  "round-robin": "Round Robin",
  custom: "Custom",
};

const SOURCE_DESCRIPTIONS = {
  robin: "Master-linked routing synth. Dial in one sound, then use output routing and local overrides for variation.",
  test: "Single-oscillator debug source. Useful for routing, speaker, and mixer checks.",
  decor: "Voice-per-output immersive source scaffold. State exists, DSP does not.",
  pieces: "Granular algorithmic source scaffold. Routing and granular DSP come later.",
};

const elements = {
  tabs: Array.from(document.querySelectorAll(".page-tab")),
  pages: Array.from(document.querySelectorAll(".page")),
  heroOutputCount: document.getElementById("heroOutputCount"),
  heroVoiceCount: document.getElementById("heroVoiceCount"),
  heroRoutingValue: document.getElementById("heroRoutingValue"),
  audioBackendValue: document.getElementById("audioBackendValue"),
  outputDeviceValue: document.getElementById("outputDeviceValue"),
  engineOutputDevice: document.getElementById("engineOutputDevice"),
  engineOutputChannels: document.getElementById("engineOutputChannels"),
  audioRunningValue: document.getElementById("audioRunningValue"),
  midiStatusValue: document.getElementById("midiStatusValue"),
  midiSourcesValue: document.getElementById("midiSourcesValue"),
  midiConnectedSourcesValue: document.getElementById("midiConnectedSourcesValue"),
  oscStatusValue: document.getElementById("oscStatusValue"),
  oscPortValue: document.getElementById("oscPortValue"),
  lastMidiNoteValue: document.getElementById("lastMidiNoteValue"),
  sampleRateValue: document.getElementById("sampleRateValue"),
  channelsValue: document.getElementById("channelsValue"),
  bufferValue: document.getElementById("bufferValue"),
  refreshButton: document.getElementById("refreshButton"),
  stateView: document.getElementById("stateView"),
  statusText: document.getElementById("statusText"),
  midiDeviceGrid: document.getElementById("midiDeviceGrid"),
  sourceMixerGrid: document.getElementById("sourceMixerGrid"),
  outputMixerGrid: document.getElementById("outputMixerGrid"),
  voiceCount: document.getElementById("voiceCount"),
  voiceCountValue: document.getElementById("voiceCountValue"),
  oscillatorsPerVoice: document.getElementById("oscillatorsPerVoice"),
  oscillatorsPerVoiceValue: document.getElementById("oscillatorsPerVoiceValue"),
  routingPreset: document.getElementById("routingPreset"),
  frequency: document.getElementById("frequency"),
  frequencyValue: document.getElementById("frequencyValue"),
  waveform: document.getElementById("waveform"),
  robinAttack: document.getElementById("robinAttack"),
  robinAttackValue: document.getElementById("robinAttackValue"),
  robinDecay: document.getElementById("robinDecay"),
  robinDecayValue: document.getElementById("robinDecayValue"),
  robinSustain: document.getElementById("robinSustain"),
  robinSustainValue: document.getElementById("robinSustainValue"),
  robinRelease: document.getElementById("robinRelease"),
  robinReleaseValue: document.getElementById("robinReleaseValue"),
  voicesGrid: document.getElementById("voicesGrid"),
  oscillatorRows: document.getElementById("oscillatorRows"),
  robinLinkBadge: document.getElementById("robinLinkBadge"),
  robinLinkNote: document.getElementById("robinLinkNote"),
  testActive: document.getElementById("testActive"),
  testMidiEnabled: document.getElementById("testMidiEnabled"),
  testFrequency: document.getElementById("testFrequency"),
  testFrequencyValue: document.getElementById("testFrequencyValue"),
  testGain: document.getElementById("testGain"),
  testGainValue: document.getElementById("testGainValue"),
  testWaveform: document.getElementById("testWaveform"),
  testAttack: document.getElementById("testAttack"),
  testAttackValue: document.getElementById("testAttackValue"),
  testDecay: document.getElementById("testDecay"),
  testDecayValue: document.getElementById("testDecayValue"),
  testSustain: document.getElementById("testSustain"),
  testSustainValue: document.getElementById("testSustainValue"),
  testRelease: document.getElementById("testRelease"),
  testReleaseValue: document.getElementById("testReleaseValue"),
  testOutputGrid: document.getElementById("testOutputGrid"),
  decorVoiceCountValue: document.getElementById("decorVoiceCountValue"),
  decorOutputGrid: document.getElementById("decorOutputGrid"),
  piecesVoiceCountValue: document.getElementById("piecesVoiceCountValue"),
  fxSaturatorEnabled: document.getElementById("fxSaturatorEnabled"),
  fxSaturatorLinkedLevels: document.getElementById("fxSaturatorLinkedLevels"),
  fxSaturatorInputLevel: document.getElementById("fxSaturatorInputLevel"),
  fxSaturatorInputLevelValue: document.getElementById("fxSaturatorInputLevelValue"),
  fxSaturatorOutputLevel: document.getElementById("fxSaturatorOutputLevel"),
  fxSaturatorOutputLevelValue: document.getElementById("fxSaturatorOutputLevelValue"),
  fxChorusEnabled: document.getElementById("fxChorusEnabled"),
  fxChorusLinkedControls: document.getElementById("fxChorusLinkedControls"),
  fxChorusDepth: document.getElementById("fxChorusDepth"),
  fxChorusDepthValue: document.getElementById("fxChorusDepthValue"),
  fxChorusSpeed: document.getElementById("fxChorusSpeed"),
  fxChorusSpeedValue: document.getElementById("fxChorusSpeedValue"),
  fxChorusPhaseSpread: document.getElementById("fxChorusPhaseSpread"),
  fxChorusPhaseSpreadValue: document.getElementById("fxChorusPhaseSpreadValue"),
  fxSidechainEnabled: document.getElementById("fxSidechainEnabled"),
  fxOutputGrid: document.getElementById("fxOutputGrid"),
  lfoEnabled: document.getElementById("lfoEnabled"),
  lfoWaveform: document.getElementById("lfoWaveform"),
  lfoDepth: document.getElementById("lfoDepth"),
  lfoDepthValue: document.getElementById("lfoDepthValue"),
  lfoClockLinked: document.getElementById("lfoClockLinked"),
  lfoTempo: document.getElementById("lfoTempo"),
  lfoTempoValue: document.getElementById("lfoTempoValue"),
  lfoRateMultiplier: document.getElementById("lfoRateMultiplier"),
  lfoRateMultiplierValue: document.getElementById("lfoRateMultiplierValue"),
  lfoFixedFrequency: document.getElementById("lfoFixedFrequency"),
  lfoFixedFrequencyValue: document.getElementById("lfoFixedFrequencyValue"),
  lfoPhaseSpread: document.getElementById("lfoPhaseSpread"),
  lfoPhaseSpreadValue: document.getElementById("lfoPhaseSpreadValue"),
  lfoPolarityFlip: document.getElementById("lfoPolarityFlip"),
  lfoUnlinkedOutputs: document.getElementById("lfoUnlinkedOutputs"),
};

let state = null;
let nextMutationSequence = 1;
let latestRenderedSequence = 0;
const inFlightParams = new Map();
const queuedParams = new Map();
let activeRangeInput = null;
let hasDeferredRender = false;

function getEngine() {
  return state?.engine ?? null;
}

function getSourceMixer() {
  return state?.sourceMixer ?? null;
}

function getMidi() {
  return state?.engine?.midi ?? { sources: [], connectedSourceCount: 0, running: false };
}

function getOutputMixer() {
  return state?.outputMixer ?? null;
}

function getRobin() {
  return state?.sources?.robin ?? null;
}

function getTest() {
  return state?.sources?.test ?? null;
}

function getDecor() {
  return state?.sources?.decor ?? null;
}

function getPieces() {
  return state?.sources?.pieces ?? null;
}

function getFx() {
  return state?.processors?.fx ?? null;
}

function clampValue(value, min, max) {
  return Math.min(max, Math.max(min, Number(value)));
}

function updateStateView() {
  elements.stateView.textContent = state ? JSON.stringify(state, null, 2) : "";
}

function drainQueuedParam(path) {
  if (!queuedParams.has(path)) {
    return;
  }

  const nextRequest = queuedParams.get(path);
  queuedParams.delete(path);

  if (nextRequest.mode === "fast") {
    dispatchParamFast(path, nextRequest.value, {
      silent: nextRequest.silent,
      onSuccess: nextRequest.onSuccess,
    });
    return;
  }

  dispatchParam(path, nextRequest.value, { silent: nextRequest.silent });
}

function renderRobinOscillatorUi() {
  renderRobinLinkState();
  renderOscillators();
}

function applyRobinMasterOscillatorUpdate(oscillatorIndex, field, rawValue) {
  const robin = getRobin();
  if (!robin?.voices?.length) {
    return;
  }

  robin.voices.forEach((voice) => {
    const oscillator = voice.oscillators?.[oscillatorIndex];
    if (!oscillator) {
      return;
    }

    if (field === "enabled") {
      oscillator.enabled = Boolean(rawValue);
      return;
    }

    if (field === "gain") {
      oscillator.gain = clampValue(rawValue, 0, 1);
      return;
    }

    if (field === "waveform") {
      oscillator.waveform = String(rawValue);
      return;
    }

    if (field === "relative") {
      const nextRelative = Boolean(rawValue);
      if (oscillator.relativeToVoice !== nextRelative) {
        if (nextRelative) {
          oscillator.frequencyValue = clampValue(
            Number(oscillator.frequencyValue) / Math.max(1, Number(voice.frequency)),
            0.01,
            8,
          );
        } else {
          oscillator.frequencyValue = clampValue(
            Number(oscillator.frequencyValue) * Math.max(1, Number(voice.frequency)),
            1,
            20000,
          );
        }
      }

      oscillator.relativeToVoice = nextRelative;
      return;
    }

    if (field === "frequency") {
      oscillator.frequencyValue = oscillator.relativeToVoice
        ? clampValue(rawValue, 0.01, 8)
        : clampValue(rawValue, 1, 20000);
    }
  });
}

function setStatus(message) {
  elements.statusText.textContent = message;
}

function beginRangeInteraction(target) {
  if (!(target instanceof HTMLInputElement) || target.type !== "range") {
    return;
  }

  activeRangeInput = target;
}

function endRangeInteraction(target = activeRangeInput) {
  if (!activeRangeInput || (target && target !== activeRangeInput)) {
    return;
  }

  activeRangeInput = null;
  if (hasDeferredRender && state) {
    hasDeferredRender = false;
    renderState(state);
  }
}

function selectPage(pageName) {
  elements.tabs.forEach((button) => {
    button.classList.toggle("is-active", button.dataset.page === pageName);
  });
  elements.pages.forEach((page) => {
    page.classList.toggle("is-active", page.id === `page-${pageName}`);
  });
}

function formatRoutingPresetLabel(value) {
  return ROUTING_LABELS[value] ?? value;
}

function syncSelectOptions(select, options, selectedValue) {
  if (!select) {
    return;
  }

  const currentOptions = Array.from(select.options).map((option) => ({
    value: option.value,
    label: option.textContent,
    disabled: option.disabled,
  }));
  const nextOptions = options.map((option) => ({
    value: String(option.value),
    label: option.label,
    disabled: Boolean(option.disabled),
  }));

  const optionsChanged = currentOptions.length !== nextOptions.length
    || currentOptions.some((option, index) => {
      const nextOption = nextOptions[index];
      return !nextOption
        || option.value !== nextOption.value
        || option.label !== nextOption.label
        || option.disabled !== nextOption.disabled;
    });

  if (optionsChanged) {
    select.replaceChildren();
    nextOptions.forEach((option) => {
      const element = document.createElement("option");
      element.value = option.value;
      element.textContent = option.label;
      element.disabled = option.disabled;
      select.appendChild(element);
    });
  }

  const resolvedSelectedValue = selectedValue == null ? "" : String(selectedValue);
  if (select.value !== resolvedSelectedValue) {
    select.value = resolvedSelectedValue;
  }
}

function renderEngineOutputControls(engine) {
  const devices = Array.isArray(engine.availableOutputDevices) ? engine.availableOutputDevices : [];
  const selectedDeviceId = engine.selectedOutputDeviceId ?? "";
  const selectedDevice = devices.find((device) => device.id === selectedDeviceId) ?? null;
  const maxOutputChannels = Math.max(1, Number(selectedDevice?.outputChannels ?? engine.maxOutputChannels ?? engine.outputChannels ?? 1));

  syncSelectOptions(
    elements.engineOutputDevice,
    devices.map((device) => ({
      value: device.id,
      label: `${device.name} (${device.outputChannels} outs${device.isDefault ? ", default" : ""})`,
    })),
    selectedDeviceId,
  );
  elements.engineOutputDevice.disabled = devices.length === 0;

  syncSelectOptions(
    elements.engineOutputChannels,
    Array.from({ length: maxOutputChannels }, (_, index) => ({
      value: String(index + 1),
      label: `${index + 1} ${index === 0 ? "output" : "outputs"}`,
    })),
    String(engine.outputChannels ?? 1),
  );
  elements.engineOutputChannels.disabled = maxOutputChannels <= 0;
}

function setFrequencyLabel(value) {
  elements.frequencyValue.textContent = `${Math.round(Number(value))} Hz`;
}

function setRobinAttackLabel(value) {
  elements.robinAttackValue.textContent = `${Math.round(Number(value))} ms`;
}

function setRobinDecayLabel(value) {
  elements.robinDecayValue.textContent = `${Math.round(Number(value))} ms`;
}

function setRobinSustainLabel(value) {
  elements.robinSustainValue.textContent = Number(value).toFixed(2);
}

function setRobinReleaseLabel(value) {
  elements.robinReleaseValue.textContent = `${Math.round(Number(value))} ms`;
}

function setTestFrequencyLabel(value) {
  elements.testFrequencyValue.textContent = `${Math.round(Number(value))} Hz`;
}

function setTestGainLabel(value) {
  elements.testGainValue.textContent = Number(value).toFixed(2);
}

function setTestAttackLabel(value) {
  elements.testAttackValue.textContent = `${Math.round(Number(value))} ms`;
}

function setTestDecayLabel(value) {
  elements.testDecayValue.textContent = `${Math.round(Number(value))} ms`;
}

function setTestSustainLabel(value) {
  elements.testSustainValue.textContent = Number(value).toFixed(2);
}

function setTestReleaseLabel(value) {
  elements.testReleaseValue.textContent = `${Math.round(Number(value))} ms`;
}

function setVoiceCountLabel(value) {
  elements.voiceCountValue.textContent = `${Math.round(Number(value))}`;
}

function setOscillatorCountLabel(value) {
  elements.oscillatorsPerVoiceValue.textContent = `${Math.round(Number(value))}`;
}

function setLfoDepthLabel(value) {
  elements.lfoDepthValue.textContent = Number(value).toFixed(2);
}

function setLfoTempoLabel(value) {
  elements.lfoTempoValue.textContent = `${Math.round(Number(value))} BPM`;
}

function setLfoRateMultiplierLabel(value) {
  elements.lfoRateMultiplierValue.textContent = `${Number(value).toFixed(3)}x`;
}

function setLfoFixedFrequencyLabel(value) {
  elements.lfoFixedFrequencyValue.textContent = `${Number(value).toFixed(2)} Hz`;
}

function setLfoPhaseSpreadLabel(value) {
  elements.lfoPhaseSpreadValue.textContent = `${Math.round(Number(value))} deg`;
}

function setSaturatorInputLabel(value) {
  elements.fxSaturatorInputLevelValue.textContent = Number(value).toFixed(2);
}

function setSaturatorOutputLabel(value) {
  elements.fxSaturatorOutputLevelValue.textContent = Number(value).toFixed(2);
}

function setChorusDepthLabel(value) {
  elements.fxChorusDepthValue.textContent = Number(value).toFixed(2);
}

function setChorusSpeedLabel(value) {
  elements.fxChorusSpeedValue.textContent = `${Number(value).toFixed(2)} Hz`;
}

function setChorusPhaseLabel(value) {
  elements.fxChorusPhaseSpreadValue.textContent = `${Math.round(Number(value))} deg`;
}

function syncLfoInputState(lfo) {
  const clockLinked = Boolean(lfo.clockLinked);
  elements.lfoTempo.disabled = !clockLinked;
  elements.lfoRateMultiplier.disabled = !clockLinked;
  elements.lfoFixedFrequency.disabled = clockLinked;
}

function formatLinkedOscillatorFrequency(oscillator) {
  if (oscillator.relativeToVoice) {
    return `${Number(oscillator.frequencyValue).toFixed(2)}x voice root`;
  }

  return `${Math.round(Number(oscillator.frequencyValue))} Hz`;
}

function oscillatorSettingsMatch(left, right) {
  return (
    left.enabled === right.enabled &&
    left.relativeToVoice === right.relativeToVoice &&
    left.waveform === right.waveform &&
    Math.abs(Number(left.gain) - Number(right.gain)) < 0.0001 &&
    Math.abs(Number(left.frequencyValue) - Number(right.frequencyValue)) < 0.0001
  );
}

function areRobinOscillatorsLinked() {
  const robin = getRobin();
  if (!robin || !robin.voices.length) {
    return true;
  }

  const referenceOscillators = robin.voices[0].oscillators;
  return robin.voices.every((voice) => {
    if (voice.oscillators.length !== referenceOscillators.length) {
      return false;
    }

    return voice.oscillators.every((oscillator, index) =>
      oscillatorSettingsMatch(oscillator, referenceOscillators[index]));
  });
}

function renderRobinLinkState() {
  const linked = areRobinOscillatorsLinked();
  elements.robinLinkBadge.textContent = linked ? "Master bank linked" : "Master bank diverged";
  elements.robinLinkBadge.className = `status-tag ${linked ? "status-tag--live" : "status-tag--warning"}`;
  elements.robinLinkNote.textContent = linked
    ? "The oscillator bank is still acting like a master section. Editing any row here applies that row to every voice."
    : "Some voices no longer match the master oscillator bank. Editing here will push the selected row back across all voices.";
}

function renderSourceMixer() {
  const sourceMixer = getSourceMixer();
  if (!sourceMixer) {
    elements.sourceMixerGrid.innerHTML = "";
    return;
  }

  elements.sourceMixerGrid.innerHTML = ["robin", "test", "decor", "pieces"]
    .map((key) => {
      const mix = sourceMixer[key];
      if (!mix) {
        return "";
      }

      const title = key[0].toUpperCase() + key.slice(1);
      const live = mix.implemented;
      const statusClass = live ? "status-tag--live" : "status-tag--planned";
      const statusLabel = live ? "Live" : "Scaffold";

      return `
        <article class="source-card ${live ? "source-card--live" : "source-card--planned"}">
          <div class="source-card__header">
            <h3>${title}</h3>
            <span class="status-tag ${statusClass}">${statusLabel}</span>
          </div>
          <p>${SOURCE_DESCRIPTIONS[key]}</p>
          <label class="toggle-pill toggle-pill--block">
            <input type="checkbox" data-source-enabled="${key}" ${mix.enabled ? "checked" : ""}>
            <span>Enabled</span>
          </label>
          <label class="field">
            <span>Source Level</span>
            <input
              type="range"
              min="0"
              max="1"
              step="0.01"
              value="${mix.level}"
              data-source-level="${key}"
            >
            <output>${Number(mix.level).toFixed(2)}</output>
          </label>
        </article>
      `;
    })
    .join("");

  bindSourceMixerControls();
}

function renderMidiDevices() {
  const midi = getMidi();
  const sourceMixer = getSourceMixer();
  if (!midi.sources.length) {
    elements.midiDeviceGrid.innerHTML = `
      <article class="output-column">
        <div class="output-column__header">
          <h3>No MIDI Sources</h3>
          <span>${midi.running ? "Host ready" : "Host offline"}</span>
        </div>
        <p>Connect a MIDI device and relaunch the app host to populate the CoreMIDI source list.</p>
      </article>
    `;
    return;
  }

  elements.midiDeviceGrid.innerHTML = midi.sources
    .map(
      (source) => {
        const targets = [
          { key: "robin", label: "Robin" },
          { key: "test", label: "Test" },
          { key: "decor", label: "Decor" },
          { key: "pieces", label: "Pieces" },
        ]
          .map(({ key, label }) => {
            const mix = sourceMixer?.[key];
            const implemented = Boolean(mix?.implemented);
            const routeEnabled = Boolean(source.routes?.[key]);
            return `
              <label class="toggle-pill toggle-pill--block">
                <input
                  type="checkbox"
                  data-midi-route="${source.index}:${key}"
                  ${routeEnabled ? "checked" : ""}
                  ${implemented ? "" : "disabled"}
                >
                <span>${implemented ? `Route to ${label}` : `${label} Pending`}</span>
              </label>
            `;
          })
          .join("");

        return `
        <article class="output-column">
          <div class="output-column__header">
            <h3>${source.name}</h3>
            <span>${source.connected ? "Connected" : "Disconnected"}</span>
          </div>

          <label class="toggle-pill toggle-pill--block">
            <input
              type="checkbox"
              data-midi-source="${source.index}"
              ${source.connected ? "checked" : ""}
            >
            <span>${source.connected ? "Receive Input" : "Connect Input"}</span>
          </label>

          <div class="output-column__group">
            <span>Synth Routes</span>
            ${targets}
          </div>

          <p>CoreMIDI source ${source.index + 1}. Connection decides if the host listens; routes decide which synths respond.</p>
        </article>
      `;
      },
    )
    .join("");

  bindMidiDeviceControls();
}

function renderOutputMixer() {
  const outputMixer = getOutputMixer();
  if (!outputMixer) {
    elements.outputMixerGrid.innerHTML = "";
    return;
  }

  elements.outputMixerGrid.innerHTML = outputMixer.outputs
    .map(
      (output) => `
        <article class="output-column">
          <div class="output-column__header">
            <h3>Output ${output.index + 1}</h3>
            <span>Trim + delay + EQ live</span>
          </div>

          <label class="field">
            <span>Level</span>
            <input
              type="range"
              min="0"
              max="1"
              step="0.01"
              value="${output.level}"
              data-output-level="${output.index}"
            >
            <output>${Number(output.level).toFixed(2)}</output>
          </label>

          <label class="field">
            <span>Delay</span>
            <input
              type="range"
              min="0"
              max="250"
              step="1"
              value="${output.delayMs}"
              data-output-delay="${output.index}"
            >
            <output>${Math.round(Number(output.delayMs))} ms</output>
          </label>

          <label class="field">
            <span>Low EQ</span>
            <input
              type="range"
              min="-24"
              max="24"
              step="0.5"
              value="${output.eq.lowDb}"
              data-output-eq="${output.index}:lowDb"
            >
            <output>${Number(output.eq.lowDb).toFixed(1)} dB</output>
          </label>

          <label class="field">
            <span>Mid EQ</span>
            <input
              type="range"
              min="-24"
              max="24"
              step="0.5"
              value="${output.eq.midDb}"
              data-output-eq="${output.index}:midDb"
            >
            <output>${Number(output.eq.midDb).toFixed(1)} dB</output>
          </label>

          <label class="field">
            <span>High EQ</span>
            <input
              type="range"
              min="-24"
              max="24"
              step="0.5"
              value="${output.eq.highDb}"
              data-output-eq="${output.index}:highDb"
            >
            <output>${Number(output.eq.highDb).toFixed(1)} dB</output>
          </label>

          <p>Delay and fixed-band EQ are live.</p>
        </article>
      `,
    )
    .join("");

  bindOutputMixerControls();
}

function renderVoices() {
  const robin = getRobin();
  if (!robin) {
    elements.voicesGrid.innerHTML = "";
    return;
  }

  elements.voicesGrid.innerHTML = robin.voices
    .map((voice) => {
      const outputButtons = voice.outputs
        .map(
          (enabled, outputIndex) => `
            <button
              type="button"
              class="route-button ${enabled ? "is-on" : ""}"
              data-voice-index="${voice.index}"
              data-output-index="${outputIndex}"
            >
              ${outputIndex + 1}
            </button>
          `,
        )
        .join("");
      const outputCount = voice.outputs.filter(Boolean).length;
      const outputRouteLabel = `${outputCount} local output ${outputCount === 1 ? "route" : "routes"} active`;

      return `
        <article class="voice-card ${voice.active ? "is-active" : ""}">
          <div class="voice-card__header">
            <h3>Voice ${voice.index + 1}</h3>
            <label class="toggle-pill">
              <input type="checkbox" data-voice-active="${voice.index}" ${voice.active ? "checked" : ""}>
              <span>Enabled</span>
            </label>
          </div>

          <p class="voice-card__meta">${outputRouteLabel}</p>

          <label class="field">
            <span>Local Root Frequency</span>
            <input
              type="range"
              min="20"
              max="20000"
              step="1"
              value="${voice.frequency}"
              data-voice-frequency="${voice.index}"
            >
            <output>${Math.round(voice.frequency)} Hz</output>
          </label>

          <label class="field">
            <span>Local Voice Level</span>
            <input
              type="range"
              min="0"
              max="1"
              step="0.01"
              value="${voice.gain}"
              data-voice-gain="${voice.index}"
            >
            <output>${Number(voice.gain).toFixed(2)}</output>
          </label>

          <div class="field">
            <span>Local Output Targets</span>
            <div class="route-matrix">
              ${outputButtons}
            </div>
          </div>
        </article>
      `;
    })
    .join("");

  bindVoiceControls();
}

function renderOscillators() {
  const robin = getRobin();
  if (!robin || !robin.voices.length) {
    elements.oscillatorRows.innerHTML = "";
    return;
  }

  const linked = areRobinOscillatorsLinked();
  const referenceVoice = robin.voices[0];

  elements.oscillatorRows.innerHTML = referenceVoice.oscillators
    .map(
      (oscillator) => `
        <section class="oscillator-strip ${linked ? "" : "is-diverged"}">
          <div class="oscillator-strip__header">
            <h3>Master Osc ${oscillator.index + 1}</h3>
            <label class="toggle-pill oscillator-strip__toggle">
              <input
                type="checkbox"
                data-osc-enabled="${oscillator.index}"
                ${oscillator.enabled ? "checked" : ""}
              >
              <span>Enabled</span>
            </label>
          </div>

          <div class="oscillator-strip__body">
            <label class="field oscillator-strip__column">
              <span>Shape</span>
              <select data-osc-waveform="${oscillator.index}">
                ${["sine", "square", "triangle", "saw", "noise"]
                  .map(
                    (waveform) => `
                      <option value="${waveform}" ${oscillator.waveform === waveform ? "selected" : ""}>
                        ${waveform[0].toUpperCase()}${waveform.slice(1)}
                      </option>
                    `,
                  )
                  .join("")}
              </select>
            </label>

            <label class="field oscillator-strip__column">
              <span>Level</span>
              <input
                type="range"
                min="0"
                max="1"
                step="0.01"
                value="${oscillator.gain}"
                data-osc-gain="${oscillator.index}"
              >
              <output>${Number(oscillator.gain).toFixed(2)}</output>
            </label>

            <label class="toggle-pill toggle-pill--block oscillator-strip__tracking">
              <input
                type="checkbox"
                data-osc-relative="${oscillator.index}"
                ${oscillator.relativeToVoice ? "checked" : ""}
              >
              <span>${oscillator.relativeToVoice ? "Track Voice Root" : "Use Absolute Hz"}</span>
            </label>

            <label class="field oscillator-strip__column oscillator-strip__column--wide">
              <span>${oscillator.relativeToVoice ? "Ratio To Voice Root" : "Absolute Frequency"}</span>
              <input
                type="range"
                min="${oscillator.relativeToVoice ? "0.01" : "20"}"
                max="${oscillator.relativeToVoice ? "8" : "20000"}"
                step="${oscillator.relativeToVoice ? "0.01" : "1"}"
                value="${oscillator.frequencyValue}"
                data-osc-frequency="${oscillator.index}"
              >
              <output>${formatLinkedOscillatorFrequency(oscillator)}</output>
            </label>
          </div>
        </section>
      `,
    )
    .join("");

  bindOscillatorControls();
}

function renderDecorOutputGrid() {
  const engine = getEngine();
  const decor = getDecor();
  if (!engine || !decor) {
    elements.decorOutputGrid.innerHTML = "";
    return;
  }

  elements.decorVoiceCountValue.textContent = `${decor.voiceCount} voices`;
  elements.decorOutputGrid.innerHTML = Array.from({ length: engine.outputChannels }, (_, outputIndex) => `
    <article class="output-column">
      <div class="output-column__header">
        <h3>Decor Voice ${outputIndex + 1}</h3>
        <span>Output ${outputIndex + 1}</span>
      </div>
      <div class="output-column__group">
        <span>Route</span>
        <div class="token-row"><span class="token">Direct speaker lock</span></div>
      </div>
      <div class="output-column__group">
        <span>Status</span>
        <div class="token-row"><span class="token token--ghost">DSP pending</span></div>
      </div>
    </article>
  `).join("");
}

function renderTestSource() {
  const test = getTest();
  if (!test) {
    elements.testOutputGrid.innerHTML = "";
    return;
  }

  elements.testOutputGrid.innerHTML = test.outputs
    .map(
      (enabled, outputIndex) => `
        <article class="output-column">
          <div class="output-column__header">
            <h3>Output ${outputIndex + 1}</h3>
            <span>${enabled ? "Routed" : "Off"}</span>
          </div>

          <label class="toggle-pill toggle-pill--block">
            <input
              type="checkbox"
              data-test-output="${outputIndex}"
              ${enabled ? "checked" : ""}
            >
            <span>${enabled ? "Send Tone Here" : "Route Tone Here"}</span>
          </label>

          <p>Single debug oscillator feed for channel and speaker checks.</p>
        </article>
      `,
    )
    .join("");

  bindTestOutputControls();
}

function renderFxOutputGrid() {
  const fx = getFx();
  if (!fx) {
    elements.fxOutputGrid.innerHTML = "";
    return;
  }

  elements.fxOutputGrid.innerHTML = fx.outputs
    .map(
      (output) => `
        <article class="output-column">
          <div class="output-column__header">
            <h3>Output ${output.index + 1}</h3>
            <span>Insert route</span>
          </div>

          <label class="toggle-pill toggle-pill--block">
            <input
              type="checkbox"
              data-fx-output-route="${output.index}"
              ${output.routeThroughFx ? "checked" : ""}
            >
            <span>Route Through FX</span>
          </label>

          <div class="output-column__group">
            <span>Chain</span>
            <div class="token-row">
              <span class="token ${fx.saturator.enabled ? "" : "token--ghost"}">Saturator</span>
              <span class="token ${fx.chorus.enabled ? "" : "token--ghost"}">Chorus</span>
              <span class="token ${fx.sidechain.enabled ? "" : "token--ghost"}">Sidechain</span>
            </div>
          </div>
        </article>
      `,
    )
    .join("");

  bindFxOutputControls();
}

function applyStateToUi(nextState) {
  state = nextState;

  const engine = getEngine();
  const test = getTest();
  const robin = getRobin();
  const pieces = getPieces();
  const fx = getFx();
  const midi = getMidi();

  elements.heroOutputCount.textContent = `${engine.outputChannels}`;
  elements.heroVoiceCount.textContent = `${robin.voiceCount}`;
  elements.heroRoutingValue.textContent = formatRoutingPresetLabel(robin.routingPreset);

  elements.audioBackendValue.textContent = engine.audioBackend;
  elements.outputDeviceValue.textContent = engine.outputDeviceName;
  elements.audioRunningValue.textContent = engine.running ? "Yes" : "No";
  elements.midiStatusValue.textContent = engine.midiEnabled ? "On" : "Off";
  elements.midiSourcesValue.textContent = `${engine.midiSourceCount}`;
  elements.midiConnectedSourcesValue.textContent = `${engine.midiConnectedSourceCount ?? midi.connectedSourceCount}`;
  elements.oscStatusValue.textContent = engine.oscEnabled ? "On" : "Off";
  elements.oscPortValue.textContent = `${engine.oscPort}`;
  elements.lastMidiNoteValue.textContent = engine.activeMidiNote >= 0 ? `${engine.activeMidiNote}` : "None";
  elements.sampleRateValue.textContent = `${Math.round(Number(engine.sampleRate))} Hz`;
  elements.channelsValue.textContent = `${engine.outputChannels}`;
  elements.bufferValue.textContent = `${engine.framesPerBuffer}`;
  renderEngineOutputControls(engine);

  elements.voiceCount.value = String(robin.voiceCount);
  setVoiceCountLabel(robin.voiceCount);

  elements.oscillatorsPerVoice.value = String(robin.oscillatorsPerVoice);
  setOscillatorCountLabel(robin.oscillatorsPerVoice);

  elements.routingPreset.value = robin.routingPreset;
  elements.frequency.value = String(robin.frequency);
  setFrequencyLabel(robin.frequency);
  elements.waveform.value = robin.waveform;
  elements.robinAttack.value = String(robin.envelope.attackMs);
  setRobinAttackLabel(robin.envelope.attackMs);
  elements.robinDecay.value = String(robin.envelope.decayMs);
  setRobinDecayLabel(robin.envelope.decayMs);
  elements.robinSustain.value = String(robin.envelope.sustain);
  setRobinSustainLabel(robin.envelope.sustain);
  elements.robinRelease.value = String(robin.envelope.releaseMs);
  setRobinReleaseLabel(robin.envelope.releaseMs);

  elements.testActive.checked = test.active;
  elements.testMidiEnabled.checked = test.midiEnabled;
  elements.testFrequency.value = String(test.frequency);
  setTestFrequencyLabel(test.frequency);
  elements.testGain.value = String(test.gain);
  setTestGainLabel(test.gain);
  elements.testWaveform.value = test.waveform;
  elements.testAttack.value = String(test.envelope.attackMs);
  setTestAttackLabel(test.envelope.attackMs);
  elements.testDecay.value = String(test.envelope.decayMs);
  setTestDecayLabel(test.envelope.decayMs);
  elements.testSustain.value = String(test.envelope.sustain);
  setTestSustainLabel(test.envelope.sustain);
  elements.testRelease.value = String(test.envelope.releaseMs);
  setTestReleaseLabel(test.envelope.releaseMs);

  elements.lfoEnabled.checked = robin.lfo.enabled;
  elements.lfoWaveform.value = robin.lfo.waveform;
  elements.lfoDepth.value = String(robin.lfo.depth);
  setLfoDepthLabel(robin.lfo.depth);
  elements.lfoClockLinked.checked = robin.lfo.clockLinked;
  elements.lfoTempo.value = String(robin.lfo.tempoBpm);
  setLfoTempoLabel(robin.lfo.tempoBpm);
  elements.lfoRateMultiplier.value = String(robin.lfo.rateMultiplier);
  setLfoRateMultiplierLabel(robin.lfo.rateMultiplier);
  elements.lfoFixedFrequency.value = String(robin.lfo.fixedFrequencyHz);
  setLfoFixedFrequencyLabel(robin.lfo.fixedFrequencyHz);
  elements.lfoPhaseSpread.value = String(robin.lfo.phaseSpreadDegrees);
  setLfoPhaseSpreadLabel(robin.lfo.phaseSpreadDegrees);
  elements.lfoPolarityFlip.checked = robin.lfo.polarityFlip;
  elements.lfoUnlinkedOutputs.checked = robin.lfo.unlinkedOutputs;
  syncLfoInputState(robin.lfo);

  elements.fxSaturatorEnabled.checked = fx.saturator.enabled;
  elements.fxSaturatorLinkedLevels.checked = fx.saturator.linkedLevels;
  elements.fxSaturatorInputLevel.value = String(fx.saturator.inputLevel);
  setSaturatorInputLabel(fx.saturator.inputLevel);
  elements.fxSaturatorOutputLevel.value = String(fx.saturator.outputLevel);
  setSaturatorOutputLabel(fx.saturator.outputLevel);
  elements.fxChorusEnabled.checked = fx.chorus.enabled;
  elements.fxChorusLinkedControls.checked = fx.chorus.linkedControls;
  elements.fxChorusDepth.value = String(fx.chorus.depth);
  setChorusDepthLabel(fx.chorus.depth);
  elements.fxChorusSpeed.value = String(fx.chorus.speedHz);
  setChorusSpeedLabel(fx.chorus.speedHz);
  elements.fxChorusPhaseSpread.value = String(fx.chorus.phaseSpreadDegrees);
  setChorusPhaseLabel(fx.chorus.phaseSpreadDegrees);
  elements.fxSidechainEnabled.checked = fx.sidechain.enabled;

  elements.piecesVoiceCountValue.textContent = `${pieces.voiceCount} voices`;

  updateStateView();

  renderRobinLinkState();
  renderMidiDevices();
  renderSourceMixer();
  renderOutputMixer();
  renderVoices();
  renderOscillators();
  renderTestSource();
  renderDecorOutputGrid();
  renderFxOutputGrid();
}

function renderState(nextState) {
  state = nextState;
  if (activeRangeInput) {
    hasDeferredRender = true;
    return;
  }

  hasDeferredRender = false;
  applyStateToUi(nextState);
}

async function refreshState({ silent = false } = {}) {
  if (!window.synth) {
    setStatus("Native bridge unavailable.");
    return;
  }

  if (!silent) {
    setStatus("Loading state from native host...");
  }
  try {
    const nextState = await window.synth.getState();
    latestRenderedSequence = nextMutationSequence++;
    renderState(nextState);
    if (!silent) {
      setStatus(`Connected. Audio running: ${nextState.engine.running ? "yes" : "no"}.`);
    }
  } catch (error) {
    setStatus(`State request failed: ${error.message}`);
  }
}

async function dispatchParam(path, value, { silent = false } = {}) {
  if (!window.synth) {
    return null;
  }

  const sequence = nextMutationSequence++;
  inFlightParams.set(path, sequence);

  try {
    const nextState = await window.synth.setParam(path, value);
    if (inFlightParams.get(path) !== sequence) {
      return nextState;
    }

    inFlightParams.delete(path);
    if (sequence >= latestRenderedSequence) {
      latestRenderedSequence = sequence;
      renderState(nextState);
    }
    if (!silent) {
      setStatus(`Updated ${path}.`);
    }
    return nextState;
  } catch (error) {
    if (inFlightParams.get(path) === sequence) {
      inFlightParams.delete(path);
    }
    setStatus(`Failed to update ${path}: ${error.message}`);
    return null;
  } finally {
    drainQueuedParam(path);
  }
}

async function dispatchParamFast(path, value, { silent = false, onSuccess = null } = {}) {
  if (!window.synth) {
    return null;
  }

  if (inFlightParams.has(path)) {
    queuedParams.set(path, {
      mode: "fast",
      value,
      silent,
      onSuccess,
    });
    return null;
  }

  const sequence = nextMutationSequence++;
  inFlightParams.set(path, sequence);

  try {
    await window.synth.setParamFast(path, value);
    if (inFlightParams.get(path) !== sequence) {
      return true;
    }

    inFlightParams.delete(path);
    if (typeof onSuccess === "function") {
      onSuccess();
    }
    if (!silent) {
      setStatus(`Updated ${path}.`);
    }
    return true;
  } catch (error) {
    if (inFlightParams.get(path) === sequence) {
      inFlightParams.delete(path);
    }
    setStatus(`Failed to update ${path}: ${error.message}`);
    return null;
  } finally {
    drainQueuedParam(path);
  }
}

function setParam(path, value) {
  dispatchParam(path, value);
}

function setParamLive(path, value) {
  if (inFlightParams.has(path)) {
    queuedParams.set(path, {
      mode: "state",
      value,
      silent: true,
    });
    return;
  }

  dispatchParam(path, value, { silent: true });
}

function setLinkedOscillatorParam(oscillatorIndex, field, value) {
  dispatchParam(`sources.robin.oscillator.${oscillatorIndex}.${field}`, value);
}

function setLinkedOscillatorParamFast(oscillatorIndex, field, value) {
  const path = `sources.robin.oscillator.${oscillatorIndex}.${field}`;
  const numericOscillatorIndex = Number(oscillatorIndex);
  dispatchParamFast(path, value, {
    onSuccess: () => {
      applyRobinMasterOscillatorUpdate(numericOscillatorIndex, field, value);
      renderRobinOscillatorUi();
    },
  });
}

function bindSourceMixerControls() {
  document.querySelectorAll("[data-source-enabled]").forEach((input) => {
    input.addEventListener("change", () => {
      setParam(`sourceMixer.${input.dataset.sourceEnabled}.enabled`, input.checked);
    });
  });

  document.querySelectorAll("[data-source-level]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      output.textContent = Number(input.value).toFixed(2);
      setParamLive(`sourceMixer.${input.dataset.sourceLevel}.level`, Number(input.value));
    });
  });
}

function bindOutputMixerControls() {
  document.querySelectorAll("[data-output-level]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      output.textContent = Number(input.value).toFixed(2);
      setParamLive(`outputMixer.output.${input.dataset.outputLevel}.level`, Number(input.value));
    });
  });

  document.querySelectorAll("[data-output-delay]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      output.textContent = `${Math.round(Number(input.value))} ms`;
      setParamLive(`outputMixer.output.${input.dataset.outputDelay}.delayMs`, Number(input.value));
    });
  });

  document.querySelectorAll("[data-output-eq]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      output.textContent = `${Number(input.value).toFixed(1)} dB`;
      const [outputIndex, band] = input.dataset.outputEq.split(":");
      setParamLive(`outputMixer.output.${outputIndex}.eq.${band}`, Number(input.value));
    });
  });
}

function bindVoiceControls() {
  document.querySelectorAll("[data-voice-active]").forEach((input) => {
    input.addEventListener("change", () => {
      setParam(`sources.robin.voice.${input.dataset.voiceActive}.active`, input.checked);
    });
  });

  document.querySelectorAll("[data-voice-frequency]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      output.textContent = `${Math.round(Number(input.value))} Hz`;
      setParamLive(`sources.robin.voice.${input.dataset.voiceFrequency}.frequency`, Number(input.value));
    });
  });

  document.querySelectorAll("[data-voice-gain]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      output.textContent = Number(input.value).toFixed(2);
      setParamLive(`sources.robin.voice.${input.dataset.voiceGain}.gain`, Number(input.value));
    });
  });

  document.querySelectorAll(".route-button").forEach((button) => {
    button.addEventListener("click", () => {
      const { voiceIndex, outputIndex } = button.dataset;
      const nextValue = !button.classList.contains("is-on");
      setParam(`sources.robin.voice.${voiceIndex}.output.${outputIndex}`, nextValue);
    });
  });
}

function bindOscillatorControls() {
  document.querySelectorAll("[data-osc-enabled]").forEach((input) => {
    input.addEventListener("change", () => {
      setLinkedOscillatorParamFast(input.dataset.oscEnabled, "enabled", input.checked);
    });
  });

  document.querySelectorAll("[data-osc-waveform]").forEach((select) => {
    select.addEventListener("change", () => {
      setLinkedOscillatorParamFast(select.dataset.oscWaveform, "waveform", select.value);
    });
  });

  document.querySelectorAll("[data-osc-relative]").forEach((input) => {
    input.addEventListener("change", () => {
      setLinkedOscillatorParamFast(input.dataset.oscRelative, "relative", input.checked);
    });
  });

  document.querySelectorAll("[data-osc-gain]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      output.textContent = Number(input.value).toFixed(2);
    });
    input.addEventListener("change", () => {
      setLinkedOscillatorParamFast(input.dataset.oscGain, "gain", Number(input.value));
    });
  });

  document.querySelectorAll("[data-osc-frequency]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      const robin = getRobin();
      const oscillatorIndex = Number(input.dataset.oscFrequency);
      const oscillator = robin?.voices?.[0]?.oscillators?.[oscillatorIndex];
      if (!oscillator) {
        return;
      }

      output.textContent = oscillator.relativeToVoice
        ? `${Number(input.value).toFixed(2)}x voice root`
        : `${Math.round(Number(input.value))} Hz`;
    });
    input.addEventListener("change", () => {
      setLinkedOscillatorParamFast(input.dataset.oscFrequency, "frequency", Number(input.value));
    });
  });
}

function bindFxOutputControls() {
  document.querySelectorAll("[data-fx-output-route]").forEach((input) => {
    input.addEventListener("change", () => {
      setParam(`processors.fx.output.${input.dataset.fxOutputRoute}.routeThroughFx`, input.checked);
    });
  });
}

function bindMidiDeviceControls() {
  document.querySelectorAll("[data-midi-source]").forEach((input) => {
    input.addEventListener("change", () => {
      setParam(`engine.midi.source.${input.dataset.midiSource}.connected`, input.checked);
    });
  });

  document.querySelectorAll("[data-midi-route]").forEach((input) => {
    input.addEventListener("change", () => {
      const [sourceIndex, target] = input.dataset.midiRoute.split(":");
      setParam(`engine.midi.source.${sourceIndex}.route.${target}`, input.checked);
    });
  });
}

function bindTestOutputControls() {
  document.querySelectorAll("[data-test-output]").forEach((input) => {
    input.addEventListener("change", () => {
      setParam(`sources.test.output.${input.dataset.testOutput}`, input.checked);
    });
  });
}

window.addEventListener("DOMContentLoaded", () => {
  document.addEventListener(
    "pointerdown",
    (event) => {
      beginRangeInteraction(event.target);
    },
    true,
  );

  document.addEventListener(
    "pointerup",
    () => {
      endRangeInteraction();
    },
    true,
  );

  document.addEventListener(
    "pointercancel",
    () => {
      endRangeInteraction();
    },
    true,
  );

  document.addEventListener(
    "change",
    (event) => {
      endRangeInteraction(event.target);
    },
    true,
  );

  document.addEventListener(
    "focusin",
    (event) => {
      beginRangeInteraction(event.target);
    },
    true,
  );

  document.addEventListener(
    "focusout",
    (event) => {
      endRangeInteraction(event.target);
    },
    true,
  );

  elements.tabs.forEach((button) => {
    button.addEventListener("click", () => {
      selectPage(button.dataset.page);
    });
  });

  elements.engineOutputDevice.addEventListener("change", () => {
    setParam("engine.outputDeviceId", elements.engineOutputDevice.value);
  });

  elements.engineOutputChannels.addEventListener("change", () => {
    setParam("engine.outputChannels", Number(elements.engineOutputChannels.value));
  });

  elements.voiceCount.addEventListener("input", () => {
    setVoiceCountLabel(elements.voiceCount.value);
    setParamLive("sources.robin.voiceCount", Number(elements.voiceCount.value));
  });

  elements.oscillatorsPerVoice.addEventListener("input", () => {
    setOscillatorCountLabel(elements.oscillatorsPerVoice.value);
    setParamLive("sources.robin.oscillatorsPerVoice", Number(elements.oscillatorsPerVoice.value));
  });

  elements.routingPreset.addEventListener("change", () => {
    setParam("sources.robin.routingPreset", elements.routingPreset.value);
  });

  elements.frequency.addEventListener("input", () => {
    setFrequencyLabel(elements.frequency.value);
    setParamLive("sources.robin.frequency", Number(elements.frequency.value));
  });

  elements.waveform.addEventListener("change", () => {
    setParam("sources.robin.waveform", elements.waveform.value);
  });

  elements.robinAttack.addEventListener("input", () => {
    setRobinAttackLabel(elements.robinAttack.value);
    setParamLive("sources.robin.envelope.attackMs", Number(elements.robinAttack.value));
  });

  elements.robinDecay.addEventListener("input", () => {
    setRobinDecayLabel(elements.robinDecay.value);
    setParamLive("sources.robin.envelope.decayMs", Number(elements.robinDecay.value));
  });

  elements.robinSustain.addEventListener("input", () => {
    setRobinSustainLabel(elements.robinSustain.value);
    setParamLive("sources.robin.envelope.sustain", Number(elements.robinSustain.value));
  });

  elements.robinRelease.addEventListener("input", () => {
    setRobinReleaseLabel(elements.robinRelease.value);
    setParamLive("sources.robin.envelope.releaseMs", Number(elements.robinRelease.value));
  });

  elements.testActive.addEventListener("change", () => {
    setParam("sources.test.active", elements.testActive.checked);
  });

  elements.testMidiEnabled.addEventListener("change", () => {
    setParam("sources.test.midiEnabled", elements.testMidiEnabled.checked);
  });

  elements.testFrequency.addEventListener("input", () => {
    setTestFrequencyLabel(elements.testFrequency.value);
    setParamLive("sources.test.frequency", Number(elements.testFrequency.value));
  });

  elements.testGain.addEventListener("input", () => {
    setTestGainLabel(elements.testGain.value);
    setParamLive("sources.test.gain", Number(elements.testGain.value));
  });

  elements.testWaveform.addEventListener("change", () => {
    setParam("sources.test.waveform", elements.testWaveform.value);
  });

  elements.testAttack.addEventListener("input", () => {
    setTestAttackLabel(elements.testAttack.value);
    setParamLive("sources.test.envelope.attackMs", Number(elements.testAttack.value));
  });

  elements.testDecay.addEventListener("input", () => {
    setTestDecayLabel(elements.testDecay.value);
    setParamLive("sources.test.envelope.decayMs", Number(elements.testDecay.value));
  });

  elements.testSustain.addEventListener("input", () => {
    setTestSustainLabel(elements.testSustain.value);
    setParamLive("sources.test.envelope.sustain", Number(elements.testSustain.value));
  });

  elements.testRelease.addEventListener("input", () => {
    setTestReleaseLabel(elements.testRelease.value);
    setParamLive("sources.test.envelope.releaseMs", Number(elements.testRelease.value));
  });

  elements.fxSaturatorEnabled.addEventListener("change", () => {
    setParam("processors.fx.saturator.enabled", elements.fxSaturatorEnabled.checked);
  });

  elements.fxSaturatorLinkedLevels.addEventListener("change", () => {
    setParam("processors.fx.saturator.linkedLevels", elements.fxSaturatorLinkedLevels.checked);
  });

  elements.fxSaturatorInputLevel.addEventListener("input", () => {
    setSaturatorInputLabel(elements.fxSaturatorInputLevel.value);
    setParamLive("processors.fx.saturator.inputLevel", Number(elements.fxSaturatorInputLevel.value));
  });

  elements.fxSaturatorOutputLevel.addEventListener("input", () => {
    setSaturatorOutputLabel(elements.fxSaturatorOutputLevel.value);
    setParamLive("processors.fx.saturator.outputLevel", Number(elements.fxSaturatorOutputLevel.value));
  });

  elements.fxChorusEnabled.addEventListener("change", () => {
    setParam("processors.fx.chorus.enabled", elements.fxChorusEnabled.checked);
  });

  elements.fxChorusLinkedControls.addEventListener("change", () => {
    setParam("processors.fx.chorus.linkedControls", elements.fxChorusLinkedControls.checked);
  });

  elements.fxChorusDepth.addEventListener("input", () => {
    setChorusDepthLabel(elements.fxChorusDepth.value);
    setParamLive("processors.fx.chorus.depth", Number(elements.fxChorusDepth.value));
  });

  elements.fxChorusSpeed.addEventListener("input", () => {
    setChorusSpeedLabel(elements.fxChorusSpeed.value);
    setParamLive("processors.fx.chorus.speedHz", Number(elements.fxChorusSpeed.value));
  });

  elements.fxChorusPhaseSpread.addEventListener("input", () => {
    setChorusPhaseLabel(elements.fxChorusPhaseSpread.value);
    setParamLive("processors.fx.chorus.phaseSpreadDegrees", Number(elements.fxChorusPhaseSpread.value));
  });

  elements.fxSidechainEnabled.addEventListener("change", () => {
    setParam("processors.fx.sidechain.enabled", elements.fxSidechainEnabled.checked);
  });

  elements.lfoEnabled.addEventListener("change", () => {
    setParam("sources.robin.lfo.enabled", elements.lfoEnabled.checked);
  });

  elements.lfoWaveform.addEventListener("change", () => {
    setParam("sources.robin.lfo.waveform", elements.lfoWaveform.value);
  });

  elements.lfoDepth.addEventListener("input", () => {
    setLfoDepthLabel(elements.lfoDepth.value);
    setParamLive("sources.robin.lfo.depth", Number(elements.lfoDepth.value));
  });

  elements.lfoClockLinked.addEventListener("change", () => {
    setParam("sources.robin.lfo.clockLinked", elements.lfoClockLinked.checked);
    syncLfoInputState({ clockLinked: elements.lfoClockLinked.checked });
  });

  elements.lfoTempo.addEventListener("input", () => {
    setLfoTempoLabel(elements.lfoTempo.value);
    setParamLive("sources.robin.lfo.tempoBpm", Number(elements.lfoTempo.value));
  });

  elements.lfoRateMultiplier.addEventListener("input", () => {
    setLfoRateMultiplierLabel(elements.lfoRateMultiplier.value);
    setParamLive("sources.robin.lfo.rateMultiplier", Number(elements.lfoRateMultiplier.value));
  });

  elements.lfoFixedFrequency.addEventListener("input", () => {
    setLfoFixedFrequencyLabel(elements.lfoFixedFrequency.value);
    setParamLive("sources.robin.lfo.fixedFrequencyHz", Number(elements.lfoFixedFrequency.value));
  });

  elements.lfoPhaseSpread.addEventListener("input", () => {
    setLfoPhaseSpreadLabel(elements.lfoPhaseSpread.value);
    setParamLive("sources.robin.lfo.phaseSpreadDegrees", Number(elements.lfoPhaseSpread.value));
  });

  elements.lfoPolarityFlip.addEventListener("change", () => {
    setParam("sources.robin.lfo.polarityFlip", elements.lfoPolarityFlip.checked);
  });

  elements.lfoUnlinkedOutputs.addEventListener("change", () => {
    setParam("sources.robin.lfo.unlinkedOutputs", elements.lfoUnlinkedOutputs.checked);
  });

  elements.refreshButton.addEventListener("click", refreshState);

  refreshState();
});
