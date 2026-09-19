"""Re-run saved-code checks after game folders move; never access a process.

Replacement executables must match every original capture's SHA-256. Only
in-memory path fields change; captures, C++ pins and existing validators do not.
"""
import argparse
import hashlib
import importlib.util
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--se-exe', type=Path, required=True)
    parser.add_argument('--ae-exe', type=Path, required=True)
    parser.add_argument('--address-root', type=Path, required=True)
    args = parser.parse_args()
    validator_path = Path(__file__).resolve().parents[1] / 'verify-addon-relocation-coverage.py'
    spec = importlib.util.spec_from_file_location('saved_addon_validator', validator_path)
    validator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(validator)
    original_captures = validator.generate.ownership.captures
    replacements = {'se': args.se_exe.resolve(), 'ae': args.ae_exe.resolve()}
    digests = {branch: hashlib.sha256(path.read_bytes()).hexdigest()
               for branch, path in replacements.items()}

    def relocated_captures(branch):
        records = original_captures(branch)
        if not records or any(record['exeSha256'].lower() != digests[branch]
                              for record in records):
            raise ValueError(f'{branch}: replacement executable differs from saved captures')
        return [dict(record, exe=str(replacements[branch])) for record in records]

    validator.generate.ownership.captures = relocated_captures
    validator.generate.mapping.ROOTS = [args.address_root.resolve()] * 2
    result = validator.verify()
    for item in result['libraries']:
        print(f"{item['runtime']}: {item['resolvedIDs']} address IDs PASS")
    print(f"Saved ownership patterns: {result['ownershipPatterns']}; "
          f"saved call sites: {len(result['savedCallsites'])} PASS")
    print(result['limitation'])


if __name__ == '__main__':
    main()
