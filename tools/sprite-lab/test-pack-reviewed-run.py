#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import tempfile
import unittest

import cv2
import numpy as np

spec = importlib.util.spec_from_file_location('packer', Path(__file__).with_name('pack-reviewed-run.py'))
packer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packer)


class PackTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def write_frames(self):
        for i in range(20):
            image = np.zeros((80, 70, 4), dtype=np.uint8)
            image[20:40, 10:30] = [i, 100, 140, 255]
            self.assertTrue(cv2.imwrite(str(self.directory / f'frame-{i:02}.png'), image))

    def test_missing_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Missing frames'):
            packer.pack(self.directory)

    def test_20_frames_in_row_major_order(self):
        self.write_frames()
        sheet, warnings = packer.pack(self.directory)
        self.assertEqual(sheet.shape, (320, 350, 4))
        self.assertEqual(warnings, [])
        for i in range(20):
            self.assertEqual(int(sheet[(i // 5) * 80 + 25, (i % 5) * 70 + 15, 0]), i)

    def test_duplicate_is_rejected(self):
        self.write_frames()
        image = cv2.imread(str(self.directory / 'frame-00.png'), cv2.IMREAD_UNCHANGED)
        cv2.imwrite(str(self.directory / 'frame-19.png'), image)
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            packer.pack(self.directory)

    def test_bad_size_is_rejected(self):
        self.write_frames()
        cv2.imwrite(str(self.directory / 'frame-07.png'), np.zeros((40, 70, 4), np.uint8))
        with self.assertRaisesRegex(ValueError, '70x80'):
            packer.pack(self.directory)

    def test_border_warns(self):
        self.write_frames()
        image = cv2.imread(str(self.directory / 'frame-00.png'), cv2.IMREAD_UNCHANGED)
        image[79, 20] = [0, 100, 100, 255]
        cv2.imwrite(str(self.directory / 'frame-00.png'), image)
        _, warnings = packer.pack(self.directory)
        self.assertEqual(len(warnings), 1)
        self.assertIn('frame-00.png', warnings[0])


if __name__ == '__main__':
    unittest.main()
