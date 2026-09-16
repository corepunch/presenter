"""CLI contract tests: no video driver, parseable JSON, exit codes and filtering."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())


class CheckCLI(unittest.TestCase):
    def test_contract(self):
        with tempfile.TemporaryDirectory(prefix="presenter-cli-") as tmp:
            deck = Path(tmp) / "test.slides"
            deck.mkdir()
            (deck / "presentation.xml").write_text('''<?xml version="1.0"?>
<!DOCTYPE presentation SYSTEM "https://corepunch.github.io/presenter/schemas/presentation.dtd">
<presentation><slide><stack verticalAlignment="center"><text>Clean slide</text></stack></slide>
<slide><stack><image src="missing&amp;&quot;.png" width="100" height="100"/></stack></slide>
<slide><stack><text>Sparse slide</text></stack></slide></presentation>''')

            def run(*args):
                return subprocess.run([BINARY, *map(str, args)], capture_output=True,
                                      text=True, env={**os.environ, "SDL_VIDEODRIVER": "not-a-driver"})

            result = run("--check", "--json", deck)
            self.assertEqual(result.returncode, 0, result.stderr)
            data = json.loads(result.stdout)
            self.assertEqual(data["schemaVersion"], 1)
            self.assertEqual(data["slidesChecked"], 3)
            missing = next(i for i in data["issues"] if i["code"] == "missing_image")
            self.assertEqual(missing["slide"], 2)
            self.assertTrue(missing["source"].endswith('missing&".png'))
            self.assertEqual(missing["severity"], "error")
            self.assertIn("bounds", missing)
            self.assertIn("suggestion", missing)
            self.assertEqual(run(deck, "--check", "--strict").returncode, 2)
            result = run(deck, "--check", "--strict", "--json", "--slide=1")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(result.stdout)["issues"], [])
            self.assertEqual(json.loads(result.stdout)["slidesChecked"], 1)
            self.assertIn("No issues found", run("--check", deck, "--slide", "1").stdout)
            for args in [("--json", deck), ("--strict", deck), ("--check",),
                         ("--check", deck, "--slide", "1bad"),
                         ("--check", deck, "--slide=0"), ("--check", deck, "--slide=4"),
                         ("--check", deck, "--screenshot", "out.png"),
                         ("--check", deck, "--unknown")]:
                with self.subTest(args=args):
                    result = run(*args)
                    self.assertEqual(result.returncode, 1)
                    self.assertEqual(result.stdout, "")
            result = run("--check", "--strict", "--json", deck, "--slide=3")
            self.assertEqual(result.returncode, 2)
            issue = next(i for i in json.loads(result.stdout)["issues"] if i["code"] == "underpopulated_slide")
            self.assertEqual(issue["slide"], 3)
            self.assertEqual(issue["severity"], "warning")
            self.assertGreater(issue["measurements"]["emptyBelowFraction"], 0.45)
            self.assertIn("underpopulated_slide", run("--check", deck, "--slide=3").stdout)
            self.assertEqual(run("--help").returncode, 0)


unittest.main()
