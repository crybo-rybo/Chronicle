"""Cartridge library: list / install / pack / resolve."""

from __future__ import annotations

import json
import shutil
import zipfile
from pathlib import Path

from chronicle.cartridge.loader import load_manifest
from chronicle.cartridge.validator import validate_package


def default_library_dir() -> Path:
    return Path.home() / ".chronicle" / "cartridges"


def list_cartridges(library_dir: Path | None = None) -> list[dict[str, str]]:
    root = library_dir or default_library_dir()
    if not root.exists():
        return []
    results: list[dict[str, str]] = []
    for path in sorted(root.iterdir()):
        if not path.is_dir():
            continue
        scenario = path / "scenario.json"
        if not scenario.exists():
            continue
        try:
            manifest = load_manifest(path)
        except Exception:  # noqa: BLE001
            continue
        results.append(
            {
                "id": manifest.id,
                "name": manifest.name,
                "version": manifest.version,
                "path": str(path),
            }
        )
    return results


def install_cartridge(source: str | Path, library_dir: Path | None = None) -> Path:
    src = Path(source).resolve()
    root = library_dir or default_library_dir()
    root.mkdir(parents=True, exist_ok=True)

    if src.is_file() and src.suffix in {".zip", ".chronicle"}:
        staging = root / f".staging_{src.stem}"
        if staging.exists():
            shutil.rmtree(staging)
        staging.mkdir(parents=True)
        with zipfile.ZipFile(src, "r") as zf:
            zf.extractall(staging)
        # If archive contained a single top-level dir, use that.
        children = [c for c in staging.iterdir() if not c.name.startswith(".")]
        package = children[0] if len(children) == 1 and children[0].is_dir() else staging
    elif src.is_dir():
        package = src
    else:
        raise FileNotFoundError(f"Cannot install: {src}")

    issues = validate_package(package)
    errors = [i for i in issues if i.level == "error"]
    if errors:
        raise ValueError("Cartridge failed validation:\n" + "\n".join(str(i) for i in errors))

    manifest = load_manifest(package)
    dest = root / manifest.id
    if dest.exists():
        shutil.rmtree(dest)
    shutil.copytree(package, dest)
    if src.is_file() and (root / f".staging_{src.stem}").exists():
        shutil.rmtree(root / f".staging_{src.stem}", ignore_errors=True)
    return dest


def pack_cartridge(package_dir: str | Path, output: str | Path) -> Path:
    src = Path(package_dir).resolve()
    issues = validate_package(src)
    errors = [i for i in issues if i.level == "error"]
    if errors:
        raise ValueError("Cartridge failed validation:\n" + "\n".join(str(i) for i in errors))
    out = Path(output)
    out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for path in src.rglob("*"):
            if path.is_file():
                zf.write(path, arcname=str(path.relative_to(src)))
    return out


def resolve_scenario(scenario: str | None, library_dir: Path | None = None) -> Path:
    if scenario:
        path = Path(scenario)
        if path.is_dir():
            return path.resolve()
        # treat as library id
        root = library_dir or default_library_dir()
        candidate = root / scenario
        if candidate.is_dir():
            return candidate.resolve()
        raise FileNotFoundError(f"Scenario not found: {scenario}")
    # default: examples/minimal relative to cwd, else bundled
    for candidate in (Path("examples/minimal"), Path("examples/minimal_scenario")):
        if candidate.is_dir():
            return candidate.resolve()
    raise FileNotFoundError("No scenario specified and no examples/minimal found")


def inspect_package(package_dir: Path) -> dict:
    manifest = load_manifest(package_dir)
    issues = validate_package(package_dir)
    errors = [str(i) for i in issues if i.level == "error"]
    warnings = [str(i) for i in issues if i.level == "warning"]
    return {
        "id": manifest.id,
        "name": manifest.name,
        "version": manifest.version,
        "schema": manifest.chronicle_schema_version,
        "path": str(package_dir),
        "metadata": manifest.metadata.model_dump(),
        "ready": not errors,
        "errors": errors,
        "warnings": warnings,
    }


def write_json(path: Path, data: dict) -> None:
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
