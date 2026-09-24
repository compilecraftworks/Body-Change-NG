"""Read-only verification of documented vanilla condition IDs against Skyrim.esm.

Does not launch Skyrim, resolve installed NPC overrides, or edit game files.
"""
import argparse
import importlib.util
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("skyrim_esm", type=Path)
    args = parser.parse_args()
    if args.skyrim_esm.name.lower() != "skyrim.esm":
        parser.error("Use the original Skyrim.esm, not a patch/replacer.")
    repo = Path(__file__).resolve().parent.parent
    spec = importlib.util.spec_from_file_location("plugin_records", repo / "tools/audit-plugin-records.py")
    audit = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(audit)
    template = (repo / "package/SKSE/Plugins/BodyChangeNGdistribution.json").read_text(encoding="utf-8-sig")
    runtime = (repo / "src/BodyChangeNG/DistributionAuthoringRuntime.cpp").read_text(encoding="utf-8-sig")
    kinds = dict(raceEditorID="RACE", factionEditorID="FACT", keyword="KYWD", npcClass="CLAS", combatStyle="CSTY")
    rows = re.findall(r'\{DistributionScope::(\w+),(\d+),"(\w+)"\}', runtime)
    expected = {(kinds[kind], editor): int(local) for kind, local, editor in rows}
    if any(editor not in template for _, _, editor in rows):
        raise ValueError("A built-in name is missing from the editable guide")
    if len(expected) != 39 or len(rows) != len(expected):
        raise ValueError("Expected 39 distinct documented condition records")
    if {kind for kind, _ in expected} != {"RACE", "FACT", "KYWD", "CLAS", "CSTY"}:
        raise ValueError("Missing condition category")
    found = {}
    for kind, form, flags, fields in audit.records(args.skyrim_esm.read_bytes(), kinds={key[0] for key in expected}):
        if kind == "TES4" and any(tag == "MAST" for tag, _ in fields):
            raise ValueError("Skyrim.esm unexpectedly depends on another master")
        editor = next((audit.string(value) for tag, value in fields if tag == "EDID"), "")
        if (kind, editor) in expected:
            if flags & 0x20:
                raise ValueError("Documented target is deleted")
            found[kind, editor] = form
    if found != expected:
        failures = [key for key, form in expected.items() if found.get(key) != form]
        raise ValueError(f"Documented record mismatch: {failures}")
    print(f"PASS: {len(found)} exact EditorID/type/local-ID mappings verified in Skyrim.esm (read-only).")


if __name__ == "__main__":
    main()
