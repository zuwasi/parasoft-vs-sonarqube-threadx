"""Export the reviewed 360-second TX rerun without replacing 60-second evidence."""
import csv
import hashlib
import json
import re
import sys
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path

repo = Path(__file__).resolve().parents[2]
raw = Path(sys.argv[1]).resolve()
previous = repo / "evaluation/reports/threadx-thorough"
out = repo / "evaluation/reports/threadx-thorough-360"
if out.exists():
    raise SystemExit("Output exists; refusing to replace reviewed evidence.")


def load(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def write(name, rows):
    with (out / name).open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def key(row):
    return tuple(row[k] for k in ("Rule", "File", "Line", "Message"))


out.mkdir()
summary, delta, clean = [], [], []
old_reviews = {key(r): r for r in load(previous / "CLEAN_FINDING_REVIEW.csv")
               if r["Run"] == "tx-clean"}
for run in json.loads((raw / "runs.json").read_text(encoding="utf-8")):
    case = "tx-" + run["State"]
    assert run["Exit"] == 0 and run["DepthValue"] == 4
    assert run["TimeoutSeconds"] == 360 and run["TimeoutStrategy"] == "time"
    report = raw / case / "report.xml"
    console = raw / case / "console.log"
    log = console.read_text(encoding="utf-8", errors="replace")
    assert 'Current timeout strategy is "time" (360 seconds)' in log
    checked = re.search(r"files checked: (\d+)", log)
    timeout = re.search(r"(\d+) timeouts occurred during Flow Analysis", log)
    assert checked and int(checked.group(1)) == 214 and timeout
    config = repo / "evaluation/configs" / (run["Configuration"] + ".properties")
    assert digest(config) == run["ConfigurationSHA256"]
    findings = [{
        "Rule": n.get("rule", ""),
        "File": n.get("locFile", "").replace("\\", "/"),
        "Line": n.get("locStartln", ""),
        "Message": " ".join(n.get("msg", "").split()),
        "Severity": n.get("sev", ""),
        "ViolationId": n.get("violId", ""),
    } for n in ET.parse(report).iter() if n.tag in ("FlowViol", "StdViol")]
    write(case + "-findings.csv", findings)
    harness = [r for r in findings if "/sample_threadx/harness/" in r["File"]]
    old = [r for r in load(previous / (case + "-findings.csv"))
           if "/sample_threadx/harness/" in r["File"]]
    old_counts, new_counts = Counter(map(key, old)), Counter(map(key, harness))
    assert not old_counts - new_counts, "A previous harness finding was lost."
    remaining = new_counts - old_counts
    for finding in harness:
        signature = key(finding)
        if remaining[signature]:
            remaining[signature] -= 1
            message = finding["Message"]
            if "g_sensor_raw" in message:
                category = "existing-baseline-concurrency-concern"
                note = "Additional access to the same unmarked shared input; not a planted TP."
            elif "log_ring" in message:
                category = "FP-single-thread-context"
                note = "Only logger_entry calls sa_log_write/sa_log_flush; not a new race detection."
            elif finding["Rule"] == "BD-TRS-REVLOCK":
                category = "assumption-dependent-not-confirmed-FP"
                note = "Failed get assumed before unconditional put; not a new planted root cause."
            elif finding["Rule"] == "BD-PB-CC":
                category = "FP-contextual-code-shape"
                note = "Constant-condition scaffold report, not a new cataloged safety defect."
            else:
                raise AssertionError("Unreviewed new diagnostic: " + str(finding))
            delta.append({"Run": case, **finding, "Classification": category, "Review": note})
        if run["State"] == "clean":
            if signature in old_reviews:
                review = old_reviews[signature]
                category, note = review["Classification"], review["Review"]
            elif "log_ring" in finding["Message"]:
                category = "FP-single-thread-context"
                note = "Only the logger thread accesses this file-static ring."
            elif "g_sensor_raw" in finding["Message"]:
                category = "unmarked-baseline-concurrency-concern"
                note = "Second diagnostic on the same existing shared-input concern."
            else:
                raise AssertionError("Unreviewed clean finding")
            clean.append({"Run": case, **finding, "Classification": category, "Review": note})
    summary.append({
        **{k: run[k] for k in (
            "Model", "State", "Tag", "Commit", "Configuration", "ConfigurationSHA256",
            "Depth", "DepthValue", "TimeoutSeconds", "TimeoutStrategy",
            "Compiler", "StartUTC", "EndUTC", "Exit",
        )},
        "FilesChecked": int(checked.group(1)),
        "XmlFindings": len(findings), "HarnessFindings": len(harness),
        "Timeouts": int(timeout.group(1)),
        "RawReportSHA256": digest(report), "ConsoleSHA256": digest(console),
    })

assert len(summary) == 2 and len(clean) == 20
assert sum(r["Run"] == "tx-clean" for r in delta) == 2
assert sum(r["Run"] == "tx-bugged" for r in delta) == 9
matrix = [{
    **{k: old[k] for k in ("BugID", "CWE", "Family", "Site", "ValidOnWin32")},
    "ThoroughTX60": old["ThoroughTX"],
    "ThoroughTX360": old["ThoroughTX"],
    "Adjudication": old["Adjudication"],
} for old in load(previous / "DETECTION_MATRIX.csv")]
assert len(matrix) == 52
assert sum(r["ValidOnWin32"] == "yes" for r in matrix) == 50
assert sum(r["ThoroughTX360"] == "TP" for r in matrix) == 20
write("run-summary.csv", summary)
write("FINDING_DELTA.csv", delta)
write("CLEAN_FINDING_REVIEW.csv", clean)
write("DETECTION_MATRIX.csv", matrix)
print("Exported 360-second evidence: 20/50 unchanged, 2 clean and 9 bugged extra diagnostics.")
print(out)
