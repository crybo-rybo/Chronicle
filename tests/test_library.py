from pathlib import Path

from chronicle.library import (
    inspect_package,
    install_cartridge,
    list_cartridges,
    pack_cartridge,
    resolve_scenario,
)

ROOT = Path(__file__).resolve().parents[1]
MINIMAL = ROOT / "examples" / "minimal"


def test_inspect_minimal():
    info = inspect_package(MINIMAL)
    assert info["id"] == "minimal"
    assert info["ready"] is True
    assert info["errors"] == []


def test_resolve_scenario_path():
    assert resolve_scenario(str(MINIMAL)) == MINIMAL.resolve()


def test_pack_and_install(tmp_path):
    archive = tmp_path / "minimal.chronicle"
    pack_cartridge(MINIMAL, archive)
    assert archive.exists()
    dest = install_cartridge(archive, library_dir=tmp_path / "lib")
    assert dest.is_dir()
    assert (dest / "scenario.json").exists()
    listed = list_cartridges(library_dir=tmp_path / "lib")
    assert any(item["id"] == "minimal" for item in listed)
