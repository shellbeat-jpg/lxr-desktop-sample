// Builds an editable review page of the manual supplement (for a claude.ai artifact
// with the "artifact" capability: the reviewer edits the text in place and saves).
// Usage: node docs/manual/review-export.mjs OUT.html [supplement.de.html]
// The page keeps supplement.html's markup inside <main id="doc">; review-import.mjs
// puts the edited text back into supplement.html.
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const out = process.argv[2];
if (!out) { console.error('usage: node review-export.mjs OUT.html'); process.exit(1); }

const src = readFileSync(join(here, process.argv[3] || 'supplement.html'), 'utf8');
let style = src.match(/<style>([\s\S]*?)<\/style>/)[1];
// fonts as data URIs (the artifact serves no font files)
style = style.replace(/url\((fonts\/[\w.-]+\.woff2)\)/g, (_, f) =>
	'url(data:font/woff2;base64,' + readFileSync(join(here, f)).toString('base64') + ')');
const de = /<html lang="de"/.test(src);
const T = de ? {
	title: 'Ergänzung Review', h1: 'OE02-Samples+ Ergänzung',
	hint: 'In einen Text klicken, ändern, dann speichern. Rot markierte Seiten sind zu voll für das PDF.',
	none: 'Keine Änderungen', save: 'Speichern', dirty: 'Ungespeicherte Änderungen', saving: 'Speichert …', saved: 'Gespeichert',
	conflict: 'Jemand anderes hat zuerst gespeichert: die Seite lädt dessen Fassung', readonly: 'Nur lesen: Speichern nicht möglich',
	failed: 'Nicht gespeichert – bitte erneut versuchen', readonlyHere: 'Hier nur lesen', over: 'Zu voll: Seite ', badge: 'Text zu lang für diese Seite'
} : {
	title: 'Supplement Review', h1: 'OE02-Samples+ Supplement',
	hint: 'Click into any text to change it, then save. Pages marked red are too full for the PDF.',
	none: 'No changes', save: 'Save', dirty: 'Unsaved changes', saving: 'Saving…', saved: 'Saved',
	conflict: 'Someone else saved first: the page reloads with their version', readonly: 'Read only: you can’t save this page',
	failed: 'Not saved – try again', readonlyHere: 'Read only here', over: 'Too full: page ', badge: 'text too long for this page'
};
const body = src.slice(src.indexOf('<body>') + 6, src.lastIndexOf('<script>'));
const docScript = src.slice(src.lastIndexOf('<script>') + 8, src.lastIndexOf('</script>'));
// the diagrams inline, marked with their file name for the import
const doc = body.replace(/\{\{svg:([\w.-]+)\}\}/g, (_, f) =>
	readFileSync(join(here, '..', 'images', f), 'utf8')
		.replace(/<\?xml[^>]*>\s*/, '')
		.replace(/<style>[\s\S]*?<\/style>/, '')
		.replace(/<title>[\s\S]*?<\/title>/, '')
		.replace(/ width="\d+" height="\d+"/, '')
		.replace('<svg ', `<svg data-src="${f}" `).trim());

const ui = `
<title>${T.title}</title>
<meta name="claude:viewport-fit-sides" content="cover">
<style>${style}</style>
<style>
/* review frame around the paper pages: desk, toolbar, overflow marks */
:root { --desk: #e4e6ea; --bar: #ffffff; --bar-fg: #1f2329; --bar-mute: #5d6570; --line: #c9ced6; --act: #1f5fbf; --act-fg: #ffffff; --warn: #c2281d; --ok: #2b7a3d; }
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { --desk: #16191e; --bar: #22262d; --bar-fg: #e9ecf1; --bar-mute: #a2aab6; --line: #3a404a; --act: #5b9cf5; --act-fg: #0d1117; --warn: #ff6b5e; --ok: #6fcf86; color-scheme: dark; } }
:root[data-theme="dark"] { --desk: #16191e; --bar: #22262d; --bar-fg: #e9ecf1; --bar-mute: #a2aab6; --line: #3a404a; --act: #5b9cf5; --act-fg: #0d1117; --warn: #ff6b5e; --ok: #6fcf86; color-scheme: dark; }
html, body { background: var(--desk); }
body::before { content: ""; position: fixed; inset: 0 0 auto; height: env(safe-area-inset-top, 0px); z-index: 10; background: var(--bar); }
#bar { position: sticky; top: env(safe-area-inset-top, 0px); z-index: 5; background: var(--bar); color: var(--bar-fg); border-bottom: 1px solid var(--line);
  font: 14px/1.4 system-ui, -apple-system, "Segoe UI", sans-serif; padding-block: 10px;
  padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px));
  display: flex; flex-wrap: wrap; gap: 8px 16px; align-items: center; }
#bar h1 { font: 600 15px/1.3 system-ui, -apple-system, "Segoe UI", sans-serif; text-transform: none; margin: 0; padding: 0; border: 0; letter-spacing: .01em; color: var(--bar-fg); }
#bar .hint { color: var(--bar-mute); flex: 1 1 260px; min-width: 0; }
#bar .state { font-variant-numeric: tabular-nums; color: var(--bar-mute); }
#bar .state.dirty { color: var(--warn); }
#bar .state.saved { color: var(--ok); }
#bar .over { color: var(--warn); }
#bar .over a { color: inherit; }
#save { font: inherit; font-weight: 600; border: 0; border-radius: 6px; padding: 7px 16px; background: var(--act); color: var(--act-fg); cursor: pointer; }
#save:disabled { opacity: .45; cursor: default; }
#save:focus-visible { outline: 2px solid var(--bar-fg); outline-offset: 2px; }
#desk { padding-block: 24px 48px; padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); overflow-x: hidden; }
#doc { width: 200mm; margin: 0 auto; display: flex; flex-direction: column; gap: 10mm; }
#doc .page { box-shadow: 0 1px 2px rgba(0,0,0,.18), 0 4px 18px rgba(0,0,0,.12); }
#doc [contenteditable]:focus { outline: 1px dashed #1f5fbf; outline-offset: 1px; }
#doc [contenteditable]:hover { background: rgba(31,95,191,.05); }
#doc .overflowing { box-shadow: inset 0 -3px 0 #c2281d; }
#doc .page.has-over::after { content: "${T.badge}"; position: absolute; top: 3mm; right: 4mm; font: 600 7pt system-ui, sans-serif; color: #fff; background: #c2281d; padding: .6mm 2mm; border-radius: 1mm; }
@media (prefers-reduced-motion: no-preference) { #bar .state { transition: color .2s; } }
</style>
<header id="bar">
  <h1>${T.h1}</h1>
  <span class="hint">${T.hint}</span>
  <span class="over" id="over"></span>
  <span class="state" id="state">${T.none}</span>
  <button id="save" type="button" disabled>${T.save}</button>
</header>
<div id="desk"><main id="doc">${doc}</main></div>
`;

const script = `<script>
(function () {
  // what the saved page is made of, taken before anything below changes the page
  const head = document.head.outerHTML;
  const pre = [];
  for (const el of document.body.children) { if (el.id === 'desk') break; pre.push(el.outerHTML); }
  const post = document.currentScript.outerHTML;
  const docEl = document.getElementById('doc');

  // page numbers, footers and contents as in the PDF (generated, not saved)
  ${docScript}
  docEl.querySelectorAll('section.page > footer, #toc > div').forEach(e => e.setAttribute('data-gen', ''));

  // the text is editable, the diagrams are not
  const editable = 'p, li, td, th, h1, h2, h3, figcaption, .note, .box';
  docEl.querySelectorAll(editable).forEach(e => {
    if (e.closest('svg, [data-gen], #toc') || e.querySelector(editable)) return;
    e.setAttribute('contenteditable', 'true');
    e.setAttribute('spellcheck', 'true');
  });

  // pages scale to the window
  const pageW = docEl.querySelector('.page').getBoundingClientRect().width;
  function fit() { const w = document.getElementById('desk').clientWidth - 32; docEl.style.zoom = Math.min(1.35, w / pageW).toFixed(3); }
  fit(); addEventListener('resize', fit);

  // overflow: the PDF build cuts what doesn't fit
  const overEl = document.getElementById('over');
  function checkOver() {
    let pages = [];
    docEl.querySelectorAll('section.page').forEach(p => {
      let over = false;
      p.querySelectorAll('.col, .full').forEach(c => { const o = c.scrollHeight > c.clientHeight + 1; c.classList.toggle('overflowing', o); over = over || o; });
      p.classList.toggle('has-over', over);
      if (over) pages.push(p.dataset.n);
    });
    overEl.textContent = pages.length ? ${JSON.stringify(T.over)} + pages.join(', ') : '';
  }
  document.fonts.ready.then(checkOver);

  const stateEl = document.getElementById('state'), saveBtn = document.getElementById('save');
  let dirty = false, artifact = null, timer = 0;
  function setState(t, cls) { stateEl.textContent = t; stateEl.className = 'state ' + (cls || ''); }
  docEl.addEventListener('input', () => {
    dirty = true; setState(${JSON.stringify(T.dirty)}, 'dirty'); saveBtn.disabled = !artifact;
    clearTimeout(timer); timer = setTimeout(checkOver, 300);
  });
  // plain text only when pasting
  docEl.addEventListener('paste', e => {
    if (!e.target.closest('[contenteditable]')) return;
    e.preventDefault();
    document.execCommand('insertText', false, (e.clipboardData || window.clipboardData).getData('text/plain'));
  });

  function savedDocument() {
    const c = docEl.cloneNode(true);
    c.removeAttribute('style');
    c.querySelectorAll('[data-gen]').forEach(e => e.remove());
    c.querySelectorAll('[contenteditable]').forEach(e => { e.removeAttribute('contenteditable'); e.removeAttribute('spellcheck'); });
    c.querySelectorAll('.overflowing').forEach(e => e.classList.remove('overflowing'));
    c.querySelectorAll('section.page').forEach(p => { p.classList.remove('even', 'has-over'); p.removeAttribute('data-n'); });
    c.querySelectorAll('[class=""]').forEach(e => e.removeAttribute('class'));
    return '<!doctype html><html>' + head + '<body>' + pre.join('') + '<div id="desk">' + c.outerHTML + '</div>' + post + '</body></html>';
  }
  saveBtn.addEventListener('click', async () => {
    if (!artifact || !dirty) return;
    saveBtn.disabled = true; setState(${JSON.stringify(T.saving)});
    try { await artifact.publish(savedDocument()); dirty = false; setState(${JSON.stringify(T.saved)}, 'saved'); }
    catch (e) {
      const code = e && e.code;
      if (code === 'conflict') setState(${JSON.stringify(T.conflict)}, 'dirty');
      else if (code === 'not_granted' || code === 'not_writer') setState(${JSON.stringify(T.readonly)}, 'dirty');
      else { setState(${JSON.stringify(T.failed)}, 'dirty'); saveBtn.disabled = false; }
    }
  });
  addEventListener('beforeunload', e => { if (dirty) { e.preventDefault(); e.returnValue = ''; } });
  window.claude && window.claude.use && window.claude.use('artifact').then(a => {
    artifact = a;
    if (!a) setState(${JSON.stringify(T.readonlyHere)});
    else if (dirty) saveBtn.disabled = false;
  });
})();
</script>`;

writeFileSync(out, ui + script + '\n');
console.log('written ' + out);
