# Infrared Analyzer Builder - Jonas.brm
$OutputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  Infrared Analyzer - Flipper Zero App Compiler" -ForegroundColor Yellow
Write-Host "  made by Jonas.brm" -ForegroundColor Green
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "[1/3] Pruefe Python und ufbt..." -ForegroundColor White
try {
    python --version | Out-Null
} catch {
    Write-Host "[FEHLER] Python wurde nicht gefunden!" -ForegroundColor Red
    exit 1
}

Write-Host "[2/3] Kompiliere FAP-Datei..." -ForegroundColor White
python -m ufbt
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FEHLER] Kompilierung fehlgeschlagen!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "[3/3] Kopiere Infrared_Analyzer.fap..." -ForegroundColor White
if (Test-Path "dist\infrared_analyzer.fap") {
    Copy-Item "dist\infrared_analyzer.fap" -Destination "Infrared_Analyzer.fap" -Force
    Write-Host "========================================================" -ForegroundColor Green
    Write-Host "  [ERFOLG] 'Infrared_Analyzer.fap' wurde erfolgreich" -ForegroundColor Green
    Write-Host "  erstellt und liegt direkt hier im Ordner!" -ForegroundColor Green
    Write-Host "========================================================" -ForegroundColor Green
} else {
    Write-Host "[FEHLER] dist\infrared_analyzer.fap nicht gefunden!" -ForegroundColor Red
}
