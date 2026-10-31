const assert = require('node:assert/strict');
const { test } = require('node:test');
const fs = require('node:fs');
const vm = require('node:vm');
const { webcrypto } = require('node:crypto');

const context = vm.createContext({ crypto: webcrypto });
vm.runInContext(fs.readFileSync('src/www/functions.js', 'utf8'), context);
const asset = name => ({ name, content_type: 'application/octet-stream' });
const rev1 = asset('homekit-grgdo1-rev1-v1.4.0.firmware.bin');
const rev2 = asset('homekit-grgdo1-rev2-v1.4.0.firmware.bin');
const legacy = asset('homekit-grgdo1-v1.2.0.firmware.bin');

test('OTA selects the matching revision before a legacy image or checksum', () => {
    const assets = [legacy, asset(rev1.name.replace('.bin', '.md5')), rev2, rev1];
    assert.equal(context.selectFirmwareAsset(assets, 'rev1'), rev1);
    assert.equal(context.selectFirmwareAsset(assets, 'rev2'), rev2);
});

test('OTA rejects the wrong revision and non-firmware assets', () => {
    assert.equal(context.selectFirmwareAsset([rev2], 'rev1'), undefined);
    assert.equal(context.selectFirmwareAsset([rev1], 'rev3'), undefined);
    assert.equal(context.selectFirmwareAsset([
        asset('homekit-grgdo1-rev1-v1.4.0.bootloader.bin'),
        asset('homekit-grgdo1-rev1-v1.4.0.firmware.md5'),
    ], 'rev1'), undefined);
});

test('OTA still supports older generic GRGDO1 releases', () => {
    assert.equal(context.selectFirmwareAsset([legacy], 'rev1'), legacy);
});
