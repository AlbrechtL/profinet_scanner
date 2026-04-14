from __future__ import annotations

from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

import pytest


@dataclass(frozen=True)
class LabConfig:
    repo_root: Path
    profile_path: Path | None
    scanner_bin: Path
    scanner_interface: str
    remote_target: str
    sudo_cmd: tuple[str, ...]
    local_expected_names: tuple[str, ...]
    remote_expected_text: str | None
    topology_chain: str | None
    topology_links_expected: bool
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


def _load_profile(pytestconfig: pytest.Config) -> tuple[Path | None, dict[str, object]]:
    profile_option = pytestconfig.getoption("lab_profile")
    if not profile_option:
        return None, {}

    profile_path = Path(profile_option).expanduser()
    if not profile_path.is_absolute():
        profile_path = Path.cwd() / profile_path

    if not profile_path.exists():
        raise pytest.UsageError(f"lab profile not found at {profile_path}")

    try:
        profile_data = json.loads(profile_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise pytest.UsageError(f"lab profile {profile_path} is not valid JSON: {exc}") from exc

    if not isinstance(profile_data, dict):
        raise pytest.UsageError(f"lab profile {profile_path} must contain a JSON object")

    return profile_path.resolve(), profile_data


def _optional_profile_string(profile: dict[str, object], key: str) -> str | None:
    value = profile.get(key)
    if value is None:
        return None
    if not isinstance(value, str):
        raise pytest.UsageError(f"profile field {key} must be a string")
    return value


def _optional_profile_int(profile: dict[str, object], key: str) -> int | None:
    value = profile.get(key)
    if value is None:
        return None
    if not isinstance(value, int):
        raise pytest.UsageError(f"profile field {key} must be an integer")
    return value


def _optional_profile_bool(profile: dict[str, object], key: str) -> bool | None:
    value = profile.get(key)
    if value is None:
        return None
    if not isinstance(value, bool):
        raise pytest.UsageError(f"profile field {key} must be a boolean")
    return value


def _profile_expected_names(profile: dict[str, object]) -> tuple[str, ...] | None:
    names = profile.get("local_expected_names")
    if names is None:
        single_name = _optional_profile_string(profile, "local_expected_name")
        if single_name:
            return (single_name,)
        return None

    if not isinstance(names, list) or not names:
        raise pytest.UsageError("profile field local_expected_names must be a non-empty JSON array")

    normalized_names: list[str] = []
    for value in names:
        if not isinstance(value, str) or not value:
            raise pytest.UsageError("profile field local_expected_names must contain non-empty strings")
        normalized_names.append(value)
    return tuple(normalized_names)


@pytest.fixture(scope="session")
def lab_config(pytestconfig: pytest.Config) -> LabConfig:
    repo_root = Path(__file__).resolve().parents[3]
    profile_path, profile = _load_profile(pytestconfig)
    scanner_bin_option = pytestconfig.getoption("scanner_bin")
    scanner_bin = (
        Path(scanner_bin_option).expanduser()
        if scanner_bin_option
        else repo_root / "build" / "SendPacket" / "pn_scanner"
    )

    if not scanner_bin.exists():
        pytest.skip(f"pn_scanner binary not found at {scanner_bin}")

    scanner_interface = pytestconfig.getoption("scanner_interface") or _optional_profile_string(profile, "scanner_interface")
    if not scanner_interface:
        pytest.skip("missing required lab option scanner_interface")

    remote_target = pytestconfig.getoption("remote_target") or _optional_profile_string(profile, "remote_target")
    if not remote_target:
        pytest.skip("missing required lab option remote_target")

    sudo_cmd_text = pytestconfig.getoption("sudo_cmd")
    sudo_cmd = tuple(shlex.split(sudo_cmd_text))
    _validate_sudo_cmd(sudo_cmd)
    remote_expected_text = pytestconfig.getoption("remote_expected_text") or _optional_profile_string(profile, "remote_expected_text")
    local_expected_names = _profile_expected_names(profile)
    if local_expected_names is None:
        local_expected_names = (pytestconfig.getoption("local_expected_name"),)
    topology_chain = _optional_profile_string(profile, "topology_chain")
    topology_links_expected = _optional_profile_bool(profile, "topology_links_expected")
    if topology_links_expected is None:
        topology_links_expected = topology_chain is not None

    if topology_links_expected and not topology_chain:
        topology_chain = pytestconfig.getoption("topology_chain")

    local_duration = _optional_profile_int(profile, "local_duration") or pytestconfig.getoption("local_duration")
    remote_duration = _optional_profile_int(profile, "remote_duration") or pytestconfig.getoption("remote_duration")
    topology_duration = _optional_profile_int(profile, "topology_duration") or pytestconfig.getoption("topology_duration")

    return LabConfig(
        repo_root=repo_root,
        profile_path=profile_path,
        scanner_bin=scanner_bin.resolve(),
        scanner_interface=scanner_interface,
        remote_target=remote_target,
        sudo_cmd=sudo_cmd,
        local_expected_names=local_expected_names,
        remote_expected_text=remote_expected_text,
        topology_chain=topology_chain,
        topology_links_expected=topology_links_expected,
        local_duration=local_duration,
        remote_duration=remote_duration,
        topology_duration=topology_duration,
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
