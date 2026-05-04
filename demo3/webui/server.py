#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import mimetypes
import os
import random
import re
import shutil
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
from urllib.parse import parse_qs, urlparse


PROJECT_ROOT = Path(__file__).resolve().parent.parent
STATIC_DIR = PROJECT_ROOT / "webui" / "static"
INPUT_DIR = PROJECT_ROOT / "input"
OUTPUT_DIR = PROJECT_ROOT / "output"
LEGACY_RESULTS_DIR = PROJECT_ROOT / "results_old"
CONFIG_PATH = PROJECT_ROOT / "include" / "config.h"
MAKEFILE_PATH = PROJECT_ROOT / "makefile"
EXECUTABLE_PATH = PROJECT_ROOT / "df2d"
BUILD_DIR = PROJECT_ROOT / "build"
BATCH_CASE_CHUNK_SIZE = 500
WSL_STATUS_TIMEOUT_SECONDS = 3.0
WSL_PROBE_TIMEOUT_SECONDS = 3.0
WSL_COLD_START_TIMEOUT_SECONDS = 12.0

NATIVE_EXECUTABLE_CANDIDATES = [
    BUILD_DIR / "Release" / "df2d.exe",
    BUILD_DIR / "Debug" / "df2d.exe",
    BUILD_DIR / "RelWithDebInfo" / "df2d.exe",
    BUILD_DIR / "MinSizeRel" / "df2d.exe",
    BUILD_DIR / "df2d.exe",
    BUILD_DIR / "df2d",
]

CASE_PATTERN = re.compile(r"^input_parameter_(\d+)\.(toml|txt)$")
ASSIGNMENT_PATTERN = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^;%#]+)\s*;?")
ANSI_ESCAPE_PATTERN = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
CASE_COMPLETION_PATTERN = re.compile(r"\[Case\s+(\d+)\]\[[^\]]+\]\s+(Converged!|Finished\b)", re.IGNORECASE)
CASE_PROGRESS_PATTERN = re.compile(
    r"\[Case\s+(\d+)\]\[([^\]]+)\]\s+(\d+(?:\.\d+)?)%\s*\|\s*"
    r"eta=([+\-\d.eE]+)\s*\|\s*err=([+\-\d.eE]+)\s*\|\s*used\s+([+\-\d.eE]+)s",
    re.IGNORECASE,
)
CASE_DONE_DETAIL_PATTERN = re.compile(
    r"\[Case\s+(\d+)\]\[([^\]]+)\]\s+(Converged!|Finished[^\|]*)"
    r"(?:\s*\|\s*eta=([+\-\d.eE]+))?(?:\s*\|\s*used\s+([+\-\d.eE]+)s)?",
    re.IGNORECASE,
)
CASE_ERROR_PATTERN = re.compile(r"\[Case\s+(\d+)\].*\b(error|failed|fatal|exception)\b", re.IGNORECASE)
CASE_TAG_PATTERN = re.compile(r"\[Case\s+(\d+)\]", re.IGNORECASE)
NUM_THREADS_PATTERN = re.compile(r"(constexpr\s+int\s+NUM_THREADS\s*=\s*)(\d+)(\s*;)")

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

GENERATOR_DEFAULTS = {
    "aRange": [0.001, 10.0, 41],
    "Pe1Range": [1000.0, 1000.0, 1],
    "Pe2Range": [0.1, 1e8, 91],
    "fixed": {
        "lam": 0.033333,
        "eps": 0.1,
        "Da": 100.0,
        "K0": 1.0,
        "ny": 50,
        "xpo_l": 0.333333,
        "xpo_r": 0.666667,
        "total_count": 10,
        "x_ini_posi": 5.0,
    },
    "coeffMax": 0.1,
    "Sc": 16667,
}

MAX_GENERATED_CASES = 100000
MAX_SCATTER_POINTS = 20000


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
    return INPUT_DIR / f"input_parameter_{case_id:04d}.toml"


def legacy_case_path(case_id: int) -> Path:
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


def read_num_threads_config() -> int:
    if not CONFIG_PATH.exists():
        return 0
    match = NUM_THREADS_PATTERN.search(read_text_file(CONFIG_PATH))
    return int(match.group(2)) if match else 0


def write_num_threads_config(value: int) -> None:
    if value < 0:
        raise ValueError("NUM_THREADS must be non-negative.")
    content = read_text_file(CONFIG_PATH)
    if not NUM_THREADS_PATTERN.search(content):
        raise ValueError("Could not find NUM_THREADS in config.h.")
    updated = NUM_THREADS_PATTERN.sub(lambda m: f"{m.group(1)}{value}{m.group(3)}", content, count=1)
    write_text_file(CONFIG_PATH, updated)


def detect_elf(path: Path) -> bool:
    if not path.exists() or not path.is_file():
        return False
    try:
        return path.read_bytes()[:4] == b"\x7fELF"
    except OSError:
        return False


def locate_native_executable() -> Path | None:
    for candidate in NATIVE_EXECUTABLE_CANDIDATES:
        if candidate.exists() and candidate.is_file():
            return candidate
    return None


def clear_cmake_cache_for_wsl() -> None:
    cache_path = BUILD_DIR / "CMakeCache.txt"
    cmake_files_path = BUILD_DIR / "CMakeFiles"
    if not cache_path.exists():
        return

    content = read_text_file(cache_path)
    cache_looks_foreign = (
        "/mnt/" in content
        or "Unix Makefiles" in content
        or "NMake Makefiles" in content
        or PROJECT_ROOT.as_posix() in content
    )
    if not cache_looks_foreign:
        return

    build_root = BUILD_DIR.resolve()
    project_root = PROJECT_ROOT.resolve()
    if project_root not in build_root.parents and build_root != project_root:
        raise RuntimeError(f"Refusing to clean unexpected build directory: {BUILD_DIR}")

    cache_path.unlink(missing_ok=True)
    if cmake_files_path.exists():
        shutil.rmtree(cmake_files_path)


def existing_cmake_cache_prefers_wsl() -> bool:
    cache_path = BUILD_DIR / "CMakeCache.txt"
    if not cache_path.exists():
        return False
    content = read_text_file(cache_path)
    return "/mnt/" in content or "Unix Makefiles" in content


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


def run_wsl_command(arguments: list[str], timeout: float = WSL_PROBE_TIMEOUT_SECONDS) -> tuple[bool, str]:
    command = ["wsl", *arguments]
    ok, output = run_command(command, timeout=timeout)
    if ok:
        return ok, output

    retry_ok, retry_output = run_command(command, timeout=WSL_COLD_START_TIMEOUT_SECONDS)
    if retry_ok or retry_output:
        return retry_ok, retry_output
    return ok, output


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


def summarize_wsl_toolchain(output: str) -> str:
    cleaned_lines: list[str] = []
    for raw_line in output.splitlines():
        line = raw_line.replace("\x00", "").strip()
        lowered = line.lower()
        if not line:
            continue
        if lowered.startswith("wsl:"):
            continue
        if "\ufffd" in line:
            continue
        if line.startswith("/") or lowered.startswith("cmake version"):
            cleaned_lines.append(line)

    if cleaned_lines:
        return " | ".join(cleaned_lines)
    return "WSL CMake toolchain is available."


def native_cmake_probe() -> dict[str, Any]:
    ok, output = run_command(["cmake", "--version"], timeout=3)
    first_line = output.splitlines()[0] if output else ""
    return {
        "available": ok,
        "message": first_line or ("CMake is available." if ok else "cmake was not found on PATH."),
    }


def wsl_probe() -> dict[str, Any]:
    ok, output = run_wsl_command(["--status"], timeout=WSL_STATUS_TIMEOUT_SECONDS)
    if not ok:
        return {
            "available": False,
            "message": summarize_wsl_error(output),
            "cmakeAvailable": False,
            "compilerAvailable": False,
        }

    ok, output = run_wsl_command(["-e", "bash", "-lc", "printf ready"])
    if not ok or "ready" not in output:
        return {
            "available": False,
            "message": summarize_wsl_error(output),
            "cmakeAvailable": False,
            "compilerAvailable": False,
        }

    ok, output = run_wsl_command(
        [
            "-e",
            "bash",
            "-lc",
            "command -v cmake && cmake --version | head -n 1 && (command -v c++ || command -v g++)",
        ]
    )
    cmake_available = ok and "cmake" in output.lower()
    return {
        "available": True,
        "message": summarize_wsl_toolchain(output),
        "cmakeAvailable": cmake_available,
        "compilerAvailable": ok and ("/c++" in output or "/g++" in output),
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
            selected[case_id] = sorted(paths, key=lambda item: (item.suffix != ".toml", item.name))[0]
    return dict(sorted(selected.items()))


def case_summary(case_id: int, file_path: Path) -> dict[str, Any]:
    return {
        "id": case_id,
        "path": str(file_path),
        "canonicalPath": str(canonical_case_path(case_id)),
        "modifiedAt": datetime.fromtimestamp(file_path.stat().st_mtime).isoformat(timespec="seconds"),
    }


def parse_case_query_ids(query: str) -> set[int] | None:
    normalized = query.strip()
    if not normalized or not re.fullmatch(r"[0-9,\-\s]+", normalized):
        return None

    case_ids: set[int] = set()
    for raw_part in re.split(r"[\s,]+", normalized):
        part = raw_part.strip()
        if not part:
            continue
        if "-" in part:
            bounds = part.split("-", 1)
            if len(bounds) != 2 or not bounds[0] or not bounds[1]:
                return None
            start = int(bounds[0])
            end = int(bounds[1])
            if start <= 0 or end <= 0 or start > end:
                return None
            case_ids.update(range(start, end + 1))
        else:
            case_id = int(part)
            if case_id <= 0:
                return None
            case_ids.add(case_id)

    return case_ids if case_ids else None


def case_matches_query(case_id: int, file_path: Path, query: str) -> bool:
    normalized = query.strip().lower()
    if not normalized:
        return True
    parsed_ids = parse_case_query_ids(normalized)
    if parsed_ids is not None:
        return case_id in parsed_ids
    padded_id = f"{case_id:04d}"
    candidates = [
        str(case_id),
        padded_id,
        file_path.name.lower(),
        str(file_path).lower(),
    ]
    return any(normalized in candidate for candidate in candidates)


def parse_legacy_case_file(path: Path) -> dict[str, Any]:
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


def parse_toml_case_file(path: Path) -> dict[str, Any]:
    values: dict[str, Any] = {"legacy_marker": 1}
    for line_number, raw_line in enumerate(read_text_file(path).splitlines(), start=1):
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        if "=" not in line:
            raise ValueError(f"{path.name}:{line_number} expected key = value.")
        key, raw_value = [part.strip() for part in line.split("=", 1)]
        if not key or not raw_value:
            raise ValueError(f"{path.name}:{line_number} key and value are required.")
        if key == "legacy_marker":
            values[key] = int(float(raw_value))
            continue
        spec = dict(PARAM_SPECS).get(key)
        if spec is None:
            continue
        try:
            numeric = float(raw_value)
        except ValueError as exc:
            raise ValueError(f"{path.name}:{line_number} contains a non-numeric value for {key}.") from exc
        if spec is int and not numeric.is_integer():
            raise ValueError(f"{path.name}:{line_number} {key} must be an integer.")
        values[key] = int(numeric) if spec is int else numeric

    missing = [name for name, _ in PARAM_SPECS if name not in values]
    if missing:
        raise ValueError(f"{path.name} is missing TOML field(s): {', '.join(missing)}.")
    return values


def parse_case_file(path: Path) -> dict[str, Any]:
    if path.suffix == ".toml":
        return parse_toml_case_file(path)
    return parse_legacy_case_file(path)


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
        if not math.isfinite(number):
            raise ValueError(f"Field {name} must be finite.")
        if caster is int and not number.is_integer():
            raise ValueError(f"Field {name} must be an integer.")
        validated[name] = int(number) if caster is int else number

    if validated["lam"] <= 0:
        raise ValueError("lam must be greater than 0.")
    if validated["ny"] <= 0:
        raise ValueError("ny must be a positive integer.")
    if validated["K0"] <= 0:
        raise ValueError("K0 must be greater than 0.")
    if validated["alpha"] == 0:
        raise ValueError("alpha must not be 0.")
    if validated["total_count"] <= 0:
        raise ValueError("total_count must be a positive integer.")
    if validated["coeff_dt"] <= 0:
        raise ValueError("coeff_dt must be greater than 0.")
    if validated["endT"] <= 0:
        raise ValueError("endT must be greater than 0.")
    if not (0.0 <= validated["xpo_l"] < validated["xpo_r"] <= 1.0):
        raise ValueError("xpo_l and xpo_r must satisfy 0 <= xpo_l < xpo_r <= 1.")

    return validated


def numeric_payload_value(payload: dict[str, Any], key: str, default: float) -> float:
    raw = payload.get(key, default)
    try:
        value = float(raw)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"Field {key} must be numeric.") from exc
    if not math.isfinite(value):
        raise ValueError(f"Field {key} must be finite.")
    return value


def int_payload_value(payload: dict[str, Any], key: str, default: int) -> int:
    value = numeric_payload_value(payload, key, default)
    if not value.is_integer():
        raise ValueError(f"Field {key} must be an integer.")
    return int(value)


def parse_log_range(payload: dict[str, Any], key: str, default: list[float | int]) -> tuple[float, float, int]:
    raw = payload.get(key, default)
    if not isinstance(raw, list | tuple) or len(raw) != 3:
        raise ValueError(f"{key} must be [min, max, count].")
    lo = float(raw[0])
    hi = float(raw[1])
    count_float = float(raw[2])
    if not all(math.isfinite(value) for value in [lo, hi, count_float]):
        raise ValueError(f"{key} values must be finite.")
    if lo <= 0 or hi <= 0:
        raise ValueError(f"{key} min and max must be greater than 0.")
    if not count_float.is_integer() or count_float < 1:
        raise ValueError(f"{key} count must be a positive integer.")
    return lo, hi, int(count_float)


def logspace_values(lo: float, hi: float, count: int) -> list[float]:
    if count == 1:
        return [lo]
    start = math.log10(lo)
    stop = math.log10(hi)
    step = (stop - start) / (count - 1)
    return [10 ** (start + step * index) for index in range(count)]


def round_generated(value: float | int) -> float | int:
    if isinstance(value, int):
        return value
    return round(float(value), 6)


def generated_case_preview(case_id: int, values: dict[str, Any]) -> dict[str, Any]:
    preview_keys = ["lam", "Pe", "Pe2", "eps", "Da", "K0", "ny", "endT", "total_count", "coeff_dt", "alpha"]
    return {"id": case_id, **{key: values[key] for key in preview_keys}}


def generated_scatter_points(rows: list[tuple[int, dict[str, Any]]]) -> dict[str, Any]:
    point_counts: dict[tuple[float, float], int] = {}
    for _, values in rows:
        key = (float(values["alpha"]), float(values["Pe2"]))
        point_counts[key] = point_counts.get(key, 0) + 1

    points = [
        {"alpha": alpha, "Pe2": pe2, "count": count}
        for (alpha, pe2), count in sorted(point_counts.items(), key=lambda item: (item[0][0], item[0][1]))
    ]
    total = len(points)
    if total > MAX_SCATTER_POINTS:
        step = total / MAX_SCATTER_POINTS
        points = [points[min(total - 1, int(index * step))] for index in range(MAX_SCATTER_POINTS)]

    return {
        "points": points,
        "total": total,
        "sampled": total > MAX_SCATTER_POINTS,
    }


def build_generation_config(payload: dict[str, Any]) -> dict[str, Any]:
    defaults = GENERATOR_DEFAULTS
    fixed_payload = payload.get("fixed", {})
    if not isinstance(fixed_payload, dict):
        raise ValueError("fixed must be an object.")

    fixed_defaults = defaults["fixed"]
    fixed = {
        "lam": numeric_payload_value(fixed_payload, "lam", fixed_defaults["lam"]),
        "eps": numeric_payload_value(fixed_payload, "eps", fixed_defaults["eps"]),
        "Da": numeric_payload_value(fixed_payload, "Da", fixed_defaults["Da"]),
        "K0": numeric_payload_value(fixed_payload, "K0", fixed_defaults["K0"]),
        "ny": int_payload_value(fixed_payload, "ny", fixed_defaults["ny"]),
        "xpo_l": numeric_payload_value(fixed_payload, "xpo_l", fixed_defaults["xpo_l"]),
        "xpo_r": numeric_payload_value(fixed_payload, "xpo_r", fixed_defaults["xpo_r"]),
        "total_count": int_payload_value(fixed_payload, "total_count", fixed_defaults["total_count"]),
        "x_ini_posi": numeric_payload_value(fixed_payload, "x_ini_posi", fixed_defaults["x_ini_posi"]),
    }

    config = {
        "aRange": parse_log_range(payload, "aRange", defaults["aRange"]),
        "Pe1Range": parse_log_range(payload, "Pe1Range", defaults["Pe1Range"]),
        "Pe2Range": parse_log_range(payload, "Pe2Range", defaults["Pe2Range"]),
        "fixed": fixed,
        "coeffMax": numeric_payload_value(payload, "coeffMax", defaults["coeffMax"]),
        "Sc": numeric_payload_value(payload, "Sc", defaults["Sc"]),
        "startId": int_payload_value(payload, "startId", max(discover_case_files().keys(), default=0) + 1),
        "overwrite": bool(payload.get("overwrite", False)),
        "dryRun": bool(payload.get("dryRun", False)),
    }
    config["dtLimitDenom"] = 20.0 * config["Sc"]

    if config["coeffMax"] <= 0:
        raise ValueError("coeffMax must be greater than 0.")
    if config["Sc"] <= 0:
        raise ValueError("Sc must be greater than 0.")
    if config["startId"] <= 0:
        raise ValueError("startId must be a positive integer.")

    counts = [config["aRange"][2], config["Pe1Range"][2], config["Pe2Range"][2]]
    total = counts[0] * counts[1] * counts[2]
    if total > MAX_GENERATED_CASES:
        raise ValueError(f"Generation would create {total} cases; limit is {MAX_GENERATED_CASES}.")

    return config


def generate_case_values(config: dict[str, Any]) -> list[tuple[int, dict[str, Any]]]:
    fixed = config["fixed"]
    a_values = logspace_values(*config["aRange"])
    pe1_values = logspace_values(*config["Pe1Range"])
    pe2_values = logspace_values(*config["Pe2Range"])
    rows: list[tuple[int, dict[str, Any]]] = []
    case_id = config["startId"]
    h = 1.0 / fixed["ny"]

    for pe2 in pe2_values:
        for pe1 in pe1_values:
            for alpha in a_values:
                end_t = 4.0 * fixed["total_count"] * 0.5 / fixed["eps"] / ((fixed["eps"] * pe1) ** (1.0 / 3.0))
                dt_limit = math.pi / (config["dtLimitDenom"] * (alpha ** 2))
                coeff_dt = min(config["coeffMax"], dt_limit / (h ** 2))
                values = {
                    "legacy_marker": 1,
                    "lam": round_generated(fixed["lam"]),
                    "Pe": round_generated(pe1),
                    "Pe2": round_generated(pe2),
                    "eps": round_generated(fixed["eps"]),
                    "Da": round_generated(fixed["Da"]),
                    "K0": round_generated(fixed["K0"]),
                    "ny": fixed["ny"],
                    "xpo_l": round_generated(fixed["xpo_l"]),
                    "xpo_r": round_generated(fixed["xpo_r"]),
                    "endT": round_generated(end_t),
                    "total_count": fixed["total_count"],
                    "coeff_dt": round_generated(coeff_dt),
                    "x_ini_posi": round_generated(fixed["x_ini_posi"]),
                    "alpha": round_generated(alpha),
                }
                rows.append((case_id, validate_case_payload(values)))
                case_id += 1
    return rows


def generate_cases(payload: dict[str, Any]) -> dict[str, Any]:
    config = build_generation_config(payload)
    rows = generate_case_values(config)
    existing = discover_case_files()
    collisions = [case_id for case_id, _ in rows if case_id in existing]
    if collisions and not config["overwrite"] and not config["dryRun"]:
        sample = ", ".join(str(case_id) for case_id in collisions[:8])
        raise ValueError(f"Case id collision: {sample}. Enable overwrite to replace existing case files.")

    if not config["dryRun"]:
        for case_id, values in rows:
            write_text_file(canonical_case_path(case_id), serialize_case(case_id, values))

    first_items = [generated_case_preview(case_id, values) for case_id, values in rows[:3]]
    last_items = [generated_case_preview(case_id, values) for case_id, values in rows[-3:]]
    return {
        "generated": 0 if config["dryRun"] else len(rows),
        "planned": len(rows),
        "dryRun": config["dryRun"],
        "startId": rows[0][0] if rows else config["startId"],
        "endId": rows[-1][0] if rows else config["startId"] - 1,
        "collisions": collisions[:20],
        "inputDir": str(INPUT_DIR),
        "preview": {
            "first": first_items,
            "last": last_items,
        },
        "scatter": generated_scatter_points(rows),
    }


def serialize_case(case_id: int, values: dict[str, Any]) -> str:
    lines = [f"legacy_marker = {int(values.get('legacy_marker', 1))}"]
    for name, caster in PARAM_SPECS:
        lines.append(f"{name} = {format_number(values[name], caster)}")
    return "\n".join(lines) + "\n"


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


def parse_remarks_values(path: Path | None) -> dict[str, float | int]:
    values: dict[str, float | int] = {}
    if not path or not path.exists():
        return values

    integer_names = {
        "case_number",
        "nx",
        "ny",
        "total_count",
        "actual_iterations",
        "output_file_count",
        "instability_events",
        "nan_events",
        "resumed_from_checkpoint",
        "resumed_at_iteration",
        "converged",
        "total_grid_cells",
    }
    for line in read_text_file(path).splitlines():
        match = ASSIGNMENT_PATTERN.match(line)
        if not match:
            continue
        name, raw_value = match.groups()
        raw_value = raw_value.strip()
        try:
            number = float(raw_value)
        except ValueError:
            continue
        if not math.isfinite(number):
            continue
        values[name] = int(number) if name in integer_names else number
    return values


def load_case_values(case_id: int) -> dict[str, Any]:
    case_path = discover_case_files().get(case_id)
    if not case_path or not case_path.exists():
        return {}
    try:
        return parse_case_file(case_path)
    except ValueError:
        return {}


def derive_result_metadata(case_id: int, remarks_path: Path | None) -> dict[str, Any]:
    case_values = load_case_values(case_id)
    remarks_values = parse_remarks_values(remarks_path)
    metadata: dict[str, Any] = dict(case_values)
    metadata.update(remarks_values)

    def number(name: str) -> float | None:
        value = metadata.get(name)
        if isinstance(value, (int, float)) and math.isfinite(float(value)):
            return float(value)
        return None

    lam = number("lam")
    ny = number("ny")
    if "nx" not in metadata and lam and ny:
        metadata["nx"] = int(ny * (1.0 / lam))
    if "h" not in metadata and ny:
        metadata["h"] = 1.0 / ny
    metadata.setdefault("xleft", 0.0)
    metadata.setdefault("yleft", 0.0)
    metadata.setdefault("yright", 1.0)

    nx = number("nx")
    yright = number("yright") or 1.0
    if "xright" not in metadata and nx is not None and ny:
        metadata["xright"] = nx / ny * yright

    xright = number("xright")
    if xright is not None:
        if "xpo_l_rel" not in metadata and "xpo_l" in case_values:
            metadata["xpo_l_rel"] = case_values["xpo_l"]
        if "xpo_r_rel" not in metadata and "xpo_r" in case_values:
            metadata["xpo_r_rel"] = case_values["xpo_r"]
        if ("xpo_l" not in metadata or "xpo_l" not in remarks_values) and "xpo_l_rel" in metadata:
            metadata["xpo_l"] = float(metadata["xpo_l_rel"]) * xright
        if ("xpo_r" not in metadata or "xpo_r" not in remarks_values) and "xpo_r_rel" in metadata:
            metadata["xpo_r"] = float(metadata["xpo_r_rel"]) * xright

    k0 = number("K0")
    if "eta_eq" not in metadata and k0 is not None:
        metadata["eta_eq"] = k0 / (1.0 + k0)

    end_t = number("endT")
    total_count = number("total_count")
    if end_t is not None and total_count and total_count > 0:
        metadata["dt_output"] = end_t / total_count

    keys = {
        "case_number",
        "lam",
        "Pe",
        "Pe2",
        "Da",
        "K0",
        "eps",
        "alpha",
        "nx",
        "ny",
        "h",
        "xleft",
        "xright",
        "yleft",
        "yright",
        "xpo_l",
        "xpo_r",
        "xpo_l_rel",
        "xpo_r_rel",
        "endT",
        "total_count",
        "dt_initial",
        "dt_output",
        "eta_eq",
        "final_sim_time",
        "final_eta_ave",
        "final_rel_error",
        "actual_iterations",
        "output_file_count",
        "converged",
        "nan_events",
    }
    return {key: metadata[key] for key in keys if key in metadata}


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
    native_cmake = native_cmake_probe()
    wsl = wsl_probe()
    logical_processors = os.cpu_count() or 1
    current_num_threads = read_num_threads_config()
    input_exists = INPUT_DIR.exists()
    output_ready = OUTPUT_DIR.exists() or OUTPUT_DIR.parent.exists()
    native_executable = locate_native_executable()
    executable_exists = native_executable is not None or EXECUTABLE_PATH.exists()
    executable_is_elf = detect_elf(BUILD_DIR / "df2d") or detect_elf(EXECUTABLE_PATH)
    makefile_exists = MAKEFILE_PATH.exists()
    wsl_cmake_available = wsl["available"] and wsl["cmakeAvailable"] and wsl["compilerAvailable"]
    build_mode = None
    if existing_cmake_cache_prefers_wsl() and wsl_cmake_available:
        build_mode = "wsl"
    elif native_cmake["available"]:
        build_mode = "native"
    elif wsl_cmake_available:
        build_mode = "wsl"

    can_run = (
        build_mode is not None
        and input_exists
        and output_ready
    )

    checks = [
        {
            "key": "nativeCMake",
            "label": "Native CMake",
            "ok": native_cmake["available"],
            "details": native_cmake["message"],
        },
        {
            "key": "wsl",
            "label": "WSL CMake fallback",
            "ok": wsl_cmake_available,
            "details": wsl["message"],
        },
        {
            "key": "cmake",
            "label": "CMake build mode",
            "ok": build_mode is not None,
            "details": f"Using {build_mode} CMake." if build_mode else "No native or WSL CMake toolchain was found.",
        },
        {
            "key": "compiler",
            "label": "Compiler toolchain",
            "ok": native_cmake["available"] or wsl["compilerAvailable"],
            "details": (
                "Native CMake will select the local compiler."
                if native_cmake["available"]
                else ("Available inside WSL." if wsl["compilerAvailable"] else "No compiler was found.")
            ),
        },
        {"key": "makefile", "label": "legacy makefile", "ok": makefile_exists, "details": str(MAKEFILE_PATH)},
        {
            "key": "executable",
            "label": "df2d executable",
            "ok": executable_exists,
            "details": str(native_executable or EXECUTABLE_PATH) if executable_exists else "Will be generated under build/.",
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
            "build": str(BUILD_DIR),
            "executable": str(native_executable or EXECUTABLE_PATH),
        },
        "buildMode": build_mode,
        "wslCMakeAvailable": wsl_cmake_available,
        "canRun": can_run,
        "executableIsElf": executable_is_elf,
        "logicalProcessors": logical_processors,
        "numThreads": current_num_threads,
        "checks": checks,
    }


def _float_or_none(value: Any) -> float | None:
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    return number if math.isfinite(number) else None


def _finite_number(value: Any) -> float | None:
    return _float_or_none(value)


def _seconds_since(iso_value: str | None) -> float | None:
    if not iso_value:
        return None
    try:
        return max(0.0, (datetime.now() - datetime.fromisoformat(iso_value)).total_seconds())
    except ValueError:
        return None


def _percentile(values: list[float], fraction: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, math.ceil(len(ordered) * fraction) - 1))
    return ordered[index]


def _compact_reason(reason: str) -> str:
    cleaned = ANSI_ESCAPE_PATTERN.sub("", reason).strip()
    cleaned = " ".join(cleaned.split())
    return cleaned[:180]


def _severity_rank(item: dict[str, Any]) -> int:
    ranks = {"failed": 0, "stopped": 1, "suspicious": 2, "running": 3, "finished": 4, "waiting": 5}
    return ranks.get(str(item.get("status", "waiting")), 9)


@dataclass
class TaskState:
    lock: threading.Lock = field(default_factory=threading.Lock)
    status: str = "idle"
    stage: str = "idle"
    case_id: int | None = None
    mode: str = "single"
    case_ids: list[int] = field(default_factory=list)
    total_cases: int = 0
    completed_cases: int = 0
    failed_cases: int = 0
    force_restart: bool = False
    started_at: str | None = None
    finished_at: str | None = None
    build_exit_code: int | None = None
    run_exit_code: int | None = None
    error: str | None = None
    build_log: list[str] = field(default_factory=list)
    run_log: list[str] = field(default_factory=list)
    completed_case_ids: set[int] = field(default_factory=set)
    case_records: dict[int, dict[str, Any]] = field(default_factory=dict)
    active_process: subprocess.Popen[str] | None = None
    stop_requested: bool = False

    def reset(self, case_ids: list[int], force_restart: bool, mode: str = "single") -> None:
        self.status = "running"
        self.stage = "building"
        self.case_id = case_ids[0] if case_ids else None
        self.mode = mode
        self.case_ids = list(case_ids)
        self.total_cases = len(case_ids)
        self.completed_cases = 0
        self.failed_cases = 0
        self.force_restart = force_restart
        self.started_at = datetime.now().isoformat(timespec="seconds")
        self.finished_at = None
        self.build_exit_code = None
        self.run_exit_code = None
        self.error = None
        self.build_log = []
        self.run_log = []
        self.completed_case_ids = set()
        self.case_records = {case_id: self._new_case_record(case_id) for case_id in case_ids}
        self.active_process = None
        self.stop_requested = False

    def _new_case_record(self, case_id: int) -> dict[str, Any]:
        values = load_case_values(case_id)
        k0 = _finite_number(values.get("K0"))
        eta_eq = k0 / (1.0 + k0) if k0 and k0 > 0 else None
        return {
            "id": case_id,
            "status": "waiting",
            "alpha": _finite_number(values.get("alpha")),
            "Pe2": _finite_number(values.get("Pe2")),
            "Pe": _finite_number(values.get("Pe")),
            "etaEq": eta_eq,
            "progressPercent": 0.0,
            "durationSeconds": None,
            "eta": None,
            "err": None,
            "backend": None,
            "startedAt": None,
            "finishedAt": None,
            "alertReason": "",
            "progressSamples": [],
            "log": [],
        }

    def append_log(self, stage: str, line: str) -> None:
        target = self.build_log if stage == "building" else self.run_log
        cleaned = ANSI_ESCAPE_PATTERN.sub("", line).rstrip("\n")
        target.append(cleaned)
        if stage != "building":
            self.update_case_from_log(cleaned)
        if len(target) > 2000:
            del target[: len(target) - 2000]

    def _record_case_log(self, case_id: int, line: str) -> dict[str, Any] | None:
        record = self.case_records.get(case_id)
        if record is None:
            return None
        record["log"].append(line)
        if len(record["log"]) > 180:
            del record["log"][: len(record["log"]) - 180]
        return record

    def update_case_from_log(self, line: str) -> None:
        for tag in CASE_TAG_PATTERN.finditer(line):
            self._record_case_log(int(tag.group(1)), line)

        progress = CASE_PROGRESS_PATTERN.search(line)
        if progress:
            case_id = int(progress.group(1))
            record = self.case_records.get(case_id)
            if record is None:
                return
            self._mark_case_started(record)
            record["backend"] = progress.group(2)
            record["progressPercent"] = max(record["progressPercent"] or 0.0, _float_or_none(progress.group(3)) or 0.0)
            record["eta"] = _float_or_none(progress.group(4))
            record["err"] = _float_or_none(progress.group(5))
            record["durationSeconds"] = _float_or_none(progress.group(6))
            samples = record["progressSamples"]
            samples.append({
                "progressPercent": record["progressPercent"],
                "eta": record["eta"],
                "err": record["err"],
                "durationSeconds": record["durationSeconds"],
            })
            if len(samples) > 8:
                del samples[: len(samples) - 8]
            self._refresh_case_alert(record)
            return

        done = CASE_DONE_DETAIL_PATTERN.search(line)
        if done:
            self.mark_case_finished(
                int(done.group(1)),
                backend=done.group(2),
                eta=_float_or_none(done.group(4)),
                duration=_float_or_none(done.group(5)),
            )
            return

        error = CASE_ERROR_PATTERN.search(line)
        if error:
            self.mark_case_failed(int(error.group(1)), line)

    def _mark_case_started(self, record: dict[str, Any]) -> None:
        if record["status"] in {"waiting", "stopped"}:
            record["status"] = "running"
        if not record["startedAt"]:
            record["startedAt"] = datetime.now().isoformat(timespec="seconds")

    def mark_case_finished(self, case_id: int, backend: str | None = None, eta: float | None = None, duration: float | None = None) -> None:
        record = self.case_records.get(case_id)
        if record is not None:
            if record.get("status") in {"failed", "stopped"}:
                return
            self._mark_case_started(record)
            record["status"] = "finished"
            record["progressPercent"] = 100.0
            if backend:
                record["backend"] = backend
            if eta is not None:
                record["eta"] = eta
            if duration is not None:
                record["durationSeconds"] = duration
            elif record["startedAt"] and record["durationSeconds"] is None:
                record["durationSeconds"] = _seconds_since(record["startedAt"])
            record["finishedAt"] = datetime.now().isoformat(timespec="seconds")
            record["alertReason"] = ""
            self._refresh_completed_case_health(record)
        if case_id in self.case_ids and case_id not in self.completed_case_ids:
            self.completed_case_ids.add(case_id)
        self.completed_cases = len(self.completed_case_ids)
        self.failed_cases = sum(1 for item in self.case_records.values() if item["status"] == "failed")

    def mark_case_failed(self, case_id: int, reason: str = "solver error") -> None:
        record = self.case_records.get(case_id)
        if record is not None:
            self._mark_case_started(record)
            record["status"] = "failed"
            record["finishedAt"] = datetime.now().isoformat(timespec="seconds")
            if record["durationSeconds"] is None and record["startedAt"]:
                record["durationSeconds"] = _seconds_since(record["startedAt"])
            record["alertReason"] = _compact_reason(reason)
        self.failed_cases = sum(1 for item in self.case_records.values() if item["status"] == "failed")

    def mark_waiting_as_stopped(self) -> None:
        for record in self.case_records.values():
            if record["status"] in {"waiting", "running", "suspicious"}:
                record["status"] = "stopped"
                record["finishedAt"] = datetime.now().isoformat(timespec="seconds")
                if record["durationSeconds"] is None and record["startedAt"]:
                    record["durationSeconds"] = _seconds_since(record["startedAt"])
                record["alertReason"] = "stopped"

    def _refresh_case_alert(self, record: dict[str, Any]) -> None:
        eta = _finite_number(record.get("eta"))
        eta_eq = _finite_number(record.get("etaEq"))
        err_values = [
            _finite_number(sample.get("err"))
            for sample in record.get("progressSamples", [])[-3:]
            if _finite_number(sample.get("err")) is not None
        ]
        reason = ""
        if eta is None or not math.isfinite(eta):
            reason = "eta is not finite"
        elif eta_eq and eta > eta_eq * 1.2:
            reason = "eta exceeds 120% of eta_eq"
        elif len(err_values) >= 3 and err_values[-1] >= err_values[0] * 0.98:
            reason = "err is not decreasing"
        record["alertReason"] = reason
        if reason and record["status"] == "running":
            record["status"] = "suspicious"
        elif not reason and record["status"] == "suspicious":
            record["status"] = "running"

    def _refresh_completed_case_health(self, record: dict[str, Any]) -> None:
        remarks = parse_remarks_values(locate_remarks_file(int(record["id"])))
        if int(remarks.get("nan_events", 0) or 0) > 0:
            record["status"] = "suspicious"
            record["alertReason"] = "NaN or instability events"
        elif int(remarks.get("instability_events", 0) or 0) > 0:
            record["status"] = "suspicious"
            record["alertReason"] = "instability events"

    def _case_public(self, record: dict[str, Any]) -> dict[str, Any]:
        duration = record.get("durationSeconds")
        if record.get("status") in {"running", "suspicious"} and duration is None and record.get("startedAt"):
            duration = _seconds_since(record.get("startedAt"))
        return {
            "id": record["id"],
            "alpha": record.get("alpha"),
            "Pe2": record.get("Pe2"),
            "Pe": record.get("Pe"),
            "status": record.get("status", "waiting"),
            "progressPercent": round(float(record.get("progressPercent") or 0.0), 3),
            "durationSeconds": round(float(duration), 3) if duration is not None else None,
            "eta": record.get("eta"),
            "err": record.get("err"),
            "backend": record.get("backend"),
            "startedAt": record.get("startedAt"),
            "finishedAt": record.get("finishedAt"),
            "alertReason": record.get("alertReason") or "",
        }

    def dashboard_snapshot(self) -> dict[str, Any]:
        cases = [self._case_public(record) for record in self.case_records.values()]
        completed = sum(1 for item in cases if item["status"] == "finished")
        failed = sum(1 for item in cases if item["status"] == "failed")
        stopped = sum(1 for item in cases if item["status"] == "stopped")
        suspicious = sum(1 for item in cases if item["status"] == "suspicious")
        running = sum(1 for item in cases if item["status"] in {"running", "suspicious"})
        waiting = sum(1 for item in cases if item["status"] == "waiting")
        total = self.total_cases or len(cases)
        progress_sum = sum(float(item.get("progressPercent") or 0.0) for item in cases)
        progress_percent = progress_sum / total if total else 0.0
        elapsed = _seconds_since(self.started_at) if self.started_at else 0.0
        cases_per_hour = (completed / elapsed * 3600.0) if completed > 0 and elapsed and elapsed > 0 else 0.0
        remaining = max(0, total - completed - failed - stopped)
        eta_seconds = (remaining / (cases_per_hour / 3600.0)) if cases_per_hour > 0 else None
        failure_rate = failed / total if total else 0.0
        durations = sorted(
            float(item["durationSeconds"])
            for item in cases
            if item["durationSeconds"] is not None and item["status"] in {"finished", "suspicious"}
        )
        duration_p95 = _percentile(durations, 0.95) if len(durations) >= 20 else None
        if duration_p95 is not None:
            for item in cases:
                duration = item.get("durationSeconds")
                if duration is not None and duration > duration_p95 and item["status"] in {"finished", "running", "suspicious"}:
                    item["alertReason"] = item["alertReason"] or "duration > P95"

        attention = [
            item for item in cases
            if item["status"] in {"failed", "stopped", "suspicious"} or item.get("alertReason")
        ]
        attention.sort(key=lambda item: (_severity_rank(item), -(item.get("durationSeconds") or 0), item["id"]))
        running_cases = sorted(
            [item for item in cases if item["status"] in {"running", "suspicious"}],
            key=lambda item: (-(item.get("durationSeconds") or 0), item["id"]),
        )
        recent_finished = sorted(
            [item for item in cases if item.get("finishedAt") and item["status"] == "finished"],
            key=lambda item: item.get("finishedAt") or "",
            reverse=True,
        )

        return {
            "kpi": {
                "total": total,
                "completed": completed,
                "running": running,
                "failed": failed,
                "waiting": waiting,
                "stopped": stopped,
                "suspicious": suspicious,
                "progressPercent": progress_percent,
                "casesPerHour": cases_per_hour,
                "etaSeconds": eta_seconds,
                "failureRate": failure_rate,
            },
            "cases": sorted(cases, key=lambda item: item["id"]),
            "runningCases": running_cases[:36],
            "attentionCases": attention[:120],
            "recentFinished": recent_finished[:36],
            "durationP95": duration_p95,
        }

    def case_log_payload(self, case_id: int) -> dict[str, Any]:
        record = self.case_records.get(case_id) or self._new_case_record(case_id)
        error_lines = [
            line for line in [*self.build_log, *self.run_log]
            if "error" in line.lower() or "failed" in line.lower() or f"[Case {case_id}]" in line
        ][-220:]
        return {
            "caseId": case_id,
            "case": self._case_public(record),
            "parameters": {
                "alpha": record.get("alpha"),
                "Pe2": record.get("Pe2"),
                "Pe": record.get("Pe"),
                "etaEq": record.get("etaEq"),
            },
            "runLog": record.get("log", [])[-220:],
            "diagnostics": error_lines,
            "buildLogTail": self.build_log[-120:],
            "globalRunLogTail": self.run_log[-120:],
        }

    def snapshot(self) -> dict[str, Any]:
        return {
            "status": self.status,
            "stage": self.stage,
            "caseId": self.case_id,
            "mode": self.mode,
            "caseIds": self.case_ids,
            "totalCases": self.total_cases,
            "completedCases": self.completed_cases,
            "failedCases": self.failed_cases,
            "forceRestart": self.force_restart,
            "startedAt": self.started_at,
            "finishedAt": self.finished_at,
            "buildExitCode": self.build_exit_code,
            "runExitCode": self.run_exit_code,
            "error": self.error,
            "buildLog": self.build_log[-500:],
            "runLog": self.run_log[-500:],
            "stopRequested": self.stop_requested,
            "dashboard": self.dashboard_snapshot(),
        }


TASK_STATE = TaskState()


@dataclass
class WarmupState:
    lock: threading.Lock = field(default_factory=threading.Lock)
    status: str = "idle"
    candidates: list[int] = field(default_factory=list)
    case_ids: list[int] = field(default_factory=list)
    current: int | None = None
    results: list[dict[str, Any]] = field(default_factory=list)
    log: list[str] = field(default_factory=list)
    best: dict[str, Any] | None = None
    started_at: str | None = None
    finished_at: str | None = None
    error: str | None = None
    active_process: subprocess.Popen[str] | None = None
    stop_requested: bool = False
    sample_seed: int | None = None

    def reset(self, candidates: list[int], case_ids: list[int], sample_seed: int) -> None:
        self.status = "running"
        self.candidates = list(candidates)
        self.case_ids = list(case_ids)
        self.current = None
        self.results = []
        self.log = []
        self.best = None
        self.started_at = datetime.now().isoformat(timespec="seconds")
        self.finished_at = None
        self.error = None
        self.active_process = None
        self.stop_requested = False
        self.sample_seed = sample_seed

    def append_log(self, line: str) -> None:
        cleaned = ANSI_ESCAPE_PATTERN.sub("", line).rstrip("\n")
        self.log.append(cleaned)
        if len(self.log) > 1000:
            del self.log[: len(self.log) - 1000]

    def add_result(self, result: dict[str, Any]) -> None:
        self.results.append(result)
        result_makespan = result.get("estimatedMakespan")
        best_makespan = self.best.get("estimatedMakespan") if self.best else None
        if self.best is None:
            self.best = result
        elif result_makespan is not None and best_makespan is not None:
            if result_makespan < best_makespan:
                self.best = result
        elif result.get("iterationsPerSecond", 0.0) > self.best.get("iterationsPerSecond", 0.0):
            self.best = result

    def snapshot(self) -> dict[str, Any]:
        return {
            "status": self.status,
            "candidates": self.candidates,
            "caseIds": self.case_ids,
            "current": self.current,
            "results": self.results,
            "best": self.best,
            "startedAt": self.started_at,
            "finishedAt": self.finished_at,
            "error": self.error,
            "log": self.log[-300:],
            "stopRequested": self.stop_requested,
            "logicalProcessors": os.cpu_count() or 1,
            "numThreads": read_num_threads_config(),
            "sampleSeed": self.sample_seed,
        }


WARMUP_STATE = WarmupState()


def estimate_warmup_makespan(result: dict[str, Any], case_count: int) -> dict[str, Any]:
    normalized_case_count = max(1, case_count)
    concurrency = max(1, int(result.get("concurrency", 1)))
    waves = math.ceil(normalized_case_count / concurrency)

    result["estimatedWaves"] = waves
    result["estimatedCaseCount"] = normalized_case_count
    tail_seconds = max(0.0, float(result.get("tailP90SecondsPerCase", 0.0) or 0.0))
    if tail_seconds > 0.0:
        result["estimatedMakespan"] = waves * tail_seconds
        result["tailAware"] = True
        return result

    per_worker_rate = max(0.0, float(result.get("perWorkerIterationsPerSecond", 0.0)))
    total_rate = max(0.0, float(result.get("iterationsPerSecond", 0.0)))
    rate = per_worker_rate if per_worker_rate > 0.0 else total_rate / concurrency
    result["estimatedMakespan"] = (waves / rate) if rate > 0.0 else None
    result["tailAware"] = False
    return result


def stream_process_output(process: subprocess.Popen[str], stage: str) -> int:
    assert process.stdout is not None
    for line in iter(process.stdout.readline, ""):
        stripped = line.strip()
        if "\x00" in line or "\ufffd" in line or stripped.lower().startswith("wsl:"):
            continue
        lowered = stripped.lower()
        if "clock skew detected" in lowered or "has modification time" in lowered:
            continue
        with TASK_STATE.lock:
            TASK_STATE.append_log(stage, line)
        if TASK_STATE.stop_requested:
            try:
                process.terminate()
            except OSError:
                pass
            break
    return process.wait()


def run_task_command(command: list[str], stage: str, cwd: Path = PROJECT_ROOT) -> int:
    with TASK_STATE.lock:
        TASK_STATE.append_log(stage, f"[info] Running: {subprocess.list2cmdline(command)}")

    process = subprocess.Popen(
        command,
        cwd=str(cwd),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    with TASK_STATE.lock:
        TASK_STATE.active_process = process
    try:
        return stream_process_output(process, stage)
    finally:
        with TASK_STATE.lock:
            if TASK_STATE.active_process is process:
                TASK_STATE.active_process = None


def stream_warmup_output(process: subprocess.Popen[str]) -> tuple[int, dict[str, Any] | None]:
    assert process.stdout is not None
    parsed_result: dict[str, Any] | None = None
    for line in iter(process.stdout.readline, ""):
        stripped = line.replace("\x00", "").replace("\ufffd", "").strip()
        json_start = stripped.find("{")
        json_end = stripped.rfind("}")
        if json_start >= 0 and json_end > json_start:
            try:
                payload = json.loads(stripped[json_start : json_end + 1])
                if payload.get("benchmark"):
                    parsed_result = payload
            except json.JSONDecodeError:
                pass
        if not stripped or stripped.lower().startswith("wsl:"):
            continue
        with WARMUP_STATE.lock:
            WARMUP_STATE.append_log(stripped)
        if WARMUP_STATE.stop_requested:
            try:
                process.terminate()
            except OSError:
                pass
            break
    return process.wait(), parsed_result


def run_warmup_command(command: list[str], cwd: Path = PROJECT_ROOT) -> tuple[int, dict[str, Any] | None]:
    with WARMUP_STATE.lock:
        WARMUP_STATE.append_log(f"[info] Running: {subprocess.list2cmdline(command)}")
    process = subprocess.Popen(
        command,
        cwd=str(cwd),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    with WARMUP_STATE.lock:
        WARMUP_STATE.active_process = process
    try:
        return stream_warmup_output(process)
    finally:
        with WARMUP_STATE.lock:
            if WARMUP_STATE.active_process is process:
                WARMUP_STATE.active_process = None


def build_solver_for_warmup(env: dict[str, Any]) -> tuple[str, list[str]]:
    build_mode = env["buildMode"]
    wsl_fallback_available = bool(env.get("wslCMakeAvailable"))
    if build_mode == "native":
        clear_cmake_cache_for_wsl()
        configure_exit_code, _ = run_warmup_command(
            ["cmake", "-S", str(PROJECT_ROOT), "-B", str(BUILD_DIR), "-DCMAKE_BUILD_TYPE=Release"]
        )
        if configure_exit_code != 0:
            if not wsl_fallback_available:
                raise RuntimeError("CMake configure failed.")
            with WARMUP_STATE.lock:
                WARMUP_STATE.append_log("[warn] Native CMake configure failed; falling back to WSL CMake.")
            build_mode = "wsl"
        else:
            build_exit_code, _ = run_warmup_command(
                ["cmake", "--build", str(BUILD_DIR), "--config", "Release", "--target", "df2d"]
            )
            run_executable = locate_native_executable()
            if build_exit_code != 0:
                if not wsl_fallback_available:
                    raise RuntimeError("Build failed.")
                with WARMUP_STATE.lock:
                    WARMUP_STATE.append_log("[warn] Native build failed; falling back to WSL CMake.")
                build_mode = "wsl"
            elif run_executable is not None:
                return "native", [str(run_executable)]
            elif not wsl_fallback_available:
                raise RuntimeError("Build succeeded, but df2d executable was not found under build/.")
            else:
                with WARMUP_STATE.lock:
                    WARMUP_STATE.append_log("[warn] Native build did not produce df2d under build/; falling back to WSL CMake.")
                build_mode = "wsl"

    if build_mode == "wsl":
        clear_cmake_cache_for_wsl()
        project_root_wsl = to_wsl_path(PROJECT_ROOT)
        configure_script = (
            f"cd {shlex.quote(project_root_wsl)} && "
            "cmake -S . -B build -DCMAKE_BUILD_TYPE=Release"
        )
        configure_exit_code, _ = run_warmup_command(["wsl", "-e", "bash", "-lc", configure_script])
        if configure_exit_code != 0:
            raise RuntimeError("CMake configure failed.")
        build_script = (
            f"cd {shlex.quote(project_root_wsl)} && "
            "sleep 1 && cmake --build build --config Release --target df2d"
        )
        build_exit_code, _ = run_warmup_command(["wsl", "-e", "bash", "-lc", build_script])
        if build_exit_code != 0:
            raise RuntimeError("Build failed.")
        return "wsl", ["wsl", "-e", "bash", "-lc"]

    raise RuntimeError("No build mode is available.")


def warmup_sample_size(case_count: int, concurrency: int) -> int:
    return min(case_count, max(4 * max(1, concurrency), 32))


def sample_warmup_cases(case_ids: list[int], concurrency: int, sample_seed: int) -> list[int]:
    if not case_ids:
        return []
    sample_size = warmup_sample_size(len(case_ids), concurrency)
    rng = random.Random((sample_seed * 1000003) + concurrency)
    if sample_size >= len(case_ids):
        sampled = list(case_ids)
        rng.shuffle(sampled)
        return sampled
    return rng.sample(case_ids, sample_size)


def numeric_list(payload: dict[str, Any], key: str) -> list[float]:
    values = payload.get(key, [])
    if not isinstance(values, list):
        return []
    result: list[float] = []
    for value in values:
        try:
            number = float(value)
        except (TypeError, ValueError):
            continue
        if math.isfinite(number):
            result.append(number)
    return result


def integer_list(payload: dict[str, Any], key: str) -> list[int]:
    values = payload.get(key, [])
    if not isinstance(values, list):
        return []
    result: list[int] = []
    for value in values:
        try:
            result.append(int(value))
        except (TypeError, ValueError):
            continue
    return result


def run_warmup_benchmarks(case_ids: list[int],
                          candidates: list[int],
                          seconds: float,
                          warmup_seconds: float,
                          sample_seed: int) -> None:
    with WARMUP_STATE.lock:
        WARMUP_STATE.reset(candidates, case_ids, sample_seed)
        WARMUP_STATE.append_log(
            f"[info] Starting warmup with {len(candidates)} candidate(s), {len(case_ids)} case(s), sample seed {sample_seed}."
        )

    try:
        env = environment_snapshot()
        if not env["canRun"]:
            raise RuntimeError("Build environment is not ready.")
        run_mode, run_command_args = build_solver_for_warmup(env)
        if WARMUP_STATE.stop_requested:
            raise RuntimeError("Warmup stopped during build.")

        project_root_wsl = to_wsl_path(PROJECT_ROOT)

        for candidate in candidates:
            sampled_case_ids = sample_warmup_cases(case_ids, candidate, sample_seed)
            case_arg = ",".join(str(case_id) for case_id in sampled_case_ids)
            with WARMUP_STATE.lock:
                if WARMUP_STATE.stop_requested:
                    break
                WARMUP_STATE.current = candidate
                WARMUP_STATE.append_log(
                    f"[info] Benchmarking concurrency {candidate} with {len(sampled_case_ids)} sampled case(s), seed {sample_seed}."
                )

            solver_args = [
                "--benchmark-concurrency", str(candidate),
                "--benchmark-cases", case_arg,
                "--benchmark-seconds", str(seconds),
                "--benchmark-warmup-seconds", str(warmup_seconds),
            ]
            if run_mode == "native":
                command = run_command_args + solver_args
            else:
                script = (
                    f"cd {shlex.quote(project_root_wsl)} && "
                    + " ".join(shlex.quote(part) for part in ["./build/df2d", *solver_args])
                )
                command = run_command_args + [script]

            exit_code, payload = run_warmup_command(command)
            if exit_code != 0:
                raise RuntimeError(f"Benchmark candidate {candidate} failed.")
            if payload is None:
                raise RuntimeError(f"Benchmark candidate {candidate} did not return JSON.")

            result = {
                "concurrency": int(payload.get("concurrency", candidate)),
                "iterationsPerSecond": float(payload.get("iterations_per_second", 0.0)),
                "perWorkerIterationsPerSecond": float(payload.get("iterations_per_second_per_worker", 0.0)),
                "totalIterations": int(payload.get("total_iterations", 0)),
                "measurementSeconds": float(payload.get("measurement_seconds", seconds)),
                "sampleCaseCount": int(payload.get("sample_case_count", len(sampled_case_ids))),
                "sampleCaseIds": sampled_case_ids,
                "sampleSeed": sample_seed,
                "workerCount": int(payload.get("worker_count", payload.get("concurrency", candidate))),
                "workerIterations": integer_list(payload, "worker_iterations"),
                "workerIterationsPerSecond": numeric_list(payload, "worker_iterations_per_second"),
                "workerThroughputMin": float(payload.get("worker_throughput_min", 0.0)),
                "workerThroughputMedian": float(payload.get("worker_throughput_median", 0.0)),
                "workerThroughputP90": float(payload.get("worker_throughput_p90", 0.0)),
                "workerThroughputSlowest": float(payload.get("worker_throughput_slowest", 0.0)),
                "tailP90SecondsPerCase": float(payload.get("tail_p90_seconds_per_case", 0.0)),
                "status": "done",
            }
            estimate_warmup_makespan(result, len(case_ids))
            with WARMUP_STATE.lock:
                WARMUP_STATE.add_result(result)

        with WARMUP_STATE.lock:
            WARMUP_STATE.finished_at = datetime.now().isoformat(timespec="seconds")
            if WARMUP_STATE.stop_requested:
                WARMUP_STATE.status = "stopped"
                WARMUP_STATE.append_log("[warn] Warmup stopped by user.")
            else:
                WARMUP_STATE.status = "finished"
                WARMUP_STATE.append_log("[info] Warmup finished.")
    except Exception as exc:  # noqa: BLE001
        with WARMUP_STATE.lock:
            WARMUP_STATE.status = "stopped" if WARMUP_STATE.stop_requested else "failed"
            WARMUP_STATE.error = None if WARMUP_STATE.stop_requested else str(exc)
            WARMUP_STATE.finished_at = datetime.now().isoformat(timespec="seconds")
            WARMUP_STATE.append_log(f"[error] {exc}")


def run_build_and_solver(case_ids: list[int], force_restart: bool = False, mode: str = "single") -> None:
    with TASK_STATE.lock:
        TASK_STATE.reset(case_ids, force_restart, mode)
        if mode == "batch":
            TASK_STATE.append_log("building", f"[info] Preparing batch build for {len(case_ids)} cases.")
        else:
            TASK_STATE.append_log("building", f"[info] Preparing build for case {case_ids[0]}.")
        if force_restart:
            TASK_STATE.append_log("building", "[info] Force restart requested for this run.")

    env = environment_snapshot()
    if not env["canRun"]:
        with TASK_STATE.lock:
            TASK_STATE.status = "failed"
            TASK_STATE.stage = "building"
            TASK_STATE.error = "Build environment is not ready."
            TASK_STATE.finished_at = datetime.now().isoformat(timespec="seconds")
            TASK_STATE.append_log("building", "[error] Build environment is not ready.")
        return

    try:
        build_mode = env["buildMode"]
        wsl_fallback_available = bool(env.get("wslCMakeAvailable"))
        if build_mode == "native":
            clear_cmake_cache_for_wsl()
            configure_exit_code = run_task_command(
                ["cmake", "-S", str(PROJECT_ROOT), "-B", str(BUILD_DIR), "-DCMAKE_BUILD_TYPE=Release"],
                "building",
            )
            if configure_exit_code != 0 or TASK_STATE.stop_requested:
                if TASK_STATE.stop_requested or not wsl_fallback_available:
                    raise RuntimeError("CMake configure failed.")
                with TASK_STATE.lock:
                    TASK_STATE.append_log(
                        "building",
                        "[warn] Native CMake configure failed; falling back to WSL CMake.",
                    )
                build_mode = "wsl"
            else:
                build_exit_code = run_task_command(
                    ["cmake", "--build", str(BUILD_DIR), "--config", "Release", "--target", "df2d"],
                    "building",
                )
                run_executable = locate_native_executable()
                if build_exit_code == 0 and run_executable is None:
                    if not wsl_fallback_available:
                        raise RuntimeError("Build succeeded, but df2d executable was not found under build/.")
                    with TASK_STATE.lock:
                        TASK_STATE.append_log(
                            "building",
                            "[warn] Native build did not produce df2d under build/; falling back to WSL CMake.",
                        )
                    build_mode = "wsl"
                else:
                    run_command_args = [str(run_executable)] if run_executable else []

        if build_mode == "wsl":
            clear_cmake_cache_for_wsl()
            project_root_wsl = to_wsl_path(PROJECT_ROOT)
            configure_script = (
                f"cd {shlex.quote(project_root_wsl)} && "
                "cmake -S . -B build -DCMAKE_BUILD_TYPE=Release"
            )
            configure_exit_code = run_task_command(
                ["wsl", "-e", "bash", "-lc", configure_script],
                "building",
            )
            if configure_exit_code != 0 or TASK_STATE.stop_requested:
                raise RuntimeError("CMake configure failed.")

            build_script = (
                f"cd {shlex.quote(project_root_wsl)} && "
                "sleep 1 && cmake --build build --config Release --target df2d"
            )
            build_exit_code = run_task_command(
                ["wsl", "-e", "bash", "-lc", build_script],
                "building",
            )
            run_command_args = ["wsl", "-e", "bash", "-lc"]

    except OSError as exc:
        with TASK_STATE.lock:
            TASK_STATE.status = "failed"
            TASK_STATE.stage = "building"
            TASK_STATE.error = f"Failed to start build: {exc}"
            TASK_STATE.finished_at = datetime.now().isoformat(timespec="seconds")
            TASK_STATE.append_log("building", f"[error] {TASK_STATE.error}")
        return
    except RuntimeError as exc:
        with TASK_STATE.lock:
            TASK_STATE.status = "stopped" if TASK_STATE.stop_requested else "failed"
            TASK_STATE.stage = "building"
            TASK_STATE.error = None if TASK_STATE.stop_requested else str(exc)
            TASK_STATE.finished_at = datetime.now().isoformat(timespec="seconds")
            if TASK_STATE.stop_requested:
                TASK_STATE.append_log("building", "[warn] Task stopped during build.")
            else:
                TASK_STATE.append_log("building", f"[error] {exc}")
        return

    with TASK_STATE.lock:
        TASK_STATE.build_exit_code = build_exit_code

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

    with TASK_STATE.lock:
        TASK_STATE.stage = "running"
        TASK_STATE.append_log("run", f"[info] Build succeeded. Running {len(case_ids)} case(s).")

    overall_exit_code = 0
    chunks = [
        case_ids[index : index + BATCH_CASE_CHUNK_SIZE]
        for index in range(0, len(case_ids), BATCH_CASE_CHUNK_SIZE)
    ]

    for chunk_index, chunk in enumerate(chunks, start=1):
        if TASK_STATE.stop_requested:
            break

        if mode == "single" and len(chunk) == 1:
            run_args = ["--case", str(chunk[0])]
        else:
            run_args = ["--cases", ",".join(str(case_id) for case_id in chunk)]
        if force_restart:
            run_args.append("--force-restart")

        if run_command_args and run_command_args[0] != "wsl":
            run_command = run_command_args + run_args
            log_run_command = [Path(run_command_args[0]).name] + run_args
        else:
            project_root_wsl = to_wsl_path(PROJECT_ROOT)
            solver_args = ["./build/df2d"] + run_args
            run_script = (
                f"cd {shlex.quote(project_root_wsl)} && "
                + " ".join(shlex.quote(part) for part in solver_args)
            )
            run_command = run_command_args + [run_script]
            log_run_command = solver_args

        with TASK_STATE.lock:
            TASK_STATE.append_log(
                "run",
                f"[info] Chunk {chunk_index}/{len(chunks)}: {' '.join(log_run_command)}",
            )

        try:
            run_exit_code = run_task_command(run_command, "run")
        except OSError as exc:
            with TASK_STATE.lock:
                TASK_STATE.status = "failed"
                TASK_STATE.stage = "running"
                TASK_STATE.error = f"Failed to start solver: {exc}"
                TASK_STATE.finished_at = datetime.now().isoformat(timespec="seconds")
                TASK_STATE.append_log("run", f"[error] {TASK_STATE.error}")
            return

        with TASK_STATE.lock:
            TASK_STATE.run_exit_code = run_exit_code
            if run_exit_code == 0:
                for case_id in chunk:
                    TASK_STATE.mark_case_finished(case_id)
            else:
                TASK_STATE.failed_cases += len(chunk)
                for case_id in chunk:
                    TASK_STATE.mark_case_failed(case_id, f"Chunk {chunk_index}/{len(chunks)} failed with exit code {run_exit_code}.")
                overall_exit_code = run_exit_code
                TASK_STATE.append_log(
                    "run",
                    f"[error] Chunk {chunk_index}/{len(chunks)} failed with exit code {run_exit_code}.",
                )

    with TASK_STATE.lock:
        TASK_STATE.finished_at = datetime.now().isoformat(timespec="seconds")
        if TASK_STATE.stop_requested:
            TASK_STATE.status = "stopped"
            TASK_STATE.mark_waiting_as_stopped()
            TASK_STATE.append_log("run", "[warn] Task stopped by user.")
        elif overall_exit_code == 0:
            TASK_STATE.status = "finished"
            TASK_STATE.append_log("run", "[info] Solver finished successfully.")
        else:
            TASK_STATE.status = "failed"
            TASK_STATE.error = "Solver exited with a non-zero status."
            TASK_STATE.append_log("run", "[error] Solver failed.")


def launch_task(case_id: int, force_restart: bool = False) -> dict[str, Any]:
    with TASK_STATE.lock:
        if TASK_STATE.status == "running":
            raise RuntimeError("Another build/run task is already active.")
    worker = threading.Thread(target=run_build_and_solver, args=([case_id], force_restart, "single"), daemon=True)
    worker.start()
    return {
        "accepted": True,
        "mode": "single",
        "caseId": case_id,
        "caseIds": [case_id],
        "totalCases": 1,
        "forceRestart": force_restart,
    }


def resolve_search_case_ids(query: str) -> list[int]:
    return [
        case_id
        for case_id, file_path in discover_case_files().items()
        if case_matches_query(case_id, file_path, query)
    ]


def launch_search_task(case_query: str, force_restart: bool = False) -> dict[str, Any]:
    case_ids = resolve_search_case_ids(case_query)
    if not case_ids:
        raise ValueError("No cases matched the current search.")
    with TASK_STATE.lock:
        if TASK_STATE.status == "running":
            raise RuntimeError("Another build/run task is already active.")
    worker = threading.Thread(target=run_build_and_solver, args=(case_ids, force_restart, "batch"), daemon=True)
    worker.start()
    return {
        "accepted": True,
        "mode": "batch",
        "caseQuery": case_query,
        "caseIds": case_ids,
        "totalCases": len(case_ids),
        "forceRestart": force_restart,
    }


def parse_candidate_list(raw_candidates: Any, total_cases: int, logical_processors: int) -> list[int]:
    if isinstance(raw_candidates, list):
        tokens = [str(item) for item in raw_candidates]
    else:
        tokens = re.split(r"[\s,]+", str(raw_candidates or "").strip())
    _ = total_cases
    limit = max(1, logical_processors)
    candidates = sorted({
        int(token)
        for token in tokens
        if token and token.isdigit() and 1 <= int(token) <= limit
    })
    if not candidates:
        raise ValueError("No valid warmup candidates were provided.")
    return candidates


def launch_warmup_task(payload: dict[str, Any]) -> dict[str, Any]:
    case_query = str(payload.get("caseQuery", "") or "")
    case_ids = resolve_search_case_ids(case_query)
    if not case_ids:
        raise ValueError("No cases matched the warmup scope.")
    logical_processors = os.cpu_count() or 1
    candidates = parse_candidate_list(payload.get("candidates", ""), len(case_ids), logical_processors)
    seconds = float(payload.get("seconds", 30.0))
    warmup_seconds = float(payload.get("warmupSeconds", 3.0))
    if not math.isfinite(seconds) or not math.isfinite(warmup_seconds) or seconds <= 0.0 or warmup_seconds <= 0.0:
        raise ValueError("Warmup durations must be positive.")
    raw_seed = payload.get("sampleSeed")
    sample_seed = int(raw_seed) if raw_seed not in (None, "") else random.SystemRandom().randrange(1, 2_147_483_647)
    if sample_seed <= 0:
        raise ValueError("sampleSeed must be a positive integer.")

    with TASK_STATE.lock:
        if TASK_STATE.status == "running":
            raise RuntimeError("A build/run task is already active.")
    with WARMUP_STATE.lock:
        if WARMUP_STATE.status == "running":
            raise RuntimeError("A warmup task is already active.")

    worker = threading.Thread(
        target=run_warmup_benchmarks,
        args=(case_ids, candidates, seconds, warmup_seconds, sample_seed),
        daemon=True,
    )
    worker.start()
    return {
        "accepted": True,
        "caseIds": case_ids,
        "candidates": candidates,
        "seconds": seconds,
        "warmupSeconds": warmup_seconds,
        "sampleSeed": sample_seed,
    }


def stop_warmup() -> dict[str, Any]:
    with WARMUP_STATE.lock:
        WARMUP_STATE.stop_requested = True
        process = WARMUP_STATE.active_process
    if process is not None:
        try:
            process.terminate()
        except OSError:
            pass
    return {"stopping": True}


def apply_warmup_concurrency(value: int) -> dict[str, Any]:
    logical_processors = os.cpu_count() or 1
    if value < 0 or value > logical_processors:
        raise ValueError(f"Concurrency must be between 0 and {logical_processors}.")
    with WARMUP_STATE.lock:
        if WARMUP_STATE.status == "running":
            raise RuntimeError("A warmup task is still running.")
    write_num_threads_config(value)
    return {"applied": True, "numThreads": value, "config": str(CONFIG_PATH)}


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
                params = parse_qs(parsed.query)
                query = params.get("q", [""])[0]
                offset = max(0, int(params.get("offset", ["0"])[0] or 0))
                limit = min(200, max(1, int(params.get("limit", ["50"])[0] or 50)))
                discovered = discover_case_files()
                filtered = [
                    (case_id, file_path)
                    for case_id, file_path in discovered.items()
                    if case_matches_query(case_id, file_path, query)
                ]
                page = filtered[offset : offset + limit]
                json_response(
                    self,
                    {
                        "items": [case_summary(case_id, file_path) for case_id, file_path in page],
                        "total": len(filtered),
                        "offset": offset,
                        "limit": limit,
                        "hasMore": offset + limit < len(filtered),
                        "maxId": max(discovered.keys(), default=0),
                    },
                )
                return
            if path.startswith("/api/cases/"):
                case_id = self._extract_id(path, "/api/cases/")
                case_path = discover_case_files().get(case_id) or canonical_case_path(case_id)
                if not case_path.exists():
                    error_response(self, f"Case {case_id} was not found.", status=404)
                    return
                values = parse_case_file(case_path)
                values.update(case_summary(case_id, case_path))
                json_response(self, values)
                return
            if path == "/api/run/status":
                with TASK_STATE.lock:
                    json_response(self, TASK_STATE.snapshot())
                return
            if path.startswith("/api/run/case-log/"):
                case_id = self._extract_id(path, "/api/run/case-log/")
                with TASK_STATE.lock:
                    json_response(self, TASK_STATE.case_log_payload(case_id))
                return
            if path == "/api/warmup/status":
                with WARMUP_STATE.lock:
                    json_response(self, WARMUP_STATE.snapshot())
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
                if payload.get("mode") == "search":
                    result = launch_search_task(
                        str(payload.get("caseQuery", "") or ""),
                        bool(payload.get("forceRestart", False)),
                    )
                else:
                    result = launch_task(
                        int(payload.get("caseId")),
                        bool(payload.get("forceRestart", False)),
                    )
                json_response(self, result, status=202)
                return
            if path == "/api/generate-cases":
                payload = self._read_json_body()
                result = generate_cases(payload)
                json_response(self, result, status=200)
                return
            if path == "/api/run/stop":
                json_response(self, stop_task())
                return
            if path == "/api/warmup/start":
                payload = self._read_json_body()
                json_response(self, launch_warmup_task(payload), status=202)
                return
            if path == "/api/warmup/stop":
                json_response(self, stop_warmup())
                return
            if path == "/api/warmup/apply":
                payload = self._read_json_body()
                json_response(self, apply_warmup_concurrency(int(payload.get("concurrency"))))
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
                "plot": derive_result_metadata(case_id, remarks_path),
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
    parser.add_argument("--port", type=int, default=8000)
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
