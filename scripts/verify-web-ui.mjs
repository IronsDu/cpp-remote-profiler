#!/usr/bin/env node
// Drive the real control panel in headless Chrome and assert that each action
// actually does something.
//
// Why this exists: several front-end defects in this project were reported as
// "the button does nothing" *after* having been declared fixed on the strength of
// curl requests against the server. curl cannot execute page JavaScript, so it
// cannot see an undefined function, a button stuck disabled by a synchronous
// throw, or a download that never starts. This script checks the browser-side
// behaviour directly:
//
//   - the page loads with no JavaScript errors
//   - every action issues the request it should, to the path it should
//   - downloads leave a file on disk
//   - buttons return to their normal state afterwards
//
// Usage:
//   scripts/verify-web-ui.mjs [base-url]
//
// Requires: a running profiler_example server and a local Chrome/Chromium plus
// puppeteer-core. If puppeteer-core is not resolvable, the script explains how to
// get it and exits 2 rather than silently skipping -- a skipped check here is
// what let the bugs through before.

import fs from 'node:fs';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import { createRequire } from 'node:module';

const BASE = (process.argv[2] || 'http://localhost:8080').replace(/\/$/, '');
const DOWNLOAD_DIR = fs.mkdtempSync(path.join(os.tmpdir(), 'verify-web-ui-'));

// Resolve puppeteer-core from the usual places without requiring an install in
// this repository.
function loadPuppeteer() {
    const require = createRequire(import.meta.url);
    const candidates = ['puppeteer-core', 'puppeteer'];
    for (const name of candidates) {
        try {
            return require(name);
        } catch {
            /* try the next */
        }
    }
    console.error(
        'puppeteer-core is required.\n' +
            '  npm install puppeteer-core            # in this repo, or\n' +
            '  npm install --prefix /tmp puppeteer-core\n' +
            'and set NODE_PATH if it lives outside this tree.',
    );
    process.exit(2);
}

function findChrome() {
    const candidates = [
        process.env.CHROME_PATH,
        '/usr/bin/google-chrome',
        '/usr/bin/google-chrome-stable',
        '/usr/bin/chromium',
        '/usr/bin/chromium-browser',
        '/snap/bin/chromium',
        '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
    ].filter(Boolean);
    for (const c of candidates) {
        if (fs.existsSync(c)) return c;
    }
    console.error('No Chrome/Chromium found. Set CHROME_PATH to its executable.');
    process.exit(2);
}

async function serverReachable() {
    return new Promise(resolve => {
        const req = http.get(`${BASE}/api/status`, res => {
            res.resume();
            resolve(res.statusCode === 200);
        });
        req.on('error', () => resolve(false));
        req.setTimeout(5000, () => {
            req.destroy();
            resolve(false);
        });
    });
}

const puppeteer = loadPuppeteer();

if (!(await serverReachable())) {
    console.error(`No profiler server answering at ${BASE}/api/status.\n` +
                  'Start one first, e.g.  TCMALLOC_SAMPLE_PARAMETER=524288 ./build/profiler_example');
    process.exit(2);
}

const results = [];
function record(name, ok, detail = '') {
    results.push({ name, ok, detail });
    console.log(`  ${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? `  -- ${detail}` : ''}`);
}

const browser = await puppeteer.launch({
    executablePath: findChrome(),
    headless: true,
    protocolTimeout: 180000,
    args: ['--no-sandbox', '--disable-dev-shm-usage'],
});

try {
    const page = await browser.newPage();
    const cdp = await page.createCDPSession();
    await cdp.send('Browser.setDownloadBehavior', {
        behavior: 'allow',
        downloadPath: DOWNLOAD_DIR,
        eventsEnabled: true,
    });

    const pageErrors = [];
    const requests = [];
    page.on('pageerror', e => pageErrors.push(e.message));
    page.on('request', r => {
        const u = r.url();
        if (u.includes('/api/')) requests.push(u);
    });

    console.log(`\nLoading ${BASE}/`);
    await page.goto(`${BASE}/`, { waitUntil: 'networkidle2', timeout: 30000 });

    record('page loads without JavaScript errors', pageErrors.length === 0, pageErrors.join(' | '));

    // The panel must not reference endpoints that no longer exist.
    const staleRefs = await page.evaluate(() => {
        const html = document.documentElement.innerHTML;
        const stale = [];
        for (const p of ['/api/cpu/', '/api/heap/', '/api/growth/', 'output_type',
                         'show_svg.html', 'show_heap_svg.html', 'show_growth_svg.html']) {
            if (html.includes(p)) stale.push(p);
        }
        return stale;
    });
    record('panel references no removed endpoints or parameters',
           staleRefs.length === 0, staleRefs.join(', '));

    const openButtons = await page.evaluate(() =>
        document.querySelectorAll('button[onclick^="openChart"], button[onclick^="openSnapshot"]').length);
    record('panel exposes the inline (open in place) actions', openButtons === 4, `found ${openButtons}`);

    const controls = await page.evaluate(() => ({
        cpuDuration: !!document.getElementById('cpu-duration'),
        cpuRenderer: !!document.getElementById('cpu-renderer'),
        heapDuration: !!document.getElementById('heap-duration'),
        heapRenderer: !!document.getElementById('heap-renderer'),
        growthRenderer: !!document.getElementById('growth-renderer'),
        snapshotButtons: !!document.getElementById('snapshot-profile-btn') &&
                         !!document.getElementById('snapshot-svg-btn'),
    }));
    record('all chart controls present', Object.values(controls).every(Boolean),
           JSON.stringify(controls));

    async function waitForFileCount(before, timeoutMs = 60000) {
        const deadline = Date.now() + timeoutMs;
        while (Date.now() < deadline) {
            const files = fs.readdirSync(DOWNLOAD_DIR).filter(f => !f.endsWith('.crdownload'));
            if (files.length > before) return files;
            await new Promise(r => setTimeout(r, 250));
        }
        return fs.readdirSync(DOWNLOAD_DIR).filter(f => !f.endsWith('.crdownload'));
    }

    async function checkDownload(name, clickExpr) {
        const before = fs.readdirSync(DOWNLOAD_DIR).filter(f => !f.endsWith('.crdownload')).length;
        requests.length = 0;
        await page.evaluate(clickExpr);
        const files = await waitForFileCount(before);
        const saved = files.length > before;
        const disabled = await page.$$eval('button', bs =>
            bs.filter(b => b.disabled).map(b => b.textContent.trim()));
        const logText = await page.$eval('#output', el => el.textContent.replace(/\s+/g, ' ').slice(0, 90));
        record(`${name}: request issued`, requests.length > 0, requests.join(', '));
        record(`${name}: file saved`, saved, `files=${files.length}`);
        record(`${name}: no button left disabled`, disabled.length === 0, disabled.join(' | '));
        if (!saved) console.log(`        log panel: ${logText}`);
    }

    console.log('\nCPU chart (flamegraph)');
    await checkDownload('cpu chart', () => downloadChart('cpu'));

    console.log('\nCPU chart (callgraph)');
    await page.select('#cpu-renderer', 'callgraph');
    await checkDownload('cpu callgraph', () => downloadChart('cpu'));

    console.log('\nHeap chart');
    await checkDownload('heap chart', () => downloadChart('heap'));

    console.log('\nHeap growth chart');
    await checkDownload('growth chart', () => downloadChart('growth'));

    console.log('\nHeap snapshot (raw profile)');
    await checkDownload('snapshot profile', () => downloadSnapshot('profile'));

    console.log('\nHeap snapshot (rendered SVG)');
    await checkDownload('snapshot svg', () => downloadSnapshot('svg'));

    // The delivery modes must actually differ in the response, not just in the
    // query string: `inline` omits Content-Disposition so the browser renders the
    // SVG as a document, `attachment` sets it so the browser downloads.
    console.log('\nDelivery modes');
    const modes = await page.evaluate(async () => {
        const out = {};
        for (const mode of ['inline', 'attachment']) {
            const r = await fetch(`/api/pprof/cpu?duration=10&renderer=flamegraph&output=${mode}`);
            out[mode] = {
                status: r.status,
                contentType: r.headers.get('content-type') || '',
                disposition: r.headers.get('content-disposition') || '',
                bytes: (await r.blob()).size,
            };
        }
        return out;
    });
    record('inline returns SVG with no Content-Disposition',
           modes.inline.status === 200 &&
               modes.inline.contentType.includes('image/svg+xml') &&
               modes.inline.disposition === '',
           JSON.stringify(modes.inline));
    record('attachment sets Content-Disposition',
           modes.attachment.status === 200 && modes.attachment.disposition.startsWith('attachment'),
           JSON.stringify(modes.attachment));
    record('both modes return the same kind of payload',
           modes.inline.bytes > 1000 && modes.attachment.bytes > 1000,
           `inline=${modes.inline.bytes}B attachment=${modes.attachment.bytes}B`);

    record('no JavaScript errors during the run', pageErrors.length === 0, pageErrors.join(' | '));

    const savedFiles = fs.readdirSync(DOWNLOAD_DIR);
    console.log(`\nFiles saved to ${DOWNLOAD_DIR}:`);
    for (const f of savedFiles) {
        console.log(`  ${f}  (${fs.statSync(path.join(DOWNLOAD_DIR, f)).size} bytes)`);
    }
    const emptyFiles = savedFiles.filter(f => fs.statSync(path.join(DOWNLOAD_DIR, f)).size === 0);
    record('every saved file is non-empty', emptyFiles.length === 0, emptyFiles.join(', '));
} finally {
    await browser.close();
}

const failed = results.filter(r => !r.ok);
console.log(`\n${results.length - failed.length}/${results.length} checks passed`);
if (failed.length) {
    console.log('Failed checks:');
    for (const f of failed) console.log(`  - ${f.name}${f.detail ? `: ${f.detail}` : ''}`);
    process.exit(1);
}
