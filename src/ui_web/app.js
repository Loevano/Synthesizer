const elements = {
  tabs: Array.from(document.querySelectorAll(".page-tab")),
  pages: Array.from(document.querySelectorAll(".page")),
  audioBackendValue: document.getElementById("audioBackendValue"),
  outputDeviceValue: document.getElementById("outputDeviceValue"),
  audioRunningValue: document.getElementById("audioRunningValue"),
  midiStatusValue: document.getElementById("midiStatusValue"),
  midiSourcesValue: document.getElementById("midiSourcesValue"),
  oscStatusValue: document.getElementById("oscStatusValue"),
  oscPortValue: document.getElementById("oscPortValue"),
  lastMidiNoteValue: document.getElementById("lastMidiNoteValue"),
  sampleRateValue: document.getElementById("sampleRateValue"),
  channelsValue: document.getElementById("channelsValue"),
  bufferValue: document.getElementById("bufferValue"),
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
  voiceCount: document.getElementById("voiceCount"),
  voiceCountValue: document.getElementById("voiceCountValue"),
  oscillatorsPerVoice: document.getElementById("oscillatorsPerVoice"),
  oscillatorsPerVoiceValue: document.getElementById("oscillatorsPerVoiceValue"),
  routingPreset: document.getElementById("routingPreset"),
  frequency: document.getElementById("frequency"),
  frequencyValue: document.getElementById("frequencyValue"),
  gain: document.getElementById("gain"),
  gainValue: document.getElementById("gainValue"),
  waveform: document.getElementById("waveform"),
  refreshButton: document.getElementById("refreshButton"),
  stateView: document.getElementById("stateView"),
  statusText: document.getElementById("statusText"),
  voicesGrid: document.getElementById("voicesGrid"),
  voiceSelect: document.getElementById("voiceSelect"),
  oscillatorRows: document.getElementById("oscillatorRows"),
};

let state = null;
let selectedVoiceIndex = 0;
let nextMutationSequence = 1;
let latestRenderedSequence = 0;
const inFlightParams = new Map();
const queuedParams = new Map();

function setStatus(message) {
  elements.statusText.textContent = message;
}

function selectPage(pageName) {
  elements.tabs.forEach((button) => {
    button.classList.toggle("is-active", button.dataset.page === pageName);
  });
  elements.pages.forEach((page) => {
    page.classList.toggle("is-active", page.id === `page-${pageName}`);
  });
}

function setFrequencyLabel(value) {
  elements.frequencyValue.textContent = `${Math.round(Number(value))} Hz`;
}

function setVoiceCountLabel(value) {
  const count = Math.round(Number(value));
  elements.voiceCountValue.textContent = `${count}`;
}

function setOscillatorCountLabel(value) {
  const count = Math.round(Number(value));
  elements.oscillatorsPerVoiceValue.textContent = `${count}`;
}

function setGainLabel(value) {
  elements.gainValue.textContent = Number(value).toFixed(2);
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
  elements.lfoPhaseSpreadValue.textContent = `${Math.round(Number(value))}°`;
}

function syncLfoInputState(lfo) {
  const clockLinked = Boolean(lfo.clockLinked);
  elements.lfoTempo.disabled = !clockLinked;
  elements.lfoRateMultiplier.disabled = !clockLinked;
  elements.lfoFixedFrequency.disabled = clockLinked;
}

function formatOscillatorFrequency(voiceFrequency, oscillator) {
  if (oscillator.relativeToVoice) {
    const resolvedFrequency = Number(voiceFrequency) * Number(oscillator.frequencyValue);
    return `${Number(oscillator.frequencyValue).toFixed(2)}x -> ${Math.round(resolvedFrequency)} Hz`;
  }

  return `${Math.round(Number(oscillator.frequencyValue))} Hz`;
}

function renderVoiceSelect() {
  if (!state) {
    return;
  }

  const previous = selectedVoiceIndex;
  elements.voiceSelect.innerHTML = state.voices
    .map((voice, index) => `<option value="${index}">Voice ${index + 1}</option>`)
    .join("");

  selectedVoiceIndex = Math.min(previous, state.voices.length - 1);
  elements.voiceSelect.value = String(Math.max(0, selectedVoiceIndex));
}

function renderVoices() {
  if (!state) {
    elements.voicesGrid.innerHTML = "";
    return;
  }

  elements.voicesGrid.innerHTML = state.voices
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

      return `
        <article class="voice-card ${voice.active ? "is-active" : ""}">
          <div class="voice-card__header">
            <h3>Voice ${voice.index + 1}</h3>
            <label class="toggle-pill">
              <input type="checkbox" data-voice-active="${voice.index}" ${voice.active ? "checked" : ""}>
              <span>On</span>
            </label>
          </div>

          <label class="field">
            <span>Base Frequency</span>
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
            <span>Voice Gain</span>
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
            <span>Outputs</span>
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
  if (!state || !state.voices.length) {
    elements.oscillatorRows.innerHTML = "";
    return;
  }

  const voice = state.voices[selectedVoiceIndex];
  elements.oscillatorRows.innerHTML = voice.oscillators
    .map(
      (oscillator) => `
        <article class="oscillator-card">
          <div class="oscillator-card__header">
            <h3>Osc ${oscillator.index + 1}</h3>
            <label class="toggle-pill">
              <input
                type="checkbox"
                data-osc-enabled="${voice.index}:${oscillator.index}"
                ${oscillator.enabled ? "checked" : ""}
              >
              <span>On</span>
            </label>
          </div>

          <label class="field">
            <span>Waveform</span>
            <select data-osc-waveform="${voice.index}:${oscillator.index}">
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

          <label class="field">
            <span>Oscillator Gain</span>
            <input
              type="range"
              min="0"
              max="1"
              step="0.01"
              value="${oscillator.gain}"
              data-osc-gain="${voice.index}:${oscillator.index}"
            >
            <output>${Number(oscillator.gain).toFixed(2)}</output>
          </label>

          <label class="toggle-pill">
            <input
              type="checkbox"
              data-osc-relative="${voice.index}:${oscillator.index}"
              ${oscillator.relativeToVoice ? "checked" : ""}
            >
            <span>Relative to Voice Root</span>
          </label>

          <label class="field">
            <span>${oscillator.relativeToVoice ? "Frequency Ratio" : "Absolute Frequency"}</span>
            <input
              type="range"
              min="${oscillator.relativeToVoice ? "0.01" : "20"}"
              max="${oscillator.relativeToVoice ? "8" : "20000"}"
              step="${oscillator.relativeToVoice ? "0.01" : "1"}"
              value="${oscillator.frequencyValue}"
              data-osc-frequency="${voice.index}:${oscillator.index}"
            >
            <output>${formatOscillatorFrequency(voice.frequency, oscillator)}</output>
          </label>
        </article>
      `,
    )
    .join("");

  bindOscillatorControls();
}

function renderState(nextState) {
  state = nextState;

  elements.audioBackendValue.textContent = nextState.audioBackend;
  elements.outputDeviceValue.textContent = nextState.outputDeviceName;
  elements.audioRunningValue.textContent = nextState.running ? "Yes" : "No";
  elements.midiStatusValue.textContent = nextState.midiEnabled ? "On" : "Off";
  elements.midiSourcesValue.textContent = `${nextState.midiSourceCount}`;
  elements.oscStatusValue.textContent = nextState.oscEnabled ? "On" : "Off";
  elements.oscPortValue.textContent = `${nextState.oscPort}`;
  elements.lastMidiNoteValue.textContent = nextState.activeMidiNote >= 0 ? `${nextState.activeMidiNote}` : "None";
  elements.sampleRateValue.textContent = `${Math.round(Number(nextState.sampleRate))} Hz`;
  elements.channelsValue.textContent = `${nextState.channels}`;
  elements.bufferValue.textContent = `${nextState.framesPerBuffer}`;

  elements.lfoEnabled.checked = nextState.lfo.enabled;
  elements.lfoWaveform.value = nextState.lfo.waveform;
  elements.lfoDepth.value = String(nextState.lfo.depth);
  setLfoDepthLabel(nextState.lfo.depth);
  elements.lfoClockLinked.checked = nextState.lfo.clockLinked;
  elements.lfoTempo.value = String(nextState.lfo.tempoBpm);
  setLfoTempoLabel(nextState.lfo.tempoBpm);
  elements.lfoRateMultiplier.value = String(nextState.lfo.rateMultiplier);
  setLfoRateMultiplierLabel(nextState.lfo.rateMultiplier);
  elements.lfoFixedFrequency.value = String(nextState.lfo.fixedFrequencyHz);
  setLfoFixedFrequencyLabel(nextState.lfo.fixedFrequencyHz);
  elements.lfoPhaseSpread.value = String(nextState.lfo.phaseSpreadDegrees);
  setLfoPhaseSpreadLabel(nextState.lfo.phaseSpreadDegrees);
  elements.lfoPolarityFlip.checked = nextState.lfo.polarityFlip;
  elements.lfoUnlinkedOutputs.checked = nextState.lfo.unlinkedOutputs;
  syncLfoInputState(nextState.lfo);

  elements.voiceCount.value = String(nextState.voiceCount);
  setVoiceCountLabel(nextState.voiceCount);

  elements.oscillatorsPerVoice.value = String(nextState.oscillatorsPerVoice);
  setOscillatorCountLabel(nextState.oscillatorsPerVoice);

  elements.routingPreset.value = nextState.routingPreset;

  elements.frequency.value = String(nextState.frequency);
  setFrequencyLabel(nextState.frequency);

  elements.gain.value = String(nextState.gain);
  setGainLabel(nextState.gain);

  elements.waveform.value = nextState.waveform;
  elements.stateView.textContent = JSON.stringify(nextState, null, 2);

  renderVoiceSelect();
  renderVoices();
  renderOscillators();
}

async function refreshState() {
  if (!window.synth) {
    setStatus("Native bridge unavailable.");
    return;
  }

  setStatus("Loading state from native host…");
  try {
    const nextState = await window.synth.getState();
    latestRenderedSequence = nextMutationSequence++;
    renderState(nextState);
    setStatus(`Connected. Audio running: ${nextState.running ? "yes" : "no"}.`);
  } catch (error) {
    setStatus(`State request failed: ${error.message}`);
  }
}

async function dispatchParam(path, value, { silent = false } = {}) {
  if (!window.synth) {
    return;
  }

  const sequence = nextMutationSequence++;
  inFlightParams.set(path, sequence);

  try {
    const nextState = await window.synth.setParam(path, value);
    if (inFlightParams.get(path) !== sequence) {
      return;
    }

    inFlightParams.delete(path);
    if (sequence >= latestRenderedSequence) {
      latestRenderedSequence = sequence;
      renderState(nextState);
    }
    if (!silent) {
      setStatus(`Updated ${path}.`);
    }
  } catch (error) {
    if (inFlightParams.get(path) === sequence) {
      inFlightParams.delete(path);
    }
    setStatus(`Failed to update ${path}: ${error.message}`);
  }

  if (queuedParams.has(path)) {
    const nextValue = queuedParams.get(path);
    queuedParams.delete(path);
    dispatchParam(path, nextValue, { silent: true });
  }
}

function setParam(path, value) {
  dispatchParam(path, value);
}

function setParamLive(path, value) {
  if (inFlightParams.has(path)) {
    queuedParams.set(path, value);
    return;
  }

  dispatchParam(path, value, { silent: true });
}

function bindVoiceControls() {
  document.querySelectorAll("[data-voice-active]").forEach((input) => {
    input.addEventListener("change", () => {
      setParam(`voice.${input.dataset.voiceActive}.active`, input.checked);
    });
  });

  document.querySelectorAll("[data-voice-frequency]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      output.textContent = `${Math.round(Number(input.value))} Hz`;
      setParamLive(`voice.${input.dataset.voiceFrequency}.frequency`, Number(input.value));
    });
  });

  document.querySelectorAll("[data-voice-gain]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      output.textContent = Number(input.value).toFixed(2);
      setParamLive(`voice.${input.dataset.voiceGain}.gain`, Number(input.value));
    });
  });

  document.querySelectorAll(".route-button").forEach((button) => {
    button.addEventListener("click", () => {
      const { voiceIndex, outputIndex } = button.dataset;
      const nextValue = !button.classList.contains("is-on");
      setParam(`voice.${voiceIndex}.output.${outputIndex}`, nextValue);
    });
  });
}

function bindOscillatorControls() {
  document.querySelectorAll("[data-osc-enabled]").forEach((input) => {
    input.addEventListener("change", () => {
      const [voiceIndex, oscillatorIndex] = input.dataset.oscEnabled.split(":");
      setParam(`voice.${voiceIndex}.oscillator.${oscillatorIndex}.enabled`, input.checked);
    });
  });

  document.querySelectorAll("[data-osc-waveform]").forEach((select) => {
    select.addEventListener("change", () => {
      const [voiceIndex, oscillatorIndex] = select.dataset.oscWaveform.split(":");
      setParam(`voice.${voiceIndex}.oscillator.${oscillatorIndex}.waveform`, select.value);
    });
  });

  document.querySelectorAll("[data-osc-relative]").forEach((input) => {
    input.addEventListener("change", () => {
      const [voiceIndex, oscillatorIndex] = input.dataset.oscRelative.split(":");
      setParam(`voice.${voiceIndex}.oscillator.${oscillatorIndex}.relative`, input.checked);
    });
  });

  document.querySelectorAll("[data-osc-gain]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      output.textContent = Number(input.value).toFixed(2);
      const [voiceIndex, oscillatorIndex] = input.dataset.oscGain.split(":");
      setParamLive(`voice.${voiceIndex}.oscillator.${oscillatorIndex}.gain`, Number(input.value));
    });
  });

  document.querySelectorAll("[data-osc-frequency]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      const [voiceIndex, oscillatorIndex] = input.dataset.oscFrequency.split(":");
      const voice = state?.voices?.[Number(voiceIndex)];
      const oscillator = voice?.oscillators?.[Number(oscillatorIndex)];
      if (!voice || !oscillator) {
        return;
      }

      output.textContent = oscillator.relativeToVoice
        ? `${Number(input.value).toFixed(2)}x -> ${Math.round(Number(input.value) * Number(voice.frequency))} Hz`
        : `${Math.round(Number(input.value))} Hz`;
      setParamLive(`voice.${voiceIndex}.oscillator.${oscillatorIndex}.frequency`, Number(input.value));
    });
  });
}

window.addEventListener("DOMContentLoaded", () => {
  elements.tabs.forEach((button) => {
    button.addEventListener("click", () => {
      selectPage(button.dataset.page);
    });
  });

  elements.voiceCount.addEventListener("input", () => {
    setVoiceCountLabel(elements.voiceCount.value);
    setParamLive("voiceCount", Number(elements.voiceCount.value));
  });

  elements.oscillatorsPerVoice.addEventListener("input", () => {
    setOscillatorCountLabel(elements.oscillatorsPerVoice.value);
    setParamLive("oscillatorsPerVoice", Number(elements.oscillatorsPerVoice.value));
  });

  elements.routingPreset.addEventListener("change", () => {
    setParam("routingPreset", elements.routingPreset.value);
  });

  elements.frequency.addEventListener("input", () => {
    setFrequencyLabel(elements.frequency.value);
    setParamLive("frequency", Number(elements.frequency.value));
  });

  elements.gain.addEventListener("input", () => {
    setGainLabel(elements.gain.value);
    setParamLive("gain", Number(elements.gain.value));
  });

  elements.waveform.addEventListener("change", () => {
    setParam("waveform", elements.waveform.value);
  });

  elements.lfoEnabled.addEventListener("change", () => {
    setParam("lfo.enabled", elements.lfoEnabled.checked);
  });

  elements.lfoWaveform.addEventListener("change", () => {
    setParam("lfo.waveform", elements.lfoWaveform.value);
  });

  elements.lfoDepth.addEventListener("input", () => {
    setLfoDepthLabel(elements.lfoDepth.value);
    setParamLive("lfo.depth", Number(elements.lfoDepth.value));
  });

  elements.lfoClockLinked.addEventListener("change", () => {
    setParam("lfo.clockLinked", elements.lfoClockLinked.checked);
    syncLfoInputState({ clockLinked: elements.lfoClockLinked.checked });
  });

  elements.lfoTempo.addEventListener("input", () => {
    setLfoTempoLabel(elements.lfoTempo.value);
    setParamLive("lfo.tempoBpm", Number(elements.lfoTempo.value));
  });

  elements.lfoRateMultiplier.addEventListener("input", () => {
    setLfoRateMultiplierLabel(elements.lfoRateMultiplier.value);
    setParamLive("lfo.rateMultiplier", Number(elements.lfoRateMultiplier.value));
  });

  elements.lfoFixedFrequency.addEventListener("input", () => {
    setLfoFixedFrequencyLabel(elements.lfoFixedFrequency.value);
    setParamLive("lfo.fixedFrequencyHz", Number(elements.lfoFixedFrequency.value));
  });

  elements.lfoPhaseSpread.addEventListener("input", () => {
    setLfoPhaseSpreadLabel(elements.lfoPhaseSpread.value);
    setParamLive("lfo.phaseSpreadDegrees", Number(elements.lfoPhaseSpread.value));
  });

  elements.lfoPolarityFlip.addEventListener("change", () => {
    setParam("lfo.polarityFlip", elements.lfoPolarityFlip.checked);
  });

  elements.lfoUnlinkedOutputs.addEventListener("change", () => {
    setParam("lfo.unlinkedOutputs", elements.lfoUnlinkedOutputs.checked);
  });

  elements.voiceSelect.addEventListener("change", () => {
    selectedVoiceIndex = Number(elements.voiceSelect.value);
    renderOscillators();
  });

  elements.refreshButton.addEventListener("click", refreshState);

  refreshState();
});
