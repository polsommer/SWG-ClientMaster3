#!/usr/bin/env python3
"""Guided interface for preparing TreeFileBuilder projects."""
from __future__ import annotations

import os
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
import traceback
from pathlib import Path
from tkinter import (
    BOTH,
    BOTTOM,
    END,
    LEFT,
    RIGHT,
    X,
    Y,
    BooleanVar,
    Listbox,
    StringVar,
    TclError,
    Text,
    Tk,
    filedialog,
    messagebox,
    ttk,
)

from typing import Any, Callable

SWG_TOOL_PATH = Path(__file__).resolve().parent / "swg-tool"
if str(SWG_TOOL_PATH) not in sys.path:
    sys.path.append(str(SWG_TOOL_PATH))

from swg_tool.response import ResponseFileBuilder, ResponseFileBuilderError
from swg_tool.treebuilder import TreeBuildResult, TreeFileBuilder, TreeFileBuilderError

_TOOLS_DIR = Path(__file__).resolve().parent
_SWG_TOOL_DIR = _TOOLS_DIR / "swg-tool"

DEFAULT_EXECUTABLE_NAMES = [
    "TreeFileBuilder",
    "TreeFileBuilder.exe",
    str(_SWG_TOOL_DIR / "TreeFileBuilder.exe"),
    str(_TOOLS_DIR / "TreeFileBuilder.exe"),
]

MAX_INDEXED_ENTRIES = 5000


class TreeFileBuilderGui:
    """Minimal workflow for building .tre/.tres files."""

    def __init__(self, master: Tk) -> None:
        self.master = master
        master.title("TreeFile Builder")
        master.minsize(720, 520)

        self._configure_style()

        self.builder_path = StringVar(value=self._find_default_executable())
        self.source_folder = StringVar()
        self.output_directory = StringVar()
        self.base_name = StringVar()
        self.passphrase = StringVar()
        self.build_tre = BooleanVar(value=True)
        self.build_tres = BooleanVar(value=False)

        self.debug_builder_path = StringVar(value="Builder not resolved yet.")
        self.debug_capabilities = StringVar(value="Capabilities not loaded.")

        self._task_queue: queue.Queue[tuple[str, str, Any, str | None]] = queue.Queue()
        self._active_task: str | None = None
        self._debug_refresh_active = False
        self._busy_widgets: list[ttk.Widget] = []

        self.update_paths: list[Path] = []
        self._source_trace_after: int | None = None
        self._builder_trace_after: int | None = None

        self._build_layout()
        self._set_status("Select a content folder to begin.")
        self._refresh_content_index()
        self._refresh_debug_info()
        self._stop_progress()
        self._set_busy_ui(False)
        self.master.after(150, self._poll_async_queue)

        self.builder_path.trace_add("write", self._on_builder_path_changed)

    # ------------------------------------------------------------------
    # UI construction helpers
    # ------------------------------------------------------------------
    def _configure_style(self) -> None:
        style = ttk.Style()
        theme = style.theme_use()
        if theme == "classic":
            try:
                style.theme_use("default")
            except Exception:  # pragma: no cover - depends on platform
                pass

    def _build_layout(self) -> None:
        container = ttk.Frame(self.master, padding=12)
        container.pack(fill=BOTH, expand=True)

        description = (
            "Configure the TreeFileBuilder executable, select a source folder,"
            " optionally add update folders, then generate .tre and .tres files."
        )
        ttk.Label(
            container,
            text=description,
            wraplength=680,
            justify="left",
        ).pack(anchor="w", pady=(0, 12))

        builder_frame = ttk.LabelFrame(container, text="TreeFileBuilder executable")
        builder_frame.pack(fill=X)

        builder_row = ttk.Frame(builder_frame)
        builder_row.pack(fill=X, padx=8, pady=8)
        ttk.Entry(builder_row, textvariable=self.builder_path).pack(
            side=LEFT, fill=X, expand=True
        )
        ttk.Button(builder_row, text="Browse", command=self._browse_builder).pack(
            side=LEFT, padx=(8, 0)
        )

        content_frame = ttk.LabelFrame(container, text="Content folders")
        content_frame.pack(fill=BOTH, expand=True, pady=12)

        self._add_labeled_entry(
            content_frame,
            "Primary source",
            self.source_folder,
            self._browse_source,
        )
        self.source_folder.trace_add("write", self._on_source_changed)

        updates_header = ttk.Frame(content_frame)
        updates_header.pack(fill=X, padx=8, pady=(12, 4))
        ttk.Label(
            updates_header,
            text="Update folders (optional)",
        ).pack(side=LEFT)

        updates_toolbar = ttk.Frame(content_frame)
        updates_toolbar.pack(fill=X, padx=8)
        ttk.Button(
            updates_toolbar, text="Add folder", command=self._add_update_folder
        ).pack(side=LEFT)
        ttk.Button(
            updates_toolbar,
            text="Remove selected",
            command=self._remove_selected_updates,
        ).pack(side=LEFT, padx=(8, 0))
        ttk.Button(
            updates_toolbar, text="Clear", command=self._clear_updates
        ).pack(side=LEFT, padx=(8, 0))

        list_container = ttk.Frame(content_frame)
        list_container.pack(fill=X, padx=8, pady=(4, 8))
        self.updates_listbox = Listbox(list_container, height=5, selectmode="extended")
        self.updates_listbox.pack(side=LEFT, fill=BOTH, expand=True)
        scroll_y = ttk.Scrollbar(list_container, orient="vertical")
        scroll_y.pack(side=RIGHT, fill=Y)
        self.updates_listbox.configure(yscrollcommand=scroll_y.set)
        scroll_y.configure(command=self.updates_listbox.yview)

        index_frame = ttk.LabelFrame(content_frame, text="Indexed contents")
        index_frame.pack(fill=BOTH, expand=True, padx=8, pady=(0, 8))

        tree_container = ttk.Frame(index_frame)
        tree_container.pack(fill=BOTH, expand=True, padx=4, pady=4)

        columns = ("type", "origin")
        self.content_tree = ttk.Treeview(
            tree_container,
            columns=columns,
            show="tree headings",
            selectmode="browse",
        )
        self.content_tree.heading("#0", text="Path")
        self.content_tree.heading("type", text="Type")
        self.content_tree.heading("origin", text="Source")
        self.content_tree.column("#0", stretch=True, width=360)
        self.content_tree.column("type", width=90, anchor="center")
        self.content_tree.column("origin", width=180, stretch=False)
        tree_scroll_y = ttk.Scrollbar(tree_container, orient="vertical")
        tree_scroll_y.pack(side=RIGHT, fill=Y)
        tree_scroll_x = ttk.Scrollbar(tree_container, orient="horizontal")
        tree_scroll_x.pack(side=BOTTOM, fill=X)

        self.content_tree.configure(
            yscrollcommand=tree_scroll_y.set, xscrollcommand=tree_scroll_x.set
        )
        tree_scroll_y.configure(command=self.content_tree.yview)
        tree_scroll_x.configure(command=self.content_tree.xview)
        self.content_tree.pack(side=LEFT, fill=BOTH, expand=True)

        summary_bar = ttk.Frame(index_frame)
        summary_bar.pack(fill=X, padx=4, pady=(0, 4))
        self.content_summary = StringVar(
            value="Select a source folder to index its contents."
        )
        ttk.Label(summary_bar, textvariable=self.content_summary).pack(anchor="w")

        output_frame = ttk.LabelFrame(container, text="Output")
        output_frame.pack(fill=X)

        self._add_labeled_entry(
            output_frame,
            "Output directory",
            self.output_directory,
            self._browse_output_directory,
        )

        name_row = ttk.Frame(output_frame)
        name_row.pack(fill=X, padx=8, pady=(4, 4))
        ttk.Label(name_row, text="Base file name").pack(side=LEFT)
        ttk.Entry(name_row, textvariable=self.base_name, width=32).pack(
            side=LEFT, padx=(12, 0)
        )

        options_row = ttk.Frame(output_frame)
        options_row.pack(fill=X, padx=8, pady=(4, 8))
        ttk.Checkbutton(
            options_row, text="Build .tre", variable=self.build_tre
        ).pack(side=LEFT)
        ttk.Checkbutton(
            options_row, text="Build .tres (encrypted)", variable=self.build_tres
        ).pack(side=LEFT, padx=(12, 0))
        ttk.Label(options_row, text="Passphrase").pack(side=LEFT, padx=(24, 0))
        ttk.Entry(options_row, textvariable=self.passphrase, show="*", width=20).pack(
            side=LEFT, padx=(6, 0)
        )

        self.generate_button = ttk.Button(
            container,
            text="Generate tree files",
            command=self._generate_tree_files,
        )
        self.generate_button.pack(anchor="e", pady=(12, 12))
        self._busy_widgets.append(self.generate_button)

        log_frame = ttk.LabelFrame(container, text="Diagnostics")
        log_frame.pack(fill=BOTH, expand=True)

        self.log_notebook = ttk.Notebook(log_frame)
        self.log_notebook.pack(fill=BOTH, expand=True, padx=8, pady=8)

        self.log_tab = ttk.Frame(self.log_notebook)
        self.debug_tab = ttk.Frame(self.log_notebook)
        self.log_notebook.add(self.log_tab, text="Activity log")
        self.log_notebook.add(self.debug_tab, text="Debugging")

        log_toolbar = ttk.Frame(self.log_tab)
        log_toolbar.pack(fill=X, padx=4, pady=(4, 0))
        ttk.Button(log_toolbar, text="Clear", command=self._clear_log).pack(side=RIGHT)
        ttk.Button(
            log_toolbar, text="Copy", command=self._copy_log_to_clipboard
        ).pack(side=RIGHT, padx=(0, 8))

        log_container = ttk.Frame(self.log_tab)
        log_container.pack(fill=BOTH, expand=True, padx=4, pady=4)
        self.log_widget = Text(log_container, wrap="none", height=12)
        self.log_widget.pack(side=LEFT, fill=BOTH, expand=True)
        log_scroll = ttk.Scrollbar(log_container, orient="vertical")
        log_scroll.pack(side=RIGHT, fill=Y)
        self.log_widget.configure(yscrollcommand=log_scroll.set)
        log_scroll.configure(command=self.log_widget.yview)

        debug_toolbar = ttk.Frame(self.debug_tab)
        debug_toolbar.pack(fill=X, padx=4, pady=(4, 0))
        self.debug_refresh_button = ttk.Button(
            debug_toolbar, text="Refresh info", command=self._refresh_debug_info
        )
        self.debug_refresh_button.pack(side=LEFT)
        self.debug_help_button = ttk.Button(
            debug_toolbar, text="Run --help", command=self._run_builder_help
        )
        self.debug_help_button.pack(side=LEFT, padx=(8, 0))
        self._busy_widgets.extend([self.debug_refresh_button, self.debug_help_button])

        info_frame = ttk.LabelFrame(self.debug_tab, text="Builder details")
        info_frame.pack(fill=X, padx=4, pady=(4, 4))
        ttk.Label(info_frame, text="Executable:").pack(anchor="w", padx=8, pady=(4, 0))
        ttk.Label(
            info_frame,
            textvariable=self.debug_builder_path,
            wraplength=620,
        ).pack(fill=X, padx=8, pady=(0, 4))
        ttk.Label(info_frame, text="Capabilities:").pack(anchor="w", padx=8)
        ttk.Label(
            info_frame,
            textvariable=self.debug_capabilities,
            wraplength=620,
        ).pack(fill=X, padx=8, pady=(0, 8))

        debug_output_frame = ttk.LabelFrame(self.debug_tab, text="Builder output")
        debug_output_frame.pack(fill=BOTH, expand=True, padx=4, pady=(0, 4))
        debug_output_container = ttk.Frame(debug_output_frame)
        debug_output_container.pack(fill=BOTH, expand=True, padx=4, pady=4)
        self.debug_output = Text(
            debug_output_container, wrap="none", height=10, state="disabled"
        )
        self.debug_output.pack(side=LEFT, fill=BOTH, expand=True)
        debug_scroll = ttk.Scrollbar(debug_output_container, orient="vertical")
        debug_scroll.pack(side=RIGHT, fill=Y)
        self.debug_output.configure(yscrollcommand=debug_scroll.set)
        debug_scroll.configure(command=self.debug_output.yview)
        self._write_debug_output(
            "Press \"Run --help\" to capture diagnostic output from TreeFileBuilder."
        )

        status_frame = ttk.Frame(container)
        status_frame.pack(fill=X, pady=(12, 0))
        ttk.Separator(status_frame, orient="horizontal").pack(fill=X, pady=(0, 6))
        self.progress = ttk.Progressbar(status_frame, mode="indeterminate")
        self.progress.pack(fill=X, pady=(0, 6))
        self.status_label = ttk.Label(status_frame, text="Ready")
        self.status_label.pack(anchor="w")

    def _add_labeled_entry(
        self,
        parent: ttk.Widget,
        label: str,
        variable: StringVar,
        callback,
    ) -> None:
        row = ttk.Frame(parent)
        row.pack(fill=X, padx=8, pady=(8, 0))
        ttk.Label(row, text=label).pack(side=LEFT)
        entry = ttk.Entry(row, textvariable=variable)
        entry.pack(side=LEFT, fill=X, expand=True, padx=(12, 0))
        ttk.Button(row, text="Browse", command=callback).pack(side=LEFT, padx=(8, 0))

    # ------------------------------------------------------------------
    # Browse helpers
    # ------------------------------------------------------------------
    def _browse_builder(self) -> None:
        initial = self.builder_path.get().strip()
        directory = Path(initial).parent if initial else Path.cwd()
        selected = filedialog.askopenfilename(
            title="Select TreeFileBuilder executable",
            initialdir=str(directory),
        )
        if selected:
            self.builder_path.set(selected)

    def _browse_source(self) -> None:
        selected = filedialog.askdirectory(title="Select source folder")
        if selected:
            path = Path(selected)
            self.source_folder.set(selected)
            if not self.output_directory.get().strip():
                self.output_directory.set(str(path.parent))
            if not self.base_name.get().strip():
                self.base_name.set(path.name)
            self._refresh_content_index()

    def _browse_output_directory(self) -> None:
        selected = filedialog.askdirectory(title="Select output directory")
        if selected:
            self.output_directory.set(selected)

    # ------------------------------------------------------------------
    # Update folder management
    # ------------------------------------------------------------------
    def _add_update_folder(self) -> None:
        selected = filedialog.askdirectory(title="Add update folder")
        if not selected:
            return
        path = Path(selected)
        if not path.exists():
            messagebox.showerror("Update folder", f"Folder not found: {path}")
            return
        if path in self.update_paths:
            return
        self.update_paths.append(path)
        self._refresh_updates_list()
        self._refresh_content_index()

    def _remove_selected_updates(self) -> None:
        selections = list(self.updates_listbox.curselection())
        if not selections:
            return
        for index in reversed(selections):
            try:
                del self.update_paths[index]
            except IndexError:
                continue
        self._refresh_updates_list()
        self._refresh_content_index()

    def _clear_updates(self) -> None:
        self.update_paths.clear()
        self._refresh_updates_list()
        self._refresh_content_index()

    def _refresh_updates_list(self) -> None:
        self.updates_listbox.delete(0, END)
        for path in self.update_paths:
            self.updates_listbox.insert(END, str(path))

    # ------------------------------------------------------------------
    # Content indexing
    # ------------------------------------------------------------------
    def _on_source_changed(self, *_args) -> None:
        if self._source_trace_after is not None:
            try:
                self.master.after_cancel(self._source_trace_after)
            except Exception:  # pragma: no cover - defensive cancellation
                pass
        self._source_trace_after = self.master.after(300, self._refresh_content_index)

    def _clear_content_tree(self) -> None:
        for child in self.content_tree.get_children():
            self.content_tree.delete(child)

    def _determine_entry_origin(
        self, file_path: Path, source_root: Path, update_roots: list[Path]
    ) -> str:
        try:
            resolved_file = file_path.resolve()
        except OSError:
            resolved_file = file_path

        if resolved_file == source_root or source_root in resolved_file.parents:
            return "Primary source"

        for update_root in update_roots:
            if resolved_file == update_root or update_root in resolved_file.parents:
                return f"Update: {update_root.name}"

        return "Additional source"

    def _refresh_content_index(self) -> None:
        if self._source_trace_after is not None:
            callback_id = self._source_trace_after
            self._source_trace_after = None
            try:
                self.master.after_cancel(callback_id)
            except Exception:  # pragma: no cover - defensive cancellation
                pass

        if not hasattr(self, "content_tree"):
            return

        self._clear_content_tree()

        source_text = self.source_folder.get().strip()
        if not source_text:
            self.content_summary.set("Select a source folder to index its contents.")
            return

        source_path = Path(source_text)
        if not source_path.exists() or not source_path.is_dir():
            self.content_summary.set("Source folder is not available.")
            return

        all_sources: list[Path] = [source_path] + self.update_paths

        try:
            builder = ResponseFileBuilder(
                entry_root=source_path, allow_overrides=True
            )
            entries = builder.build_entries(all_sources)
        except ResponseFileBuilderError as exc:
            self.content_summary.set(f"Indexing failed: {exc}")
            return
        except Exception as exc:  # pragma: no cover - unexpected failures
            self.content_summary.set(f"Unexpected indexing error: {exc}")
            return

        total_entries = len(entries)
        displayed_entries = entries[:MAX_INDEXED_ENTRIES]
        truncated = total_entries > len(displayed_entries)

        source_root = source_path.resolve()
        update_roots = [path.resolve() for path in self.update_paths]

        root_label = f"{source_path.name or source_path}/"
        root_id = self.content_tree.insert(
            "",
            "end",
            text=root_label,
            values=("Folder", "Primary source"),
            open=True,
        )

        node_lookup: dict[str, str] = {"": root_id}

        for entry in displayed_entries:
            parts = entry.entry.split("/") if entry.entry else []
            parent_key = ""
            for index, part in enumerate(parts):
                key = f"{parent_key}/{part}" if parent_key else part
                is_file = index == len(parts) - 1
                if key not in node_lookup:
                    parent_id = node_lookup[parent_key]
                    label = f"{part}{'' if is_file else '/'}"
                    values = ("File", self._determine_entry_origin(entry.source, source_root, update_roots))
                    if not is_file:
                        values = ("Folder", "")
                    node_id = self.content_tree.insert(
                        parent_id,
                        "end",
                        text=label,
                        values=values,
                        open=False,
                    )
                    node_lookup[key] = node_id
                parent_key = key

        folder_count = 1 + len(self.update_paths)
        folder_label = "folder" if folder_count == 1 else "folders"
        summary = f"Indexed {total_entries} files from {folder_count} {folder_label}."
        if truncated:
            summary += f" Showing first {MAX_INDEXED_ENTRIES} entries."
        self.content_summary.set(summary)

    def _on_builder_path_changed(self, *_args) -> None:
        if self._builder_trace_after is not None:
            try:
                self.master.after_cancel(self._builder_trace_after)
            except Exception:  # pragma: no cover - defensive cancellation
                pass
        self._builder_trace_after = self.master.after(400, self._refresh_debug_info)

    # ------------------------------------------------------------------
    # Log helpers
    # ------------------------------------------------------------------
    def _log(self, text: str) -> None:
        def append() -> None:
            self.log_widget.insert(END, text)
            self.log_widget.see(END)

        if threading.current_thread() is threading.main_thread():
            append()
        else:
            self.master.after(0, append)

    def _clear_log(self) -> None:
        self.log_widget.delete("1.0", END)

    def _copy_log_to_clipboard(self) -> None:
        content = self.log_widget.get("1.0", END)
        self.master.clipboard_clear()
        self.master.clipboard_append(content)

    def _write_debug_output(self, content: str) -> None:
        def update() -> None:
            self.debug_output.configure(state="normal")
            self.debug_output.delete("1.0", END)
            self.debug_output.insert("1.0", content)
            self.debug_output.configure(state="disabled")
            self.debug_output.see(END)

        if threading.current_thread() is threading.main_thread():
            update()
        else:
            self.master.after(0, update)

    def _set_status(
        self, message: str, *, error: bool = False, busy: bool = False
    ) -> None:
        def update() -> None:
            color = "#a00" if error else "#0a5c0a"
            self.status_label.configure(text=message, foreground=color)
            if busy:
                self._start_progress()
            else:
                self._stop_progress()

        if threading.current_thread() is threading.main_thread():
            update()
        else:
            self.master.after(0, update)

    def _start_progress(self) -> None:
        if not hasattr(self, "progress"):
            return

        def start() -> None:
            try:
                self.progress.configure(mode="indeterminate")
                self.progress.start(12)
            except TclError:
                pass

        if threading.current_thread() is threading.main_thread():
            start()
        else:
            self.master.after(0, start)

    def _stop_progress(self) -> None:
        if not hasattr(self, "progress"):
            return

        def stop() -> None:
            try:
                self.progress.stop()
                self.progress.configure(mode="determinate", value=0, maximum=100)
            except TclError:
                pass

        if threading.current_thread() is threading.main_thread():
            stop()
        else:
            self.master.after(0, stop)

    def _set_busy_ui(self, busy: bool) -> None:
        state = "disabled" if busy else "normal"

        cursor_name = "watch" if sys.platform != "win32" else "wait"

        try:
            self.master.configure(cursor=cursor_name if busy else "")
        except TclError:
            pass

        for widget in self._busy_widgets:
            try:
                widget.configure(state=state)
            except TclError:
                continue

    def _start_background_task(
        self,
        task_id: str,
        worker: Callable[[], Any],
        *,
        status_message: str | None = None,
    ) -> None:
        if self._active_task is not None:
            messagebox.showinfo(
                "Operation in progress",
                "Please wait for the current operation to finish.",
            )
            return

        self._active_task = task_id
        self._set_busy_ui(True)
        if status_message:
            self._set_status(status_message, busy=True)
        else:
            self._set_status("Working...", busy=True)

        thread = threading.Thread(
            target=self._run_background_task,
            args=(task_id, worker),
            daemon=True,
        )
        thread.start()

    def _run_background_task(self, task_id: str, worker: Callable[[], Any]) -> None:
        try:
            result = worker()
        except Exception as exc:  # pragma: no cover - defensive logging
            tb_text = traceback.format_exc()
            self._task_queue.put((task_id, "error", exc, tb_text))
        else:
            self._task_queue.put((task_id, "success", result, None))

    def _poll_async_queue(self) -> None:
        processed = False
        while True:
            try:
                task_id, outcome, payload, detail = self._task_queue.get_nowait()
            except queue.Empty:
                break

            processed = True

            if task_id == "generation":
                if outcome == "success":
                    self._handle_generation_success(payload)
                else:
                    self._handle_generation_error(payload, detail)
            elif task_id == "builder_help":
                if outcome == "success":
                    self._handle_builder_help_success(payload)
                else:
                    self._handle_builder_help_error(payload, detail)
            elif task_id == "debug_refresh":
                if outcome == "success":
                    self._handle_debug_refresh_success(payload)
                else:
                    self._handle_debug_refresh_error(payload, detail)
                self._debug_refresh_active = False

            if task_id == self._active_task:
                self._active_task = None
                self._set_busy_ui(False)

        if not processed and self._active_task is None:
            self._stop_progress()

        self.master.after(150, self._poll_async_queue)

    # ------------------------------------------------------------------
    # Debugging helpers
    # ------------------------------------------------------------------
    def _run_builder_help(self) -> None:
        self.log_notebook.select(self.debug_tab)

        def worker() -> tuple[TreeFileBuilder, subprocess.CompletedProcess[str]]:
            builder = self._resolve_builder()
            completed = subprocess.run(  # nosec B603 - executable path chosen by user
                [str(builder.executable), "--help"],
                capture_output=True,
                text=True,
                check=False,
            )
            return builder, completed

        self._start_background_task(
            "builder_help", worker, status_message="Running TreeFileBuilder --help..."
        )

    def _handle_builder_help_success(
        self, payload: tuple[TreeFileBuilder, subprocess.CompletedProcess[str]]
    ) -> None:
        builder, completed = payload
        output_lines = [
            f"Executable: {builder.executable}",
            f"Command: {builder.executable} --help",
            f"Return code: {completed.returncode}",
            "Stdout:",
            completed.stdout.strip() or "<no output>",
            "Stderr:",
            completed.stderr.strip() or "<no output>",
            "",
        ]
        self._write_debug_output("\n".join(output_lines))
        self._set_status("Captured TreeFileBuilder --help output.")
        self._log("Captured TreeFileBuilder --help output.\n")

    def _handle_builder_help_error(self, exc: Exception, traceback_text: str | None) -> None:
        if isinstance(exc, TreeFileBuilderError):
            message = str(exc)
            messagebox.showerror("TreeFileBuilder", message)
            self._write_debug_output(f"Unable to run --help:\n{message}")
        else:
            message = f"Failed to run --help: {exc}"
            messagebox.showerror("TreeFileBuilder", message)
            detail = f"{message}\n\n{traceback_text}" if traceback_text else message
            self._write_debug_output(detail)
        self._set_status("Failed to capture builder help output.", error=True)
        self._log(f"Debug error: {exc}\n")

    def _refresh_debug_info(self) -> None:
        if self._builder_trace_after is not None:
            callback_id = self._builder_trace_after
            self._builder_trace_after = None
            try:
                self.master.after_cancel(callback_id)
            except Exception:  # pragma: no cover - defensive cancellation
                pass

        if self._debug_refresh_active:
            return

        self._debug_refresh_active = True
        self.debug_builder_path.set("Resolving TreeFileBuilder executable...")
        self.debug_capabilities.set("Detecting capabilities...")

        def worker() -> None:
            try:
                builder = self._resolve_builder()
            except Exception as exc:  # pragma: no cover - defensive logging
                tb_text = traceback.format_exc()
                self._task_queue.put(("debug_refresh", "error", exc, tb_text))
            else:
                self._task_queue.put(("debug_refresh", "success", builder, None))

        threading.Thread(target=worker, daemon=True).start()

    def _handle_debug_refresh_success(self, builder: TreeFileBuilder) -> None:
        self.debug_builder_path.set(str(builder.executable))
        caps = builder.capabilities
        features = [
            ("Passphrase (--passphrase)", caps.supports_passphrase),
            ("Encrypt (--encrypt)", caps.supports_encrypt),
            ("Disable encrypt (--noEncrypt)", caps.supports_no_encrypt),
            ("Quiet (--quiet)", caps.supports_quiet),
            ("Dry run (--noCreate)", caps.supports_dry_run),
        ]
        supported = ", ".join(name for name, flag in features if flag) or "None"
        missing = ", ".join(name for name, flag in features if not flag)
        summary = f"Supports: {supported}."
        if missing:
            summary += f" Missing: {missing}."
        self.debug_capabilities.set(summary)

    def _handle_debug_refresh_error(self, exc: Exception, traceback_text: str | None) -> None:
        if isinstance(exc, TreeFileBuilderError):
            message = str(exc)
        else:
            message = f"Unable to resolve TreeFileBuilder executable: {exc}"
        self.debug_builder_path.set(message)
        self.debug_capabilities.set("Capabilities unavailable.")
        detail = message
        if traceback_text and not isinstance(exc, TreeFileBuilderError):
            detail = f"{message}\n\n{traceback_text}"
        self._write_debug_output(f"Unable to resolve TreeFileBuilder executable:\n{detail}")

    # ------------------------------------------------------------------
    # Core workflow
    # ------------------------------------------------------------------
    def _find_default_executable(self) -> str:
        for candidate in DEFAULT_EXECUTABLE_NAMES:
            if os.path.basename(candidate) == candidate:
                found = shutil.which(candidate)
                if found:
                    return found
            else:
                path = Path(candidate)
                if path.exists():
                    return str(path)
        return ""

    def _handle_generation_success(self, results: list[TreeBuildResult]) -> None:
        if not results:
            self._set_status("Nothing to do.")
            return

        self.log_notebook.select(self.log_tab)

        for result in results:
            self._log(
                "\n".join(
                    [
                        f"Command: {' '.join(result.command)}",
                        f"Return code: {result.returncode}",
                        f"Output file: {result.output}",
                        "Stdout:",
                        result.stdout.strip() or "<no output>",
                        "Stderr:",
                        result.stderr.strip() or "<no output>",
                        "",
                    ]
                )
            )

        self._set_status("Tree files generated successfully.")

    def _handle_generation_error(
        self, exc: Exception, traceback_text: str | None
    ) -> None:
        self.log_notebook.select(self.log_tab)

        if isinstance(exc, ValueError):
            title = "Generation failed"
            message = str(exc)
        elif isinstance(exc, (ResponseFileBuilderError, TreeFileBuilderError)):
            title = "Generation failed"
            message = str(exc)
        else:
            title = "Unexpected error"
            message = f"{exc}"

        messagebox.showerror(title, message)

        if isinstance(exc, (ResponseFileBuilderError, TreeFileBuilderError, ValueError)):
            self._log(f"Error: {message}\n")
        else:
            detail = traceback_text or message
            self._log(f"Unexpected error: {message}\n{detail}\n")

        self._set_status("Generation failed", error=True)

    def _generate_tree_files(self) -> None:
        try:
            validated_inputs = self._validate_inputs()
        except ValueError as exc:
            messagebox.showerror("Generation failed", str(exc))
            self._log(f"Error: {exc}\n")
            self._set_status("Generation failed", error=True)
            return

        self.log_notebook.select(self.log_tab)

        def worker() -> list[TreeBuildResult]:
            return self._run_generation(validated_inputs)

        self._start_background_task(
            "generation", worker, status_message="Generating tree files..."
        )

    def _validate_inputs(self) -> tuple[Path, Path, str, bool, bool, str]:
        source = Path(self.source_folder.get().strip())
        if not source.exists() or not source.is_dir():
            raise ValueError("Please select a valid source folder.")

        output_dir_text = self.output_directory.get().strip()
        output_dir = Path(output_dir_text or source.parent)
        output_dir.mkdir(parents=True, exist_ok=True)

        base_name = self.base_name.get().strip()
        if not base_name:
            raise ValueError("Please enter a base file name.")

        build_tre = self.build_tre.get()
        build_tres = self.build_tres.get()
        if not build_tre and not build_tres:
            raise ValueError("Select at least one output format (.tre or .tres).")

        passphrase = self.passphrase.get().strip()
        if build_tres and not passphrase:
            raise ValueError("Provide a passphrase to generate .tres files.")

        return source, output_dir, base_name, build_tre, build_tres, passphrase

    def _prepare_response_file(self, source: Path, temp_dir: Path) -> Path:
        sources = [source] + self.update_paths
        builder = ResponseFileBuilder(entry_root=source, allow_overrides=True)
        response_path = temp_dir / "treebuilder.rsp"
        result = builder.write(destination=response_path, sources=sources)
        self._log(
            f"Response file created at {response_path} with {len(result.entries)} entries.\n"
        )
        return result.path

    def _resolve_builder(self) -> TreeFileBuilder:
        builder_override = self.builder_path.get().strip() or None
        return TreeFileBuilder(executable=builder_override)

    def _run_generation(
        self,
        inputs: tuple[Path, Path, str, bool, bool, str] | None = None,
    ) -> list[TreeBuildResult]:
        if inputs is None:
            inputs = self._validate_inputs()

        source, output_dir, base_name, build_tre, build_tres, passphrase = inputs

        with tempfile.TemporaryDirectory() as temp:
            temp_path = Path(temp)
            response_file = self._prepare_response_file(source, temp_path)

            builder = self._resolve_builder()
            results = []

            if build_tre:
                output_path = (output_dir / f"{base_name}.tre").resolve()
                self._log(f"\nBuilding {output_path}...\n")
                result = builder.build(
                    response_file=response_file,
                    output_file=output_path,
                    disable_encrypt=True,
                )
                results.append(result)

            if build_tres:
                output_path = (output_dir / f"{base_name}.tres").resolve()
                self._log(f"\nBuilding {output_path}...\n")
                result = builder.build(
                    response_file=response_file,
                    output_file=output_path,
                    force_encrypt=True,
                    passphrase=passphrase,
                )
                results.append(result)

            return results


def main() -> None:
    try:
        root = Tk()
    except TclError as exc:
        print("Unable to start TreeFileBuilderGui:", exc, file=sys.stderr)
        if sys.platform != "win32" and not os.environ.get("DISPLAY"):
            print(
                "No graphical display detected. Set the DISPLAY environment variable "
                "or run on a system with a GUI.",
                file=sys.stderr,
            )
        sys.exit(1)

    gui = TreeFileBuilderGui(root)
    root.mainloop()


if __name__ == "__main__":
    main()
