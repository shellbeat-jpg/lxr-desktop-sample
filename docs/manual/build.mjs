// Builds docs/OE02-Samples_Manual_Supplement.pdf from docs/manual/supplement.html.
// Usage: node docs/manual/build.mjs [--check] [--src supplement.de.html] [--out NAME.pdf]
// Needs Node and Playwright with Chromium (npm i playwright; npx playwright install chromium).
// --check only lists columns whose content doesn't fit their page.
import { readFileSync, writeFileSync, unlinkSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { chromium } = require('playwright');
const here = dirname(fileURLToPath(import.meta.url));
const docs = join(here, '..');

// include the SVGs of docs/images (without their own style, the page draws them in black)
const arg = (n, d) => { const i = process.argv.indexOf(n); return i > 0 ? process.argv[i + 1] : d; };
const srcName = arg('--src', 'supplement.html');
const outName = arg('--out', srcName === 'supplement.html' ? 'OE02-Samples_Manual_Supplement.pdf'
	: 'OE02-Samples_Manual_Supplement.' + srcName.replace(/^supplement\.|\.html$/g, '') + '.pdf');
let html = readFileSync(join(here, srcName), 'utf8');
html = html.replace(/\{\{svg:([\w.-]+)\}\}/g, (_, f) =>
	readFileSync(join(docs, 'images', f), 'utf8')
		.replace(/<style>[\s\S]*?<\/style>/, '')
		.replace(/<title>[\s\S]*?<\/title>/, '')
		.replace(/ width="\d+" height="\d+"/, ''));
const tmp = join(here, '.' + srcName.replace(/\.html$/, '') + '.build.html');
writeFileSync(tmp, html);

const browser = await chromium.launch();
const page = await browser.newPage();
await page.goto('file://' + tmp);
await page.evaluate(() => document.fonts.ready);
const over = await page.evaluate(() => [...document.querySelectorAll('.col, .full')]
	.filter(c => c.scrollHeight > c.clientHeight + 1)
	.map(c => `page ${c.closest('section').dataset.n}: ${c.scrollHeight - c.clientHeight}px too high`));
if (over.length) console.log('Overflow:\n' + over.join('\n'));
if (!process.argv.includes('--check')) {
	await page.pdf({ path: join(docs, outName), width: '200mm', height: '128mm',
		printBackground: true, preferCSSPageSize: true });
	console.log('written docs/' + outName);
}
await browser.close();
unlinkSync(tmp);
