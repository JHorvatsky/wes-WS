# Apply LVGL 8.3 patch to lvgl_esp32_drivers and prepare config. Run from project root.
# Patch only applies to the pinned commit (LVGL 8.3 compatible). See README.
$ErrorActionPreference = "Stop"

$RequiredCommit = "335d444e450c6669a9f8f91c350def5dfaf53788"

if (-not (Test-Path "components\lvgl_esp32_drivers\CMakeLists.txt")) {
    Write-Error "Submodule not updated. Run: git submodule update --init --recursive"
    exit 1
}

Push-Location components\lvgl_esp32_drivers
try {
    $commit = git rev-parse HEAD 2>$null
    if ($commit -ne $RequiredCommit) {
        Write-Host "Error: lvgl_esp32_drivers must be on commit $RequiredCommit for this patch."
        Write-Host "  Run: cd components\lvgl_esp32_drivers; git checkout $RequiredCommit"
        exit 1
    }
    git apply -p1 ..\..\patches\lvgl_esp32_drivers_8-3.patch
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Patch failed to apply. Ensure the submodule is on commit $RequiredCommit."
        exit 1
    }
    Write-Host "LVGL driver patch applied successfully."
} finally {
    Pop-Location
}

if (Test-Path "sdkconfig") {
    Write-Host "sdkconfig already exists, leaving it unchanged."
} elseif (Test-Path "sdkconfig.defaults") {
    Copy-Item "sdkconfig.defaults" "sdkconfig"
    Write-Host "sdkconfig.defaults copied to sdkconfig."
} else {
    Write-Warning "sdkconfig.defaults not found; no sdkconfig created."
}
