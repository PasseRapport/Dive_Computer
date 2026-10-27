"""Run: REFERENCE_ARCHIVE=/path/decotengu-0.14.1.tar.gz python3 -m unittest discover -s tools/decompression_reference -v"""
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import runner


class ReferenceTests(unittest.TestCase):
    def invoke(self, archive, report):
        return subprocess.run([sys.executable, '-I', str(runner.HERE / 'runner.py'), 'smoke',
                               '--archive', str(archive), '--report', str(report)], capture_output=True, text=True)

    def test_missing_archive_reports_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            proc = self.invoke(root / 'missing.tar.gz', root / 'report.json')
            self.assertNotEqual(proc.returncode, 0)
            result = json.loads(proc.stdout)
            self.assertEqual(result['status'], 'error')
            self.assertEqual(result['error']['code'], 'archive_missing')
            self.assertEqual(json.loads((root / 'report.json').read_text()), result)

    def test_tampered_archive_rejected_before_execution(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'tampered.tar.gz'
            path.write_bytes(b'not the pinned archive')
            with patch.object(runner.tarfile, 'open') as extract, patch.object(runner.subprocess, 'run') as execute:
                with self.assertRaises(runner.ReferenceError) as caught:
                    runner.smoke(path)
                self.assertEqual(caught.exception.code, 'archive_hash_mismatch')
                extract.assert_not_called()
                execute.assert_not_called()
            proc = self.invoke(path, Path(temp) / 'report.json')
            self.assertNotEqual(proc.returncode, 0)
            self.assertEqual(json.loads(proc.stdout)['error']['code'], 'archive_hash_mismatch')

    @unittest.skipUnless(os.environ.get('REFERENCE_ARCHIVE'), 'Set REFERENCE_ARCHIVE to test verified real dependency')
    def test_verified_archive_reproducibility_and_policy(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(os.environ['REFERENCE_ARCHIVE'])
            reports = []
            for i in range(2):
                proc = self.invoke(path, Path(temp) / f'report-{i}.json')
                self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
                reports.append(json.loads(proc.stdout))
            self.assertEqual(reports[0], reports[1])
            self.assertEqual(reports[0]['report_schema'], 1)
            self.assertEqual(reports[0]['runner_sha256'], runner.hashlib.sha256((runner.HERE / 'runner.py').read_bytes()).hexdigest())
            result = reports[0]['result']
            self.assertTrue(result['repeated_numerical_identity'])
            self.assertEqual(result['ambient_pressure_bar'], 4.028794875)
            for key in ('initial_tissues_bar', 'final_tissues_bar'):
                self.assertEqual(len(result[key]), 16)
                self.assertTrue(all(math.isfinite(v) for pair in result[key] for v in pair))
            for n2, he in result['initial_tissues_bar']:
                self.assertEqual(n2, 0.79 * (1.01325 - 0.0627))
                self.assertEqual(he, 0)
            self.assertTrue(math.isfinite(result['ceiling_absolute_pressure_bar_gf70']))
            self.assertIn('dependency-smoke-only', reports[0]['evidence_scope'])

    @unittest.skipUnless(os.environ.get('REFERENCE_ARCHIVE'), 'Set REFERENCE_ARCHIVE to test verified real dependency')
    def test_missing_dependency_failure_propagated(self):
        failure = {'status': 'error', 'error': {'code': 'dependency_missing', 'message': 'fixture import failure'}}
        with patch.object(runner.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1, json.dumps(failure), '')):
            with self.assertRaises(runner.ReferenceError) as caught:
                runner.smoke(Path(os.environ['REFERENCE_ARCHIVE']))
            self.assertEqual(caught.exception.code, 'dependency_missing')

    @unittest.skipUnless(os.environ.get('REFERENCE_ARCHIVE'), 'Set REFERENCE_ARCHIVE for policy test')
    def test_inconsistent_policy_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / 'runner.py').write_bytes((runner.HERE / 'runner.py').read_bytes())
            manifest = json.loads((runner.HERE / 'manifest.json').read_text())
            manifest['policy']['model'] = 'ZH_L16B_GF'
            (root / 'manifest.json').write_text(json.dumps(manifest))
            proc = subprocess.run([sys.executable, '-I', str(root / 'runner.py'), 'smoke',
                                   '--archive', os.environ['REFERENCE_ARCHIVE']], capture_output=True, text=True)
            self.assertNotEqual(proc.returncode, 0)
            self.assertEqual(json.loads(proc.stdout)['error']['code'], 'model_policy_mismatch')

    def test_worker_rejects_unverified_input(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'fake.tar.gz'
            path.write_bytes(b'unverified')
            proc = subprocess.run([sys.executable, '-I', str(runner.HERE / 'runner.py'),
                                   '_worker', str(path)], capture_output=True, text=True)
            self.assertNotEqual(proc.returncode, 0)
            self.assertEqual(json.loads(proc.stdout)['error']['code'], 'archive_hash_mismatch')

    def test_report_cannot_overwrite_archive_or_alias(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'archive'
            path.write_bytes(b'preserve me')
            alias = Path(temp) / 'alias'
            os.link(path, alias)
            for command in ('smoke', 'fetch'):
                for report in (path, alias):
                    proc = subprocess.run([sys.executable, '-I', str(runner.HERE / 'runner.py'), command,
                                           '--archive', str(path), '--report', str(report)], capture_output=True, text=True)
                    self.assertNotEqual(proc.returncode, 0)
                    self.assertEqual(json.loads(proc.stdout)['error']['code'], 'path_conflict')
                    self.assertEqual(path.read_bytes(), b'preserve me')

    def test_report_failure_is_nonzero_and_structured(self):
        with tempfile.TemporaryDirectory() as temp:
            proc = self.invoke(Path(temp) / 'missing.tar.gz', Path(temp) / 'no-directory/report.json')
            self.assertNotEqual(proc.returncode, 0)
            self.assertEqual(json.loads(proc.stdout)['error']['code'], 'report_write_failed')


if __name__ == '__main__':
    unittest.main()
