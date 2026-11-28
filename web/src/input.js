// Keyboard (physical keys: WASD on QWERTY = ZQSD on AZERTY), mouse with
// pointer lock, and a standard gamepad.

export class Input {
  constructor(el) {
    this.el = el;
    this.down = new Set();
    this.pressed = new Set();
    this.mouseDX = 0;
    this.mouseDY = 0;
    this.buttons = 0;
    this.clicked = 0;
    this.wheel = 0;
    this.locked = false;
    this.pad = null;
    this.padPrev = [];
    this.sensitivity = 1;
    addEventListener('keydown', (e) => {
      if (e.repeat) return;
      this.down.add(e.code);
      this.pressed.add(e.code);
      if (['Space', 'Tab', 'ArrowUp', 'ArrowDown'].includes(e.code)) e.preventDefault();
    });
    addEventListener('keyup', (e) => this.down.delete(e.code));
    addEventListener('blur', () => { this.down.clear(); this.buttons = 0; });
    el.addEventListener('mousedown', (e) => {
      this.buttons |= 1 << e.button;
      this.clicked |= 1 << e.button;
    });
    addEventListener('mouseup', (e) => { this.buttons &= ~(1 << e.button); });
    addEventListener('mousemove', (e) => {
      if (!this.locked) return;
      this.mouseDX += e.movementX;
      this.mouseDY += e.movementY;
    });
    addEventListener('wheel', (e) => { this.wheel += Math.sign(e.deltaY); }, { passive: true });
    el.addEventListener('contextmenu', (e) => e.preventDefault());
    document.addEventListener('pointerlockchange', () => { this.locked = document.pointerLockElement === el; });
  }

  lock() {
    if (!this.locked && this.el.requestPointerLock) {
      try { const p = this.el.requestPointerLock(); if (p && p.catch) p.catch(() => {}); } catch (e) { /* ignore */ }
    }
  }
  unlock() { if (document.exitPointerLock) document.exitPointerLock(); }

  key(code) { return this.down.has(code); }
  hit(code) { return this.pressed.has(code); }

  pollPad() {
    const pads = navigator.getGamepads ? navigator.getGamepads() : [];
    this.pad = null;
    for (const p of pads) if (p && p.connected) { this.pad = p; break; }
    if (!this.pad) return;
    const b = this.pad.buttons.map((x) => x.pressed);
    const map = { 0: 'Space', 1: 'KeyF', 2: 'KeyE', 3: 'KeyF', 9: 'Escape', 4: 'KeyQwheel', 5: 'KeyEwheel', 8: 'KeyM', 10: 'ShiftLeft' };
    for (const [i, code] of Object.entries(map)) {
      if (b[i] && !this.padPrev[i]) this.pressed.add(code);
      if (b[i]) this.down.add('pad:' + code); else this.down.delete('pad:' + code);
    }
    if (b[4] && !this.padPrev[4]) this.wheel -= 1;
    if (b[5] && !this.padPrev[5]) this.wheel += 1;
    this.padPrev = b;
  }

  // Movement axes (-1..1).
  axis() {
    let x = 0, y = 0;
    if (this.key('KeyW') || this.key('ArrowUp')) y += 1;
    if (this.key('KeyS') || this.key('ArrowDown')) y -= 1;
    if (this.key('KeyA') || this.key('ArrowLeft')) x -= 1;
    if (this.key('KeyD') || this.key('ArrowRight')) x += 1;
    if (this.pad) {
      const ax = this.pad.axes;
      if (Math.abs(ax[0]) > 0.15) x += ax[0];
      if (Math.abs(ax[1]) > 0.15) y -= ax[1];
    }
    return { x: Math.max(-1, Math.min(1, x)), y: Math.max(-1, Math.min(1, y)) };
  }
  look() {
    let dx = this.mouseDX * 0.0024 * this.sensitivity, dy = this.mouseDY * 0.0024 * this.sensitivity;
    if (this.pad) {
      const ax = this.pad.axes;
      if (Math.abs(ax[2]) > 0.15) dx += ax[2] * 0.05;
      if (Math.abs(ax[3]) > 0.15) dy += ax[3] * 0.04;
    }
    return { dx, dy };
  }
  // Car controls.
  throttle() {
    let t = 0;
    if (this.key('KeyW') || this.key('ArrowUp')) t += 1;
    if (this.key('KeyS') || this.key('ArrowDown')) t -= 1;
    if (this.pad) t += (this.pad.buttons[7]?.value || 0) - (this.pad.buttons[6]?.value || 0);
    return Math.max(-1, Math.min(1, t));
  }
  steer() {
    let s = 0;
    if (this.key('KeyA') || this.key('ArrowLeft')) s += 1;
    if (this.key('KeyD') || this.key('ArrowRight')) s -= 1;
    if (this.pad && Math.abs(this.pad.axes[0]) > 0.12) s -= this.pad.axes[0];
    return Math.max(-1, Math.min(1, s));
  }
  aiming() { return (this.buttons & 4) !== 0 || (this.pad && this.pad.buttons[6]?.pressed && !this.inCar); }
  firing() { return (this.buttons & 1) !== 0 || (this.pad && this.pad.buttons[7]?.pressed && !this.inCar); }
  sprint() { return this.key('ShiftLeft') || this.key('ShiftRight') || this.key('pad:ShiftLeft'); }
  action(code) { return this.hit(code); }

  endFrame() {
    this.pressed.clear();
    this.mouseDX = this.mouseDY = 0;
    this.clicked = 0;
    this.wheel = 0;
  }
}
