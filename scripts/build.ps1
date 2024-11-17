Param(
    [Parameter(Mandatory=$false)]
    [Switch] $clean,

    [Parameter(Mandatory=$false)]
    [Switch] $glDebug,

    [Parameter(Mandatory=$false)]
    [Switch] $help
)

if ($help -eq $true) {
    Write-Output "`"Build`" - Copiles your mod into a `".so`" or a `".a`" library"
    Write-Output "`n-- Arguments --`n"

    Write-Output "-Clean `t`t Deletes the `"build`" folder, so that the entire library is rebuilt"

    exit
}

# if user specified clean, remove all build files
if ($clean.IsPresent) {
    if (Test-Path -Path "build") {
        remove-item build -R
    }
}


if (($clean.IsPresent) -or (-not (Test-Path -Path "build"))) {
    New-Item -Path build -ItemType Directory
}

Set-Location "java"
Move-Item "app/build.gradle.vscode-disabled" "app/build.gradle"
& ./gradlew build
if ($LASTEXITCODE -ne 0) {
    Move-Item "app/build.gradle" "app/build.gradle.vscode-disabled"
    exit $LASTEXITCODE
}
Move-Item "app/build.gradle" "app/build.gradle.vscode-disabled"
if (Test-Path -Path "temp") {
    Remove-Item -Path "temp" -Recurse
}
New-Item -ItemType Directory -Path "temp" | Out-Null
Expand-Archive -Path "app/build/outputs/apk/release/app-release-unsigned.apk" -DestinationPath "temp"
Move-Item "temp/classes.dex" "../assets/classes.dex" -Force
Remove-Item -Path "temp" -Recurse
Set-Location ..

$def = "OFF"
if ($glDebug.IsPresent) {
    $def = "ON"
}

& cmake -G "Ninja" -DCMAKE_BUILD_TYPE="RelWithDebInfo" -DGL_DEBUG="$def" -B build
& cmake --build ./build
