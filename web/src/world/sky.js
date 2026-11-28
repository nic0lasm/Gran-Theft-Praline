// Day and night over Lyon: sun path at 45.76° N, sky gradient, fog, and the
// lights that follow the player so shadows stay sharp where it matters.

import * as THREE from 'three';
import { clamp, lerp } from '../util.js';

const LAT = (45.76 * Math.PI) / 180;

export class Sky {
  constructor(scene) {
    this.scene = scene;
    this.time = 10.5; // hours
    this.dayLength = 18 * 60; // real seconds for 24 game hours
    this.sun = new THREE.DirectionalLight(0xffffff, 3);
    this.sun.castShadow = true;
    this.sun.shadow.mapSize.set(2048, 2048);
    const sc = this.sun.shadow.camera;
    sc.left = -70; sc.right = 70; sc.top = 70; sc.bottom = -70;
    sc.near = 1; sc.far = 400;
    this.sun.shadow.bias = -0.0004;
    this.sun.shadow.normalBias = 0.04;
    scene.add(this.sun, this.sun.target);
    this.hemi = new THREE.HemisphereLight(0xbcd4ff, 0x8a7a66, 1.1);
    scene.add(this.hemi);
    this.fog = new THREE.Fog(0xc8d6e6, 120, 900);
    scene.fog = this.fog;

    this.uniforms = {
      top: { value: new THREE.Color() },
      horizon: { value: new THREE.Color() },
      sunDir: { value: new THREE.Vector3() },
      sunColor: { value: new THREE.Color() },
      night: { value: 0 },
    };
    const mat = new THREE.ShaderMaterial({
      uniforms: this.uniforms,
      side: THREE.BackSide,
      depthWrite: false,
      fog: false,
      vertexShader: `varying vec3 vDir; void main(){ vDir = normalize(position); gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); gl_Position.z = gl_Position.w; }`,
      fragmentShader: `uniform vec3 top; uniform vec3 horizon; uniform vec3 sunDir; uniform vec3 sunColor; uniform float night; varying vec3 vDir;
        float hash(vec3 p){ p = fract(p*0.3183099+.1); p*=17.0; return fract(p.x*p.y*p.z*(p.x+p.y+p.z)); }
        void main(){
          vec3 d = normalize(vDir);
          float h = clamp(d.y, -0.2, 1.0);
          vec3 col = mix(horizon, top, pow(max(h,0.0), 0.55));
          float s = max(dot(d, sunDir), 0.0);
          col += sunColor * (pow(s, 900.0) * 6.0 + pow(s, 12.0) * 0.25);
          if (night > 0.5) {
            float st = hash(floor(d*400.0));
            col += vec3(step(0.9975, st)) * (night-0.5) * 1.6 * smoothstep(0.05,0.3,d.y);
          }
          gl_FragColor = vec4(col, 1.0);
        }`,
    });
    this.dome = new THREE.Mesh(new THREE.SphereGeometry(1500, 32, 16), mat);
    this.dome.renderOrder = -1;
    scene.add(this.dome);
    this.moon = new THREE.DirectionalLight(0x8899cc, 0);
    scene.add(this.moon, this.moon.target);
    this.night = 0;
  }

  get hours() { return this.time; }
  label() {
    const h = Math.floor(this.time), m = Math.floor((this.time - h) * 60);
    return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`;
  }

  update(dt, focus) {
    this.time = (this.time + (dt * 24) / this.dayLength) % 24;
    // Simple solar model for late spring.
    const decl = (18 * Math.PI) / 180;
    const ha = ((this.time - 12) / 12) * Math.PI;
    const alt = Math.asin(Math.sin(LAT) * Math.sin(decl) + Math.cos(LAT) * Math.cos(decl) * Math.cos(ha));
    let az = Math.atan2(Math.sin(ha), Math.cos(ha) * Math.sin(LAT) - Math.tan(decl) * Math.cos(LAT));
    // az: 0 = south, positive west. Scene: +X east, +Z south.
    const dir = new THREE.Vector3(-Math.sin(az) * Math.cos(alt), Math.sin(alt), Math.cos(az) * Math.cos(alt));
    const day = clamp((Math.sin(alt) + 0.08) / 0.3, 0, 1);
    const golden = clamp(1 - Math.abs(Math.sin(alt) - 0.08) / 0.18, 0, 1);
    this.night = 1 - day;

    const sunCol = new THREE.Color(1, 0.96, 0.9).lerp(new THREE.Color(1, 0.55, 0.28), golden);
    this.sun.color.copy(sunCol);
    this.sun.intensity = 2.6 * day;
    const fx = focus.x, fz = focus.z;
    this.sun.position.set(fx + dir.x * 200, Math.max(dir.y, 0.05) * 200, fz + dir.z * 200);
    this.sun.target.position.set(fx, 0, fz);
    this.sun.castShadow = day > 0.05;

    this.moon.intensity = 0.45 * this.night;
    this.moon.position.set(fx - 80, 160, fz + 60);
    this.moon.target.position.set(fx, 0, fz);

    const dayTop = new THREE.Color(0x4f86c9), dayHor = new THREE.Color(0xcfe0ee);
    const dusk = new THREE.Color(0xf29a62), nightTop = new THREE.Color(0x070b18), nightHor = new THREE.Color(0x26222e);
    const top = nightTop.clone().lerp(dayTop, day);
    const hor = nightHor.clone().lerp(dayHor, day).lerp(dusk, golden * 0.7);
    this.uniforms.top.value.copy(top);
    this.uniforms.horizon.value.copy(hor);
    this.uniforms.sunDir.value.copy(dir);
    this.uniforms.sunColor.value.copy(sunCol).multiplyScalar(day);
    this.uniforms.night.value = this.night;
    this.dome.position.set(fx, 0, fz);

    this.hemi.intensity = lerp(0.35, 1.15, day);
    this.hemi.color.copy(new THREE.Color(0x3a4a78).lerp(new THREE.Color(0xbcd4ff), day));
    this.hemi.groundColor.copy(new THREE.Color(0x2a2420).lerp(new THREE.Color(0x8a7a66), day));
    this.fog.color.copy(hor);
    this.fog.near = lerp(60, 140, day);
    this.fog.far = lerp(420, 950, day);
    return { day, night: this.night, golden };
  }
}
