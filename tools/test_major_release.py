import copy
import tempfile
import unittest
from pathlib import Path
from check_major_release import check


class MajorReleaseTests(unittest.TestCase):
    def test_legacy_release_keeps_existing_gate(self):check('1.3.1',{},Path('.'))

    def test_missing_open_or_unproven_features_cannot_release(self):
        for features in [[],[dict(id='P1-A',status='open')],[dict(id='P2-A',status='fixed')]]:
            with self.subTest(features=features),self.assertRaises(ValueError):check('1.4.0',dict(target_version='1.4.0',items=features),Path('.'))

    def test_evidence_paths_and_target_match(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'test.cpp').touch();(root/'validation.md').touch()
            register=dict(target_version='1.4.0',items=[dict(id='P1-A',status='fixed',evidence=dict(tests=['test.cpp'],validation='validation.md'))])
            check('1.4.0',register,root) # Synthetic fixture; not engine acceptance.
            for bad in ['missing.cpp','../outside.cpp']:
                copy_register=copy.deepcopy(register);copy_register['items'][0]['evidence']['tests']=[bad]
                with self.assertRaises(ValueError):check('1.4.0',copy_register,root)
            with self.assertRaises(ValueError):check('1.4.1',register,root)
            register['items']*=2
            with self.assertRaises(ValueError):check('1.4.0',register,root)


if __name__=='__main__':unittest.main()
