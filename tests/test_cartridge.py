from pathlib import Path

from chronicle.cartridge.loader import load_package
from chronicle.cartridge.validator import validate_package

ROOT = Path(__file__).resolve().parents[1]
MINIMAL = ROOT / "examples" / "minimal"
BROKEN_WHEEL = ROOT / "examples" / "broken_wheel"


def test_minimal_validates():
    issues = validate_package(MINIMAL)
    errors = [i for i in issues if i.level == "error"]
    assert errors == [], errors


def test_broken_wheel_validates():
    issues = validate_package(BROKEN_WHEEL)
    errors = [i for i in issues if i.level == "error"]
    assert errors == [], errors


def test_load_minimal():
    world = load_package(MINIMAL)
    assert world.manifest.id == "minimal"
    assert "foyer" in world.locations
    assert "warden" in world.npcs
