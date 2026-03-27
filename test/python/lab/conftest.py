from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path
import re
import shlex
import subprocess

import pytest


@dataclass(frozen=True)
class LabConfig:
    repo_root: Path
    scanner_bin: Path
    scanner_interface: str
    remote_target: str
    sudo_cmd: tuple[str, ...]
    local_expected_name: str
    remote_expected_text: str | None
    topology_chain: str
    local_duration: int
    remote_duration: int
    topology_duration: int


@dataclass(frozen=True)
class ScannerRunResult:
    command: tuple[str, ...]
    returncode: int
    stdout: str
    stderr: str

    @property
    def combined_output(self) -> str:
        return f"{self.stdout}\n{self.stderr}".strip()


def _required_option(pytestconfig: pytest.Config, name: str) -> str:
    value = pytestconfig.getoption(name)
    if value:
        return value
    pytest.skip(f"missing required lab option {name}")


def _validate_sudo_cmd(sudo_cmd: tuple[str, ...]) -> None:
    if os.geteuid() == 0 or not sudo_cmd:
        return

    wrapper = sudo_cmd[0]
    if wrapper not in {"doas", "sudo"}:
        return

    if "-n" in sudo_cmd:
        return

    raise pytest.UsageError(
        "interactive privilege wrappers are not supported inside the lab test subprocess. "
        "Run pytest itself as root, for example '.venv-lab-tests/bin/pytest ...' under doas, "
        "or use a non-interactive wrapper such as '--sudo-cmd "
        "\"doas -n\"' or '--sudo-cmd \"sudo -n\"'."
    )


@pytest.fixture(scope="session")
def lab_config(pytestconfig: pytest.Config) -> LabConfig:
    repo_root = Path(__file__).resolve().parents[3]
    scanner_bin_option = pytestconfig.getoption("scanner_bin")
    scanner_bin = (
        Path(scanner_bin_option).expanduser()
        if scanner_bin_option
        else repo_root / "build" / "SendPacket" / "pn_scanner"
    )

    if not scanner_bin.exists():
        pytest.skip(f"pn_scanner binary not found at {scanner_bin}")

    scanner_interface = _required_option(pytestconfig, "scanner_interface")
    remote_target = _required_option(pytestconfig, "remote_target")
    sudo_cmd_text = pytestconfig.getoption("sudo_cmd")
    sudo_cmd = tuple(shlex.split(sudo_cmd_text))
    _validate_sudo_cmd(sudo_cmd)
    remote_expected_text = pytestconfig.getoption("remote_expected_text") or None

    return LabConfig(
        repo_root=repo_root,
        scanner_bin=scanner_bin.resolve(),
        scanner_interface=scanner_interface,
        remote_target=remote_target,
        sudo_cmd=sudo_cmd,
        local_expected_name=pytestconfig.getoption("local_expected_name"),
        remote_expected_text=remote_expected_text,
        topology_chain=pytestconfig.getoption("topology_chain"),
        local_duration=pytestconfig.getoption("local_duration"),
        remote_duration=pytestconfig.getoption("remote_duration"),
        topology_duration=pytestconfig.getoption("topology_duration"),
    )


def _duration_for_mode(lab_config: LabConfig, mode: str) -> int:
    if mode == "local":
        return lab_config.local_duration
    if mode == "remote":
        return lab_config.remote_duration
    if mode == "topology":
        return lab_config.topology_duration
    raise ValueError(f"unsupported mode {mode}")


@pytest.fixture(scope="session")
def run_scanner(lab_config: LabConfig):
    def _run(mode: str, extra_args: list[str] | None = None) -> ScannerRunResult:
        duration = _duration_for_mode(lab_config, mode)
        command = [
            *lab_config.sudo_cmd,
            str(lab_config.scanner_bin),
            "--interface",
            lab_config.scanner_interface,
            "--mode",
            mode,
            "--duration",
            str(duration),
        ]
        if mode == "remote":
            command.extend(["--target", lab_config.remote_target])
        if extra_args:
            command.extend(extra_args)

        completed = subprocess.run(
            command,
            cwd=lab_config.repo_root,
            capture_output=True,
            text=True,
            timeout=duration + 30,
            check=False,
        )
        return ScannerRunResult(
            command=tuple(command),
            returncode=completed.returncode,
            stdout=completed.stdout,
            stderr=completed.stderr,
        )

    return _run


def normalize_whitespace(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()
