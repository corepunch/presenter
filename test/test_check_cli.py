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
<presentation><slide><stack><text>Clean slide</text></stack></slide>
<slide><stack><image src="missing&amp;&quot;.png" width="100" height="100"/></stack></slide></presentation>''')

            def run(*args):
                return subprocess.run([BINARY, *map(str, args)], capture_output=True,
                                      text=True, env={**os.environ, "SDL_VIDEODRIVER": "not-a-driver"})

            result = run("--check", "--json", deck)
            self.assertEqual(result.returncode, 0, result.stderr)
            data = json.loads(result.stdout)
            self.assertEqual(data["schemaVersion"], 1)
            self.assertEqual(data["slidesChecked"], 2)
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
                         ("--check", deck, "--slide=0"), ("--check", deck, "--slide=3"),
                         ("--check", deck, "--screenshot", "out.png"),
                         ("--check", deck, "--unknown")]:
                with self.subTest(args=args):
                    result = run(*args)
                    self.assertEqual(result.returncode, 1)
                    self.assertEqual(result.stdout, "")
            self.assertEqual(run("--help").returncode, 0)


unittest.main()
