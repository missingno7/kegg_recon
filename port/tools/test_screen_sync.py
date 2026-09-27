#!/usr/bin/env python3
"""Small parser and scheduler tests for retrace-synchronized screen captures."""
from __future__ import annotations

import unittest

import refshot
import smoke


class FrameShotTests(unittest.TestCase):
    def test_frame_shots_preserve_sorted_targets(self) -> None:
        self.assertEqual(smoke.parse_frame_shots("10=before,25=after"),
                         [(10, "before"), (25, "after")])

    def test_frame_shot_rejects_unsorted_or_duplicate_names(self) -> None:
        with self.assertRaises(ValueError):
            smoke.parse_frame_shots("25=after,10=before")
        with self.assertRaises(ValueError):
            smoke.parse_frame_shots("10=shot,20=shot")

    def test_frame_shot_limit_matches_host_scheduler(self) -> None:
        spec = ",".join(f"{frame}=f{frame}" for frame in range(64))
        self.assertEqual(len(smoke.parse_frame_shots(spec)), 64)
        with self.assertRaisesRegex(ValueError, "at most 64"):
            smoke.parse_frame_shots(spec + ",64=f64")

    def test_dosbox_hotkey_compensates_for_modifier_lead_in(self) -> None:
        self.assertEqual(refshot.hotkey_dispatch_ms(1000), 940)
        self.assertEqual(refshot.hotkey_dispatch_ms(60), 0)
        self.assertEqual(refshot.hotkey_dispatch_ms(0), 0)


if __name__ == "__main__":
    unittest.main()
