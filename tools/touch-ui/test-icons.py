#!/usr/bin/env python3
"""Check that every Android touch action points at a packaged RGBA icon."""
from pathlib import Path
import re
import struct
import unittest

ROOT = Path(__file__).resolve().parents[2]
CPP = ROOT / 'Hurrican' / 'src' / 'SDLPort' / 'TouchControls.cpp'
TEXTURES = ROOT / 'Hurrican' / 'data' / 'textures'
EXPECTED = ['jump', 'fire', 'blitz', 'powerline', 'grenade', 'smartbomb', 'weapon', 'pause', 'debug']


class IconTests(unittest.TestCase):
    def test_asset_version_updates_existing_installations(self):
        storage = (ROOT / 'Hurrican' / 'src' / 'SDLPort' / 'AndroidPort.cpp').read_text()
        match = re.search(r'ANDROID_ASSET_VERSION\s*=\s*"(\d+)"', storage)
        self.assertIsNotNone(match)
        self.assertGreaterEqual(int(match.group(1)), 8)

    def test_shell_and_attribution(self):
        data = (TEXTURES / 'touch_tactical_shell.png').read_bytes()
        self.assertEqual(data[:8], b'\x89PNG\r\n\x1a\n')
        self.assertEqual(struct.unpack('>IIBB', data[16:26]), (256, 256, 8, 6))
        notice = (ROOT / 'Hurrican/data/licenses/GameIcons-NOTICE.txt').read_text()
        for author in ('Delapouite', 'Lorc', 'Sbed', 'Guard13007'):
            self.assertIn(author, notice)
        self.assertTrue((ROOT / 'Hurrican/data/licenses/GameIcons-LICENSE.txt').is_file())

    def test_mapping_and_png_headers(self):
        table = CPP.read_text().split('const ActionIconDef ACTION_ICONS[ACTION_COUNT] = {', 1)[1].split('};', 1)[0]
        entries = re.findall(r'\{"touch_tactical_(\w+)\.png",\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*1,\s*1\}', table)
        self.assertEqual([item[0] for item in entries], EXPECTED)
        for name, *dimensions in entries:
            with self.subTest(icon=name):
                self.assertEqual(dimensions, ['128'] * 4)
                data = (TEXTURES / f'touch_tactical_{name}.png').read_bytes()
                self.assertEqual(data[:8], b'\x89PNG\r\n\x1a\n')
                self.assertEqual(data[12:16], b'IHDR')
                width, height, depth, color = struct.unpack('>IIBB', data[16:26])
                self.assertEqual((width, height, depth, color), (128, 128, 8, 6))


if __name__ == '__main__':
    unittest.main()
