from __future__ import annotations

import pytest


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("profinet-lab")
    group.addoption(
        "--scanner-bin",
        action="store",
        default=None,
        help="Path to the pn_scanner binary. Defaults to build/SendPacket/pn_scanner.",
    )
    group.addoption(
        "--scanner-interface",
        action="store",
        default=None,
        help="Capture interface passed through to pn_scanner --interface.",
    )
    group.addoption(
        "--remote-target",
        action="store",
        default=None,
        help="Single remote target IP passed through to pn_scanner --target.",
    )
    group.addoption(
        "--sudo-cmd",
        action="store",
        default="",
        help="Optional privilege wrapper, for example 'doas' or 'sudo -n'.",
    )
    group.addoption(
        "--lab-profile",
        action="store",
        default=None,
        help=(
            "Optional JSON file describing scanner-interface, remote-target, expected "
            "station names, optional topology expectations, and durations for a lab or virtual setup."
        ),
    )
    group.addoption(
        "--local-expected-name",
        action="store",
        default="testxasensorf28b",
        help="Stable NameOfStation expected in local mode output.",
    )
    group.addoption(
        "--remote-expected-text",
        action="store",
        default="",
        help="Optional stable text expected somewhere in remote mode output.",
    )
    group.addoption(
        "--topology-chain",
        action="store",
        default="",
        help="Expected topology chain text for topology mode assertions when no profile provides it.",
    )
    group.addoption(
        "--local-duration",
        action="store",
        type=int,
        default=15,
        help="Duration passed to pn_scanner for local mode.",
    )
    group.addoption(
        "--remote-duration",
        action="store",
        type=int,
        default=15,
        help="Duration passed to pn_scanner for remote mode.",
    )
    group.addoption(
        "--topology-duration",
        action="store",
        type=int,
        default=60,
        help="Duration passed to pn_scanner for topology mode.",
    )


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line("markers", "lab: requires the real PROFINET lab setup")
    config.addinivalue_line(
        "markers",
        "privileged: requires elevated privileges to run pn_scanner on a capture interface",
    )
    config.addinivalue_line(
        "markers",
        "virtual: runs against the containerized virtual PROFINET test environment",
    )
