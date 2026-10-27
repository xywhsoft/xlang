"""Verify typed navigation with optional coordinate scratch refused.

Reuses the actual native build and existing complete navigation oracle.
Only this derived fixture intercepts uint32 scratch init/resize; the normal
server has no allocator hook or mode switch.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source, native, output = (p.resolve() for p in (args.source, args.native, args.output))
    output.mkdir(parents=True, exist_ok=False)
    report_path = native / "build-result.json"
    report = json.loads(report_path.read_text("utf8"))
    assert report["status"] == "passed"
    commands: list[dict] = []
    result = dict(status="failed", commands=commands, scope=__doc__)

    def run(command: list[str], label: str, cwd: Path | None = None) -> str:
        completed = subprocess.run(command, cwd=cwd, capture_output=True, timeout=300)
        log = output / f"{label}.log"
        log.write_bytes(completed.stdout + completed.stderr)
        commands.append(dict(command=command, exit=completed.returncode,
                             log=str(log), sha256=sha(log)))
        (output / "commands.json").write_text(json.dumps(commands, indent=2), "utf8")
        assert completed.returncode == 0, log.read_text("utf8", errors="replace")
        print(label, completed.stdout.decode("utf8", "replace")[-1200:], flush=True)
        return completed.stdout.decode("utf8", "replace")

    hook = output / "coordinate_hooks.h"
    hook.write_text(
        '#include <xrt.h>\n'
        'bool coordinateFaultInit(xarray*,size_t);\n'
        'bool coordinateFaultResize(xarray*,size_t);\n'
        'void coordinateFaultUnit(xarray*);\n'
        '#define xrtArrayInit coordinateFaultInit\n'
        '#define xrtArrayResize coordinateFaultResize\n'
        '#define xrtArrayUnit coordinateFaultUnit\n', "utf8")
    driver = output / "coordinate_fault_driver.c"
    driver.write_text(r'''#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
static int mode,bad;static unsigned attempts,refusals;static xarray* scratch;
bool coordinateFaultInit(xarray* a,size_t size)
{
 if(size==sizeof(uint32_t)){++attempts;if(mode==1){++refusals;return false;}}
 bool okay=xrtArrayInit(a,size);
 if(okay && size==sizeof(uint32_t)){if(scratch)bad=1;scratch=a;}
 return okay;
}
bool coordinateFaultResize(xarray* a,size_t count)
{
 if(a==scratch && mode==2){++refusals;return false;}
 return xrtArrayResize(a,count);
}
void coordinateFaultUnit(xarray* a)
{if(a==scratch)scratch=NULL;xrtArrayUnit(a);}
int coordinateOriginalNavigationMain(void);
int main(int argc,char** argv)
{
 mode=argc>1 ? atoi(argv[1]) : 0;
 int code=coordinateOriginalNavigationMain();
 if(code || bad || scratch || !attempts || (mode && !refusals)){fprintf(stderr,"coordinate fault proof failed:mode=%d attempts=%u refusals=%u bad=%d live=%d\n",mode,attempts,refusals,bad,scratch!=NULL);return 1;}
 printf("coordinate scratch mode=%d attempts=%u refusals=%u;complete original navigation oracle and independent readers passed\n",mode,attempts,refusals);return 0;
}
''', "utf8")

    def compile_record(name: str) -> list[str]:
        return next(c for c in report["commands"] if "-c" in c
                    and any(Path(v).name == name + ".c" for v in c))

    definitions = compile_record("semantic_definition")
    compiler = definitions[0]
    flags = definitions[1:definitions.index("-c")]
    projected = output / "coordinate_definitions.o"
    run([compiler, "-I" + str(source / "src"), *flags, "-include", str(hook),
         "-c", str(source / "src/services/semantic_definition.c"), "-o", str(projected)],
        "definitions-compile")
    fixture_record = compile_record("navigation_cache_test")
    fixture_flags = fixture_record[1:fixture_record.index("-c")]
    fixture = output / "coordinate_navigation_fixture.o"
    run([compiler, "-I" + str(source / "src"), *fixture_flags,
         "-Dmain=coordinateOriginalNavigationMain", "-c",
         str(source / "tests/navigation_cache_test.c"), "-o", str(fixture)],
        "navigation-compile")
    driver_object = output / "coordinate_fault_driver.o"
    run([compiler, "-I" + str(source / "src"), *fixture_flags,
         "-c", str(driver), "-o", str(driver_object)], "driver-compile")
    link = next(c for c in report["commands"] if "-c" not in c
                and c[-1].endswith("navigation_cache_test.exe"))
    command = [str(fixture) if Path(v).name == "navigation_cache_test.o"
               else str(projected) if Path(v).name == "semantic_definition.o"
               else v for v in link]
    command.insert(1, str(driver_object))
    executable = output / "coordinate_fault_test.exe"
    command[-1] = str(executable)
    run(command, "fixture-link")
    cases = []
    for mode in range(3):
        cwd = output / f"run-{mode}"
        cwd.mkdir()
        text = run([str(executable), str(mode)], f"run-{mode}", cwd)
        assert "navigation_cache_test passed:" in text and f"mode={mode}" in text
        cases.append(dict(mode=mode, completeNavigationParity=True))
    result.update(status="passed", cases=cases,
                  nativeBuild=dict(path=str(report_path), sha256=sha(report_path)),
                  sources={str(p): sha(p) for p in
                           [source / "src/services/semantic_definition.c",
                            source / "src/services/semantic_coordinate_dense.inc",
                            source / "src/services/semantic_coordinate_sorted.inc",
                            source / "tests/navigation_cache_test.c", hook, driver]},
                  limits="Only native uint32 coordinate scratch init/resize refusal,not arbitrary XRT/CRT/compiler OOM or global memory bounds.")
    (output / "result.json").write_text(json.dumps(result, indent=2), "utf8")


if __name__ == "__main__":
    main()
