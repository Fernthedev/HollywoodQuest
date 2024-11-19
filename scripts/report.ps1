if (Test-Path "./ndkpath.txt") {
    $NDKPath = Get-Content ./ndkpath.txt
}
else {
    $NDKPath = $ENV:ANDROID_NDK_HOME
}

python $NDKPath/SimplePerf/report_html.py
