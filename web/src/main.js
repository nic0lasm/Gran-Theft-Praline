import './style.css';
import { Game } from './game.js';

const loading = document.getElementById('loading-text');
const game = new Game(document.getElementById('app'));
window.__gtp = game; // handy for debugging from the console

game.init((msg) => { loading.textContent = msg; }).then(() => {
  document.getElementById('loading').style.display = 'none';
  const title = document.getElementById('title');
  title.style.display = 'flex';
  const cont = document.getElementById('btn-continue');
  cont.style.display = game.hasSave() ? 'block' : 'none';
  const start = (load) => {
    title.style.display = 'none';
    document.getElementById('hud').style.display = 'block';
    game.newGame(load);
  };
  document.getElementById('btn-new').onclick = () => start(false);
  cont.onclick = () => start(true);
  document.getElementById('btn-resume').onclick = () => game.setPaused(false);
  document.getElementById('btn-save').onclick = () => {
    game.hud.toast('Sauvegarde : allez à la planque (marqueur vert).');
    game.setPaused(false);
  };
  const vol = document.getElementById('vol');
  vol.oninput = () => game.audio.setVolume(vol.value / 100);
  const sens = document.getElementById('sens');
  sens.oninput = () => { game.input.sensitivity = sens.value / 50; };
  window.__ready = true;
}).catch((e) => {
  loading.textContent = 'Erreur de chargement : ' + e.message;
  console.error(e);
});
