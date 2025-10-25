"""Wrapper utilities for invoking the legacy TreeFileBuilder executable."""

from __future__ import annotations

import os
import shutil
import subprocess
import threading
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, List, Optional, Sequence, TextIO


_PACKAGE_DIR = Path(__file__).resolve().parent
_SWG_TOOL_DIR = _PACKAGE_DIR.parent
_TOOLS_DIR = _SWG_TOOL_DIR.parent

DEFAULT_EXECUTABLE_NAMES: Sequence[str] = (
    "TreeFileBuilder",
    "TreeFileBuilder.exe",
    str(_SWG_TOOL_DIR / "TreeFileBuilder.exe"),
    str(_TOOLS_DIR / "TreeFileBuilder.exe"),
)


@dataclass
class TreeBuildResult:
    """Represents the outcome of a TreeFileBuilder invocation."""

    command: List[str]
    stdout: str
    stderr: str
    returncode: int
    output: Path


@dataclass(frozen=True)
class TreeFileBuilderCapabilities:
    """Represents feature support exposed by a TreeFileBuilder executable."""

    supports_passphrase: bool = True
    supports_encrypt: bool = True
    supports_no_encrypt: bool = True
    supports_quiet: bool = True
    supports_dry_run: bool = True


class TreeFileBuilderError(RuntimeError):
    """Raised when TreeFileBuilder cannot be executed successfully."""

    def __init__(self, message: str, *, result: Optional[TreeBuildResult] = None) -> None:
        super().__init__(message)
        self.result = result


class TreeFileBuilder:
    """High level helper to invoke the TreeFileBuilder executable."""

    def __init__(self, executable: Optional[Path | str] = None) -> None:
        self.executable = self._resolve_executable(executable)
        self.capabilities = self._probe_capabilities()

    def _resolve_executable(self, override: Optional[Path | str]) -> Path:
        def _candidate(path_like: Path | str) -> Optional[Path]:
            path = Path(path_like).expanduser()
            if path.exists():
                return path
            return None

        candidate_list: List[Path] = []
        if override is not None:
            resolved = _candidate(override)
            if resolved is None:
                raise TreeFileBuilderError(
                    f"TreeFileBuilder executable not found: {Path(override).expanduser()}"
                )
            candidate_list.append(resolved)

        env_override = os.environ.get("TREEFILEBUILDER_PATH")
        if env_override:
            resolved = _candidate(env_override)
            if resolved:
                candidate_list.append(resolved)

        for candidate in DEFAULT_EXECUTABLE_NAMES:
            if os.path.basename(candidate) == candidate:
                resolved_path = shutil.which(candidate)
                if resolved_path:
                    candidate_list.append(Path(resolved_path))
            else:
                resolved = _candidate(candidate)
                if resolved:
                    candidate_list.append(resolved)

        for item in candidate_list:
            if item.exists():
                return item

        raise TreeFileBuilderError(
            "Unable to locate TreeFileBuilder executable. Set TREEFILEBUILDER_PATH or provide --builder."
        )

    def _probe_capabilities(self) -> TreeFileBuilderCapabilities:
        """Inspect the executable help output to determine supported flags."""

        try:
            completed = subprocess.run(  # nosec B603 - executable path provided by user
                [str(self.executable), "--help"],
                capture_output=True,
                text=True,
                check=False,
            )
        except OSError:
            return TreeFileBuilderCapabilities()

        help_text = (completed.stdout + "\n" + completed.stderr).lower()

        def _supports(*tokens: str) -> bool:
            return any(token in help_text for token in tokens)

        return TreeFileBuilderCapabilities(
            supports_passphrase=_supports("--passphrase", "-p <"),
            supports_encrypt=_supports("--encrypt", "-e"),
            supports_no_encrypt=_supports("--noencrypt", "-n"),
            supports_quiet=_supports("--quiet", "-q"),
            supports_dry_run=_supports("--nocreate", "-c"),
        )

    def _run_builder(
        self,
        command: List[str],
        stdout_callback: Optional[Callable[[str], None]],
        stderr_callback: Optional[Callable[[str], None]],
    ) -> subprocess.CompletedProcess[str]:
        if stdout_callback is None and stderr_callback is None:
            return subprocess.run(  # nosec B603 - trusted executable path provided by user
                command,
                capture_output=True,
                text=True,
                check=False,
            )

        process = subprocess.Popen(  # nosec B603 - trusted executable path provided by user
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

        stdout_chunks: List[str] = []
        stderr_chunks: List[str] = []

        def _consume(
            stream: Optional[TextIO],
            chunks: List[str],
            callback: Optional[Callable[[str], None]],
        ) -> None:
            if stream is None:
                return

            try:
                for chunk in iter(stream.readline, ""):
                    chunks.append(chunk)
                    if callback is not None:
                        callback(chunk)
            finally:
                stream.close()

        stdout_thread = threading.Thread(
            target=_consume,
            args=(process.stdout, stdout_chunks, stdout_callback),
        )
        stderr_thread = threading.Thread(
            target=_consume,
            args=(process.stderr, stderr_chunks, stderr_callback),
        )
        stdout_thread.start()
        stderr_thread.start()

        returncode = process.wait()
        stdout_thread.join()
        stderr_thread.join()

        return subprocess.CompletedProcess(
            args=command,
            returncode=returncode,
            stdout="".join(stdout_chunks),
            stderr="".join(stderr_chunks),
        )

    def build(
        self,
        response_file: Path,
        output_file: Path,
        *,
        no_toc_compression: bool = False,
        no_file_compression: bool = False,
        dry_run: bool = False,
        quiet: bool = False,
        force_encrypt: bool = False,
        disable_encrypt: bool = False,
        passphrase: Optional[str] = None,
        stdout_callback: Optional[Callable[[str], None]] = None,
        stderr_callback: Optional[Callable[[str], None]] = None,
    ) -> TreeBuildResult:
        if force_encrypt and disable_encrypt:
            raise TreeFileBuilderError(
                "Cannot specify both force_encrypt and disable_encrypt."
            )

        output_file = output_file.expanduser().resolve()
        if output_file.parent:
            output_file.parent.mkdir(parents=True, exist_ok=True)

        capabilities = self.capabilities

        if passphrase and not capabilities.supports_passphrase:
            raise TreeFileBuilderError(
                "This TreeFileBuilder executable does not support the --passphrase option."
                " Upgrade the toolchain or omit --passphrase."
            )
        if force_encrypt and not capabilities.supports_encrypt:
            raise TreeFileBuilderError(
                "This TreeFileBuilder executable does not support the --encrypt option."
                " Upgrade the toolchain or omit --encrypt."
            )
        if disable_encrypt and not capabilities.supports_no_encrypt:
            raise TreeFileBuilderError(
                "This TreeFileBuilder executable does not support the --noEncrypt option."
                " Upgrade the toolchain or omit --no-encrypt."
            )
        if quiet and not capabilities.supports_quiet:
            raise TreeFileBuilderError(
                "This TreeFileBuilder executable does not support the --quiet option."
                " Upgrade the toolchain or omit --quiet."
            )
        if dry_run and not capabilities.supports_dry_run:
            raise TreeFileBuilderError(
                "This TreeFileBuilder executable does not support the --noCreate option."
                " Upgrade the toolchain or omit --dry-run."
            )

        command: List[str] = [
            str(self.executable),
            f"--responseFile={response_file}",
        ]

        if no_toc_compression:
            command.append("--noTOCCompression")
        if no_file_compression:
            command.append("--noFileCompression")
        if dry_run and capabilities.supports_dry_run:
            command.append("--noCreate")
        if quiet and capabilities.supports_quiet:
            command.append("--quiet")
        if force_encrypt and capabilities.supports_encrypt:
            command.append("--encrypt")
        if disable_encrypt and capabilities.supports_no_encrypt:
            command.append("--noEncrypt")
        if passphrase and capabilities.supports_passphrase:
            command.extend(["--passphrase", passphrase])

        command.append(str(output_file))

        completed = self._run_builder(command, stdout_callback, stderr_callback)

        result = TreeBuildResult(
            command=command,
            stdout=completed.stdout,
            stderr=completed.stderr,
            returncode=completed.returncode,
            output=output_file,
        )

        if completed.returncode != 0:
            raise TreeFileBuilderError(
                f"TreeFileBuilder exited with code {completed.returncode}",
                result=result,
            )

        return result


__all__ = [
    "TreeFileBuilder",
    "TreeFileBuilderError",
    "TreeBuildResult",
    "TreeFileBuilderCapabilities",
    "DEFAULT_EXECUTABLE_NAMES",
]
