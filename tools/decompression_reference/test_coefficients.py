"""Offline audit tests; set REFERENCE_ARCHIVE and COEFFICIENT_SOURCE for acceptance."""
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import audit_coefficients as audit


class SchemaTests(unittest.TestCase):
    def setUp(self):
        self.table = json.loads((audit.HERE / 'nitrogen_coefficients.v1.json').read_text())

    def test_schema_mutations_rejected(self):
        mutations = []
        t = copy.deepcopy(self.table); t['rows'].reverse(); mutations.append(t)
        t = copy.deepcopy(self.table); t['rows'].pop(); mutations.append(t)
        t = copy.deepcopy(self.table); t['units']['a'] = 'Pa'; mutations.append(t)
        t = copy.deepcopy(self.table); t['rows'][0]['half_life'] = '5.0'; mutations.append(t)
        t = copy.deepcopy(self.table); t['rows'][5]['a'] = '0.50430'; mutations.append(t)
        t = copy.deepcopy(self.table); t['schema_version'] = True; mutations.append(t)
        t = copy.deepcopy(self.table); t['rows'][1]['compartment'] = 2.0; mutations.append(t)
        for t in mutations:
            with self.subTest(t=t), self.assertRaises(ValueError):
                audit.validate(t)

    def test_duplicate_keys_rejected(self):
        with self.assertRaisesRegex(ValueError, 'duplicate JSON key'):
            audit.strict_json('{"a": "wrong", "a": "right"}')

    def test_missing_archive_without_evidence(self):
        with tempfile.TemporaryDirectory() as tmp:
            missing = Path(tmp) / 'missing'
            result = audit.audit(missing, missing, missing, missing)
            self.assertEqual(result['status'], 'error')
            self.assertEqual(result['errors'][0]['code'], 'archive_missing')

    def test_invalid_dependency_manifest_structured(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in ('audit_coefficients.py', 'runner.py'):
                (root / name).write_bytes((audit.HERE / name).read_bytes())
            for contents in (None, '{bad'):
                if contents is not None:
                    (root / 'manifest.json').write_text(contents)
                proc = subprocess.run([sys.executable, '-I', str(root / 'audit_coefficients.py'),
                                       '--archive', str(root / 'missing'), '--source', str(root / 'source'),
                                       '--report', str(root / 'report')], capture_output=True, text=True)
                self.assertEqual(proc.returncode, 1)
                self.assertIn('dependency initialization failed', json.loads(proc.stdout)['errors'][0]['message'])


@unittest.skipUnless(os.environ.get('REFERENCE_ARCHIVE') and os.environ.get('COEFFICIENT_SOURCE'), 'Set both evidence paths for acceptance')
class AuditTests(unittest.TestCase):
    def setUp(self):
        self.archive = Path(os.environ['REFERENCE_ARCHIVE'])
        self.source = Path(os.environ['COEFFICIENT_SOURCE'])
        self.manifest = audit.HERE / 'nitrogen_coefficients.v1.json'
        self.printed = audit.HERE / 'nitrogen_printed_table.v1.json'

    def run_audit(self, **changes):
        args = dict(archive=self.archive, source=self.source, manifest=self.manifest, transcription=self.printed)
        args.update(changes)
        return audit.audit(**args)

    def test_all_48_match_and_repeat(self):
        r = self.run_audit()
        self.assertEqual(r['status'], 'pass', r)
        self.assertEqual(len(r['checks']), 48)
        self.assertTrue(all(c['match'] for c in r['checks']))
        self.assertEqual(r, self.run_audit())
        self.assertEqual(r['source']['row_selection'], '1, 2–16; alternative row 1b excluded')

    def test_changed_value_identified(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'values.json'
            t = json.loads(self.manifest.read_text())
            t['rows'][5]['a'] = '0.5403'
            path.write_text(json.dumps(t))
            r = self.run_audit(manifest=path)
            self.assertEqual(r['status'], 'error')
            bad = [c for c in r['checks'] if not c['match']]
            self.assertEqual([(c['compartment'], c['field']) for c in bad], [(6, 'a')])

    def test_missing_and_modified_source_fail(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'source'
            self.assertEqual(self.run_audit(source=path)['status'], 'error')
            path.write_bytes(b'wrong source')
            self.assertIn('source hash mismatch', self.run_audit(source=path)['errors'][0]['message'])

    def test_archive_failure_precedes_ast(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'archive'; path.write_bytes(b'wrong archive')
            with patch.object(audit, 'reference_constants') as parse:
                r = self.run_audit(archive=path)
                self.assertEqual(r['status'], 'error')
                parse.assert_not_called()

    def test_print_transcription_mutation_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'print.json'; t = json.loads(self.printed.read_text())
            t['rows'][3]['b'] = '0.7725'; path.write_text(json.dumps(t))
            r = self.run_audit(transcription=path)
            self.assertEqual(r['status'], 'error')
            self.assertEqual([c['compartment'] for c in r['checks'] if not c['match']], [4])

    def test_provenance_mutation_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'print.json'; t = json.loads(self.printed.read_text())
            t['source']['row_selection'] = '1b, 2–16'
            path.write_text(json.dumps(t))
            self.assertEqual(self.run_audit(transcription=path)['status'], 'error')

    def test_cli_preserves_aliased_input_and_reports_success(self):
        with tempfile.TemporaryDirectory() as tmp:
            archive = Path(tmp) / 'archive'; archive.write_bytes(self.archive.read_bytes())
            report = Path(tmp) / 'report'; os.link(archive, report)
            cmd = [sys.executable, '-I', str(audit.HERE / 'audit_coefficients.py'), '--archive', str(archive), '--source', str(self.source), '--report', str(report)]
            # Isolated mode omits script directory; CLI must explicitly support local module import.
            proc = subprocess.run(cmd, capture_output=True, text=True)
            self.assertNotEqual(proc.returncode, 0)
            self.assertEqual(json.loads(proc.stdout)['errors'][0]['code'], 'path_conflict')
            self.assertEqual(archive.read_bytes(), self.archive.read_bytes())
            report.unlink()
            proc = subprocess.run(cmd, capture_output=True, text=True)
            self.assertEqual(proc.returncode, 0, proc.stderr + proc.stdout)
            self.assertEqual(json.loads(report.read_text())['status'], 'pass')


if __name__ == '__main__':
    unittest.main()
