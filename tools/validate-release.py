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
        print(f"Saída recente de {log}:", file=sys.stderr, flush=True)
        try:
            lines = log.read_text(errors="replace").splitlines()
            for line in lines[-80:]:
                print(line, file=sys.stderr)
        except OSError:
            pass
        raise RuntimeError(f"Falha ({result.returncode}); confira {log}")


def isolated_test_run(binary, env, output):
    """Retry a signal-killed Qt/WebKit suite with one test slot per process."""
    functions_log = output / "test-functions.log"
    with functions_log.open("w") as stream:
        listed = subprocess.run([str(binary), "-functions"], env=env, stdout=stream,
                                stderr=subprocess.STDOUT, check=False)
    if listed.returncode:
        raise RuntimeError(f"Não foi possível listar testes ({listed.returncode}); confira {functions_log}")
    functions = []
    for line in functions_log.read_text(errors="replace").splitlines():
        value = line.strip()
        if value.endswith("()") and value[:-2].replace("_", "").isalnum():
            functions.append(value[:-2])
    if not functions:
        raise RuntimeError("zenith-core-tests não listou funções de teste")

    suite = ET.Element("testsuite", name="CoreTests", tests="0", failures="0", errors="0",
                       skipped="0", time="0")
    text_parts = ["Suite original encerrada por sinal; reexecutando os testes em processos separados.\n"]
    failed = []
    duration = 0.0
    benchmark = output / "benchmark.json"
    for function in functions:
        case_dir = output / "isolated-tests"
        case_dir.mkdir(exist_ok=True)
        text_log = case_dir / f"{function}.txt"
        xml_log = case_dir / f"{function}.xml"
        case_env = env.copy()
        if function == "benchmarksBrowserWorkflows":
            case_env["ZENITH_BENCHMARK_OUTPUT"] = str(benchmark)
        command = [binary, function, "-o", f"{text_log},txt", "-o", f"{xml_log},junitxml"]
        print("Executando teste isolado:", function, flush=True)
        with (case_dir / f"{function}.process.log").open("w") as process_log:
            result = subprocess.run(list(map(str, command)), env=case_env, stdout=process_log,
                                    stderr=subprocess.STDOUT, timeout=300, check=False)
        if text_log.exists():
            text_parts.append(f"\n===== {function} (exit {result.returncode}) =====\n")
            text_parts.append(text_log.read_text(errors="replace"))
        cases = []
        if xml_log.exists():
            try:
                cases = list(ET.parse(xml_log).getroot().iter("testcase"))
            except ET.ParseError:
                cases = []
        selected = [case for case in cases if case.get("name") == function]
        if not selected:
            selected = [ET.Element("testcase", name=function, classname="CoreTests", time="0")]
            issue = ET.SubElement(selected[0], "error", message=f"processo terminou com código {result.returncode}")
            issue.text = (case_dir / f"{function}.process.log").read_text(errors="replace")
        for case in selected:
            suite.append(case)
            duration += float(case.get("time", "0"))
            if case.find("failure") is not None or case.find("error") is not None or result.returncode:
                failed.append(function)
        if result.returncode:
            log = case_dir / f"{function}.process.log"
            text_parts.append(f"\nPROCESS OUTPUT ({result.returncode}):\n")
            text_parts.append(log.read_text(errors="replace"))

    for name, index in (("initTestCase", 0), ("cleanupTestCase", -1)):
        if functions:
            xml_log = output / "isolated-tests" / f"{functions[index]}.xml"
            if xml_log.exists():
                try:
                    for case in ET.parse(xml_log).getroot().iter("testcase"):
                        if case.get("name") == name:
                            suite.append(case)
                            duration += float(case.get("time", "0"))
                            if case.find("failure") is not None or case.find("error") is not None:
                                failed.append(name)
                            break
                except ET.ParseError:
                    pass
    cases = list(suite.iter("testcase"))
    failures = sum(case.find("failure") is not None for case in cases)
    errors = sum(case.find("error") is not None for case in cases)
    skipped = sum(case.find("skipped") is not None for case in cases)
    suite.set("tests", str(len(cases)))
    suite.set("failures", str(failures))
    suite.set("errors", str(errors))
    suite.set("skipped", str(skipped))
    suite.set("time", f"{duration:.3f}")
    ET.ElementTree(suite).write(output / "tests.xml", encoding="utf-8", xml_declaration=True)
    (output / "tests.txt").write_text("".join(text_parts))
    if failed:
        unique = ", ".join(dict.fromkeys(failed))
        raise RuntimeError(f"Falharam testes isolados: {unique}")


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
    parser.add_argument("--isolate-tests", action="store_true", help="Executar cada teste Qt em processo próprio")
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
            if args.isolate_tests:
                report["test_execution_mode"] = "isolated"
                isolated_test_run(tests, env, output)
            else:
                test_command = [tests, "-o", str(output / "tests.txt") + ",txt", "-o",
                                str(output / "tests.xml") + ",junitxml"]
                with (output / "tests-process.log").open("w") as stream:
                    test_result = subprocess.run(list(map(str, test_command)), env=env, stdout=stream,
                                                 stderr=subprocess.STDOUT, timeout=300, check=False)
            if not args.isolate_tests and test_result.returncode < 0:
                report["test_execution_mode"] = "isolated_after_signal"
                report["suite_process_signal"] = -test_result.returncode
                print(f"Suíte morreu pelo sinal {-test_result.returncode}; isolando cada teste.",
                      file=sys.stderr, flush=True)
                isolated_test_run(tests, env, output)
            elif not args.isolate_tests and test_result.returncode:
                print(f"Saída recente de {output / 'tests-process.log'}:", file=sys.stderr, flush=True)
                for line in (output / "tests-process.log").read_text(errors="replace").splitlines()[-80:]:
                    print(line, file=sys.stderr)
                raise RuntimeError(f"Falha ({test_result.returncode}); confira {output / 'tests-process.log'}")
            elif not args.isolate_tests:
                report["test_execution_mode"] = "single_process"
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
