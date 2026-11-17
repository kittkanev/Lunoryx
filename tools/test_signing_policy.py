import unittest
import json
import tempfile
from pathlib import Path
from signing_policy import FIELDS,readiness,artifact_status


class SigningPolicyTests(unittest.TestCase):
    def test_absent_credentials_never_claim_publisher(self):
        for platform,expected in [('windows','unsigned'),('macos','ad_hoc')]:
            status=readiness(platform,{})
            self.assertEqual(status['mode'],expected);self.assertFalse(status['publisher_verified']);self.assertFalse(status['credentials_present'])

    def test_every_partial_configuration_fails_without_values(self):
        for platform,fields in FIELDS.items():
            for missing in fields:
                environment={name:'secret-value-not-for-output' for name in fields if name!=missing}
                with self.subTest(platform=platform,missing=missing),self.assertRaises(ValueError) as error:readiness(platform,environment)
                self.assertNotIn('secret-value-not-for-output',str(error.exception))

    def test_complete_credentials_are_only_unverified_configuration(self):
        windows={name:'secret' for name in FIELDS['windows']};windows['LUNORYX_WINDOWS_TIMESTAMP_URL']='https://timestamp.example.invalid'
        macos={name:'secret' for name in FIELDS['macos']};macos['LUNORYX_MACOS_SIGN_IDENTITY']='Developer ID Application: Example (ABCDEFGHIJ)'
        for platform,env in [('windows',windows),('macos',macos)]:
            state=readiness(platform,env);self.assertEqual(state['mode'],'configured_unverified');self.assertFalse(state['publisher_verified'])
        windows['LUNORYX_WINDOWS_TIMESTAMP_URL']='http://insecure.invalid'
        with self.assertRaises(ValueError):readiness('windows',windows)
        macos['LUNORYX_MACOS_SIGN_IDENTITY']='-'
        with self.assertRaises(ValueError):readiness('macos',macos)

    def test_artifact_status_requires_bound_verified_metadata(self):
        self.assertFalse(artifact_status('windows',None,'source')['publisher_verified'])
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'status.json'
            for bad in [dict(status='unsigned',publisher_verified='false',source_sha='source'),dict(status='unsigned',publisher_verified=False,source_sha='wrong'),dict(status='authenticode',publisher_verified=True,source_sha='source',files=[])]:
                path.write_text(json.dumps(bad))
                with self.assertRaises(ValueError):artifact_status('windows',path,'source')
            value=dict(status='authenticode',publisher_verified=True,source_sha='source',files=[dict(name=name,status='valid',timestamped=True) for name in ['engine.exe','Runtime/Luaryx.Worker.exe','Runtime/Luaryx.Worker.dll']])
            path.write_text(json.dumps(value));self.assertTrue(artifact_status('windows',path,'source')['publisher_verified'])
            value=dict(status='developer_id_notarized',publisher_verified=True,source_sha='source',stapled=False,identity='Developer ID Application: Test',submission_id='test-fixture')
            path.write_text(json.dumps(value))
            with self.assertRaises(ValueError):artifact_status('macos',path,'source')


if __name__=='__main__':unittest.main()
