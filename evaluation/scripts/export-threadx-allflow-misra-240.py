"""Export the controlled 240-second all-Flow and Flow+MISRA rerun."""
import argparse
import csv
import hashlib
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

repo = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("raw_run_root", type=Path, help="Output root printed by the scan runner")
parser.add_argument("--output", type=Path, default=repo / "evaluation/reports/threadx-allflow-misra-240",
                    help="New directory for normalized reports (must not already exist)")
parser.add_argument("--states-root", type=Path, default=Path(r"C:\projects\parasoft-vs-SQ\states"),
                    help="Parent directory containing the clean and bugged source worktrees")
args = parser.parse_args()
raw = args.raw_run_root.resolve()
out = args.output.resolve()
states_root = args.states_root.resolve()
old = repo / "evaluation/reports/threadx-thorough-360"
if out.exists():
    raise SystemExit(f"Refusing to replace reviewed evidence: {out}")


def read_csv(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def write_csv(name, rows, fields=None):
    if fields is None:
        fields = list(rows[0]) if rows else []
    with (out / name).open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse_findings(report):
    return [{
        "Rule": node.get("rule", ""),
        "Analyzer": node.get("analyzer", ""),
        "File": node.get("locFile", "").replace("\\", "/"),
        "Line": node.get("locStartln", ""),
        "Message": " ".join(node.get("msg", "").split()),
        "Severity": node.get("sev", ""),
        "ViolationId": node.get("violId", ""),
        "Kind": node.tag,
    } for node in ET.parse(report).iter() if node.tag in ("FlowViol", "StdViol")]


def marker_sites(source_root):
    result = {}
    for path in source_root.glob("*.c"):
        for line, text in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            match = re.search(r"PLANTED DEFECT BUG-(\d+)", text)
            if match:
                result[int(match.group(1))] = (path.name, line)
    assert len(result) == 52
    return result


runs = json.loads((raw / "runs.json").read_text(encoding="utf-8-sig"))
assert len(runs) == 4
catalog = read_csv(old / "DETECTION_MATRIX.csv")
prior_tp = {int(row["BugID"][-3:]) for row in catalog
            if row["ValidOnWin32"] == "yes" and row["ThoroughTX360"] == "TP"}
assert len(prior_tp) == 20
sites = marker_sites(states_root / "bugged/ports/win32/vs_2019/example_build/sample_threadx/harness")
assert sum(row["ValidOnWin32"] == "yes" for row in catalog) == 50
out.mkdir()
summary, matrices, clean_review = [], [], []
findings_by_case = {}
previous_reviews = {
    (row["Rule"], row["File"], row["Line"], row["Message"]): row
    for row in read_csv(old / "CLEAN_FINDING_REVIEW.csv") if row["Run"] == "tx-clean"
}

for run in runs:
    assert run["Exit"] == 0 and run["DepthValue"] == 4
    assert run["TimeoutSeconds"] == 240 and run["TimeoutStrategy"] == "time"
    assert run["FlowCheckersEnabled"] == 144
    assert run["MisraCheckersEnabled"] == (388 if run["Variant"] == "misra" else 0)
    case = run["Variant"] + "-" + run["State"]
    report, log = raw / case / "report.xml", raw / case / "console.log"
    config = repo / "evaluation/configs" / (run["Configuration"] + ".properties")
    assert sha(config) == run["ConfigurationSHA256"]
    log_text = log.read_text(encoding="utf-8", errors="replace")
    assert 'Current timeout strategy is "time" (240 seconds)' in log_text
    checked = re.search(r"files checked: (\d+)", log_text)
    timeouts = re.search(r"(\d+) timeouts occurred during Flow Analysis", log_text)
    assert checked and int(checked.group(1)) == 214 and timeouts
    findings = parse_findings(report)
    findings_by_case[case] = findings
    write_csv(case + "-findings.csv", findings)
    harness = [f for f in findings if "/sample_threadx/harness/" in f["File"]]
    summary.append({
        "Case": case, "State": run["State"], "Configuration": run["Configuration"],
        "ConfigurationSHA256": run["ConfigurationSHA256"], "Tag": run["Tag"],
        "Commit": run["Commit"], "Depth": "Thorough", "DepthValue": 4,
        "TimeoutStrategy": "time", "TimeoutSeconds": 240,
        "FlowCheckersEnabled": 144, "AggressiveAndUnvalidatedOptions": 25,
        "ExtraNotinitOptions": 2, "MISRA2025CheckersEnabled": run["MisraCheckersEnabled"],
        "Compiler": run["Compiler"], "StartUTC": run["StartUTC"], "EndUTC": run["EndUTC"],
        "Exit": run["Exit"], "FilesChecked": int(checked.group(1)),
        "XmlFindings": len(findings), "HarnessFindings": len(harness),
        "Timeouts": int(timeouts.group(1)),
        "RawReportSHA256": sha(report), "ConsoleSHA256": sha(log),
    })

def near(findings, bug, rules=None):
    file_name, line = sites[bug]
    return [f for f in findings
            if Path(f["File"]).name == file_name and abs(int(f["Line"] or 0) - line) <= 20
            and (rules is None or f["Rule"] in rules)]

allflow = findings_by_case["flow-bugged"]
combined = findings_by_case["misra-bugged"]
prior_still_found = set()
for bug in prior_tp:
    base_matches = near(allflow, bug)
    combined_matches = near(combined, bug)
    if bug == 49:
        # The interprocedural NOTINIT report is at a later use in the same helper chain.
        base_matches = near(allflow, bug) or [
            f for f in allflow if Path(f["File"]).name == sites[bug][0]
            and f["Rule"] == "BD-PB-NOTINIT"
        ]
        combined_matches = near(combined, bug) or [
            f for f in combined if Path(f["File"]).name == sites[bug][0]
            and f["Rule"] == "BD-PB-NOTINIT"
        ]
    assert base_matches and combined_matches, f"Prior TP not preserved: BUG-{bug:03}"
    prior_still_found.add(bug)
new_tp_rule = "BD-TRS-MLOCK"
assert any(f["Rule"] == new_tp_rule and Path(f["File"]).name == "harness_concurrency.c"
           and f["Line"] == "39" for f in allflow)
assert any(f["Rule"] == new_tp_rule and Path(f["File"]).name == "harness_concurrency.c"
           and f["Line"] == "39" for f in combined)
assert 23 not in prior_tp
tp_allflow = prior_still_found | {23}
tp_combined = prior_still_found | {23}
assert len(tp_allflow) == len(tp_combined) == 21

for row in catalog:
    bug = int(row["BugID"][-3:])
    if row["ValidOnWin32"] == "no":
        result, reason = "INVALID", row["Adjudication"]
    elif bug in tp_allflow:
        result = "TP"
        reason = ("BD-TRS-MLOCK at harness_concurrency.c:39 detects the planted "
                  "unlocked g_stats update." if bug == 23 else row["Adjudication"])
    elif row["ThoroughTX360"] in ("FN-PARTIAL", "FN-INDIRECT"):
        result, reason = row["ThoroughTX360"], row["Adjudication"]
    else:
        result, reason = "FN", row["Adjudication"]
    matrices.append({
        "BugID": row["BugID"], "CWE": row["CWE"], "Family": row["Family"],
        "Site": row["Site"], "ValidOnWin32": row["ValidOnWin32"],
        "ThoroughTX360": row["ThoroughTX360"], "AllFlow240": result,
        "AllFlow240Adjudication": reason,
        "FlowPlusMISRA2025_240": result,
        "FlowPlusMISRA2025_240Adjudication": (
            reason if bug != 23 else reason + " (MISRAC2025-DIR_5_1-a is an alias; "
            "counted once, not as another detection.)"),
    })
write_csv("DETECTION_MATRIX.csv", matrices)

# Preserve the prior clean review, and adjudicate only changed or added clean rows.
clean_harness = [f for f in findings_by_case["flow-clean"]
                 if "/sample_threadx/harness/" in f["File"]]
reviewed_clean = set()
for f in clean_harness:
    identity = (f["Rule"], f["File"], f["Line"], f["Message"])
    if identity in previous_reviews:
        old_row = previous_reviews[identity]
        category, note = old_row["Classification"], old_row["Review"]
    elif f["Rule"] == "BD-TRS-MLOCK" and f["File"].endswith("harness_init.c") and f["Line"] == "65":
        category = "clean-baseline-shared-state-warning"
        note = ("The clean callback also updates g_stats without stats_mutex. "
                "This is an unplanted clean-state concurrency concern, distinct "
                "from the separate BUG-023 site.")
    elif f["Rule"] == "BD-TRS-THRDR" and f["File"].endswith("harness_init.c") and f["Line"] == "215":
        category = "FP-ThreadX-queue-model-context"
        note = "The access is through tx_queue_receive; ThreadX synchronizes its queue API."
    elif f["Rule"] == "BD-TRS-THRDR" and f["File"].endswith("harness_sizearith.c"):
        category = "FP-single-thread-context"
        note = "The file-static log ring is accessed only by the logger thread."
    elif f["Rule"] == "BD-API-STRSIZE" and f["File"].endswith("harness_sizearith.c") and f["Line"] == "161":
        category = "duplicate-confirmation-baseline-defect"
        note = "Same sa_src[128] over-read already reported by BD-PB-OVERFRD at this line; not a new defect."
    else:
        raise AssertionError("Unreviewed all-Flow clean finding: " + str(identity))
    clean_review.append({"Run": "flow-clean", **f, "Classification": category, "Review": note})
assert len(clean_review) == len(clean_harness) == 26
write_csv("CLEAN_FLOW_REVIEW.csv", clean_review)

write_csv("run-summary.csv", summary)
print(f"Exported {len(summary)} scans; all Flow strict TP={len(tp_allflow)}/50, "
      f"Flow+MISRA strict TP={len(tp_combined)}/50.")
print("All prior 20 direct TPs preserved; BUG-023 newly detected by BD-TRS-MLOCK.")
print("Clean Flow findings reviewed:", len(clean_review))
print("Evidence:", out)
