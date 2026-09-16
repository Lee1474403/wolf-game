[CmdletBinding()]
param(
    [string]$QtVersion = "6.10.2",
    [string]$QtRoot = "D:\Qt",
    [string]$AndroidSdk = "$env:LOCALAPPDATA\Android\Sdk",
    [string]$JavaHome = "D:\Android\jbr",
    [string]$NdkVersion = "27.2.12479018",
    [switch]$PrepareOnly
)

$ErrorActionPreference = "Stop"
$appLabel = -join @([char]0x6708, [char]0x591C, [char]0x8BAE, [char]0x4F1A)

$clientDir = $PSScriptRoot
$repoRoot = Split-Path -Parent $clientDir
$buildDir = Join-Path $clientDir "build-android-release"
$androidBuildDir = Join-Path $buildDir "android-build"
$outputDir = Join-Path $repoRoot "artifacts"

$qtAndroidDir = Join-Path $QtRoot "$QtVersion\android_arm64_v8a"
$qtHostDir = Join-Path $QtRoot "$QtVersion\mingw_64"
$qmake = Join-Path $qtAndroidDir "bin\qmake.bat"
$androidDeployQt = Join-Path $qtHostDir "bin\androiddeployqt.exe"
$make = Join-Path $QtRoot "Tools\mingw1310_64\bin\mingw32-make.exe"
$ndkDir = Join-Path $AndroidSdk "ndk\$NdkVersion"
$projectFile = Join-Path $clientDir "WerewolfClient.pro"
$deploymentSettings = Join-Path $buildDir "android-WerewolfClient-deployment-settings.json"

$requiredPaths = @(
    $qmake,
    $androidDeployQt,
    $make,
    $AndroidSdk,
    $ndkDir,
    (Join-Path $JavaHome "bin\java.exe"),
    $projectFile
)

foreach ($requiredPath in $requiredPaths) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Missing Android build dependency: $requiredPath"
    }
}

$env:ANDROID_SDK_ROOT = $AndroidSdk
$env:ANDROID_HOME = $AndroidSdk
$env:ANDROID_NDK_ROOT = $ndkDir
$env:JAVA_HOME = $JavaHome
$env:PATH = "$JavaHome\bin;$QtRoot\Tools\mingw1310_64\bin;$AndroidSdk\platform-tools;$env:PATH"

New-Item -ItemType Directory -Force -Path $buildDir, $androidBuildDir, $outputDir | Out-Null

Push-Location $buildDir
try {
    & $qmake $projectFile "CONFIG+=release"
    if ($LASTEXITCODE -ne 0) {
        throw "qmake configuration failed with exit code $LASTEXITCODE"
    }

    & $make "-j8"
    if ($LASTEXITCODE -ne 0) {
        throw "C++ compilation failed with exit code $LASTEXITCODE"
    }

    & $make "-f" "Makefile" "INSTALL_ROOT=$androidBuildDir" "install"
    if ($LASTEXITCODE -ne 0) {
        throw "Android library installation failed with exit code $LASTEXITCODE"
    }

    & $androidDeployQt "--input" $deploymentSettings "--output" $androidBuildDir "--aux-mode"
    if ($LASTEXITCODE -ne 0) {
        throw "androiddeployqt failed with exit code $LASTEXITCODE"
    }

    # Qt aux-mode generates a default manifest and may drop the custom icon.
    # Overlay the resources and patch the generated manifest before Gradle runs.
    $customAndroidRes = Join-Path $clientDir "android\res"
    $generatedAndroidRes = Join-Path $androidBuildDir "res"
    New-Item -ItemType Directory -Force -Path $generatedAndroidRes | Out-Null
    Copy-Item -Path (Join-Path $customAndroidRes "*") -Destination $generatedAndroidRes -Recurse -Force

    $generatedManifestPath = Join-Path $androidBuildDir "AndroidManifest.xml"
    $generatedManifestText = Get-Content -LiteralPath $generatedManifestPath -Raw
    if ($generatedManifestText -match "%%INSERT_") {
        throw "androiddeployqt did not finish replacing AndroidManifest.xml placeholders"
    }
    [xml]$generatedManifest = $generatedManifestText
    $androidNamespace = "http://schemas.android.com/apk/res/android"
    $applicationNode = $generatedManifest.SelectSingleNode("/manifest/application")
    $activityNode = $generatedManifest.SelectSingleNode("/manifest/application/activity")

    if (-not $applicationNode -or -not $activityNode) {
        throw "Generated AndroidManifest.xml is missing application or activity nodes"
    }

    $applicationNode.SetAttribute("label", $androidNamespace, $appLabel)
    $applicationNode.SetAttribute("icon", $androidNamespace, "@mipmap/ic_launcher")
    $applicationNode.SetAttribute("roundIcon", $androidNamespace, "@mipmap/ic_launcher")
    $activityNode.SetAttribute("label", $androidNamespace, $appLabel)
    $activityNode.SetAttribute("screenOrientation", $androidNamespace, "portrait")
    $generatedManifest.Save($generatedManifestPath)
}
finally {
    Pop-Location
}

if ($PrepareOnly) {
    Write-Host "Android project prepared: $androidBuildDir" -ForegroundColor Green
    return
}

$gradleBat = Get-ChildItem -Path "$env:USERPROFILE\.gradle\wrapper\dists\gradle-8.14.3-bin\*\gradle-8.14.3\bin\gradle.bat" -File -ErrorAction SilentlyContinue |
    Select-Object -First 1 -ExpandProperty FullName

if (-not $gradleBat) {
    $gradleZip = Join-Path $clientDir "third_party\gradle-8.14.3-bin.zip"
    $localGradleRoot = Join-Path $buildDir "tools"
    $gradleBat = Join-Path $localGradleRoot "gradle-8.14.3\bin\gradle.bat"

    if (-not (Test-Path -LiteralPath $gradleBat)) {
        if (-not (Test-Path -LiteralPath $gradleZip)) {
            throw "Gradle 8.14.3 was not found in the user cache or at $gradleZip"
        }
        New-Item -ItemType Directory -Force -Path $localGradleRoot | Out-Null
        Expand-Archive -LiteralPath $gradleZip -DestinationPath $localGradleRoot -Force
    }
}

Push-Location $androidBuildDir
try {
    & $gradleBat "--no-daemon" "assembleDebug"
    if ($LASTEXITCODE -ne 0) {
        throw "Gradle APK build failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

$builtApk = Get-ChildItem -Path (Join-Path $androidBuildDir "build\outputs\apk") -Recurse -File -Filter "*.apk" |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if (-not $builtApk) {
    throw "The build completed without producing an APK"
}

$finalApk = Join-Path $outputDir "WerewolfClient-arm64-v8a-debug.apk"
Copy-Item -LiteralPath $builtApk.FullName -Destination $finalApk -Force

$apksigner = Join-Path $AndroidSdk "build-tools\36.0.0\apksigner.bat"
if (Test-Path -LiteralPath $apksigner) {
    & $apksigner "verify" "--verbose" $finalApk
    if ($LASTEXITCODE -ne 0) {
        throw "APK signature verification failed with exit code $LASTEXITCODE"
    }
}

$aapt = Join-Path $AndroidSdk "build-tools\36.0.0\aapt.exe"
if (Test-Path -LiteralPath $aapt) {
    $apkBadging = (& $aapt "dump" "badging" $finalApk 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to read APK metadata; aapt exited with code $LASTEXITCODE"
    }
    # Windows PowerShell 5.1 may decode aapt UTF-8 output using the active ANSI code page.
    # Verify that a label exists without comparing non-ASCII text in captured console output.
    if ($apkBadging -notmatch "application-label:'[^']+'") {
        throw "APK verification failed: application label is missing"
    }
    if ($apkBadging -notmatch "application:.*icon='[^']+'") {
        throw "APK verification failed: launcher icon is missing"
    }
}

Write-Host ""
Write-Host "APK build succeeded: $finalApk" -ForegroundColor Green
