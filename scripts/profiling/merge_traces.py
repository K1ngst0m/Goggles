#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import io
import json
import pathlib
import subprocess
import sys
from dataclasses import dataclass


@dataclass
class ZoneEvent:
    name: str
    timestamp_ns: int
    duration_ns: int
    thread_id: int


@dataclass
class PlotEvent:
    name: str
    timestamp_ns: int
    value: float
    thread_id: int


@dataclass
class TraceEvents:
    zones: list[ZoneEvent]
    plots: list[PlotEvent]


def run_csv_export(csvexport_bin: pathlib.Path, trace_path: pathlib.Path) -> TraceEvents:
    command = [str(csvexport_bin), "-u", "-p", str(trace_path)]
    result = subprocess.run(command, check=True, capture_output=True, text=True)
    reader = csv.DictReader(io.StringIO(result.stdout))

    zones: list[ZoneEvent] = []
    plots: list[PlotEvent] = []

    for row in reader:
        name = (row.get("name") or "").strip()
        ts_text = (row.get("ns_since_start") or "").strip()
        if not ts_text:
            continue

        try:
            timestamp_ns = int(float(ts_text))
        except ValueError:
            continue

        thread_text = (row.get("thread") or "").strip()
        try:
            thread_id = int(thread_text) if thread_text else 0
        except ValueError:
            thread_id = 0

        duration_text = (row.get("exec_time_ns") or "").strip()
        value_text = (row.get("value") or "").strip()

        if duration_text:
            try:
                duration_ns = int(float(duration_text))
            except ValueError:
                continue
            if name:
                zones.append(
                    ZoneEvent(
                        name=name,
                        timestamp_ns=timestamp_ns,
                        duration_ns=duration_ns,
                        thread_id=thread_id,
                    )
                )
            continue

        if value_text and name:
            try:
                value = float(value_text)
            except ValueError:
                continue
            plots.append(
                PlotEvent(
                    name=name,
                    timestamp_ns=timestamp_ns,
                    value=value,
                    thread_id=thread_id,
                )
            )

    return TraceEvents(zones=zones, plots=plots)


def first_marker_timestamps(plots: list[PlotEvent], marker_name: str) -> dict[int, int]:
    marker_timestamps: dict[int, int] = {}
    for event in plots:
        if event.name != marker_name:
            continue
        frame_number = int(round(event.value))
        if frame_number not in marker_timestamps:
            marker_timestamps[frame_number] = event.timestamp_ns
    return marker_timestamps


def build_trace_events(
    trace: TraceEvents, pid: int, shift_ns: int, role_name: str
) -> tuple[list[dict], set[int]]:
    events: list[dict] = []
    thread_ids: set[int] = set()

    for zone in trace.zones:
        ts_ns = zone.timestamp_ns + shift_ns
        events.append(
            {
                "name": zone.name,
                "ph": "X",
                "pid": pid,
                "tid": zone.thread_id,
                "ts": ts_ns / 1000.0,
                "dur": zone.duration_ns / 1000.0,
            }
        )
        thread_ids.add(zone.thread_id)

    for plot in trace.plots:
        ts_ns = plot.timestamp_ns + shift_ns
        events.append(
            {
                "name": plot.name,
                "ph": "C",
                "pid": pid,
                "tid": plot.thread_id,
                "ts": ts_ns / 1000.0,
                "args": {"value": plot.value},
            }
        )
        thread_ids.add(plot.thread_id)

    for thread_id in sorted(thread_ids):
        events.append(
            {
                "name": "thread_name",
                "ph": "M",
                "pid": pid,
                "tid": thread_id,
                "args": {"name": f"{role_name}-thread-{thread_id}"},
            }
        )

    return events, thread_ids


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Merge two Tracy traces into one timeline trace")
    parser.add_argument("--viewer-trace", required=True, type=pathlib.Path)
    parser.add_argument("--layer-trace", required=True, type=pathlib.Path)
    parser.add_argument("--output-trace", required=True, type=pathlib.Path)
    parser.add_argument("--summary-out", required=True, type=pathlib.Path)
    parser.add_argument("--csvexport-bin", required=True, type=pathlib.Path)
    parser.add_argument("--import-chrome-bin", required=True, type=pathlib.Path)
    parser.add_argument("--chrome-out", required=True, type=pathlib.Path)
    parser.add_argument("--viewer-marker", default="goggles_source_frame")
    parser.add_argument("--layer-marker", default="goggles_layer_frame")
    parser.add_argument("--disable-marker-alignment", action="store_true")
    return parser.parse_args()


def write_summary(
    summary_out: pathlib.Path,
    *,
    success: bool,
    warnings: list[str],
    alignment_method: str,
    alignment_frame: int | None,
    layer_shift_ns: int,
    normalization_shift_ns: int,
    viewer_trace: TraceEvents | None = None,
    layer_trace: TraceEvents | None = None,
    viewer_threads: int = 0,
    layer_threads: int = 0,
    merged_events: int = 0,
) -> None:
    summary = {
        "success": success,
        "warnings": warnings,
        "alignment": {
            "method": alignment_method,
            "frame_number": alignment_frame,
            "layer_shift_ns": layer_shift_ns,
            "normalization_shift_ns": normalization_shift_ns,
        },
        "counts": {
            "viewer_zones": len(viewer_trace.zones) if viewer_trace is not None else 0,
            "viewer_plots": len(viewer_trace.plots) if viewer_trace is not None else 0,
            "layer_zones": len(layer_trace.zones) if layer_trace is not None else 0,
            "layer_plots": len(layer_trace.plots) if layer_trace is not None else 0,
            "viewer_threads": viewer_threads,
            "layer_threads": layer_threads,
            "merged_events": merged_events,
        },
    }
    summary_out.parent.mkdir(parents=True, exist_ok=True)
    with summary_out.open("w", encoding="utf-8") as handle:
        json.dump(summary, handle, indent=2)


def main() -> int:
    args = parse_args()

    warnings: list[str] = []
    alignment_method = "relative_time"
    alignment_frame = None
    layer_shift_ns = 0
    normalization_shift_ns = 0

    for trace_path in (args.viewer_trace, args.layer_trace):
        if not trace_path.is_file() or trace_path.stat().st_size == 0:
            message = f"Missing or empty trace file: {trace_path}"
            print(f"Error: {message}", file=sys.stderr)
            warnings.append(message)
            write_summary(
                args.summary_out,
                success=False,
                warnings=warnings,
                alignment_method=alignment_method,
                alignment_frame=alignment_frame,
                layer_shift_ns=layer_shift_ns,
                normalization_shift_ns=normalization_shift_ns,
            )
            return 1

    try:
        viewer_trace = run_csv_export(args.csvexport_bin, args.viewer_trace)
        layer_trace = run_csv_export(args.csvexport_bin, args.layer_trace)
    except FileNotFoundError:
        message = f"Tracy csvexport tool not found: {args.csvexport_bin}"
        print(f"Error: {message}", file=sys.stderr)
        warnings.append(message)
        write_summary(
            args.summary_out,
            success=False,
            warnings=warnings,
            alignment_method=alignment_method,
            alignment_frame=alignment_frame,
            layer_shift_ns=layer_shift_ns,
            normalization_shift_ns=normalization_shift_ns,
        )
        return 1
    except subprocess.CalledProcessError as error:
        print(error.stdout, end="", file=sys.stderr)
        print(error.stderr, end="", file=sys.stderr)
        message = "Tracy csvexport failed while parsing trace files."
        print(f"Error: {message}", file=sys.stderr)
        warnings.append(message)
        write_summary(
            args.summary_out,
            success=False,
            warnings=warnings,
            alignment_method=alignment_method,
            alignment_frame=alignment_frame,
            layer_shift_ns=layer_shift_ns,
            normalization_shift_ns=normalization_shift_ns,
        )
        return 1

    if args.disable_marker_alignment:
        warnings.append("Marker alignment disabled; using relative-time fallback.")
    else:
        viewer_markers = first_marker_timestamps(viewer_trace.plots, args.viewer_marker)
        layer_markers = first_marker_timestamps(layer_trace.plots, args.layer_marker)
        common_frames = sorted(set(viewer_markers).intersection(layer_markers))
        if common_frames:
            alignment_method = "frame_marker"
            alignment_frame = common_frames[0]
            layer_shift_ns = viewer_markers[alignment_frame] - layer_markers[alignment_frame]
        else:
            warnings.append(
                "No common frame marker values found across traces; using relative-time fallback."
            )

    viewer_events, viewer_threads = build_trace_events(viewer_trace, 1, 0, "viewer")
    layer_events, layer_threads = build_trace_events(layer_trace, 2, layer_shift_ns, "layer")
    merged_events = viewer_events + layer_events

    min_ts_us = None
    for event in merged_events:
        ts_value = event.get("ts")
        if isinstance(ts_value, (int, float)):
            if min_ts_us is None or ts_value < min_ts_us:
                min_ts_us = ts_value

    if min_ts_us is not None and min_ts_us < 0:
        normalization_shift_ns = int(round((-min_ts_us) * 1000.0))
        for event in merged_events:
            if "ts" in event:
                event["ts"] = float(event["ts"]) + (normalization_shift_ns / 1000.0)

    merged_events.sort(key=lambda event: float(event.get("ts", 0.0)))

    chrome_payload = {"traceEvents": merged_events}
    args.chrome_out.parent.mkdir(parents=True, exist_ok=True)
    with args.chrome_out.open("w", encoding="utf-8") as handle:
        json.dump(chrome_payload, handle, indent=2)

    import_command = [str(args.import_chrome_bin), str(args.chrome_out), str(args.output_trace)]
    try:
        import_result = subprocess.run(import_command, capture_output=True, text=True)
    except FileNotFoundError:
        message = f"Tracy import tool not found: {args.import_chrome_bin}"
        print(f"Error: {message}", file=sys.stderr)
        warnings.append(message)
        write_summary(
            args.summary_out,
            success=False,
            warnings=warnings,
            alignment_method=alignment_method,
            alignment_frame=alignment_frame,
            layer_shift_ns=layer_shift_ns,
            normalization_shift_ns=normalization_shift_ns,
            viewer_trace=viewer_trace,
            layer_trace=layer_trace,
            viewer_threads=len(viewer_threads),
            layer_threads=len(layer_threads),
            merged_events=len(merged_events),
        )
        return 1

    if import_result.returncode != 0:
        print(import_result.stdout, end="", file=sys.stderr)
        print(import_result.stderr, end="", file=sys.stderr)
        warnings.append("Failed to import merged chrome trace into .tracy output.")
        write_summary(
            args.summary_out,
            success=False,
            warnings=warnings,
            alignment_method=alignment_method,
            alignment_frame=alignment_frame,
            layer_shift_ns=layer_shift_ns,
            normalization_shift_ns=normalization_shift_ns,
            viewer_trace=viewer_trace,
            layer_trace=layer_trace,
            viewer_threads=len(viewer_threads),
            layer_threads=len(layer_threads),
            merged_events=len(merged_events),
        )
        return 1

    write_summary(
        args.summary_out,
        success=True,
        warnings=warnings,
        alignment_method=alignment_method,
        alignment_frame=alignment_frame,
        layer_shift_ns=layer_shift_ns,
        normalization_shift_ns=normalization_shift_ns,
        viewer_trace=viewer_trace,
        layer_trace=layer_trace,
        viewer_threads=len(viewer_threads),
        layer_threads=len(layer_threads),
        merged_events=len(merged_events),
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
