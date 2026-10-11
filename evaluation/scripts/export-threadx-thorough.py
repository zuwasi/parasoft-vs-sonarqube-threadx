"""Export and check the independently adjudicated Thorough/TX rerun evidence."""
import csv
import hashlib
import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_csv(path, rows, fields):
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


raw = Path(sys.argv[1]).resolve()
repo = Path(__file__).resolve().parents[2]
out = repo / "evaluation" / "reports" / "threadx-thorough"
if out.exists():
    raise SystemExit("Output exists; refuse to replace adjudicated evidence.")
out.mkdir()
all_findings = {}
run_rows = []
for run in json.loads((raw / "runs.json").read_text(encoding="utf-8")):
    case = run["Model"] + "-" + run["State"]
    report = raw / case / "report.xml"
    console = raw / case / "console.log"
    log = console.read_text(encoding="utf-8", errors="replace")
    timeouts = re.search(r"(\d+) timeouts occurred during Flow Analysis", log)
    checked = re.search(r"files checked: (\d+)", log)
    assert run["Exit"] == 0 and run["DepthValue"] == 4
    assert checked and int(checked.group(1)) == 214
    config = repo / "evaluation" / "configs" / (run["Configuration"] + ".properties")
    assert sha256(config) == run["ConfigurationSHA256"]
    findings = []
    for node in ET.parse(report).iter():
        if node.tag not in ("FlowViol", "StdViol"):
            continue
        findings.append({
            "Rule": node.get("rule", ""),
            "File": node.get("locFile", "").replace("\\", "/"),
            "Line": node.get("locStartln", ""),
            "Message": " ".join(node.get("msg", "").split()),
            "Severity": node.get("sev", ""),
            "ViolationId": node.get("violId", ""),
        })
    all_findings[case] = findings
    write_csv(out / (case + "-findings.csv"), findings, list(findings[0]))
    run_rows.append({
        **{key: run[key] for key in (
            "Model", "State", "Tag", "Commit", "Configuration",
            "ConfigurationSHA256", "Depth", "DepthValue", "Compiler",
            "StartUTC", "EndUTC", "Exit",
        )},
        "FilesChecked": int(checked.group(1)),
        "XmlFindings": len(findings),
        "HarnessFindings": sum("/sample_threadx/harness/" in f["File"] for f in findings),
        "Timeouts": int(timeouts.group(1)) if timeouts else 0,
        "RawReportSHA256": sha256(report),
        "ConsoleSHA256": sha256(console),
    })
write_csv(out / "run-summary.csv", run_rows, list(run_rows[0]))

# Reviewed root-cause evidence. Multiple diagnostics for one bug count only once.
evidence = {
    "BUG-003": ("harness_sizearith.c", "BD-PB-ARRAY", 68),
    "BUG-010": ("harness_sizearith.c", "BD-PB-ARRAY", 259),
    "BUG-018": ("harness_memory.c", "BD-RES-FREE", 180),
    "BUG-020": ("harness_memory.c", "BD-RES-INVFREE", 237),
    "BUG-022": ("harness_memory.c", "BD-RES-LEAKS", 316),
    "BUG-032": ("harness_nullpaths.c", "BD-PB-NP", 96),
    "BUG-033": ("harness_nullpaths.c", "BD-PB-NP", 115),
    "BUG-034": ("harness_nullpaths.c", "BD-PB-NOTINIT", 154),
    "BUG-037": ("harness_nullpaths.c", "BD-PB-NOTINIT", 211),
    "BUG-038": ("harness_nullpaths.c", "BD-PB-NP", 226),
    "BUG-040": ("harness_taint.c", "BD-PB-OVERFWR", 75),
    "BUG-042": ("harness_taint.c", "BD-PB-ARRAY", 104),
    "BUG-046": ("harness_taint.c", "BD-PB-ARRAY", 182),
    "BUG-048": ("harness_uninit.c", "BD-PB-NOTINIT", 69),
    "BUG-049": ("harness_uninit.c", "BD-PB-NOTINIT", 88),
    "BUG-050": ("harness_uninit.c", "BD-PB-NOTINIT", 130),
}
tx_evidence = {
    "BUG-025": ("harness_concurrency.c", "BD-TRS-ORDER", 75),
    "BUG-026": ("harness_concurrency.c", "BD-TRS-THRDR", 111),
    "BUG-028": ("harness_concurrency.c", "BD-TRS-REVLOCK", 155),
    "BUG-029": ("harness_concurrency.c", "BD-TRS-THRDR", 166),
}


def matched(case, item):
    file, rule, line = item
    return any(f["File"].endswith("/" + file) and f["Rule"] == rule
               and f["Line"] == str(line) for f in all_findings[case])


matrix = []
with (repo / "evaluation/reports/adjudication/DETECTION_MATRIX.csv").open(
        encoding="utf-8-sig", newline="") as stream:
    for old in csv.DictReader(stream):
        bug = old["BugID"]
        control = old["ParasoftFlowAggressive"]
        tx = control
        notes = old["Adjudication"]
        item = evidence.get(bug)
        if item:
            assert matched("control-bugged", item) and matched("tx-bugged", item), bug
            control = tx = "TP"
            notes = f"{item[1]} at {item[0]}:{item[2]}"
        if bug in tx_evidence:
            item = tx_evidence[bug]
            assert not matched("control-bugged", item)
            assert matched("tx-bugged", item), bug
            tx = "TP"
            notes = f"TX model: {item[1]} at {item[0]}:{item[2]}"
        if bug == "BUG-024":
            notes = ("No matching TOCTOU report. The table_mutex leak at init.c:225 "
                     "follows a failed acquire/early-return path; not this defect.")
        matrix.append({
            **{key: old[key] for key in ("BugID", "CWE", "Family", "Site", "ValidOnWin32")},
            "OriginalAggressive": old["ParasoftFlowAggressive"],
            "ThoroughControl": control,
            "ThoroughTX": tx,
            "Adjudication": notes,
        })
assert sum(m["ValidOnWin32"] == "yes" for m in matrix) == 50
assert sum(m["ThoroughControl"] == "TP" for m in matrix) == 16
assert sum(m["ThoroughTX"] == "TP" for m in matrix) == 20
write_csv(out / "DETECTION_MATRIX.csv", matrix, list(matrix[0]))

review = []
for case in ("control-clean", "tx-clean"):
    for finding in all_findings[case]:
        if "/sample_threadx/harness/" not in finding["File"]:
            continue
        rule = finding["Rule"]
        if rule == "BD-PB-OVERFRD":
            classification = "confirmed-baseline-defect"
            note = "Known sa_src[128] over-read; predates planted diff."
        elif rule == "BD-TRS-THRDR" and "g_sensor_raw" in finding["Message"]:
            classification = "unmarked-baseline-concurrency-concern"
            note = ("Ordinary volatile simulated input shared by producer/logger/maintenance; "
                    "not one of the planted defects. Do not call it a false positive.")
        elif rule == "BD-TRS-THRDR":
            classification = "FP-RTOS-model-context"
            note = ("ThreadX pool/event control object passed to its synchronized API; "
                    "no application mutex is required around normal API use.")
        elif rule == "BD-TRS-REVLOCK":
            classification = "assumption-dependent-not-confirmed-FP"
            note = ("Path assumes failed get via TX_MUTEX_ERROR/TX_CALLER_ERROR, followed "
                    "by unchecked put. Successful initialization/thread context is not proven.")
        else:
            classification = "FP-contextual-code-shape"
            note = ("Non-catalog scaffold/copy/overwrite/constant-condition diagnostic; "
                    "not counted as a planted safety-defect detection.")
        review.append({"Run": case, **finding, "Classification": classification, "Review": note})
write_csv(out / "CLEAN_FINDING_REVIEW.csv", review, list(review[0]))
print("Exported four runs; asserted 16/50 control and 20/50 TX.")
print(out)
