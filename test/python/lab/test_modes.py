import re
import shlex

import pytest


def normalize_whitespace(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def assert_success(result) -> None:
    assert result.returncode == 0, (
        "pn_scanner failed\n"
        f"command: {' '.join(shlex.quote(part) for part in result.command)}\n"
        f"stdout:\n{result.stdout}\n"
        f"stderr:\n{result.stderr}"
    )


@pytest.mark.lab
@pytest.mark.privileged
def test_local_mode_discovers_expected_station(run_scanner, lab_config) -> None:
    result = run_scanner("local")

    assert_success(result)
    assert "Starting local scan" in result.stdout
    assert "Topology results (stdout):" not in result.stdout
    assert f"Name: {lab_config.local_expected_name}" in result.stdout


@pytest.mark.lab
@pytest.mark.privileged
def test_remote_mode_reports_target_device(run_scanner, lab_config) -> None:
    result = run_scanner("remote")

    assert_success(result)
    assert "Starting remote scan" in result.stdout
    assert f"IP: {lab_config.remote_target}" in result.stdout
    assert re.search(r"(?m)^  Type: \S.+$", result.stdout), result.stdout
    assert re.search(r"(?m)^  UDP Port: \d+$", result.stdout), result.stdout
    if lab_config.remote_expected_text:
        assert lab_config.remote_expected_text in result.stdout


@pytest.mark.lab
@pytest.mark.privileged
def test_topology_mode_reports_expected_chain(run_scanner, lab_config) -> None:
    result = run_scanner("topology")

    assert_success(result)
    assert "Starting topology scan" in result.stdout
    assert "Topology source: PROFINET RPC peer data." in result.stdout
    assert normalize_whitespace(lab_config.topology_chain) in normalize_whitespace(result.stdout)
