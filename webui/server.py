#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import mimetypes
import re
import shlex
import subprocess
import threading
import webbrowser
from dataclasses import dataclass, field
from datetime import datetime
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import urlparse


PROJECT_ROOT = Path(__file__).resolve().parent.parent
STATIC_DIR = PROJECT_ROOT / "webui" / "static"
INPUT_DIR = PROJECT_ROOT / "input"
OUTPUT_DIR = PROJECT_ROOT / "output"
LEGACY_RESULTS_DIR = PROJECT_ROOT / "results_old"
CONFIG_PATH = PROJECT_ROOT / "include" / "config.h"
MAKEFILE_PATH = PROJECT_ROOT / "makefile"
EXECUTABLE_PATH = PROJECT_ROOT / "df2d"

CASE_PATTERN = re.compile(r"^input_parameter_(\d+)\.txt$")
ASSIGNMENT_PATTERN = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^;]+);")

PARAM_SPECS = [
    ("lam", float),
    ("Pe", float),
    ("Pe2", float),
    ("eps", float),
    ("Da", float),
    ("K0", float),
    ("ny", int),
    ("xpo_l", float),
    ("xpo_r", float),
    ("endT", float),
    ("total_count", int),
    ("coeff_dt", float),
    ("x_ini_posi", float),
    ("alpha", float),
]


def json_response(handler: BaseHTTPRequestHandler, payload: Any, status: int = 200) -> None:
    body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Content-Length", str(len(body)))
    handler.end_headers()
    handler.wfile.write(body)


def error_response(handler: BaseHTTPRequestHandler, message: str, status: int = 400) -> None:
    json_response(handler, {"error": message}, status=status)


def canonical_case_path(case_id: int) -> Path:
    return INPUT_DIR / f"input_parameter_{case_id:04d}.txt"


def format_number(value: float | int, caster: type) -> str:
    if caster is int:
        return str(int(value))
    return f"{float(value):.12g}"


def read_text_file(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def write_text_file(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def detect_elf(path: Path) -> bool:
    if not path.exists() or not path.is_file():
        return False
    try:
        return path.read_bytes()[:4] == b"\x7fELF"
    except OSError:
        return False


def to_wsl_path(path: Path) -> str:
    drive = path.drive.rstrip(":").lower()
    tail = path.as_posix().split(":/", 1)[-1]
    return f"/mnt/{drive}/{tail}"


def run_command(command: list[str], timeout: float = 10.0) -> tuple[bool, str]:
    try:
        completed = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=timeout,
            encoding="utf-8",
            errors="replace",
            cwd=str(PROJECT_ROOT),
        )
    except subprocess.TimeoutExpired:
        return False, f"Command timed out after {timeout} seconds: {' '.join(command)}"
    except (OSError, subprocess.SubprocessError) as exc:
        return False, str(exc)
    output = (completed.stdout or "") + (completed.stderr or "")
    return completed.returncode == 0, output.strip()


def summarize_wsl_error(output: str) -> str:
    if "\x00" in output:
        return "WSL is not configured yet. Install or initialize it with `wsl --install`."
    cleaned = output.replace("\x00", " ").strip()
    normalized = " ".join(cleaned.split())
    lowered = normalized.lower()
    if not normalized:
        return "WSL is not available or not configured."
    if "wsl" in lowered and "install" in lowered:
        return "WSL is not configured yet. Install or initialize it with `wsl --install`."
    return normalized


def wsl_probe() -> dict[str, Any]:
    ok, output = run_command(["wsl", "--status"], timeout=2)
    if not ok:
        return {
            "available": False,
            "message": summarize_wsl_error(output),
            "makeAvailable": False,
            "compilerAvailable": False,
        }

    ok, output = run_command(["wsl", "-e", "bash", "-lc", "printf ready"], timeout=3)
    if not ok or "ready" not in output:
        return {
            "available": False,
            "message": summarize_wsl_error(output),
            "makeAvailable": False,
            "compilerAvailable": False,
        }

    ok, output = run_command(
        ["wsl", "-e", "bash", "-lc", "command -v make && command -v g++ && g++ --version | head -n 1"],
        timeout=3,
    )
    return {
        "available": True,
        "message": output or "WSL is available.",
        "makeAvailable": ok and "/make" in output,
        "compilerAvailable": ok and "/g++" in output,
    }


def discover_case_files() -> dict[int, Path]:
    INPUT_DIR.mkdir(parents=True, exist_ok=True)
    found: dict[int, list[Path]] = {}
    for entry in INPUT_DIR.iterdir():
        if not entry.is_file():
            continue
        match = CASE_PATTERN.match(entry.name)
        if not match:
            continue
        case_id = int(match.group(1))
        found.setdefault(case_id, []).append(entry)

    selected: dict[int, Path] = {}
    for case_id, paths in found.items():
        canonical = canonical_case_path(case_id)
        if canonical in paths:
            selected[case_id] = canonical
        else:
            selected[case_id] = sorted(paths, key=lambda item: item.name)[0]
    return dict(sorted(selected.items()))


def parse_case_file(path: Path) -> dict[str, Any]:
    content = read_text_file(path).strip()
    if not content:
        raise ValueError(f"{path.name} is empty.")

    tokens = content.split()
    expected = 1 + len(PARAM_SPECS)
    if len(tokens) < expected:
        raise ValueError(f"{path.name} has {len(tokens)} tokens, expected at least {expected}.")

    try:
        legacy_marker = int(float(tokens[0]))
    except ValueError as exc:
        raise ValueError(f"{path.name} has an invalid leading marker.") from exc

    values: dict[str, Any] = {"legacy_marker": legacy_marker}

    for index, (name, caster) in enumerate(PARAM_SPECS, start=1):
        raw = tokens[index]
        try:
            numeric = float(raw)
        except ValueError as exc:
            raise ValueError(f"{path.name} contains a non-numeric value for {name}.") from exc
        values[name] = int(numeric) if caster is int else numeric

    return values


def validate_case_payload(payload: dict[str, Any]) -> dict[str, Any]:
    validated: dict[str, Any] = {"legacy_marker": int(payload.get("legacy_marker", 1))}
    for name, caster in PARAM_SPECS:
        if name not in payload:
            raise ValueError(f"Missing field: {name}")
        raw = payload[name]
        try:
            number = float(raw)
        except (TypeError, ValueError) as exc:
            raise ValueError(f"Field {name} must be numeric.") from exc
        validated[name] = int(number) if caster is int else number

    if validated["ny"] <= 0:
        raise ValueError("ny must be a positive integer.")
    if validated["total_count"] <= 0:
        raise ValueError("total_count must be a positive integer.")
    if validated["coeff_dt"] <= 0:
        raise ValueError("coeff_dt must be greater than 0.")
    if validated["endT"] <= 0:
        raise ValueError("endT must be greater than 0.")
    if not (0.0 <= validated["xpo_l"] < validated["xpo_r"] <= 1.0):
        raise ValueError("xpo_l and xpo_r must satisfy 0 <= xpo_l < xpo_r <= 1.")

    return validated


def serialize_case(case_id: int, values: dict[str, Any]) -> str:
    parts = [str(values.get("legacy_marker", 1))]
    for name, caster in PARAM_SPECS:
        parts.append(format_number(values[name], caster))
    return " ".join(parts) + "\n"


def patch_case_selection(case_id: int) -> None:
    if not CONFIG_PATH.exists():
        raise FileNotFoundError("include/config.h not found.")

    content = read_text_file(CONFIG_PATH)
    updated = re.sub(
        r"constexpr bool USE_CASE_LIST = (true|false);",
        "constexpr bool USE_CASE_LIST = true;",
        content,
        count=1,
    )
    updated = re.sub(
        r"inline std::vector<int> get_case_list\(\)\s*\{\s*return\s*\{[^}]*\};\s*\}",
        f"inline std::vector<int> get_case_list() {{\n        return {{{case_id}}};\n    }}",
        updated,
        count=1,
        flags=re.MULTILINE | re.DOTALL,
    )
    write_text_file(CONFIG_PATH, updated)


def parse_eta_series(path: Path) -> list[dict[str, float]]:
    points: list[dict[str, float]] = []
    if not path or not path.exists():
        return points
    for line in read_text_file(path).splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        parts = stripped.split()
        if len(parts) < 3:
            continue
        try:
            time_value, eta_value, derivative = map(float, parts[:3])
        except ValueError:
            continue
        points.append({"time": time_value, "eta": eta_value, "dEtaDt": derivative})
    return points


def parse_remarks_summary(path: Path) -> dict[str, str]:
    summary: dict[str, str] = {}
    if not path or not path.exists():
        return summary

    interesting = {
        "converged",
        "actual_iterations",
        "time_total",
        "time_computation",
        "time_initialization",
        "time_io",
        "final_sim_time",
        "final_eta_ave",
        "final_rel_error",
        "output_file_count",
        "nan_events",
    }

    for line in read_text_file(path).splitlines():
        match = ASSIGNMENT_PATTERN.match(line)
        if not match:
            continue
        name, value = match.groups()
        if name in interesting:
            summary[name] = value.strip()
    return summary


def parse_matrix_file(path: Path) -> list[list[float]]:
    matrix: list[list[float]] = []
    if not path.exists():
        return matrix
    for line in read_text_file(path).splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        try:
            row = [float(part) for part in stripped.split()]
        except ValueError:
            continue
        if row:
            matrix.append(row)
    return matrix


def parse_profile_file(path: Path) -> list[dict[str, float]]:
    points: list[dict[str, float]] = []
    if not path.exists():
        return points
    for line in read_text_file(path).splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        parts = stripped.split()
        if len(parts) < 2:
            continue
        try:
            x_value, eta_value = map(float, parts[:2])
        except ValueError:
            continue
        points.append({"x": x_value, "eta": eta_value})
    return points


def snapshot_directory(case_id: int) -> Path:
    return OUTPUT_DIR / f"data_{case_id}"


def list_snapshots(case_id: int) -> list[dict[str, Any]]:
    data_dir = snapshot_directory(case_id)
    if not data_dir.exists():
        return []

    snapshots: dict[int, dict[str, Any]] = {}
    for entry in data_dir.iterdir():
        if not entry.is_file():
            continue
        match = re.match(r"^(cc|ee)_(\d+)\.(m|dat)$", entry.name)
        if not match:
            continue
        prefix, count_text, extension = match.groups()
        count = int(count_text)
        item = snapshots.setdefault(count, {"count": count, "cc": False, "ee": False, "files": []})
        item[prefix] = True
        item["files"].append(entry.name)
        item["format"] = extension
    return [snapshots[key] for key in sorted(snapshots)]


def locate_eta_file(case_id: int) -> Path | None:
    primary = OUTPUT_DIR / f"eta_ave_{case_id}.m"
    if primary.exists():
        return primary
    legacy = LEGACY_RESULTS_DIR / f"eta_ave_{case_id}.m"
    if legacy.exists():
        return legacy
    return None


def locate_remarks_file(case_id: int) -> Path | None:
    primary = OUTPUT_DIR / f"remarks_{case_id}.m"
    if primary.exists():
        return primary
    legacy = LEGACY_RESULTS_DIR / f"remarks_{case_id}.m"
    if legacy.exists():
        return legacy
    return None


def environment_snapshot() -> dict[str, Any]:
    wsl = wsl_probe()
    input_exists = INPUT_DIR.exists()
    output_ready = OUTPUT_DIR.exists() or OUTPUT_DIR.parent.exists()
    executable_exists = EXECUTABLE_PATH.exists()
    executable_is_elf = detect_elf(EXECUTABLE_PATH)
    makefile_exists = MAKEFILE_PATH.exists()
    can_run = (
        wsl["available"]
        and wsl["makeAvailable"]
        and wsl["compilerAvailable"]
        and makefile_exists
        and input_exists
        and output_ready
    )

    checks = [
        {"key": "wsl", "label": "WSL/Linux runtime", "ok": wsl["available"], "details": wsl["message"]},
        {
            "key": "make",
            "label": "make toolchain",
            "ok": wsl["makeAvailable"],
            "details": "Available inside WSL." if wsl["makeAvailable"] else "make was not found in WSL.",
        },
        {
            "key": "compiler",
            "label": "g++ compiler",
            "ok": wsl["compilerAvailable"],
            "details": "Available inside WSL." if wsl["compilerAvailable"] else "g++ was not found in WSL.",
        },
        {"key": "makefile", "label": "makefile", "ok": makefile_exists, "details": str(MAKEFILE_PATH)},
        {
            "key": "executable",
            "label": "df2d executable",
            "ok": executable_exists,
            "details": "ELF binary detected." if executable_is_elf else "Missing or not an ELF binary.",
        },
        {"key": "input", "label": "input directory", "ok": input_exists, "details": str(INPUT_DIR)},
        {"key": "output", "label": "output directory", "ok": output_ready, "details": str(OUTPUT_DIR)},
    ]

    return {
        "projectRoot": str(PROJECT_ROOT),
        "paths": {
            "input": str(INPUT_DIR),
            "output": str(OUTPUT_DIR),
            "config": str(CONFIG_PATH),
            "makefile": str(MAKEFILE_PATH),
            "executable": str(EXECUTABLE_PATH),
        },
        "canRun": can_run,
        "executableIsElf": executable_is_elf,
        "checks": checks,
    }


@dataclass
class TaskState:
    lock: threading.Lock = field(default_factory=threading.Lock)
    status: str = "idle"
    stage: str = "idle"
    case_id: int | None = None
    started_at: str | None = None
    finished_at: str | None = None
    build_exit_code: int | None = None
    run_exit_code: int | None = None
    error: str | None = None
    build_log: list[str] = field(default_factory=list)
    run_log: list[str] = field(default_factory=list)
    active_process: subprocess.Popen[str] | None = None
    stop_requested: bool = False

    def reset(self, case_id: int) -> None:
        self.status = "running"
        self.stage = "building"
        self.case_id = case_id
        self.started_at = datetime.now().isoformat(timespec="seconds")
        self.finished_at = None
        self.build_exit_code = None
        self.run_exit_code = None
        self.error = None
        self.build_log = []
        self.run_log = []
        self.active_process = None
        self.stop_requested = False

    def append_log(self, stage: str, line: str) -> None:
        target = self.build_log if stage == "building" else self.run_log
        target.append(line.rstrip("\n"))
        if len(target) > 2000:
            del target[: len(target) - 2000]

    def snapshot(self) -> dict[str, Any]:
        return {
            "status": self.status,
            "stage": self.stage,
            "caseId": self.case_id,
            "startedAt": self.started_at,
            "finishedAt": self.finished_at,
            "buildExitCode": self.build_exit_code,
            "runExitCode": self.run_exit_code,
            "error": self.error,
            "buildLog": self.build_log[-500:],
            "runLog": self.run_log[-500:],
            "stopRequested": self.stop_requested,
        }


TASK_STATE = TaskState()


def stream_process_output(process: subprocess.Popen[str], stage: str) -> int:
    assert process.stdout is not None
    for line in iter(process.stdout.readline, ""):
        with TASK_STATE.lock:
            TASK_STATE.append_log(stage, line)
        if TASK_STATE.stop_requested:
            try:
                process.terminate()
            except OSError:
                pass
            break
    return process.wait()


def run_build_and_solver(case_id: int) -> None:
    with TASK_STATE.lock:
        TASK_STATE.reset(case_id)
        TASK_STATE.append_log("building", f"[info] Preparing build for case {case_id}.")

    try:
        patch_case_selection(case_id)
    except Exception as exc:  # noqa: BLE001
        with TASK_STATE.lock:
            TASK_STATE.status = "failed"
            TASK_STATE.stage = "building"
            TASK_STATE.error = f"Failed to update config.h for case selection: {exc}"
            TASK_STATE.finished_at = datetime.now().isoformat(timespec="seconds")
            TASK_STATE.append_log("building", f"[error] {TASK_STATE.error}")
        return

    env = environment_snapshot()
    if not env["canRun"]:
        with TASK_STATE.lock:
            TASK_STATE.status = "failed"
            TASK_STATE.stage = "building"
            TASK_STATE.error = "Build environment is not ready."
            TASK_STATE.finished_at = datetime.now().isoformat(timespec="seconds")
            TASK_STATE.append_log("building", "[error] Build environment is not ready.")
        return

    project_root_wsl = to_wsl_path(PROJECT_ROOT)
    build_script = f"cd {shlex.quote(project_root_wsl)} && make clean && make"

    try:
        build_process = subprocess.Popen(
            ["wsl", "-e", "bash", "-lc", build_script],
            cwd=str(PROJECT_ROOT),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except OSError as exc:
        with TASK_STATE.lock:
            TASK_STATE.status = "failed"
            TASK_STATE.stage = "building"
            TASK_STATE.error = f"Failed to start build: {exc}"
            TASK_STATE.finished_at = datetime.now().isoformat(timespec="seconds")
            TASK_STATE.append_log("building", f"[error] {TASK_STATE.error}")
        return

    with TASK_STATE.lock:
        TASK_STATE.active_process = build_process
        TASK_STATE.append_log("building", "[info] Running: make clean && make")

    build_exit_code = stream_process_output(build_process, "building")
    with TASK_STATE.lock:
        TASK_STATE.build_exit_code = build_exit_code
        TASK_STATE.active_process = None

    if build_exit_code != 0 or TASK_STATE.stop_requested:
        with TASK_STATE.lock:
            TASK_STATE.status = "stopped" if TASK_STATE.stop_requested else "failed"
            TASK_STATE.stage = "building"
            TASK_STATE.error = None if TASK_STATE.stop_requested else "Build failed."
            TASK_STATE.finished_at = datetime.now().isoformat(timespec="seconds")
            if TASK_STATE.stop_requested:
                TASK_STATE.append_log("building", "[warn] Task stopped during build.")
            else:
                TASK_STATE.append_log("building", "[error] Build failed.")
        return

    run_script = f"cd {shlex.quote(project_root_wsl)} && ./df2d"
    with TASK_STATE.lock:
        TASK_STATE.stage = "running"
        TASK_STATE.append_log("run", "[info] Build succeeded. Launching solver.")

    try:
        run_process = subprocess.Popen(
            ["wsl", "-e", "bash", "-lc", run_script],
            cwd=str(PROJECT_ROOT),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except OSError as exc:
        with TASK_STATE.lock:
            TASK_STATE.status = "failed"
            TASK_STATE.stage = "running"
            TASK_STATE.error = f"Failed to start solver: {exc}"
            TASK_STATE.finished_at = datetime.now().isoformat(timespec="seconds")
            TASK_STATE.append_log("run", f"[error] {TASK_STATE.error}")
        return

    with TASK_STATE.lock:
        TASK_STATE.active_process = run_process

    run_exit_code = stream_process_output(run_process, "run")
    with TASK_STATE.lock:
        TASK_STATE.run_exit_code = run_exit_code
        TASK_STATE.active_process = None
        TASK_STATE.finished_at = datetime.now().isoformat(timespec="seconds")

        if TASK_STATE.stop_requested:
            TASK_STATE.status = "stopped"
            TASK_STATE.append_log("run", "[warn] Task stopped by user.")
        elif run_exit_code == 0:
            TASK_STATE.status = "finished"
            TASK_STATE.append_log("run", "[info] Solver finished successfully.")
        else:
            TASK_STATE.status = "failed"
            TASK_STATE.error = "Solver exited with a non-zero status."
            TASK_STATE.append_log("run", "[error] Solver failed.")


def launch_task(case_id: int) -> dict[str, Any]:
    with TASK_STATE.lock:
        if TASK_STATE.status == "running":
            raise RuntimeError("Another build/run task is already active.")
    worker = threading.Thread(target=run_build_and_solver, args=(case_id,), daemon=True)
    worker.start()
    return {"accepted": True, "caseId": case_id}


def stop_task() -> dict[str, Any]:
    with TASK_STATE.lock:
        TASK_STATE.stop_requested = True
        process = TASK_STATE.active_process
    if process is not None:
        try:
            process.terminate()
        except OSError:
            pass
    return {"accepted": True}


class SolverUIHandler(BaseHTTPRequestHandler):
    server_version = "PDEWebUI/0.1"

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        path = parsed.path
        try:
            if path == "/api/env":
                json_response(self, environment_snapshot())
                return
            if path == "/api/cases":
                payload = []
                for case_id, file_path in discover_case_files().items():
                    payload.append(
                        {
                            "id": case_id,
                            "path": str(file_path),
                            "canonicalPath": str(canonical_case_path(case_id)),
                            "modifiedAt": datetime.fromtimestamp(file_path.stat().st_mtime).isoformat(timespec="seconds"),
                        }
                    )
                json_response(self, payload)
                return
            if path.startswith("/api/cases/"):
                case_id = self._extract_id(path, "/api/cases/")
                case_path = discover_case_files().get(case_id) or canonical_case_path(case_id)
                if not case_path.exists():
                    error_response(self, f"Case {case_id} was not found.", status=404)
                    return
                values = parse_case_file(case_path)
                values.update({"id": case_id, "path": str(case_path), "canonicalPath": str(canonical_case_path(case_id))})
                json_response(self, values)
                return
            if path == "/api/run/status":
                with TASK_STATE.lock:
                    json_response(self, TASK_STATE.snapshot())
                return
            if path.startswith("/api/results/"):
                self._handle_result_request(path)
                return
            self._serve_static(path)
        except ValueError as exc:
            error_response(self, str(exc), status=400)
        except Exception as exc:  # noqa: BLE001
            error_response(self, f"Internal error: {exc}", status=500)

    def do_PUT(self) -> None:  # noqa: N802
        path = urlparse(self.path).path
        try:
            if path.startswith("/api/cases/"):
                case_id = self._extract_id(path, "/api/cases/")
                payload = self._read_json_body()
                values = validate_case_payload(payload)
                target = canonical_case_path(case_id)
                write_text_file(target, serialize_case(case_id, values))
                json_response(self, {"saved": True, "id": case_id, "path": str(target)})
                return
            error_response(self, "Unknown endpoint.", status=404)
        except ValueError as exc:
            error_response(self, str(exc), status=400)
        except Exception as exc:  # noqa: BLE001
            error_response(self, f"Internal error: {exc}", status=500)

    def do_POST(self) -> None:  # noqa: N802
        path = urlparse(self.path).path
        try:
            if path == "/api/build-and-run":
                payload = self._read_json_body()
                result = launch_task(int(payload.get("caseId")))
                json_response(self, result, status=202)
                return
            if path == "/api/run/stop":
                json_response(self, stop_task())
                return
            error_response(self, "Unknown endpoint.", status=404)
        except RuntimeError as exc:
            error_response(self, str(exc), status=409)
        except ValueError as exc:
            error_response(self, str(exc), status=400)
        except Exception as exc:  # noqa: BLE001
            error_response(self, f"Internal error: {exc}", status=500)

    def log_message(self, format: str, *args: Any) -> None:  # noqa: A003
        message = format % args
        print(f"[{self.log_date_time_string()}] {self.address_string()} {message}")

    def _handle_result_request(self, path: str) -> None:
        suffix = path.removeprefix("/api/results/")
        parts = [part for part in suffix.split("/") if part]
        if not parts:
            error_response(self, "Missing result case id.", status=404)
            return
        case_id = int(parts[0])
        if len(parts) == 1:
            eta_path = locate_eta_file(case_id)
            remarks_path = locate_remarks_file(case_id)
            snapshots = list_snapshots(case_id)
            payload = {
                "caseId": case_id,
                "etaAvailable": eta_path is not None,
                "remarksAvailable": remarks_path is not None,
                "etaPath": str(eta_path) if eta_path else None,
                "remarksPath": str(remarks_path) if remarks_path else None,
                "summary": parse_remarks_summary(remarks_path) if remarks_path else {},
                "snapshotCount": len(snapshots),
                "latestSnapshot": snapshots[-1]["count"] if snapshots else None,
            }
            json_response(self, payload)
            return
        if len(parts) == 2 and parts[1] == "eta":
            eta_path = locate_eta_file(case_id)
            json_response(self, {"caseId": case_id, "points": parse_eta_series(eta_path)})
            return
        if len(parts) == 2 and parts[1] == "snapshots":
            json_response(self, {"caseId": case_id, "snapshots": list_snapshots(case_id)})
            return
        if len(parts) == 3 and parts[1] == "snapshot":
            count = int(parts[2])
            data_dir = snapshot_directory(case_id)
            cc_path = data_dir / f"cc_{count}.m"
            ee_path = data_dir / f"ee_{count}.m"
            if not cc_path.exists() and not ee_path.exists():
                error_response(self, f"Snapshot {count} for case {case_id} was not found.", status=404)
                return
            json_response(
                self,
                {"caseId": case_id, "count": count, "matrix": parse_matrix_file(cc_path), "profile": parse_profile_file(ee_path)},
            )
            return
        error_response(self, "Unknown result endpoint.", status=404)

    def _read_json_body(self) -> dict[str, Any]:
        content_length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(content_length) if content_length else b"{}"
        if not body:
            return {}
        try:
            return json.loads(body.decode("utf-8"))
        except json.JSONDecodeError as exc:
            raise ValueError("Request body must be valid JSON.") from exc

    def _extract_id(self, path: str, prefix: str) -> int:
        raw = path.removeprefix(prefix).split("/", 1)[0]
        if not raw:
            raise ValueError("Missing case id.")
        return int(raw)

    def _serve_static(self, path: str) -> None:
        relative = "index.html" if path in {"", "/"} else path.lstrip("/")
        file_path = (STATIC_DIR / relative).resolve()
        if not str(file_path).startswith(str(STATIC_DIR.resolve())) or not file_path.exists() or not file_path.is_file():
            self.send_error(HTTPStatus.NOT_FOUND)
            return
        mime_type, _ = mimetypes.guess_type(file_path.name)
        data = file_path.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", f"{mime_type or 'application/octet-stream'}; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


def main() -> None:
    parser = argparse.ArgumentParser(description="Local web UI for the PDE solver.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8123)
    parser.add_argument("--open-browser", action="store_true")
    args = parser.parse_args()

    server = ThreadingHTTPServer((args.host, args.port), SolverUIHandler)
    url = f"http://{args.host}:{args.port}"
    print(f"Serving PDE web UI at {url}")
    print(f"Project root: {PROJECT_ROOT}")

    if args.open_browser:
        threading.Timer(0.8, lambda: webbrowser.open(url)).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server...")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
