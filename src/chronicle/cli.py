"""Chronicle CLI."""

from __future__ import annotations

import os
from pathlib import Path

import typer

from chronicle import __version__
from chronicle.cartridge.validator import validate_package
from chronicle.game.cartridge_game import CartridgeGame
from chronicle.game.tiny import TinyRoomGame
from chronicle.library import (
    default_library_dir,
    inspect_package,
    install_cartridge,
    list_cartridges,
    pack_cartridge,
    resolve_scenario,
)
from chronicle.providers.openai_compat import OpenAICompatProvider
from chronicle.providers.stub import StubProvider
from chronicle.runtime import ConsoleRuntime

app = typer.Typer(
    name="chronicle",
    help="Offline console for LLM-driven NPC mystery and social-sim cartridges.",
    no_args_is_help=False,
    add_completion=False,
)


def _build_provider(base_url: str | None, model: str | None):
    url = (
        base_url or os.environ.get("CHRONICLE_BASE_URL") or os.environ.get("ZOO_BASE_URL") or ""
    ).strip()
    mdl = (model or os.environ.get("CHRONICLE_MODEL") or os.environ.get("ZOO_MODEL") or "").strip()
    key = os.environ.get("CHRONICLE_API_KEY") or os.environ.get("OPENAI_API_KEY") or "not-needed"
    if url and mdl:
        return OpenAICompatProvider(base_url=url, model=mdl, api_key=key)
    return StubProvider()


@app.callback(invoke_without_command=True)
def main(
    ctx: typer.Context,
    scenario: Path | None = typer.Option(
        None,
        "--scenario",
        help="Path to a scenario package directory",
        exists=False,
        file_okay=False,
        dir_okay=True,
    ),
    base_url: str | None = typer.Option(None, "--base-url", help="OpenAI-compatible base URL"),
    model: str | None = typer.Option(None, "--model", help="Model name"),
    tiny: bool = typer.Option(False, "--tiny", help="Run the built-in TinyRoom harness demo"),
    version: bool = typer.Option(False, "--version", help="Show version and exit"),
) -> None:
    """Run a cartridge (default) or a subcommand."""
    if version:
        typer.echo(f"chronicle {__version__}")
        raise typer.Exit(0)
    if ctx.invoked_subcommand is not None:
        return
    if tiny:
        runtime = ConsoleRuntime(TinyRoomGame(), provider=_build_provider(base_url, model))
        raise typer.Exit(runtime.run())

    try:
        path = resolve_scenario(str(scenario) if scenario else None)
    except FileNotFoundError as exc:
        typer.secho(str(exc), fg=typer.colors.RED, err=True)
        raise typer.Exit(1) from exc

    issues = validate_package(path)
    errors = [i for i in issues if i.level == "error"]
    if errors:
        for issue in errors:
            typer.secho(str(issue), fg=typer.colors.RED, err=True)
        raise typer.Exit(1)

    game = CartridgeGame(path)
    # Allow cartridge config endpoint if env not set.
    provider = _build_provider(base_url, model)
    if isinstance(provider, StubProvider) and game.world.config.has_llm_endpoint():
        provider = OpenAICompatProvider(
            base_url=game.world.config.llm_base_url,
            model=game.world.config.llm_model,
            api_key=game.world.config.llm_api_key or "not-needed",
            temperature=game.world.config.temperature,
            max_tokens=game.world.config.max_response_tokens,
            timeout_s=game.world.config.inference_timeout_ms / 1000.0,
        )
    runtime = ConsoleRuntime(game, provider=provider)
    raise typer.Exit(runtime.run())


@app.command("run")
def run_cmd(
    cartridge_id: str = typer.Argument(..., help="Installed cartridge id"),
    base_url: str | None = typer.Option(None, "--base-url"),
    model: str | None = typer.Option(None, "--model"),
) -> None:
    """Run an installed library cartridge by id."""
    path = default_library_dir() / cartridge_id
    if not path.is_dir():
        typer.secho(f"Not installed: {cartridge_id}", fg=typer.colors.RED, err=True)
        raise typer.Exit(1)
    game = CartridgeGame(path)
    runtime = ConsoleRuntime(game, provider=_build_provider(base_url, model))
    raise typer.Exit(runtime.run())


@app.command("validate")
def validate_cmd(
    scenario: Path = typer.Option(..., "--scenario", help="Scenario package directory"),
) -> None:
    """Validate a scenario package without playing."""
    issues = validate_package(scenario)
    errors = [i for i in issues if i.level == "error"]
    warnings = [i for i in issues if i.level == "warning"]
    for issue in warnings:
        typer.secho(str(issue), fg=typer.colors.YELLOW)
    for issue in errors:
        typer.secho(str(issue), fg=typer.colors.RED)
    if errors:
        typer.secho("INVALID", fg=typer.colors.RED)
        raise typer.Exit(1)
    typer.secho("OK", fg=typer.colors.GREEN)


@app.command("inspect")
def inspect_cmd(
    scenario: Path = typer.Option(..., "--scenario", help="Scenario package directory"),
) -> None:
    """Inspect cartridge identity and readiness."""
    info = inspect_package(scenario.resolve())
    typer.echo(f"id:      {info['id']}")
    typer.echo(f"name:    {info['name']}")
    typer.echo(f"version: {info['version']}")
    typer.echo(f"schema:  {info['schema']}")
    typer.echo(f"path:    {info['path']}")
    typer.echo(f"ready:   {info['ready']}")
    for err in info["errors"]:
        typer.secho(err, fg=typer.colors.RED)
    for warn in info["warnings"]:
        typer.secho(warn, fg=typer.colors.YELLOW)


@app.command("list")
def list_cmd() -> None:
    """List installed cartridges."""
    items = list_cartridges()
    if not items:
        typer.echo("No cartridges installed.")
        return
    for item in items:
        typer.echo(f"{item['id']}\t{item['name']}\tv{item['version']}\t{item['path']}")


@app.command("install")
def install_cmd(
    path: Path = typer.Argument(..., help="Scenario directory or .chronicle/.zip archive"),
) -> None:
    """Install a cartridge into the local library."""
    try:
        dest = install_cartridge(path)
    except (ValueError, FileNotFoundError) as exc:
        typer.secho(str(exc), fg=typer.colors.RED, err=True)
        raise typer.Exit(1) from exc
    typer.secho(f"Installed to {dest}", fg=typer.colors.GREEN)


@app.command("pack")
def pack_cmd(
    scenario: Path = typer.Option(..., "--scenario"),
    output: Path = typer.Option(..., "--output"),
) -> None:
    """Pack a validated scenario directory into an archive."""
    try:
        out = pack_cartridge(scenario, output)
    except ValueError as exc:
        typer.secho(str(exc), fg=typer.colors.RED, err=True)
        raise typer.Exit(1) from exc
    typer.secho(f"Wrote {out}", fg=typer.colors.GREEN)


if __name__ == "__main__":
    app()
