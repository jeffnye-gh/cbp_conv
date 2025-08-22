import gzip
import io
import json
import os
import subprocess
import tempfile
from pathlib import Path

TOOL = Path("./bin/trace2json")

def run_cmd(args, **kwargs):
    """Run a command and return (rc, stdout, stderr)."""
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, **kwargs)
    out, err = proc.communicate()
    return proc.returncode, out, err

def test_json_to_text_roundtrip_tmpdir():
    """NDJSON (.gz) -> text (.txt). Always runs with a tiny synthetic sample."""
    assert TOOL.exists(), "trace2json binary not found; did build fail?"

    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        # Create a small NDJSON file with one ALU and one branch and one load
        ndjson_lines = [
            {
                "pc": "0x0000000000000010",
                "type": "aluOp",
                "A": {"bank": 1, "idx": 1, "val": "0x0000000000000001"},
                "D": {"bank": 1, "idx": 2, "val": "0x0000000000000002"},
            },
            {
                "pc": "0x0000000000000020",
                "type": "condBrOp",
                "taken": True,
                "target": "0x00000000000000A0"
            },
            {
                "pc": "0x0000000000000030",
                "type": "loadOp",
                "ea": "0x0000000000001000",
                "size": 8,
                "A": {"bank": 1, "idx": 31, "val": "0x0000000000002000"},
                "D": {"bank": 1, "idx": 5,  "val": "0x000000000000DEAD"}
            },
        ]
        ndjson_path = td / "tiny.jsonl.gz"
        with gzip.open(ndjson_path, "wt") as gz:
            for obj in ndjson_lines:
                gz.write(json.dumps(obj) + "\n")

        txt_out = td / "tiny.txt"
        rc, out, err = run_cmd([str(TOOL), "--in", str(ndjson_path), "--out", str(txt_out)])
        assert rc == 0, f"json->text failed: {err}"

        assert txt_out.exists(), "text output not created"
        text = txt_out.read_text()
        # Spot-check a few expected fragments
        assert "type: aluOp" in text
        assert "type: condBrOp" in text
        assert "type: loadOp" in text
        assert "PC: 0x10" in text or "PC: 0x0000000000000010" in text

def test_cbp_to_ndjson_if_sample_exists():
    """
    CBP binary (.gz) -> NDJSON (.jsonl). Run only when sample exists in repo.
    This is a smoke test: we just ensure the tool emits something and valid JSON line(s).
    """
    sample = Path("traces/sample_int_trace.gz")
    if not sample.exists():
        import pytest
        pytest.skip("Sample CBP trace not present in repo (traces/sample_int_trace.gz); skipping.")

    assert TOOL.exists(), "trace2json binary not found; did build fail?"
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        out_path = td / "out.jsonl"
        rc, out, err = run_cmd([str(TOOL), "--in", str(sample), "--out", str(out_path), "--limit", "1000"])
        assert rc == 0, f"cbp->ndjson failed: {err}"
        assert out_path.exists(), "NDJSON output not created"

        # Ensure at least one valid JSON line
        first = out_path.read_text().splitlines()[:5]
        assert first, "No lines emitted"
        import json as _json
        _json.loads(first[0])  # should not raise

