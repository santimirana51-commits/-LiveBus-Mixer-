from pathlib import Path
import subprocess
import sys


def main() -> int:
    if sys.platform != "win32":
        print("LiveBus Mixer can only be run on Windows.")
        return 1

    app_directory = Path(__file__).resolve().parent
    candidates = (
        app_directory / "bin" / "LiveBusMixer.exe",
        app_directory / "LiveBusMixer.exe",
        app_directory / "LiveBusMixer_artefacts" / "Release" / "LiveBusMixer.exe",
        app_directory / "build" / "LiveBusMixer_artefacts" / "Release" / "LiveBusMixer.exe",
    )
    executable = next((path for path in candidates if path.is_file()), None)
    if executable is None:
        print("LiveBusMixer.exe was not found. Extract the release package completely,")
        print("including its bin folder, then run this launcher again.")
        return 1

    subprocess.Popen([str(executable)], cwd=str(executable.parent))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())