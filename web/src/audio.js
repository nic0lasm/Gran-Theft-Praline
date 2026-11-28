// Synthesized sound: no audio files, everything is built with WebAudio.

export class Audio {
  constructor() {
    this.ctx = null;
    this.master = null;
    this.volume = 0.6;
  }

  start() {
    if (this.ctx) { if (this.ctx.state === 'suspended') this.ctx.resume(); return; }
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return;
    this.ctx = new AC();
    this.master = this.ctx.createGain();
    this.master.gain.value = this.volume;
    this.master.connect(this.ctx.destination);
    // Noise buffer.
    const len = this.ctx.sampleRate * 1;
    this.noise = this.ctx.createBuffer(1, len, this.ctx.sampleRate);
    const d = this.noise.getChannelData(0);
    for (let i = 0; i < len; i++) d[i] = Math.random() * 2 - 1;
    // Engine: two detuned saws through a low-pass.
    this.engGain = this.ctx.createGain();
    this.engGain.gain.value = 0;
    this.engFilter = this.ctx.createBiquadFilter();
    this.engFilter.type = 'lowpass';
    this.engFilter.frequency.value = 600;
    this.engOsc = [this.ctx.createOscillator(), this.ctx.createOscillator()];
    this.engOsc[0].type = 'sawtooth'; this.engOsc[1].type = 'square';
    for (const o of this.engOsc) { o.frequency.value = 40; o.connect(this.engFilter); o.start(); }
    this.engFilter.connect(this.engGain).connect(this.master);
    // Siren: one oscillator, frequency driven per frame.
    this.sirenGain = this.ctx.createGain();
    this.sirenGain.gain.value = 0;
    this.sirenOsc = this.ctx.createOscillator();
    this.sirenOsc.type = 'triangle';
    this.sirenOsc.frequency.value = 600;
    this.sirenOsc.connect(this.sirenGain).connect(this.master);
    this.sirenOsc.start();
    // City ambience: filtered noise.
    const amb = this.ctx.createBufferSource();
    amb.buffer = this.noise; amb.loop = true;
    const af = this.ctx.createBiquadFilter(); af.type = 'lowpass'; af.frequency.value = 400;
    this.ambGain = this.ctx.createGain(); this.ambGain.gain.value = 0.05;
    amb.connect(af).connect(this.ambGain).connect(this.master);
    amb.start();
  }

  setVolume(v) { this.volume = v; if (this.master) this.master.gain.value = v; }

  engine(on, rpm01, load) {
    if (!this.ctx) return;
    const t = this.ctx.currentTime;
    this.engGain.gain.setTargetAtTime(on ? 0.06 + load * 0.05 : 0, t, 0.1);
    const f = 38 + rpm01 * 110;
    this.engOsc[0].frequency.setTargetAtTime(f, t, 0.05);
    this.engOsc[1].frequency.setTargetAtTime(f * 0.5, t, 0.05);
    this.engFilter.frequency.setTargetAtTime(300 + rpm01 * 1400, t, 0.05);
  }

  siren(level, time) {
    if (!this.ctx) return;
    const t = this.ctx.currentTime;
    this.sirenGain.gain.setTargetAtTime(level * 0.07, t, 0.15);
    // French two-tone (deux-tons): alternate every 0.6 s.
    this.sirenOsc.frequency.setTargetAtTime(Math.floor(time / 0.6) % 2 ? 435 : 580, t, 0.01);
  }

  noiseBurst(dur, freq, vol, q = 0.8, type = 'lowpass') {
    if (!this.ctx) return;
    const t = this.ctx.currentTime;
    const src = this.ctx.createBufferSource();
    src.buffer = this.noise;
    const f = this.ctx.createBiquadFilter(); f.type = type; f.frequency.value = freq; f.Q.value = q;
    const g = this.ctx.createGain();
    g.gain.setValueAtTime(vol, t);
    g.gain.exponentialRampToValueAtTime(0.001, t + dur);
    src.connect(f).connect(g).connect(this.master);
    src.start(t, Math.random() * 0.5);
    src.stop(t + dur + 0.05);
  }

  tone(freq, dur, vol = 0.1, type = 'sine', when = 0, slide = 0) {
    if (!this.ctx) return;
    const t = this.ctx.currentTime + when;
    const o = this.ctx.createOscillator(); o.type = type; o.frequency.setValueAtTime(freq, t);
    if (slide) o.frequency.exponentialRampToValueAtTime(Math.max(20, freq + slide), t + dur);
    const g = this.ctx.createGain();
    g.gain.setValueAtTime(vol, t);
    g.gain.exponentialRampToValueAtTime(0.001, t + dur);
    o.connect(g).connect(this.master);
    o.start(t); o.stop(t + dur + 0.05);
  }

  gunshot(kind, distK = 1) {
    const v = 0.5 * distK;
    if (kind === 'shotgun') { this.noiseBurst(0.35, 1200, v * 1.3); this.tone(70, 0.2, v * 0.5, 'sine', 0, -40); }
    else if (kind === 'smg') { this.noiseBurst(0.09, 2500, v * 0.7); }
    else { this.noiseBurst(0.18, 1800, v); this.tone(110, 0.1, v * 0.4, 'sine', 0, -60); }
  }
  punch() { this.noiseBurst(0.08, 300, 0.35); }
  crash(k) { this.noiseBurst(0.3, 500, Math.min(0.6, 0.1 + k * 0.03)); this.noiseBurst(0.15, 3000, Math.min(0.3, k * 0.015), 0.5, 'highpass'); }
  explosion() { this.noiseBurst(1.4, 200, 0.9); this.noiseBurst(0.5, 900, 0.5); }
  horn() { this.tone(392, 0.35, 0.08, 'square'); this.tone(330, 0.35, 0.06, 'square'); }
  pickup() { this.tone(880, 0.12, 0.08, 'triangle'); this.tone(1320, 0.18, 0.08, 'triangle', 0.08); }
  cash() { this.tone(1500, 0.06, 0.06, 'square'); this.tone(2000, 0.12, 0.05, 'square', 0.06); }
  door() { this.noiseBurst(0.12, 700, 0.25); }
  star() { this.tone(660, 0.15, 0.07, 'square'); this.tone(520, 0.2, 0.07, 'square', 0.12); }
  pass() { [523, 659, 784, 1046].forEach((f, i) => this.tone(f, 0.35, 0.09, 'triangle', i * 0.13)); }
  fail() { [392, 330, 262].forEach((f, i) => this.tone(f, 0.4, 0.09, 'sawtooth', i * 0.18)); }
  click() { this.tone(1000, 0.03, 0.04, 'square'); }
}
