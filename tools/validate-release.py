#!/usr/bin/env python3
"""Run isolated Zenith validation and keep the evidence beside its report."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import tarfile
import tempfile
import time
import xml.etree.ElementTree as ET


def run(command, env, log, timeout=300):
    print("Executando:", " ".join(map(str, command)), flush=True)
    with log.open("w") as stream:
        result = subprocess.run(list(map(str, command)), env=env, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=timeout, check=False)
    if result.returncode:
        raise RuntimeError(f"Falha ({result.returncode}); confira {log}")


def test_counts(path):
    root = ET.parse(path).getroot()
    cases = list(root.iter("testcase"))
    if not cases:
        raise RuntimeError("O relatório não contém testes executados")
    failed = sum(case.find("failure") is not None or case.find("error") is not None for case in cases)
    skipped = sum(case.find("skipped") is not None for case in cases)
    return {"total": len(cases), "failed": failed, "skipped": skipped,
            "passed": len(cases) - failed - skipped}


def benchmark_summary(path):
    data = json.loads(path.read_text())
    rounds = data["rounds"]
    summary = {"rounds": len(rounds), "scope": data["scope"], "memory": {}, "latency": {}}
    for stage in ("initial", "five_local_pages", "ten_additional_blank_tabs", "after_tools"):
        samples = [row[stage] for row in rounds]
        complete = len(samples) == 3 and all(sample["complete"] for sample in samples)
        summary["memory"][stage] = {"complete": complete}
        if complete:
            for metric in ("pss_kib", "rss_kib"):
                values = [sample[metric] for sample in samples]
                summary["memory"][stage][metric] = {"median": statistics.median(values),
                                                      "min": min(values), "max": max(values)}
    for action in ("route_latency_ms", "tab_switch_latency_ms"):
        samples = sorted(value for row in rounds for value in row[action])
        summary["latency"][action] = {"samples": len(samples), "median": statistics.median(samples),
                                      "p95": samples[max(0, (len(samples) * 95 + 99) // 100 - 1)]}
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True, type=Path, help="Diretório já compilado pelo CMake")
    parser.add_argument("--output", type=Path, help="Pasta para logs, capturas e relatório")
    parser.add_argument("--offscreen", action="store_true", help="Somente widgets; não aprova integração gráfica")
    parser.add_argument("--package", action="store_true", help="Gerar e conferir também o pacote TGZ")
    args = parser.parse_args()
    build = args.build.resolve()
    output = (args.output or Path.home() / ".cache/zenith-validation" / time.strftime("%Y%m%d-%H%M%S")).resolve()
    output.mkdir(parents=True, exist_ok=True)
    report = {"started": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "platform": platform.platform(),
              "graphical_required": not args.offscreen, "release_ready": False, "status": "running"}
    stage = "preflight"
    try:
        binary = build / "zenith-browser"
        tests = build / "zenith-core-tests"
        if not binary.is_file() or not tests.is_file():
            raise RuntimeError("Compile o navegador e zenith-core-tests antes da validação")
        if args.offscreen and args.package:
            raise RuntimeError("A validação de pacote exige integração gráfica")
        with tempfile.TemporaryDirectory(prefix="zenith-validation-") as isolated:
            env = os.environ.copy()
            for key, child in (("XDG_CONFIG_HOME", "config"), ("XDG_DATA_HOME", "data"), ("XDG_CACHE_HOME", "cache")):
                env[key] = str(Path(isolated) / child)
            env["ZENITH_CAPTURE_UI"] = "1"
            env["ZENITH_CAPTURE_DIR"] = str(output / "captures")
            env["QT_SCALE_FACTOR"] = "1"
            if args.offscreen:
                env.pop("ZENITH_REQUIRE_DISPLAY", None)
                env["DISPLAY"] = ""
                env["QT_QPA_PLATFORM"] = "offscreen"
            else:
                env["ZENITH_REQUIRE_DISPLAY"] = "1"
                env["ZENITH_BENCHMARK_OUTPUT"] = str(output / "benchmark.json")
            stage = "version"
            run([binary, "--version"], env, output / "version.txt", 20)
            report["version"] = (output / "version.txt").read_text().strip()
            report["binary_sha256"] = hashlib.sha256(binary.read_bytes()).hexdigest()
            stage = "tests"
            run([tests, "-o", str(output / "tests.txt") + ",txt", "-o",
                 str(output / "tests.xml") + ",junitxml"], env, output / "tests-process.log")
            report["tests"] = test_counts(output / "tests.xml")
            if report["tests"]["failed"] or (not args.offscreen and report["tests"]["skipped"]):
                raise RuntimeError("Integração exige testes sem falhas nem skips")
            stage = "scale_matrix"
            report["scales"] = {}
            for factor in ("1.25", "1.5", "2"):
                scaled = output / ("scale-" + factor)
                scaled.mkdir(parents=True, exist_ok=True)
                scaled_env = env.copy()
                scaled_env["QT_SCALE_FACTOR"] = factor
                scaled_env["ZENITH_CAPTURE_DIR"] = str(scaled / "captures")
                run([tests, "laysOutInternalScreens", "laysOutLibrariesAtLargeText", "-o",
                     str(scaled / "tests.txt") + ",txt", "-o", str(scaled / "tests.xml") + ",junitxml"],
                    scaled_env, scaled / "process.log")
                report["scales"][factor] = test_counts(scaled / "tests.xml")
                if report["scales"][factor]["failed"] or report["scales"][factor]["skipped"]:
                    raise RuntimeError(f"Matriz de escala incompleta em {factor}")
            if args.offscreen:
                report["status"] = "widgets_only"
            else:
                stage = "benchmark"
                report["benchmark"] = benchmark_summary(output / "benchmark.json")
                report["memory_complete"] = all(value["complete"] for value in report["benchmark"]["memory"].values())
                stage = "startup"
                run([binary, "--private", "--smoke-test"], env, output / "startup.log", 25)
                report["status"] = "graphical_tests_passed"
                if args.package:
                    stage = "package"
                    packages = output / "packages"
                    run(["cpack", "--config", build / "CPackConfig.cmake", "-B", packages], env, output / "package.log")
                    archives = list(packages.glob("ZenithBrowser-*.tar.gz"))
                    if len(archives) != 1:
                        raise RuntimeError("Esperado um único pacote TGZ nesta rodada")
                    archive = archives[0]
                    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
                    (packages / "SHA256SUMS").write_text(f"{digest}  {archive.name}\n")
                    unpacked = output / "unpacked"
                    with tarfile.open(archive) as bundle:
                        bundle.extractall(unpacked, filter="data")
                    installed = list(unpacked.glob("*/bin/zenith-browser"))
                    if len(installed) != 1:
                        raise RuntimeError("Executável ausente no pacote")
                    stage = "package_version"
                    run([installed[0], "--version"], env, output / "package-version.txt", 20)
                    if (output / "package-version.txt").read_text().strip() != report["version"]:
                        raise RuntimeError("Versões do pacote e da compilação divergem")
                    stage = "package_startup"
                    run([installed[0], "--private", "--smoke-test"], env, output / "package-startup.log", 25)
                    report["package"] = {"file": archive.name, "sha256": digest}
        report["remaining_manual_checks"] = ["compositor_and_monitors", "assistive_technology", "video", "upgrade_from_previous_package"]
        print(f"Validação concluída: {report['status']}. Evidências: {output}")
        return 0
    except (OSError, RuntimeError, subprocess.TimeoutExpired, ValueError, KeyError, ET.ParseError, tarfile.TarError) as error:
        report.update(status="failed", failed_stage=stage, error=str(error))
        print(f"Validação interrompida em {stage}: {error}", file=sys.stderr)
        return 1
    finally:
        (output / "validation.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    sys.exit(main())
