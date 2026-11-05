// Takes the text of an edited review page (review-export.mjs) back into supplement.html.
// Usage: node docs/manual/review-import.mjs REVIEWED.html [supplement.de.html]
// Replaces the pages of supplement.html (between <body> and its script) with those of
// the review page; the inlined diagrams become their placeholders again.
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const inFile = process.argv[2];
if (!inFile) { console.error('usage: node review-import.mjs REVIEWED.html'); process.exit(1); }

const page = readFileSync(inFile, 'utf8');
const m = page.match(/<main id="doc"[^>]*>([\s\S]*)<\/main>/);
if (!m) { console.error('no <main id="doc"> in ' + inFile); process.exit(1); }
const doc = m[1]
	.replace(/<svg data-src="([\w.-]+)"[\s\S]*?<\/svg>/g, (_, f) => `{{svg:${f}}}`)
	.replace(/ (data-[\w-]+)=""/g, ' $1')
	.replace(/&nbsp;/g, ' ')
	// undo what the browser's serializer adds: <tbody>, closing tags of empty SVG shapes
	.replace(/<\/?tbody>/g, '')
	.replace(/<(path|rect|line|circle|ellipse|polyline|polygon|use|stop)\b([^>]*)><\/\1>/g, '<$1$2/>');

const p = join(here, process.argv[3] || 'supplement.html');
const src = readFileSync(p, 'utf8');
const a = src.indexOf('<body>') + 6, b = src.lastIndexOf('<script>');
writeFileSync(p, src.slice(0, a) + doc + src.slice(b));
console.log('updated ' + p + '; check: git diff docs/manual/supplement.html, then node docs/manual/build.mjs');
