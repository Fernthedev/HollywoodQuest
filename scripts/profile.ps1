
if (Test-Path "./ndkpath.txt") {
    $NDKPath = Get-Content ./ndkpath.txt
} else {
    $NDKPath = $ENV:ANDROID_NDK_HOME
}

if ($null -eq (& adb shell pidof com.beatgames.beatsaber)) {
    & $PSScriptRoot/restart-game.ps1
}

python $NDKPath/SimplePerf/app_profiler.py --disable_adb_root --ndk_path $NDKPath --app com.beatgames.beatsaber -r “-g -e cpu-cycles”

python $NDKPath/SimplePerf/report_html.py
