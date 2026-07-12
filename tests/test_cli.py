from pathlib import Path

from typer.testing import CliRunner

from chronicle.cli import app

ROOT = Path(__file__).resolve().parents[1]
MINIMAL = ROOT / "examples" / "minimal"
runner = CliRunner()


def test_cli_help():
    result = runner.invoke(app, ["--help"])
    assert result.exit_code == 0
    assert "validate" in result.stdout


def test_cli_version():
    result = runner.invoke(app, ["--version"])
    assert result.exit_code == 0
    assert "chronicle" in result.stdout


def test_cli_validate_ok():
    result = runner.invoke(app, ["validate", "--scenario", str(MINIMAL)])
    assert result.exit_code == 0
    assert "OK" in result.stdout


def test_cli_inspect():
    result = runner.invoke(app, ["inspect", "--scenario", str(MINIMAL)])
    assert result.exit_code == 0
    assert "minimal" in result.stdout


def test_cli_list_emptyish():
    result = runner.invoke(app, ["list"])
    assert result.exit_code == 0
