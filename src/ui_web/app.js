const ROUTING_LABELS = {
  forward: "Forward",
  backward: "Backward",
  random: "Random",
  "round-robin": "Round Robin",
  "all-outputs": "All Outputs",
  custom: "Custom",
};

const SOURCE_ROUTE_LABELS = {
  dry: "Dry",
  fx: "Through FX Chain",
};

const UI_RESET_DEFAULTS = {
  sourceMixer: {
    robin: { enabled: false, level: 0.15, routeTarget: "dry" },
    test: { enabled: true, level: 0.15, routeTarget: "dry" },
    decor: { enabled: false, level: 0, routeTarget: "dry" },
    pieces: { enabled: false, level: 0, routeTarget: "dry" },
  },
  outputMixer: {
    level: 1,
    delayMs: 0,
    eq: {
      lowDb: 0,
      midDb: 0,
      highDb: 0,
    },
  },
  robin: {
    voiceCount: 8,
    oscillatorsPerVoice: 6,
    routingPreset: "forward",
    frequency: 400,
    gain: 1,
    transposeSemitones: 0,
    fineTuneCents: 0,
    vcf: {
      cutoffHz: 18000,
      resonance: 0.707,
    },
    envVcf: {
      attackMs: 20,
      decayMs: 250,
      sustain: 0,
      releaseMs: 220,
      amount: 0,
    },
    envelope: {
      attackMs: 10,
      decayMs: 80,
      sustain: 0.8,
      releaseMs: 200,
    },
  },
  robinVoice: {
    active: true,
    linkedToMaster: true,
    frequency: 400,
    gain: 1,
    vcf: {
      cutoffHz: 18000,
      resonance: 0.707,
    },
    envVcf: {
      attackMs: 20,
      decayMs: 250,
      sustain: 0,
      releaseMs: 220,
      amount: 0,
    },
    envelope: {
      attackMs: 10,
      decayMs: 80,
      sustain: 0.8,
      releaseMs: 200,
    },
  },
  oscillator: {
    gain: 1,
    relativeToVoice: true,
    frequencyValue: 1,
    waveform: "sine",
  },
  test: {
    active: false,
    midiEnabled: true,
    frequency: 220,
    gain: 0.4,
    waveform: "sine",
    envelope: {
      attackMs: 10,
      decayMs: 80,
      sustain: 0.8,
      releaseMs: 200,
    },
  },
  fx: {
    saturator: {
      enabled: false,
      inputLevel: 1,
      outputLevel: 1,
    },
    chorus: {
      enabled: false,
      depth: 0.5,
      speedHz: 0.25,
      phaseSpreadDegrees: 0,
    },
    sidechain: {
      enabled: false,
    },
  },
  lfo: {
    enabled: false,
    waveform: "sine",
    depth: 0.5,
    clockLinked: false,
    tempoBpm: 120,
    rateMultiplier: 1,
    fixedFrequencyHz: 2,
    phaseSpreadDegrees: 0,
    polarityFlip: false,
    unlinkedOutputs: false,
  },
  midiRoute: {
    robin: true,
    test: true,
    decor: false,
    pieces: false,
  },
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
  transposeSemitones: document.getElementById("transposeSemitones"),
  transposeSemitonesValue: document.getElementById("transposeSemitonesValue"),
  fineTuneCents: document.getElementById("fineTuneCents"),
  fineTuneCentsValue: document.getElementById("fineTuneCentsValue"),
  robinVcfCutoff: document.getElementById("robinVcfCutoff"),
  robinVcfCutoffValue: document.getElementById("robinVcfCutoffValue"),
  robinVcfResonance: document.getElementById("robinVcfResonance"),
  robinVcfResonanceValue: document.getElementById("robinVcfResonanceValue"),
  robinEnvVcfAttack: document.getElementById("robinEnvVcfAttack"),
  robinEnvVcfAttackValue: document.getElementById("robinEnvVcfAttackValue"),
  robinEnvVcfDecay: document.getElementById("robinEnvVcfDecay"),
  robinEnvVcfDecayValue: document.getElementById("robinEnvVcfDecayValue"),
  robinEnvVcfSustain: document.getElementById("robinEnvVcfSustain"),
  robinEnvVcfSustainValue: document.getElementById("robinEnvVcfSustainValue"),
  robinEnvVcfRelease: document.getElementById("robinEnvVcfRelease"),
  robinEnvVcfReleaseValue: document.getElementById("robinEnvVcfReleaseValue"),
  robinEnvVcfAmount: document.getElementById("robinEnvVcfAmount"),
  robinEnvVcfAmountValue: document.getElementById("robinEnvVcfAmountValue"),
  robinAttack: document.getElementById("robinAttack"),
  robinAttackValue: document.getElementById("robinAttackValue"),
  robinDecay: document.getElementById("robinDecay"),
  robinDecayValue: document.getElementById("robinDecayValue"),
  robinSustain: document.getElementById("robinSustain"),
  robinSustainValue: document.getElementById("robinSustainValue"),
  robinRelease: document.getElementById("robinRelease"),
  robinReleaseValue: document.getElementById("robinReleaseValue"),
  voicesGrid: document.getElementById("voicesGrid"),
  selectedVoiceEditor: document.getElementById("selectedVoiceEditor"),
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
  fxSaturatorInputLevel: document.getElementById("fxSaturatorInputLevel"),
  fxSaturatorInputLevelValue: document.getElementById("fxSaturatorInputLevelValue"),
  fxSaturatorOutputLevel: document.getElementById("fxSaturatorOutputLevel"),
  fxSaturatorOutputLevelValue: document.getElementById("fxSaturatorOutputLevelValue"),
  fxChorusEnabled: document.getElementById("fxChorusEnabled"),
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
let selectedRobinVoiceIndex = null;

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

function formatLevelPercent(value) {
  return `${Math.round(Number(value) * 100)}%`;
}

function formatSignedDb(value) {
  const numericValue = Number(value);
  return `${numericValue > 0 ? "+" : ""}${numericValue.toFixed(1)} dB`;
}

function findResetControl(target) {
  if (!(target instanceof Element)) {
    return null;
  }

  const routeButton = target.closest("button[data-source-route-button]");
  if (routeButton instanceof HTMLButtonElement) {
    return routeButton;
  }

  const rotary = target.closest(".rotary");
  if (rotary instanceof HTMLElement) {
    const input = rotary.previousElementSibling;
    if (input instanceof HTMLInputElement) {
      return input;
    }
  }

  const directControl = target.closest("input, select");
  if (directControl instanceof HTMLInputElement || directControl instanceof HTMLSelectElement) {
    return directControl;
  }

  const label = target.closest("label");
  if (label instanceof HTMLLabelElement) {
    const nestedControl = label.querySelector("input, select");
    if (nestedControl instanceof HTMLInputElement || nestedControl instanceof HTMLSelectElement) {
      return nestedControl;
    }
  }

  return null;
}

function getStaticResetValue(controlId) {
  const staticDefaults = {
    voiceCount: UI_RESET_DEFAULTS.robin.voiceCount,
    oscillatorsPerVoice: UI_RESET_DEFAULTS.robin.oscillatorsPerVoice,
    routingPreset: UI_RESET_DEFAULTS.robin.routingPreset,
    transposeSemitones: UI_RESET_DEFAULTS.robin.transposeSemitones,
    fineTuneCents: UI_RESET_DEFAULTS.robin.fineTuneCents,
    robinVcfCutoff: UI_RESET_DEFAULTS.robin.vcf.cutoffHz,
    robinVcfResonance: UI_RESET_DEFAULTS.robin.vcf.resonance,
    robinEnvVcfAttack: UI_RESET_DEFAULTS.robin.envVcf.attackMs,
    robinEnvVcfDecay: UI_RESET_DEFAULTS.robin.envVcf.decayMs,
    robinEnvVcfSustain: UI_RESET_DEFAULTS.robin.envVcf.sustain,
    robinEnvVcfRelease: UI_RESET_DEFAULTS.robin.envVcf.releaseMs,
    robinEnvVcfAmount: UI_RESET_DEFAULTS.robin.envVcf.amount,
    robinAttack: UI_RESET_DEFAULTS.robin.envelope.attackMs,
    robinDecay: UI_RESET_DEFAULTS.robin.envelope.decayMs,
    robinSustain: UI_RESET_DEFAULTS.robin.envelope.sustain,
    robinRelease: UI_RESET_DEFAULTS.robin.envelope.releaseMs,
    testActive: UI_RESET_DEFAULTS.test.active,
    testMidiEnabled: UI_RESET_DEFAULTS.test.midiEnabled,
    testFrequency: UI_RESET_DEFAULTS.test.frequency,
    testGain: UI_RESET_DEFAULTS.test.gain,
    testWaveform: UI_RESET_DEFAULTS.test.waveform,
    testAttack: UI_RESET_DEFAULTS.test.envelope.attackMs,
    testDecay: UI_RESET_DEFAULTS.test.envelope.decayMs,
    testSustain: UI_RESET_DEFAULTS.test.envelope.sustain,
    testRelease: UI_RESET_DEFAULTS.test.envelope.releaseMs,
    fxSaturatorEnabled: UI_RESET_DEFAULTS.fx.saturator.enabled,
    fxSaturatorInputLevel: UI_RESET_DEFAULTS.fx.saturator.inputLevel,
    fxSaturatorOutputLevel: UI_RESET_DEFAULTS.fx.saturator.outputLevel,
    fxChorusEnabled: UI_RESET_DEFAULTS.fx.chorus.enabled,
    fxChorusDepth: UI_RESET_DEFAULTS.fx.chorus.depth,
    fxChorusSpeed: UI_RESET_DEFAULTS.fx.chorus.speedHz,
    fxChorusPhaseSpread: UI_RESET_DEFAULTS.fx.chorus.phaseSpreadDegrees,
    fxSidechainEnabled: UI_RESET_DEFAULTS.fx.sidechain.enabled,
    lfoEnabled: UI_RESET_DEFAULTS.lfo.enabled,
    lfoWaveform: UI_RESET_DEFAULTS.lfo.waveform,
    lfoDepth: UI_RESET_DEFAULTS.lfo.depth,
    lfoClockLinked: UI_RESET_DEFAULTS.lfo.clockLinked,
    lfoTempo: UI_RESET_DEFAULTS.lfo.tempoBpm,
    lfoRateMultiplier: UI_RESET_DEFAULTS.lfo.rateMultiplier,
    lfoFixedFrequency: UI_RESET_DEFAULTS.lfo.fixedFrequencyHz,
    lfoPhaseSpread: UI_RESET_DEFAULTS.lfo.phaseSpreadDegrees,
    lfoPolarityFlip: UI_RESET_DEFAULTS.lfo.polarityFlip,
    lfoUnlinkedOutputs: UI_RESET_DEFAULTS.lfo.unlinkedOutputs,
  };

  return Object.hasOwn(staticDefaults, controlId) ? staticDefaults[controlId] : undefined;
}

function getResetValueForControl(control) {
  if (control instanceof HTMLButtonElement && control.dataset.sourceRouteButton) {
    const [sourceKey] = control.dataset.sourceRouteButton.split(":");
    return UI_RESET_DEFAULTS.sourceMixer[sourceKey]?.routeTarget;
  }

  if (!(control instanceof HTMLInputElement || control instanceof HTMLSelectElement)) {
    return undefined;
  }

  if (control.id) {
    const staticValue = getStaticResetValue(control.id);
    if (staticValue !== undefined) {
      return staticValue;
    }
  }

  if (control.dataset.sourceEnabled) {
    return UI_RESET_DEFAULTS.sourceMixer[control.dataset.sourceEnabled]?.enabled;
  }

  if (control.dataset.sourceLevel) {
    return UI_RESET_DEFAULTS.sourceMixer[control.dataset.sourceLevel]?.level;
  }

  if (control.dataset.outputLevel) {
    return UI_RESET_DEFAULTS.outputMixer.level;
  }

  if (control.dataset.outputDelay) {
    return UI_RESET_DEFAULTS.outputMixer.delayMs;
  }

  if (control.dataset.outputEq) {
    const [, band] = control.dataset.outputEq.split(":");
    return UI_RESET_DEFAULTS.outputMixer.eq[band];
  }

  if (control.dataset.voiceActive) {
    return UI_RESET_DEFAULTS.robinVoice.active;
  }

  if (control.dataset.voiceLinked) {
    return UI_RESET_DEFAULTS.robinVoice.linkedToMaster;
  }

  if (control.dataset.voiceFrequency) {
    return UI_RESET_DEFAULTS.robinVoice.frequency;
  }

  if (control.dataset.voiceGain) {
    return UI_RESET_DEFAULTS.robinVoice.gain;
  }

  if (control.dataset.voiceVcf) {
    const [, field] = control.dataset.voiceVcf.split(":");
    return UI_RESET_DEFAULTS.robinVoice.vcf[field];
  }

  if (control.dataset.voiceEnvVcf) {
    const [, field] = control.dataset.voiceEnvVcf.split(":");
    return UI_RESET_DEFAULTS.robinVoice.envVcf[field];
  }

  if (control.dataset.voiceEnvelope) {
    const [, field] = control.dataset.voiceEnvelope.split(":");
    return UI_RESET_DEFAULTS.robinVoice.envelope[field];
  }

  if (control.dataset.voiceOscEnabled) {
    const [, oscillatorIndex] = control.dataset.voiceOscEnabled.split(":");
    return Number(oscillatorIndex) === 0;
  }

  if (control.dataset.voiceOscWaveform) {
    return UI_RESET_DEFAULTS.oscillator.waveform;
  }

  if (control.dataset.voiceOscRelative) {
    return UI_RESET_DEFAULTS.oscillator.relativeToVoice;
  }

  if (control.dataset.voiceOscGain) {
    return UI_RESET_DEFAULTS.oscillator.gain;
  }

  if (control.dataset.voiceOscFrequency) {
    const [voiceIndex, oscillatorIndex] = control.dataset.voiceOscFrequency.split(":");
    const oscillator = getRobin()?.voices?.[Number(voiceIndex)]?.oscillators?.[Number(oscillatorIndex)];
    if (!oscillator) {
      return UI_RESET_DEFAULTS.oscillator.frequencyValue;
    }
    return oscillator.relativeToVoice
      ? UI_RESET_DEFAULTS.oscillator.frequencyValue
      : (getRobin()?.voices?.[Number(voiceIndex)]?.frequency ?? UI_RESET_DEFAULTS.robinVoice.frequency);
  }

  if (control.dataset.oscEnabled) {
    return Number(control.dataset.oscEnabled) === 0;
  }

  if (control.dataset.oscWaveform) {
    return UI_RESET_DEFAULTS.oscillator.waveform;
  }

  if (control.dataset.oscRelative) {
    return UI_RESET_DEFAULTS.oscillator.relativeToVoice;
  }

  if (control.dataset.oscGain) {
    return UI_RESET_DEFAULTS.oscillator.gain;
  }

  if (control.dataset.oscFrequency) {
    const oscillator = getRobinMasterOscillators()[Number(control.dataset.oscFrequency)];
    if (!oscillator) {
      return UI_RESET_DEFAULTS.oscillator.frequencyValue;
    }
    return oscillator.relativeToVoice
      ? UI_RESET_DEFAULTS.oscillator.frequencyValue
      : (getRobin()?.frequency ?? UI_RESET_DEFAULTS.robin.frequency);
  }

  if (control.dataset.testOutput) {
    return Number(control.dataset.testOutput) < 2;
  }

  if (control.dataset.midiRoute) {
    const [, target] = control.dataset.midiRoute.split(":");
    return UI_RESET_DEFAULTS.midiRoute[target];
  }

  return undefined;
}

function resetControlToDefault(control) {
  const resetValue = getResetValueForControl(control);
  if (resetValue === undefined) {
    return false;
  }

  if (control instanceof HTMLButtonElement && control.dataset.sourceRouteButton) {
    const [sourceKey] = control.dataset.sourceRouteButton.split(":");
    const defaultButton = document.querySelector(`[data-source-route-button="${sourceKey}:${resetValue}"]`);
    if (!(defaultButton instanceof HTMLButtonElement)) {
      return false;
    }
    defaultButton.click();
    return true;
  }

  if (control instanceof HTMLInputElement) {
    if (control.type === "checkbox") {
      const nextChecked = Boolean(resetValue);
      if (control.checked === nextChecked) {
        return true;
      }
      control.checked = nextChecked;
      control.dispatchEvent(new Event("change", { bubbles: true }));
      return true;
    }

    if (control.type === "range") {
      const nextValue = String(resetValue);
      const changed = control.value !== nextValue;
      control.value = nextValue;
      if (changed) {
        control.dispatchEvent(new Event("input", { bubbles: true }));
      }
      control.dispatchEvent(new Event("change", { bubbles: true }));
      return true;
    }
  }

  if (control instanceof HTMLSelectElement) {
    const nextValue = String(resetValue);
    if (control.value === nextValue) {
      return true;
    }
    control.value = nextValue;
    control.dispatchEvent(new Event("change", { bubbles: true }));
    return true;
  }

  return false;
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

function getRangeMetrics(input) {
  const min = Number(input.min ?? 0);
  const max = Number(input.max ?? 100);
  const parsedStep = input.step === "any" ? Number.NaN : Number(input.step ?? 1);
  const step = Number.isFinite(parsedStep) && parsedStep > 0 ? parsedStep : 0;

  return {
    min: Number.isFinite(min) ? min : 0,
    max: Number.isFinite(max) ? max : 100,
    step,
  };
}

function roundToStep(value, min, max, step) {
  const clampedValue = clampValue(value, min, max);
  if (!Number.isFinite(step) || step <= 0) {
    return clampedValue;
  }

  const steppedValue = min + (Math.round((clampedValue - min) / step) * step);
  return clampValue(Number(steppedValue.toFixed(6)), min, max);
}

function valueToRatio(input) {
  const { min, max } = getRangeMetrics(input);
  if (max <= min) {
    return 0;
  }

  return (clampValue(input.value, min, max) - min) / (max - min);
}

function rotaryDragValue(input, startValue, startEvent, moveEvent) {
  const { min, max, step } = getRangeMetrics(input);
  if (max <= min) {
    return min;
  }

  const range = max - min;
  const deltaPixels = (startEvent.clientY - moveEvent.clientY) + ((moveEvent.clientX - startEvent.clientX) * 0.35);
  const pixelsForFullRange = moveEvent.shiftKey ? 420 : 180;
  const nextValue = startValue + ((deltaPixels / pixelsForFullRange) * range);
  return roundToStep(nextValue, min, max, step);
}

function dispatchRotaryInput(input, nextValue, { commit = false } = {}) {
  const { min, max, step } = getRangeMetrics(input);
  const roundedValue = roundToStep(nextValue, min, max, step);
  const nextValueText = String(roundedValue);
  const changed = input.value !== nextValueText;
  input.value = nextValueText;
  syncRotaryControl(input);

  if (changed) {
    input.dispatchEvent(new Event("input", { bubbles: true }));
  }

  if (commit) {
    input.dispatchEvent(new Event("change", { bubbles: true }));
  }
}

function syncRotaryControl(input) {
  const wrapper = input.nextElementSibling;
  if (!(wrapper instanceof HTMLElement) || !wrapper.classList.contains("rotary")) {
    return;
  }

  const ratio = valueToRatio(input);
  const angle = (-135 + (ratio * 270)).toFixed(2);
  const output = input.parentElement?.querySelector("output");
  const label = input.parentElement?.querySelector("span");

  wrapper.style.setProperty("--rotary-angle", `${angle}deg`);
  wrapper.style.setProperty("--rotary-ratio", ratio.toFixed(4));
  wrapper.classList.toggle("is-disabled", input.disabled);
  wrapper.setAttribute("aria-valuenow", String(input.value));
  wrapper.setAttribute("aria-valuetext", output?.textContent?.trim() ?? String(input.value));
  wrapper.setAttribute("aria-valuemin", String(getRangeMetrics(input).min));
  wrapper.setAttribute("aria-valuemax", String(getRangeMetrics(input).max));
  if (label?.textContent) {
    wrapper.setAttribute("aria-label", label.textContent.trim());
  }
}

function createRotaryControl(input) {
  input.classList.add("rotary-input");
  input.tabIndex = -1;
  input.parentElement?.classList.add("field--rotary");

  const wrapper = document.createElement("div");
  wrapper.className = "rotary";
  wrapper.tabIndex = input.disabled ? -1 : 0;
  wrapper.setAttribute("role", "slider");
  wrapper.innerHTML = `
    <div class="rotary__dial">
      <div class="rotary__ring"></div>
      <div class="rotary__indicator"></div>
      <div class="rotary__cap"></div>
    </div>
  `;

  input.insertAdjacentElement("afterend", wrapper);

  input.addEventListener("input", () => {
    syncRotaryControl(input);
  });

  input.addEventListener("change", () => {
    syncRotaryControl(input);
  });

  wrapper.addEventListener("pointerdown", (event) => {
    if (input.disabled) {
      return;
    }

    event.preventDefault();
    wrapper.focus();
    beginRangeInteraction(input);
    wrapper.setPointerCapture(event.pointerId);
    const startValue = Number(input.value);
    const startEvent = {
      clientX: event.clientX,
      clientY: event.clientY,
    };

    const handleMove = (moveEvent) => {
      if (moveEvent.pointerId !== event.pointerId) {
        return;
      }
      dispatchRotaryInput(input, rotaryDragValue(input, startValue, startEvent, moveEvent));
    };

    const handleFinish = (finishEvent) => {
      if (finishEvent.pointerId !== event.pointerId) {
        return;
      }

      wrapper.removeEventListener("pointermove", handleMove);
      wrapper.removeEventListener("pointerup", handleFinish);
      wrapper.removeEventListener("pointercancel", handleFinish);
      endRangeInteraction(input);
      dispatchRotaryInput(input, Number(input.value), { commit: true });
    };

    wrapper.addEventListener("pointermove", handleMove);
    wrapper.addEventListener("pointerup", handleFinish);
    wrapper.addEventListener("pointercancel", handleFinish);
  });

  wrapper.addEventListener("wheel", (event) => {
    if (input.disabled) {
      return;
    }

    event.preventDefault();
    const { min, max, step } = getRangeMetrics(input);
    const delta = event.deltaY < 0 ? 1 : -1;
    const stepAmount = step > 0 ? step : (max - min) / 100;
    beginRangeInteraction(input);
    dispatchRotaryInput(input, Number(input.value) + (delta * stepAmount), { commit: true });
    endRangeInteraction(input);
  }, { passive: false });

  wrapper.addEventListener("keydown", (event) => {
    if (input.disabled) {
      return;
    }

    const { min, max, step } = getRangeMetrics(input);
    const stepAmount = step > 0 ? step : (max - min) / 100;
    let nextValue = Number(input.value);
    let handled = true;

    if (event.key === "ArrowUp" || event.key === "ArrowRight") {
      nextValue += stepAmount;
    } else if (event.key === "ArrowDown" || event.key === "ArrowLeft") {
      nextValue -= stepAmount;
    } else if (event.key === "PageUp") {
      nextValue += stepAmount * 10;
    } else if (event.key === "PageDown") {
      nextValue -= stepAmount * 10;
    } else if (event.key === "Home") {
      nextValue = min;
    } else if (event.key === "End") {
      nextValue = max;
    } else {
      handled = false;
    }

    if (!handled) {
      return;
    }

    event.preventDefault();
    beginRangeInteraction(input);
    dispatchRotaryInput(input, nextValue, { commit: true });
    endRangeInteraction(input);
  });

  return wrapper;
}

function ensureRotaryControls(root = document) {
  const scope = root && typeof root.querySelectorAll === "function" ? root : document;
  scope.querySelectorAll('input[type="range"]').forEach((input) => {
    if (!(input instanceof HTMLInputElement)) {
      return;
    }

    if (input.dataset.controlStyle === "linear") {
      return;
    }

    if (!input.classList.contains("rotary-input")) {
      createRotaryControl(input);
    }

    const wrapper = input.nextElementSibling;
    if (wrapper instanceof HTMLElement && wrapper.classList.contains("rotary")) {
      wrapper.tabIndex = input.disabled ? -1 : 0;
    }
    syncRotaryControl(input);
  });
}

function getRobinMasterOscillators() {
  return getRobin()?.masterOscillators ?? [];
}

function cloneMasterStateToVoiceUi(voice) {
  const robin = getRobin();
  if (!robin || !voice) {
    return;
  }

  voice.frequency = clampValue(robin.frequency ?? 400, 20, 20000);
  voice.gain = clampValue(robin.gain ?? 1, 0, 1);
  voice.vcf = {
    cutoffHz: clampValue(robin.vcf?.cutoffHz ?? 18000, 20, 20000),
    resonance: clampValue(robin.vcf?.resonance ?? 0.707, 0.1, 10),
  };
  voice.envVcf = {
    attackMs: clampValue(robin.envVcf?.attackMs ?? 20, 0, 5000),
    decayMs: clampValue(robin.envVcf?.decayMs ?? 250, 0, 5000),
    sustain: clampValue(robin.envVcf?.sustain ?? 0, 0, 1),
    releaseMs: clampValue(robin.envVcf?.releaseMs ?? 220, 0, 5000),
    amount: clampValue(robin.envVcf?.amount ?? 0, 0, 1),
  };
  voice.envelope = {
    attackMs: clampValue(robin.envelope?.attackMs ?? 10, 0, 5000),
    decayMs: clampValue(robin.envelope?.decayMs ?? 80, 0, 5000),
    sustain: clampValue(robin.envelope?.sustain ?? 0.8, 0, 1),
    releaseMs: clampValue(robin.envelope?.releaseMs ?? 200, 0, 5000),
  };
  voice.oscillators = getRobinMasterOscillators().map((oscillator, index) => ({
    index,
    enabled: Boolean(oscillator.enabled),
    gain: clampValue(oscillator.gain ?? 1, 0, 1),
    relativeToVoice: Boolean(oscillator.relativeToVoice),
    frequencyValue: Number(oscillator.frequencyValue ?? 1),
    waveform: String(oscillator.waveform ?? "sine"),
  }));
}

function parseVoiceOscillatorKey(key) {
  const [voiceIndex, oscillatorIndex] = String(key).split(":").map(Number);
  if (!Number.isFinite(voiceIndex) || !Number.isFinite(oscillatorIndex)) {
    return null;
  }

  return { voiceIndex, oscillatorIndex };
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

function refreshRobinMasterOscillatorUi(field) {
  renderRobinLinkState();
  renderOscillators();
}

function applyRobinMasterOscillatorUpdate(oscillatorIndex, field, rawValue) {
  const robin = getRobin();
  const oscillator = robin?.masterOscillators?.[oscillatorIndex];
  if (!robin || !oscillator) {
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
      const masterFrequency = Math.max(1, Number(robin.frequency ?? 400));
      if (nextRelative) {
        oscillator.frequencyValue = clampValue(
          Number(oscillator.frequencyValue) / masterFrequency,
          0.01,
          8,
        );
      } else {
        oscillator.frequencyValue = clampValue(
          Number(oscillator.frequencyValue) * masterFrequency,
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
}

function applyRobinPitchOffsetUpdate(field, rawValue) {
  const robin = getRobin();
  if (!robin) {
    return;
  }

  if (field === "transposeSemitones") {
    robin.transposeSemitones = clampValue(rawValue, -12, 12);
    return;
  }

  if (field === "fineTuneCents") {
    robin.fineTuneCents = clampValue(rawValue, -100, 100);
  }
}

function applyRobinVcfUpdate(field, rawValue) {
  const robin = getRobin();
  if (!robin?.vcf) {
    return;
  }

  if (field === "cutoffHz") {
    robin.vcf.cutoffHz = clampValue(rawValue, 20, 20000);
    return;
  }

  if (field === "resonance") {
    robin.vcf.resonance = clampValue(rawValue, 0.1, 10);
  }
}

function applyRobinEnvelopeUpdate(field, rawValue) {
  const robin = getRobin();
  if (!robin?.envelope) {
    return;
  }

  if (field === "sustain") {
    robin.envelope.sustain = clampValue(rawValue, 0, 1);
    return;
  }

  robin.envelope[field] = clampValue(rawValue, 0, 5000);
}

function applyRobinEnvVcfUpdate(field, rawValue) {
  const robin = getRobin();
  if (!robin?.envVcf) {
    return;
  }

  if (field === "sustain" || field === "amount") {
    robin.envVcf[field] = clampValue(rawValue, 0, 1);
    return;
  }

  robin.envVcf[field] = clampValue(rawValue, 0, 5000);
}

function applyRobinVoiceUpdate(voiceIndex, field, rawValue) {
  const robin = getRobin();
  const voice = robin?.voices?.[voiceIndex];
  if (!voice) {
    return;
  }

  if (field === "active") {
    voice.active = Boolean(rawValue);
    return;
  }

  if (field === "linkedToMaster") {
    voice.linkedToMaster = Boolean(rawValue);
    return;
  }

  if (field === "frequency") {
    voice.frequency = clampValue(rawValue, 20, 20000);
    return;
  }

  if (field === "gain") {
    voice.gain = clampValue(rawValue, 0, 1);
  }
}

function applyRobinVoiceVcfUpdate(voiceIndex, field, rawValue) {
  const voice = getRobin()?.voices?.[voiceIndex];
  if (!voice?.vcf) {
    return;
  }

  if (field === "cutoffHz") {
    voice.vcf.cutoffHz = clampValue(rawValue, 20, 20000);
    return;
  }

  if (field === "resonance") {
    voice.vcf.resonance = clampValue(rawValue, 0.1, 10);
  }
}

function applyRobinVoiceEnvVcfUpdate(voiceIndex, field, rawValue) {
  const voice = getRobin()?.voices?.[voiceIndex];
  if (!voice?.envVcf) {
    return;
  }

  if (field === "sustain" || field === "amount") {
    voice.envVcf[field] = clampValue(rawValue, 0, 1);
    return;
  }

  voice.envVcf[field] = clampValue(rawValue, 0, 5000);
}

function applyRobinVoiceEnvelopeUpdate(voiceIndex, field, rawValue) {
  const voice = getRobin()?.voices?.[voiceIndex];
  if (!voice?.envelope) {
    return;
  }

  if (field === "sustain") {
    voice.envelope.sustain = clampValue(rawValue, 0, 1);
    return;
  }

  voice.envelope[field] = clampValue(rawValue, 0, 5000);
}

function applyRobinVoiceOscillatorUpdate(voiceIndex, oscillatorIndex, field, rawValue) {
  const voice = getRobin()?.voices?.[voiceIndex];
  const oscillator = voice?.oscillators?.[oscillatorIndex];
  if (!voice || !oscillator) {
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
      const voiceFrequency = Math.max(1, Number(voice.frequency ?? 400));
      if (nextRelative) {
        oscillator.frequencyValue = clampValue(
          Number(oscillator.frequencyValue) / voiceFrequency,
          0.01,
          8,
        );
      } else {
        oscillator.frequencyValue = clampValue(
          Number(oscillator.frequencyValue) * voiceFrequency,
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
}

function applySourceMixerUpdate(sourceKey, field, rawValue) {
  const sourceMixer = getSourceMixer();
  const slot = sourceMixer?.[sourceKey];
  if (!slot) {
    return;
  }

  if (field === "enabled") {
    slot.enabled = Boolean(rawValue);
    return;
  }

  if (field === "level") {
    slot.level = clampValue(rawValue, 0, 1);
    return;
  }

  if (field === "routeTarget") {
    slot.routeTarget = String(rawValue);
  }
}

function applyOutputMixerUpdate(outputIndex, field, rawValue) {
  const output = getOutputMixer()?.outputs?.[outputIndex];
  if (!output) {
    return;
  }

  if (field === "level") {
    output.level = clampValue(rawValue, 0, 1);
    return;
  }

  if (field === "delayMs") {
    output.delayMs = clampValue(rawValue, 0, 250);
  }
}

function applyOutputEqUpdate(outputIndex, band, rawValue) {
  const eq = getOutputMixer()?.outputs?.[outputIndex]?.eq;
  if (!eq || !(band in eq)) {
    return;
  }

  eq[band] = clampValue(rawValue, -24, 24);
}

function applyRobinRoutingPresetUpdate(rawValue) {
  const robin = getRobin();
  if (!robin) {
    return;
  }

  robin.routingPreset = String(rawValue);
}

function applyTestStateUpdate(field, rawValue) {
  const test = getTest();
  if (!test) {
    return;
  }

  if (field === "active" || field === "midiEnabled") {
    test[field] = Boolean(rawValue);
    return;
  }

  if (field === "waveform") {
    test.waveform = String(rawValue);
    return;
  }

  if (field === "frequency") {
    test.frequency = clampValue(rawValue, 20, 20000);
    return;
  }

  if (field === "gain") {
    test.gain = clampValue(rawValue, 0, 1);
  }
}

function applyTestEnvelopeUpdate(field, rawValue) {
  const envelope = getTest()?.envelope;
  if (!envelope) {
    return;
  }

  if (field === "sustain") {
    envelope.sustain = clampValue(rawValue, 0, 1);
    return;
  }

  envelope[field] = clampValue(rawValue, 0, 5000);
}

function applyTestOutputUpdate(outputIndex, rawValue) {
  const outputs = getTest()?.outputs;
  if (!outputs || outputIndex >= outputs.length) {
    return;
  }

  outputs[outputIndex] = Boolean(rawValue);
}

function applyLfoUpdate(field, rawValue) {
  const lfo = getRobin()?.lfo;
  if (!lfo) {
    return;
  }

  if (field === "enabled"
    || field === "clockLinked"
    || field === "polarityFlip"
    || field === "unlinkedOutputs") {
    lfo[field] = Boolean(rawValue);
    return;
  }

  if (field === "waveform") {
    lfo.waveform = String(rawValue);
    return;
  }

  if (field === "depth") {
    lfo.depth = clampValue(rawValue, 0, 1);
    return;
  }

  if (field === "phaseSpreadDegrees") {
    lfo.phaseSpreadDegrees = clampValue(rawValue, 0, 360);
    return;
  }

  if (field === "tempoBpm") {
    lfo.tempoBpm = clampValue(rawValue, 20, 300);
    return;
  }

  if (field === "rateMultiplier") {
    lfo.rateMultiplier = clampValue(rawValue, 0.125, 8);
    return;
  }

  if (field === "fixedFrequencyHz") {
    lfo.fixedFrequencyHz = clampValue(rawValue, 0.01, 40);
  }
}

function applySaturatorUpdate(field, rawValue) {
  const saturator = getFx()?.saturator;
  if (!saturator) {
    return;
  }

  if (field === "enabled") {
    saturator[field] = Boolean(rawValue);
    return;
  }

  if (field === "inputLevel" || field === "outputLevel") {
    saturator[field] = clampValue(rawValue, 0, 2);
  }
}

function applyChorusUpdate(field, rawValue) {
  const chorus = getFx()?.chorus;
  if (!chorus) {
    return;
  }

  if (field === "enabled") {
    chorus[field] = Boolean(rawValue);
    return;
  }

  if (field === "depth") {
    chorus.depth = clampValue(rawValue, 0, 1);
    return;
  }

  if (field === "speedHz") {
    chorus.speedHz = clampValue(rawValue, 0.01, 20);
    return;
  }

  if (field === "phaseSpreadDegrees") {
    chorus.phaseSpreadDegrees = clampValue(rawValue, 0, 360);
  }
}

function applySidechainUpdate(rawValue) {
  const sidechain = getFx()?.sidechain;
  if (!sidechain) {
    return;
  }

  sidechain.enabled = Boolean(rawValue);
}

function syncMidiConnectedCounts() {
  const engine = getEngine();
  const midi = getMidi();
  if (!engine || !Array.isArray(midi?.sources)) {
    return;
  }

  const connectedSourceCount = midi.sources.filter((source) => source.connected).length;
  engine.midiConnectedSourceCount = connectedSourceCount;
  if (engine.midi) {
    engine.midi.connectedSourceCount = connectedSourceCount;
  }
}

function applyMidiSourceConnectionUpdate(sourceIndex, rawValue) {
  const source = getMidi()?.sources?.[sourceIndex];
  if (!source) {
    return;
  }

  source.connected = Boolean(rawValue);
  syncMidiConnectedCounts();
}

function applyMidiRouteUpdate(sourceIndex, target, rawValue) {
  const routes = getMidi()?.sources?.[sourceIndex]?.routes;
  if (!routes || !(target in routes)) {
    return;
  }

  routes[target] = Boolean(rawValue);
}

function applyTempStateMutation(applyLocalState = null, rerender = null) {
  if (typeof applyLocalState === "function") {
    applyLocalState();
  }

  if (activeRangeInput) {
    hasDeferredRender = true;
    return;
  }

  if (typeof rerender === "function") {
    rerender();
  }
  updateStateView();
}

function setParamTemp(path, value, { silent = false, applyLocalState = null, rerender = null } = {}) {
  dispatchParamFast(path, value, {
    silent,
    onSuccess: () => {
      applyTempStateMutation(applyLocalState, rerender);
    },
  });
}

function setParamTempLive(path, value, applyLocalState = null) {
  setParamTemp(path, value, {
    silent: true,
    applyLocalState,
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

function setTransposeSemitonesLabel(value) {
  const numericValue = Math.round(Number(value));
  const prefix = numericValue > 0 ? "+" : "";
  elements.transposeSemitonesValue.textContent = `${prefix}${numericValue} st`;
}

function setFineTuneCentsLabel(value) {
  const numericValue = Math.round(Number(value));
  const prefix = numericValue > 0 ? "+" : "";
  elements.fineTuneCentsValue.textContent = `${prefix}${numericValue} ct`;
}

function setRobinVcfCutoffLabel(value) {
  elements.robinVcfCutoffValue.textContent = `${Math.round(Number(value))} Hz`;
}

function setRobinVcfResonanceLabel(value) {
  elements.robinVcfResonanceValue.textContent = Number(value).toFixed(2);
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

function setRobinEnvVcfAttackLabel(value) {
  elements.robinEnvVcfAttackValue.textContent = `${Math.round(Number(value))} ms`;
}

function setRobinEnvVcfDecayLabel(value) {
  elements.robinEnvVcfDecayValue.textContent = `${Math.round(Number(value))} ms`;
}

function setRobinEnvVcfSustainLabel(value) {
  elements.robinEnvVcfSustainValue.textContent = Number(value).toFixed(2);
}

function setRobinEnvVcfReleaseLabel(value) {
  elements.robinEnvVcfReleaseValue.textContent = `${Math.round(Number(value))} ms`;
}

function setRobinEnvVcfAmountLabel(value) {
  elements.robinEnvVcfAmountValue.textContent = Number(value).toFixed(2);
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

function renderWaveformOptions(selectedWaveform) {
  return ["sine", "square", "triangle", "saw", "noise"]
    .map(
      (waveform) => `
        <option value="${waveform}" ${selectedWaveform === waveform ? "selected" : ""}>
          ${waveform[0].toUpperCase()}${waveform.slice(1)}
        </option>
      `,
    )
    .join("");
}

function formatOscillatorFrequencyLabel(oscillator) {
  if (oscillator.relativeToVoice) {
    return `${Number(oscillator.frequencyValue).toFixed(2)}x voice root`;
  }

  return `${Math.round(Number(oscillator.frequencyValue))} Hz`;
}

function syncLfoInputState(lfo) {
  const clockLinked = Boolean(lfo.clockLinked);
  elements.lfoTempo.disabled = !clockLinked;
  elements.lfoRateMultiplier.disabled = !clockLinked;
  elements.lfoFixedFrequency.disabled = clockLinked;
}

function renderRobinLinkState() {
  const robin = getRobin();
  if (!robin) {
    elements.robinLinkBadge.textContent = "Robin offline";
    elements.robinLinkBadge.className = "status-tag";
    elements.robinLinkNote.textContent = "Live state unavailable.";
    return;
  }

  const voiceCount = robin.voices.length;
  const linkedCount = robin.voices.filter((voice) => voice.linkedToMaster).length;
  const localCount = voiceCount - linkedCount;
  const enabledCount = robin.voices.filter((voice) => voice.active).length;

  if (voiceCount === 0) {
    elements.robinLinkBadge.textContent = "No voices";
    elements.robinLinkBadge.className = "status-tag";
    elements.robinLinkNote.textContent = "Live state: increase the voice count to build a spatial voice bank.";
    return;
  }

  if (localCount === 0) {
    elements.robinLinkBadge.textContent = "All voices linked";
    elements.robinLinkBadge.className = "status-tag status-tag--live";
    elements.robinLinkNote.textContent =
      `Live state: ${enabledCount} of ${voiceCount} voices are enabled. The whole instrument is currently reading as one shared spatial template.`;
    return;
  }

  if (linkedCount === 0) {
    elements.robinLinkBadge.textContent = "All voices local";
    elements.robinLinkBadge.className = "status-tag status-tag--warning";
    elements.robinLinkNote.textContent =
      `Live state: ${enabledCount} of ${voiceCount} voices are enabled. Every voice is currently its own local override, so the synth will sound more fragmented and less master-led.`;
    return;
  }

  elements.robinLinkBadge.textContent = `${linkedCount} linked / ${localCount} local`;
  elements.robinLinkBadge.className = "status-tag status-tag--warning";
  elements.robinLinkNote.textContent =
    `Live state: ${enabledCount} of ${voiceCount} voices are enabled, with ${linkedCount} linked and ${localCount} local. This is a good balance for keeping one coherent sound while letting a few voices break away.`;
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
      const routeTarget = mix.routeTarget ?? "dry";
      const statusLabel = live ? "Live" : "Scaffold";
      const routeSummary = routeTarget === "fx" ? "FX path" : "Dry path";

      return `
        <article class="mixer-strip mixer-strip--source ${live ? "mixer-strip--live" : "mixer-strip--planned"}">
          <div class="mixer-strip__header">
            <div class="mixer-strip__title-row">
              <h3>${title}</h3>
              <span class="mixer-strip__status">${statusLabel}</span>
            </div>
            <span class="mixer-strip__subtitle">${mix.enabled ? routeSummary : "Muted"}</span>
          </div>

          <label class="mixer-strip__toggle">
            <input type="checkbox" data-source-enabled="${key}" ${mix.enabled ? "checked" : ""}>
            <span>Enabled</span>
          </label>

          <div class="mixer-strip__switcher" role="group" aria-label="${title} signal path">
            <button
              type="button"
              class="mixer-route-button ${routeTarget === "dry" ? "is-active" : ""}"
              data-source-route-button="${key}:dry"
              ${live ? "" : "disabled"}
            >
              Dry
            </button>
            <button
              type="button"
              class="mixer-route-button ${routeTarget === "fx" ? "is-active" : ""}"
              data-source-route-button="${key}:fx"
              ${live ? "" : "disabled"}
            >
              FX
            </button>
          </div>

          <label class="mixer-strip__fader mixer-strip__fader--main">
            <span>Level</span>
            <output>${formatLevelPercent(mix.level)}</output>
            <input
              type="range"
              data-control-style="linear"
              min="0"
              max="1"
              step="0.01"
              value="${mix.level}"
              data-source-level="${key}"
            >
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
        <article class="mixer-strip mixer-strip--output mixer-strip--live">
          <div class="mixer-strip__header">
            <div class="mixer-strip__title-row">
              <h3>Out ${output.index + 1}</h3>
              <span class="mixer-strip__status">Live</span>
            </div>
            <span class="mixer-strip__subtitle">Dry + FX sum</span>
          </div>

          <label class="mixer-strip__fader mixer-strip__fader--main">
            <span>Level</span>
            <output>${formatLevelPercent(output.level)}</output>
            <input
              type="range"
              data-control-style="linear"
              min="0"
              max="1"
              step="0.01"
              value="${output.level}"
              data-output-level="${output.index}"
            >
          </label>

          <div class="mixer-mini-bank">
            <label class="mixer-mini-slider">
              <span>Dly</span>
              <output>${Math.round(Number(output.delayMs))} ms</output>
              <input
                type="range"
                data-control-style="linear"
                min="0"
                max="250"
                step="1"
                value="${output.delayMs}"
                data-output-delay="${output.index}"
              >
            </label>

            <label class="mixer-mini-slider">
              <span>Low</span>
              <output>${formatSignedDb(output.eq.lowDb)}</output>
              <input
                type="range"
                data-control-style="linear"
                min="-24"
                max="24"
                step="0.5"
                value="${output.eq.lowDb}"
                data-output-eq="${output.index}:lowDb"
              >
            </label>

            <label class="mixer-mini-slider">
              <span>Mid</span>
              <output>${formatSignedDb(output.eq.midDb)}</output>
              <input
                type="range"
                data-control-style="linear"
                min="-24"
                max="24"
                step="0.5"
                value="${output.eq.midDb}"
                data-output-eq="${output.index}:midDb"
              >
            </label>

            <label class="mixer-mini-slider">
              <span>High</span>
              <output>${formatSignedDb(output.eq.highDb)}</output>
              <input
                type="range"
                data-control-style="linear"
                min="-24"
                max="24"
                step="0.5"
                value="${output.eq.highDb}"
                data-output-eq="${output.index}:highDb"
              >
            </label>
          </div>
        </article>
      `,
    )
    .join("");

  bindOutputMixerControls();
}

function getSelectedRobinVoice() {
  const robin = getRobin();
  if (!robin?.voices?.length) {
    selectedRobinVoiceIndex = null;
    return null;
  }

  const localVoices = robin.voices.filter((voice) => !voice.linkedToMaster);
  if (!localVoices.length) {
    selectedRobinVoiceIndex = null;
    return null;
  }

  const selectedVoice = robin.voices.find((voice) => voice.index === selectedRobinVoiceIndex && !voice.linkedToMaster);
  if (selectedVoice) {
    return selectedVoice;
  }

  selectedRobinVoiceIndex = localVoices[0].index;
  return localVoices[0];
}

function renderVoiceOverviewTokens(voice, masterOscillatorCount) {
  if (voice.linkedToMaster) {
    return `
      <span class="token token--ghost">Master root</span>
      <span class="token token--ghost">Master gain</span>
      <span class="token token--ghost">${masterOscillatorCount} master osc</span>
      <span class="token token--ghost">VCF + ENV</span>
    `;
  }

  const enabledOscillatorCount = voice.oscillators.filter((oscillator) => oscillator.enabled).length;
  return `
    <span class="token">${Math.round(Number(voice.frequency))} Hz</span>
    <span class="token">${Number(voice.gain).toFixed(2)} gain</span>
    <span class="token">${enabledOscillatorCount}/${voice.oscillators.length} osc</span>
    <span class="token">Local VCF + ENV</span>
  `;
}

function renderSelectedVoiceEditor(selectedVoice) {
  if (!elements.selectedVoiceEditor) {
    return;
  }

  if (!selectedVoice) {
    elements.selectedVoiceEditor.innerHTML = `
      <div class="panel-header">
        <div>
          <p class="section-kicker">Selected Voice</p>
          <h2>Local Voice Editor</h2>
        </div>
        <span class="status-tag">Waiting</span>
      </div>
      <p class="note">
        Unlink a Robin voice to open one local editor at a time. The overview above stays dense while detailed editing
        happens here.
      </p>
    `;
    return;
  }

  elements.selectedVoiceEditor.innerHTML = `
    <div class="panel-header">
      <div>
        <p class="section-kicker">Selected Voice</p>
        <h2>Voice ${selectedVoice.index + 1} Editor</h2>
      </div>
      <div class="robin-voice-editor__actions">
        <span class="status-tag status-tag--warning">Local Override</span>
        <button type="button" class="ghost-button ghost-button--compact" data-voice-reset-master="${selectedVoice.index}">
          Reset to Master State
        </button>
      </div>
    </div>
    <p class="note">
      This voice is unlinked from the master. Edit it here without crowding the overview above.
    </p>

    <div class="robin-voice-editor__layout">
      <section class="robin-voice-module">
        <div class="robin-voice-module__header">
          <h3>OSC Bank</h3>
          <span>${selectedVoice.oscillators.length} slots</span>
        </div>

        <div class="robin-voice-osc-grid">
          ${selectedVoice.oscillators
            .map((oscillator) => `
              <section class="robin-voice-osc">
                <div class="robin-voice-module__header">
                  <h4>Osc ${oscillator.index + 1}</h4>
                  <label class="toggle-pill">
                    <input
                      type="checkbox"
                      data-voice-osc-enabled="${selectedVoice.index}:${oscillator.index}"
                      ${oscillator.enabled ? "checked" : ""}
                    >
                    <span>Enabled</span>
                  </label>
                </div>

                <div class="robin-voice-osc__fields">
                  <div class="robin-voice-osc__config">
                    <label class="field field--compact">
                      <span>Shape</span>
                      <select data-voice-osc-waveform="${selectedVoice.index}:${oscillator.index}">
                        ${renderWaveformOptions(oscillator.waveform)}
                      </select>
                    </label>

                    <label class="toggle-pill toggle-pill--block robin-voice-osc__toggle">
                      <input
                        type="checkbox"
                        data-voice-osc-relative="${selectedVoice.index}:${oscillator.index}"
                        ${oscillator.relativeToVoice ? "checked" : ""}
                      >
                      <span>${oscillator.relativeToVoice ? "Track Voice Root" : "Use Absolute Hz"}</span>
                    </label>
                  </div>

                  <div class="robin-voice-osc__knobs">
                    <label class="field field--compact">
                      <span>Level</span>
                      <input
                        type="range"
                        min="0"
                        max="1"
                        step="0.01"
                        value="${oscillator.gain}"
                        data-voice-osc-gain="${selectedVoice.index}:${oscillator.index}"
                      >
                      <output>${Number(oscillator.gain).toFixed(2)}</output>
                    </label>

                    <label class="field field--compact">
                      <span>${oscillator.relativeToVoice ? "Ratio" : "Frequency"}</span>
                      <input
                        type="range"
                        min="${oscillator.relativeToVoice ? "0.01" : "20"}"
                        max="${oscillator.relativeToVoice ? "8" : "20000"}"
                        step="${oscillator.relativeToVoice ? "0.01" : "1"}"
                        value="${oscillator.frequencyValue}"
                        data-voice-osc-frequency="${selectedVoice.index}:${oscillator.index}"
                      >
                      <output>${formatOscillatorFrequencyLabel(oscillator)}</output>
                    </label>
                  </div>
                </div>
              </section>
            `)
            .join("")}
        </div>
      </section>

      <div class="robin-voice-editor__stack">
        <section class="robin-voice-module robin-voice-module--env robin-voice-module--env-vcf">
          <div class="robin-voice-module__header">
            <h3>Pitch</h3>
            <span>Root and gain</span>
          </div>

          <div class="robin-voice-module__grid robin-voice-module__grid--compact">
            <label class="field field--compact">
              <span>Local Root Frequency</span>
              <input
                type="range"
                min="20"
                max="20000"
                step="1"
                value="${selectedVoice.frequency}"
                data-voice-frequency="${selectedVoice.index}"
              >
              <output>${Math.round(Number(selectedVoice.frequency))} Hz</output>
            </label>

            <label class="field field--compact">
              <span>Local Voice Level</span>
              <input
                type="range"
                min="0"
                max="1"
                step="0.01"
                value="${selectedVoice.gain}"
                data-voice-gain="${selectedVoice.index}"
              >
              <output>${Number(selectedVoice.gain).toFixed(2)}</output>
            </label>
          </div>
        </section>

        <section class="robin-voice-module robin-voice-module--env robin-voice-module--amp">
          <div class="robin-voice-module__header">
            <h3>VCF</h3>
            <span>Cutoff and resonance</span>
          </div>

          <div class="robin-voice-module__grid robin-voice-module__grid--compact">
            <label class="field field--compact">
              <span>Cutoff</span>
              <input
                type="range"
                min="20"
                max="20000"
                step="1"
                value="${selectedVoice.vcf.cutoffHz}"
                data-voice-vcf="${selectedVoice.index}:cutoffHz"
              >
              <output>${Math.round(Number(selectedVoice.vcf.cutoffHz))} Hz</output>
            </label>

            <label class="field field--compact">
              <span>Resonance</span>
              <input
                type="range"
                min="0.1"
                max="10"
                step="0.01"
                value="${selectedVoice.vcf.resonance}"
                data-voice-vcf="${selectedVoice.index}:resonance"
              >
              <output>${Number(selectedVoice.vcf.resonance).toFixed(2)}</output>
            </label>
          </div>
        </section>

        <section class="robin-voice-module robin-voice-module--env robin-voice-module--env-vcf">
          <div class="robin-voice-module__header">
            <h3>ENV VCF</h3>
            <span>Filter contour</span>
          </div>

          <div class="robin-voice-module__grid">
            <label class="field">
              <span>Attack</span>
              <input
                type="range"
                min="0"
                max="5000"
                step="1"
                value="${selectedVoice.envVcf.attackMs}"
                data-voice-env-vcf="${selectedVoice.index}:attackMs"
              >
              <output>${Math.round(Number(selectedVoice.envVcf.attackMs))} ms</output>
            </label>

            <label class="field">
              <span>Decay</span>
              <input
                type="range"
                min="0"
                max="5000"
                step="1"
                value="${selectedVoice.envVcf.decayMs}"
                data-voice-env-vcf="${selectedVoice.index}:decayMs"
              >
              <output>${Math.round(Number(selectedVoice.envVcf.decayMs))} ms</output>
            </label>

            <label class="field">
              <span>Sustain</span>
              <input
                type="range"
                min="0"
                max="1"
                step="0.01"
                value="${selectedVoice.envVcf.sustain}"
                data-voice-env-vcf="${selectedVoice.index}:sustain"
              >
              <output>${Number(selectedVoice.envVcf.sustain).toFixed(2)}</output>
            </label>

            <label class="field">
              <span>Release</span>
              <input
                type="range"
                min="0"
                max="5000"
                step="1"
                value="${selectedVoice.envVcf.releaseMs}"
                data-voice-env-vcf="${selectedVoice.index}:releaseMs"
              >
              <output>${Math.round(Number(selectedVoice.envVcf.releaseMs))} ms</output>
            </label>

            <label class="field">
              <span>Amount</span>
              <input
                type="range"
                min="0"
                max="1"
                step="0.01"
                value="${selectedVoice.envVcf.amount}"
                data-voice-env-vcf="${selectedVoice.index}:amount"
              >
              <output>${Number(selectedVoice.envVcf.amount).toFixed(2)}</output>
            </label>
          </div>
        </section>

        <section class="robin-voice-module robin-voice-module--env robin-voice-module--amp">
          <div class="robin-voice-module__header">
            <h3>AMP</h3>
            <span>ENV VCA</span>
          </div>

          <div class="robin-voice-module__grid">
            <label class="field">
              <span>Attack</span>
              <input
                type="range"
                min="0"
                max="5000"
                step="1"
                value="${selectedVoice.envelope.attackMs}"
                data-voice-envelope="${selectedVoice.index}:attackMs"
              >
              <output>${Math.round(Number(selectedVoice.envelope.attackMs))} ms</output>
            </label>

            <label class="field">
              <span>Decay</span>
              <input
                type="range"
                min="0"
                max="5000"
                step="1"
                value="${selectedVoice.envelope.decayMs}"
                data-voice-envelope="${selectedVoice.index}:decayMs"
              >
              <output>${Math.round(Number(selectedVoice.envelope.decayMs))} ms</output>
            </label>

            <label class="field">
              <span>Sustain</span>
              <input
                type="range"
                min="0"
                max="1"
                step="0.01"
                value="${selectedVoice.envelope.sustain}"
                data-voice-envelope="${selectedVoice.index}:sustain"
              >
              <output>${Number(selectedVoice.envelope.sustain).toFixed(2)}</output>
            </label>

            <label class="field">
              <span>Release</span>
              <input
                type="range"
                min="0"
                max="5000"
                step="1"
                value="${selectedVoice.envelope.releaseMs}"
                data-voice-envelope="${selectedVoice.index}:releaseMs"
              >
              <output>${Math.round(Number(selectedVoice.envelope.releaseMs))} ms</output>
            </label>
          </div>
        </section>
      </div>
    </div>
  `;
}

function renderVoices() {
  const robin = getRobin();
  if (!robin) {
    elements.voicesGrid.innerHTML = "";
    if (elements.selectedVoiceEditor) {
      elements.selectedVoiceEditor.innerHTML = "";
    }
    return;
  }

  const selectedVoice = getSelectedRobinVoice();
  const masterOscillatorCount = getRobinMasterOscillators().length;
  elements.voicesGrid.innerHTML = robin.voices
    .map((voice) => {
      const linked = Boolean(voice.linkedToMaster);
      const selected = selectedVoice?.index === voice.index;
      const rowClasses = [
        "voice-overview-row",
        voice.active ? "is-active" : "is-inactive",
        linked ? "is-linked" : "is-local",
        selected ? "is-selected" : "",
      ].join(" ");
      return `
        <article class="${rowClasses}">
          <div class="voice-overview-row__identity">
            <strong>Voice ${voice.index + 1}</strong>
            <span class="voice-overview-row__state">${linked ? "Linked" : "Local"}</span>
          </div>

          <div class="voice-overview-row__summary token-row">
            ${renderVoiceOverviewTokens(voice, masterOscillatorCount)}
          </div>

          <div class="voice-overview-row__controls">
            <label class="toggle-pill">
              <input type="checkbox" data-voice-active="${voice.index}" ${voice.active ? "checked" : ""}>
              <span>Enabled</span>
            </label>
            <label class="toggle-pill">
              <input type="checkbox" data-voice-linked="${voice.index}" ${linked ? "checked" : ""}>
              <span>Linked</span>
            </label>
          </div>

          <div class="voice-overview-row__actions">
            ${linked
              ? `<span class="voice-overview-row__hint">${voice.active ? "Using master" : "Disabled"}</span>`
              : `
                <button
                  type="button"
                  class="ghost-button ghost-button--compact ${selected ? "is-selected" : ""}"
                  data-voice-select="${voice.index}"
                >
                  ${selected ? "Editing" : "Edit"}
                </button>
              `}
          </div>
        </article>
      `;
    })
    .join("");

  renderSelectedVoiceEditor(selectedVoice);
  bindVoiceControls();
  ensureRotaryControls(elements.selectedVoiceEditor);
}

function renderOscillators() {
  const oscillators = getRobinMasterOscillators();
  if (!oscillators.length) {
    elements.oscillatorRows.innerHTML = "";
    return;
  }

  elements.oscillatorRows.innerHTML = oscillators
    .map(
      (oscillator) => `
        <section class="robin-voice-osc robin-master-osc">
          <div class="robin-voice-module__header">
            <h4>Master Osc ${oscillator.index + 1}</h4>
            <label class="toggle-pill">
              <input
                type="checkbox"
                data-osc-enabled="${oscillator.index}"
                ${oscillator.enabled ? "checked" : ""}
              >
              <span>Enabled</span>
            </label>
          </div>

          <div class="robin-voice-osc__fields">
            <div class="robin-voice-osc__config">
              <label class="field field--compact">
                <span>Shape</span>
                <select data-osc-waveform="${oscillator.index}">
                  ${renderWaveformOptions(oscillator.waveform)}
                </select>
              </label>

              <label class="toggle-pill toggle-pill--block robin-voice-osc__toggle">
                <input
                  type="checkbox"
                  data-osc-relative="${oscillator.index}"
                  ${oscillator.relativeToVoice ? "checked" : ""}
                >
                <span>${oscillator.relativeToVoice ? "Track Voice Root" : "Use Absolute Hz"}</span>
              </label>
            </div>

            <div class="robin-voice-osc__knobs">
              <label class="field field--compact">
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

              <label class="field field--compact">
                <span>${oscillator.relativeToVoice ? "Ratio" : "Frequency"}</span>
                <input
                  type="range"
                  min="${oscillator.relativeToVoice ? "0.01" : "20"}"
                  max="${oscillator.relativeToVoice ? "8" : "20000"}"
                  step="${oscillator.relativeToVoice ? "0.01" : "1"}"
                  value="${oscillator.frequencyValue}"
                  data-osc-frequency="${oscillator.index}"
                >
                <output>${formatOscillatorFrequencyLabel(oscillator)}</output>
              </label>
            </div>
          </div>
        </section>
      `,
    )
    .join("");

  bindOscillatorControls();
  ensureRotaryControls(elements.oscillatorRows);
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
            <span>Dry + FX paths</span>
          </div>

          <div class="output-column__group">
            <span>Paths</span>
            <div class="token-row">
              <span class="token">Dry</span>
              <span class="token">Through FX Chain</span>
            </div>
          </div>

          <div class="output-column__group">
            <span>Chain</span>
            <div class="token-row">
              <span class="token ${fx.saturator.enabled ? "" : "token--ghost"}">Saturator</span>
              <span class="token ${fx.chorus.enabled ? "" : "token--ghost"}">Chorus</span>
              <span class="token ${fx.sidechain.enabled ? "" : "token--ghost"}">Sidechain</span>
            </div>
          </div>

          <p>Source Mixer decides whether each source feeds the dry path or this output FX chain.</p>
        </article>
      `,
    )
    .join("");
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
  elements.transposeSemitones.value = String(robin.transposeSemitones);
  setTransposeSemitonesLabel(robin.transposeSemitones);
  elements.fineTuneCents.value = String(robin.fineTuneCents);
  setFineTuneCentsLabel(robin.fineTuneCents);
  elements.robinVcfCutoff.value = String(robin.vcf.cutoffHz);
  setRobinVcfCutoffLabel(robin.vcf.cutoffHz);
  elements.robinVcfResonance.value = String(robin.vcf.resonance);
  setRobinVcfResonanceLabel(robin.vcf.resonance);
  elements.robinEnvVcfAttack.value = String(robin.envVcf.attackMs);
  setRobinEnvVcfAttackLabel(robin.envVcf.attackMs);
  elements.robinEnvVcfDecay.value = String(robin.envVcf.decayMs);
  setRobinEnvVcfDecayLabel(robin.envVcf.decayMs);
  elements.robinEnvVcfSustain.value = String(robin.envVcf.sustain);
  setRobinEnvVcfSustainLabel(robin.envVcf.sustain);
  elements.robinEnvVcfRelease.value = String(robin.envVcf.releaseMs);
  setRobinEnvVcfReleaseLabel(robin.envVcf.releaseMs);
  elements.robinEnvVcfAmount.value = String(robin.envVcf.amount);
  setRobinEnvVcfAmountLabel(robin.envVcf.amount);
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
  elements.fxSaturatorInputLevel.value = String(fx.saturator.inputLevel);
  setSaturatorInputLabel(fx.saturator.inputLevel);
  elements.fxSaturatorOutputLevel.value = String(fx.saturator.outputLevel);
  setSaturatorOutputLabel(fx.saturator.outputLevel);
  elements.fxChorusEnabled.checked = fx.chorus.enabled;
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
  ensureRotaryControls();
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

function setStructuralParam(path, value) {
  dispatchParam(path, value);
}

function setLinkedOscillatorParam(oscillatorIndex, field, value) {
  dispatchParam(`sources.robin.oscillator.${oscillatorIndex}.${field}`, value);
}

function setLinkedOscillatorParamFast(oscillatorIndex, field, value) {
  const path = `sources.robin.oscillator.${oscillatorIndex}.${field}`;
  const numericOscillatorIndex = Number(oscillatorIndex);
  setParamTemp(path, value, {
    silent: true,
    applyLocalState: () => {
      applyRobinMasterOscillatorUpdate(numericOscillatorIndex, field, value);
    },
    rerender: () => {
      refreshRobinMasterOscillatorUi(field);
    },
  });
}

function bindSourceMixerControls() {
  document.querySelectorAll("[data-source-enabled]").forEach((input) => {
    input.addEventListener("change", () => {
      const sourceKey = input.dataset.sourceEnabled;
      setParamTemp(`sourceMixer.${sourceKey}.enabled`, input.checked, {
        applyLocalState: () => {
          applySourceMixerUpdate(sourceKey, "enabled", input.checked);
        },
        rerender: () => {
          renderSourceMixer();
        },
      });
    });
  });

  document.querySelectorAll("[data-source-level]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      const sourceKey = input.dataset.sourceLevel;
      const nextValue = Number(input.value);
      output.textContent = formatLevelPercent(nextValue);
      setParamTempLive(`sourceMixer.${sourceKey}.level`, nextValue, () => {
        applySourceMixerUpdate(sourceKey, "level", nextValue);
      });
    });
  });

  document.querySelectorAll("[data-source-route-button]").forEach((button) => {
    button.addEventListener("click", () => {
      const [sourceKey, nextValue] = button.dataset.sourceRouteButton.split(":");
      setParamTemp(`sourceMixer.${sourceKey}.routeTarget`, nextValue, {
        applyLocalState: () => {
          applySourceMixerUpdate(sourceKey, "routeTarget", nextValue);
        },
        rerender: () => {
          renderSourceMixer();
        },
      });
    });
  });
}

function bindOutputMixerControls() {
  document.querySelectorAll("[data-output-level]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      const outputIndex = Number(input.dataset.outputLevel);
      const nextValue = Number(input.value);
      output.textContent = formatLevelPercent(nextValue);
      setParamTempLive(`outputMixer.output.${outputIndex}.level`, nextValue, () => {
        applyOutputMixerUpdate(outputIndex, "level", nextValue);
      });
    });
  });

  document.querySelectorAll("[data-output-delay]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      const outputIndex = Number(input.dataset.outputDelay);
      const nextValue = Number(input.value);
      output.textContent = `${Math.round(Number(input.value))} ms`;
      setParamTempLive(`outputMixer.output.${outputIndex}.delayMs`, nextValue, () => {
        applyOutputMixerUpdate(outputIndex, "delayMs", nextValue);
      });
    });
  });

  document.querySelectorAll("[data-output-eq]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      const nextValue = Number(input.value);
      output.textContent = formatSignedDb(nextValue);
      const [outputIndex, band] = input.dataset.outputEq.split(":");
      setParamTempLive(`outputMixer.output.${outputIndex}.eq.${band}`, nextValue, () => {
        applyOutputEqUpdate(Number(outputIndex), band, nextValue);
      });
    });
  });
}

function bindVoiceControls() {
  document.querySelectorAll("[data-voice-active]").forEach((input) => {
    input.addEventListener("change", () => {
      const voiceIndex = Number(input.dataset.voiceActive);
      setParamTemp(`sources.robin.voice.${voiceIndex}.active`, input.checked, {
        applyLocalState: () => {
          applyRobinVoiceUpdate(voiceIndex, "active", input.checked);
        },
        rerender: () => {
          renderVoices();
        },
      });
    });
  });

  document.querySelectorAll("[data-voice-select]").forEach((button) => {
    button.addEventListener("click", () => {
      selectedRobinVoiceIndex = Number(button.dataset.voiceSelect);
      renderVoices();
    });
  });

  document.querySelectorAll("[data-voice-linked]").forEach((input) => {
    input.addEventListener("change", () => {
      const voiceIndex = Number(input.dataset.voiceLinked);
      if (!input.checked) {
        selectedRobinVoiceIndex = voiceIndex;
      }
      setParamTemp(`sources.robin.voice.${voiceIndex}.linkedToMaster`, input.checked, {
        applyLocalState: () => {
          applyRobinVoiceUpdate(voiceIndex, "linkedToMaster", input.checked);
        },
        rerender: () => {
          renderRobinLinkState();
          renderVoices();
        },
      });
    });
  });

  document.querySelectorAll("[data-voice-reset-master]").forEach((button) => {
    button.addEventListener("click", () => {
      const voiceIndex = Number(button.dataset.voiceResetMaster);
      const voice = getRobin()?.voices?.[voiceIndex];
      if (!voice) {
        return;
      }

      setParamTemp(`sources.robin.voice.${voiceIndex}.resetToMasterState`, 1, {
        applyLocalState: () => {
          cloneMasterStateToVoiceUi(voice);
        },
        rerender: () => {
          renderVoices();
        },
      });
    });
  });

  document.querySelectorAll("[data-voice-frequency]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      const nextValue = Number(input.value);
      output.textContent = `${Math.round(nextValue)} Hz`;
      setParamTempLive(
        `sources.robin.voice.${input.dataset.voiceFrequency}.frequency`,
        nextValue,
        () => {
          applyRobinVoiceUpdate(Number(input.dataset.voiceFrequency), "frequency", nextValue);
        },
      );
    });
  });

  document.querySelectorAll("[data-voice-gain]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      const nextValue = Number(input.value);
      output.textContent = nextValue.toFixed(2);
      setParamTempLive(
        `sources.robin.voice.${input.dataset.voiceGain}.gain`,
        nextValue,
        () => {
          applyRobinVoiceUpdate(Number(input.dataset.voiceGain), "gain", nextValue);
        },
      );
    });
  });

  document.querySelectorAll("[data-voice-vcf]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      const [voiceIndex, field] = input.dataset.voiceVcf.split(":");
      const numericVoiceIndex = Number(voiceIndex);
      const nextValue = Number(input.value);
      output.textContent = field === "cutoffHz"
        ? `${Math.round(nextValue)} Hz`
        : nextValue.toFixed(2);
      setParamTempLive(`sources.robin.voice.${voiceIndex}.vcf.${field}`, nextValue, () => {
        applyRobinVoiceVcfUpdate(numericVoiceIndex, field, nextValue);
      });
    });
  });

  document.querySelectorAll("[data-voice-env-vcf]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      const [voiceIndex, field] = input.dataset.voiceEnvVcf.split(":");
      const numericVoiceIndex = Number(voiceIndex);
      const nextValue = Number(input.value);
      output.textContent = field === "sustain" || field === "amount"
        ? nextValue.toFixed(2)
        : `${Math.round(nextValue)} ms`;
      setParamTempLive(`sources.robin.voice.${voiceIndex}.envVcf.${field}`, nextValue, () => {
        applyRobinVoiceEnvVcfUpdate(numericVoiceIndex, field, nextValue);
      });
    });
  });

  document.querySelectorAll("[data-voice-envelope]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      const [voiceIndex, field] = input.dataset.voiceEnvelope.split(":");
      const numericVoiceIndex = Number(voiceIndex);
      const nextValue = Number(input.value);
      output.textContent = field === "sustain"
        ? nextValue.toFixed(2)
        : `${Math.round(nextValue)} ms`;
      setParamTempLive(`sources.robin.voice.${voiceIndex}.envelope.${field}`, nextValue, () => {
        applyRobinVoiceEnvelopeUpdate(numericVoiceIndex, field, nextValue);
      });
    });
  });

  document.querySelectorAll("[data-voice-osc-enabled]").forEach((input) => {
    input.addEventListener("change", () => {
      const key = parseVoiceOscillatorKey(input.dataset.voiceOscEnabled);
      if (!key) {
        return;
      }
      setParamTemp(`sources.robin.voice.${key.voiceIndex}.oscillator.${key.oscillatorIndex}.enabled`, input.checked, {
        applyLocalState: () => {
          applyRobinVoiceOscillatorUpdate(key.voiceIndex, key.oscillatorIndex, "enabled", input.checked);
        },
      });
    });
  });

  document.querySelectorAll("[data-voice-osc-waveform]").forEach((select) => {
    select.addEventListener("change", () => {
      const key = parseVoiceOscillatorKey(select.dataset.voiceOscWaveform);
      if (!key) {
        return;
      }
      setParamTemp(`sources.robin.voice.${key.voiceIndex}.oscillator.${key.oscillatorIndex}.waveform`, select.value, {
        applyLocalState: () => {
          applyRobinVoiceOscillatorUpdate(key.voiceIndex, key.oscillatorIndex, "waveform", select.value);
        },
      });
    });
  });

  document.querySelectorAll("[data-voice-osc-relative]").forEach((input) => {
    input.addEventListener("change", () => {
      const key = parseVoiceOscillatorKey(input.dataset.voiceOscRelative);
      if (!key) {
        return;
      }
      setParamTemp(`sources.robin.voice.${key.voiceIndex}.oscillator.${key.oscillatorIndex}.relative`, input.checked, {
        applyLocalState: () => {
          applyRobinVoiceOscillatorUpdate(key.voiceIndex, key.oscillatorIndex, "relative", input.checked);
        },
        rerender: () => {
          renderVoices();
        },
      });
    });
  });

  document.querySelectorAll("[data-voice-osc-gain]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      const key = parseVoiceOscillatorKey(input.dataset.voiceOscGain);
      if (!key) {
        return;
      }
      const nextValue = Number(input.value);
      output.textContent = nextValue.toFixed(2);
      setParamTempLive(`sources.robin.voice.${key.voiceIndex}.oscillator.${key.oscillatorIndex}.gain`, nextValue, () => {
        applyRobinVoiceOscillatorUpdate(key.voiceIndex, key.oscillatorIndex, "gain", nextValue);
      });
    });
  });

  document.querySelectorAll("[data-voice-osc-frequency]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      const key = parseVoiceOscillatorKey(input.dataset.voiceOscFrequency);
      if (!key) {
        return;
      }

      const oscillator = getRobin()?.voices?.[key.voiceIndex]?.oscillators?.[key.oscillatorIndex];
      if (!oscillator) {
        return;
      }

      const nextValue = Number(input.value);
      output.textContent = oscillator.relativeToVoice
        ? `${nextValue.toFixed(2)}x voice root`
        : `${Math.round(nextValue)} Hz`;
      setParamTempLive(`sources.robin.voice.${key.voiceIndex}.oscillator.${key.oscillatorIndex}.frequency`, nextValue, () => {
        applyRobinVoiceOscillatorUpdate(key.voiceIndex, key.oscillatorIndex, "frequency", nextValue);
      });
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
      const nextValue = Number(input.value);
      output.textContent = nextValue.toFixed(2);
      setLinkedOscillatorParamFast(input.dataset.oscGain, "gain", nextValue);
    });
  });

  document.querySelectorAll("[data-osc-frequency]").forEach((input) => {
    const output = input.parentElement.querySelector("output");
    input.addEventListener("input", () => {
      const oscillatorIndex = Number(input.dataset.oscFrequency);
      const oscillator = getRobinMasterOscillators()[oscillatorIndex];
      if (!oscillator) {
        return;
      }

      output.textContent = oscillator.relativeToVoice
        ? `${Number(input.value).toFixed(2)}x voice root`
        : `${Math.round(Number(input.value))} Hz`;
      setLinkedOscillatorParamFast(input.dataset.oscFrequency, "frequency", Number(input.value));
    });
  });
}

function bindMidiDeviceControls() {
  document.querySelectorAll("[data-midi-source]").forEach((input) => {
    input.addEventListener("change", () => {
      const sourceIndex = Number(input.dataset.midiSource);
      setParamTemp(`engine.midi.source.${sourceIndex}.connected`, input.checked, {
        applyLocalState: () => {
          applyMidiSourceConnectionUpdate(sourceIndex, input.checked);
        },
        rerender: () => {
          renderMidiDevices();
        },
      });
    });
  });

  document.querySelectorAll("[data-midi-route]").forEach((input) => {
    input.addEventListener("change", () => {
      const [sourceIndex, target] = input.dataset.midiRoute.split(":");
      const numericSourceIndex = Number(sourceIndex);
      setParamTemp(`engine.midi.source.${sourceIndex}.route.${target}`, input.checked, {
        applyLocalState: () => {
          applyMidiRouteUpdate(numericSourceIndex, target, input.checked);
        },
        rerender: () => {
          renderMidiDevices();
        },
      });
    });
  });
}

function bindTestOutputControls() {
  document.querySelectorAll("[data-test-output]").forEach((input) => {
    input.addEventListener("change", () => {
      const outputIndex = Number(input.dataset.testOutput);
      setParamTemp(`sources.test.output.${outputIndex}`, input.checked, {
        applyLocalState: () => {
          applyTestOutputUpdate(outputIndex, input.checked);
        },
        rerender: () => {
          renderTestSource();
        },
      });
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

  document.addEventListener(
    "dblclick",
    (event) => {
      const control = findResetControl(event.target);
      if (!control) {
        return;
      }

      if (resetControlToDefault(control)) {
        event.preventDefault();
      }
    },
    true,
  );

  elements.tabs.forEach((button) => {
    button.addEventListener("click", () => {
      selectPage(button.dataset.page);
    });
  });

  elements.engineOutputDevice.addEventListener("change", () => {
    setStructuralParam("engine.outputDeviceId", elements.engineOutputDevice.value);
  });

  elements.engineOutputChannels.addEventListener("change", () => {
    setStructuralParam("engine.outputChannels", Number(elements.engineOutputChannels.value));
  });

  elements.voiceCount.addEventListener("input", () => {
    setVoiceCountLabel(elements.voiceCount.value);
  });

  elements.voiceCount.addEventListener("change", () => {
    setStructuralParam("sources.robin.voiceCount", Number(elements.voiceCount.value));
  });

  elements.oscillatorsPerVoice.addEventListener("input", () => {
    setOscillatorCountLabel(elements.oscillatorsPerVoice.value);
  });

  elements.oscillatorsPerVoice.addEventListener("change", () => {
    setStructuralParam("sources.robin.oscillatorsPerVoice", Number(elements.oscillatorsPerVoice.value));
  });

  elements.routingPreset.addEventListener("change", () => {
    const nextValue = elements.routingPreset.value;
    setParamTemp("sources.robin.routingPreset", nextValue, {
      applyLocalState: () => {
        applyRobinRoutingPresetUpdate(nextValue);
      },
      rerender: () => {
        elements.heroRoutingValue.textContent = formatRoutingPresetLabel(nextValue);
      },
    });
  });

  elements.transposeSemitones.addEventListener("input", () => {
    const nextValue = Number(elements.transposeSemitones.value);
    setTransposeSemitonesLabel(nextValue);
    setParamTempLive("sources.robin.transposeSemitones", nextValue, () => {
      applyRobinPitchOffsetUpdate("transposeSemitones", nextValue);
    });
  });

  elements.fineTuneCents.addEventListener("input", () => {
    const nextValue = Number(elements.fineTuneCents.value);
    setFineTuneCentsLabel(nextValue);
    setParamTempLive("sources.robin.fineTuneCents", nextValue, () => {
      applyRobinPitchOffsetUpdate("fineTuneCents", nextValue);
    });
  });

  elements.robinVcfCutoff.addEventListener("input", () => {
    const nextValue = Number(elements.robinVcfCutoff.value);
    setRobinVcfCutoffLabel(nextValue);
    setParamTempLive("sources.robin.vcf.cutoffHz", nextValue, () => {
      applyRobinVcfUpdate("cutoffHz", nextValue);
    });
  });

  elements.robinVcfResonance.addEventListener("input", () => {
    const nextValue = Number(elements.robinVcfResonance.value);
    setRobinVcfResonanceLabel(nextValue);
    setParamTempLive("sources.robin.vcf.resonance", nextValue, () => {
      applyRobinVcfUpdate("resonance", nextValue);
    });
  });

  elements.robinEnvVcfAttack.addEventListener("input", () => {
    const nextValue = Number(elements.robinEnvVcfAttack.value);
    setRobinEnvVcfAttackLabel(nextValue);
    setParamTempLive("sources.robin.envVcf.attackMs", nextValue, () => {
      applyRobinEnvVcfUpdate("attackMs", nextValue);
    });
  });

  elements.robinEnvVcfDecay.addEventListener("input", () => {
    const nextValue = Number(elements.robinEnvVcfDecay.value);
    setRobinEnvVcfDecayLabel(nextValue);
    setParamTempLive("sources.robin.envVcf.decayMs", nextValue, () => {
      applyRobinEnvVcfUpdate("decayMs", nextValue);
    });
  });

  elements.robinEnvVcfSustain.addEventListener("input", () => {
    const nextValue = Number(elements.robinEnvVcfSustain.value);
    setRobinEnvVcfSustainLabel(nextValue);
    setParamTempLive("sources.robin.envVcf.sustain", nextValue, () => {
      applyRobinEnvVcfUpdate("sustain", nextValue);
    });
  });

  elements.robinEnvVcfRelease.addEventListener("input", () => {
    const nextValue = Number(elements.robinEnvVcfRelease.value);
    setRobinEnvVcfReleaseLabel(nextValue);
    setParamTempLive("sources.robin.envVcf.releaseMs", nextValue, () => {
      applyRobinEnvVcfUpdate("releaseMs", nextValue);
    });
  });

  elements.robinEnvVcfAmount.addEventListener("input", () => {
    const nextValue = Number(elements.robinEnvVcfAmount.value);
    setRobinEnvVcfAmountLabel(nextValue);
    setParamTempLive("sources.robin.envVcf.amount", nextValue, () => {
      applyRobinEnvVcfUpdate("amount", nextValue);
    });
  });

  elements.robinAttack.addEventListener("input", () => {
    const nextValue = Number(elements.robinAttack.value);
    setRobinAttackLabel(nextValue);
    setParamTempLive("sources.robin.envelope.attackMs", nextValue, () => {
      applyRobinEnvelopeUpdate("attackMs", nextValue);
    });
  });

  elements.robinDecay.addEventListener("input", () => {
    const nextValue = Number(elements.robinDecay.value);
    setRobinDecayLabel(nextValue);
    setParamTempLive("sources.robin.envelope.decayMs", nextValue, () => {
      applyRobinEnvelopeUpdate("decayMs", nextValue);
    });
  });

  elements.robinSustain.addEventListener("input", () => {
    const nextValue = Number(elements.robinSustain.value);
    setRobinSustainLabel(nextValue);
    setParamTempLive("sources.robin.envelope.sustain", nextValue, () => {
      applyRobinEnvelopeUpdate("sustain", nextValue);
    });
  });

  elements.robinRelease.addEventListener("input", () => {
    const nextValue = Number(elements.robinRelease.value);
    setRobinReleaseLabel(nextValue);
    setParamTempLive("sources.robin.envelope.releaseMs", nextValue, () => {
      applyRobinEnvelopeUpdate("releaseMs", nextValue);
    });
  });

  elements.testActive.addEventListener("change", () => {
    const nextValue = elements.testActive.checked;
    setParamTemp("sources.test.active", nextValue, {
      applyLocalState: () => {
        applyTestStateUpdate("active", nextValue);
      },
    });
  });

  elements.testMidiEnabled.addEventListener("change", () => {
    const nextValue = elements.testMidiEnabled.checked;
    setParamTemp("sources.test.midiEnabled", nextValue, {
      applyLocalState: () => {
        applyTestStateUpdate("midiEnabled", nextValue);
      },
    });
  });

  elements.testFrequency.addEventListener("input", () => {
    const nextValue = Number(elements.testFrequency.value);
    setTestFrequencyLabel(nextValue);
    setParamTempLive("sources.test.frequency", nextValue, () => {
      applyTestStateUpdate("frequency", nextValue);
    });
  });

  elements.testGain.addEventListener("input", () => {
    const nextValue = Number(elements.testGain.value);
    setTestGainLabel(nextValue);
    setParamTempLive("sources.test.gain", nextValue, () => {
      applyTestStateUpdate("gain", nextValue);
    });
  });

  elements.testWaveform.addEventListener("change", () => {
    const nextValue = elements.testWaveform.value;
    setParamTemp("sources.test.waveform", nextValue, {
      applyLocalState: () => {
        applyTestStateUpdate("waveform", nextValue);
      },
    });
  });

  elements.testAttack.addEventListener("input", () => {
    const nextValue = Number(elements.testAttack.value);
    setTestAttackLabel(nextValue);
    setParamTempLive("sources.test.envelope.attackMs", nextValue, () => {
      applyTestEnvelopeUpdate("attackMs", nextValue);
    });
  });

  elements.testDecay.addEventListener("input", () => {
    const nextValue = Number(elements.testDecay.value);
    setTestDecayLabel(nextValue);
    setParamTempLive("sources.test.envelope.decayMs", nextValue, () => {
      applyTestEnvelopeUpdate("decayMs", nextValue);
    });
  });

  elements.testSustain.addEventListener("input", () => {
    const nextValue = Number(elements.testSustain.value);
    setTestSustainLabel(nextValue);
    setParamTempLive("sources.test.envelope.sustain", nextValue, () => {
      applyTestEnvelopeUpdate("sustain", nextValue);
    });
  });

  elements.testRelease.addEventListener("input", () => {
    const nextValue = Number(elements.testRelease.value);
    setTestReleaseLabel(nextValue);
    setParamTempLive("sources.test.envelope.releaseMs", nextValue, () => {
      applyTestEnvelopeUpdate("releaseMs", nextValue);
    });
  });

  elements.fxSaturatorEnabled.addEventListener("change", () => {
    const nextValue = elements.fxSaturatorEnabled.checked;
    setParamTemp("processors.fx.saturator.enabled", nextValue, {
      applyLocalState: () => {
        applySaturatorUpdate("enabled", nextValue);
      },
    });
  });

  elements.fxSaturatorInputLevel.addEventListener("input", () => {
    const nextValue = Number(elements.fxSaturatorInputLevel.value);
    setSaturatorInputLabel(nextValue);
    setParamTempLive("processors.fx.saturator.inputLevel", nextValue, () => {
      applySaturatorUpdate("inputLevel", nextValue);
    });
  });

  elements.fxSaturatorOutputLevel.addEventListener("input", () => {
    const nextValue = Number(elements.fxSaturatorOutputLevel.value);
    setSaturatorOutputLabel(nextValue);
    setParamTempLive("processors.fx.saturator.outputLevel", nextValue, () => {
      applySaturatorUpdate("outputLevel", nextValue);
    });
  });

  elements.fxChorusEnabled.addEventListener("change", () => {
    const nextValue = elements.fxChorusEnabled.checked;
    setParamTemp("processors.fx.chorus.enabled", nextValue, {
      applyLocalState: () => {
        applyChorusUpdate("enabled", nextValue);
      },
    });
  });

  elements.fxChorusDepth.addEventListener("input", () => {
    const nextValue = Number(elements.fxChorusDepth.value);
    setChorusDepthLabel(nextValue);
    setParamTempLive("processors.fx.chorus.depth", nextValue, () => {
      applyChorusUpdate("depth", nextValue);
    });
  });

  elements.fxChorusSpeed.addEventListener("input", () => {
    const nextValue = Number(elements.fxChorusSpeed.value);
    setChorusSpeedLabel(nextValue);
    setParamTempLive("processors.fx.chorus.speedHz", nextValue, () => {
      applyChorusUpdate("speedHz", nextValue);
    });
  });

  elements.fxChorusPhaseSpread.addEventListener("input", () => {
    const nextValue = Number(elements.fxChorusPhaseSpread.value);
    setChorusPhaseLabel(nextValue);
    setParamTempLive("processors.fx.chorus.phaseSpreadDegrees", nextValue, () => {
      applyChorusUpdate("phaseSpreadDegrees", nextValue);
    });
  });

  elements.fxSidechainEnabled.addEventListener("change", () => {
    const nextValue = elements.fxSidechainEnabled.checked;
    setParamTemp("processors.fx.sidechain.enabled", nextValue, {
      applyLocalState: () => {
        applySidechainUpdate(nextValue);
      },
    });
  });

  elements.lfoEnabled.addEventListener("change", () => {
    const nextValue = elements.lfoEnabled.checked;
    setParamTemp("sources.robin.lfo.enabled", nextValue, {
      applyLocalState: () => {
        applyLfoUpdate("enabled", nextValue);
      },
    });
  });

  elements.lfoWaveform.addEventListener("change", () => {
    const nextValue = elements.lfoWaveform.value;
    setParamTemp("sources.robin.lfo.waveform", nextValue, {
      applyLocalState: () => {
        applyLfoUpdate("waveform", nextValue);
      },
    });
  });

  elements.lfoDepth.addEventListener("input", () => {
    const nextValue = Number(elements.lfoDepth.value);
    setLfoDepthLabel(nextValue);
    setParamTempLive("sources.robin.lfo.depth", nextValue, () => {
      applyLfoUpdate("depth", nextValue);
    });
  });

  elements.lfoClockLinked.addEventListener("change", () => {
    const nextValue = elements.lfoClockLinked.checked;
    setParamTemp("sources.robin.lfo.clockLinked", nextValue, {
      applyLocalState: () => {
        applyLfoUpdate("clockLinked", nextValue);
      },
      rerender: () => {
        syncLfoInputState(getRobin()?.lfo ?? { clockLinked: nextValue });
      },
    });
  });

  elements.lfoTempo.addEventListener("input", () => {
    const nextValue = Number(elements.lfoTempo.value);
    setLfoTempoLabel(nextValue);
    setParamTempLive("sources.robin.lfo.tempoBpm", nextValue, () => {
      applyLfoUpdate("tempoBpm", nextValue);
    });
  });

  elements.lfoRateMultiplier.addEventListener("input", () => {
    const nextValue = Number(elements.lfoRateMultiplier.value);
    setLfoRateMultiplierLabel(nextValue);
    setParamTempLive("sources.robin.lfo.rateMultiplier", nextValue, () => {
      applyLfoUpdate("rateMultiplier", nextValue);
    });
  });

  elements.lfoFixedFrequency.addEventListener("input", () => {
    const nextValue = Number(elements.lfoFixedFrequency.value);
    setLfoFixedFrequencyLabel(nextValue);
    setParamTempLive("sources.robin.lfo.fixedFrequencyHz", nextValue, () => {
      applyLfoUpdate("fixedFrequencyHz", nextValue);
    });
  });

  elements.lfoPhaseSpread.addEventListener("input", () => {
    const nextValue = Number(elements.lfoPhaseSpread.value);
    setLfoPhaseSpreadLabel(nextValue);
    setParamTempLive("sources.robin.lfo.phaseSpreadDegrees", nextValue, () => {
      applyLfoUpdate("phaseSpreadDegrees", nextValue);
    });
  });

  elements.lfoPolarityFlip.addEventListener("change", () => {
    const nextValue = elements.lfoPolarityFlip.checked;
    setParamTemp("sources.robin.lfo.polarityFlip", nextValue, {
      applyLocalState: () => {
        applyLfoUpdate("polarityFlip", nextValue);
      },
    });
  });

  elements.lfoUnlinkedOutputs.addEventListener("change", () => {
    const nextValue = elements.lfoUnlinkedOutputs.checked;
    setParamTemp("sources.robin.lfo.unlinkedOutputs", nextValue, {
      applyLocalState: () => {
        applyLfoUpdate("unlinkedOutputs", nextValue);
      },
    });
  });

  elements.refreshButton.addEventListener("click", refreshState);

  refreshState();
});
