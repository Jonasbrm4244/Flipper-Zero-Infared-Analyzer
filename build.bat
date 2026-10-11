@echo off
setlocal
chcp 65001 >nul
title Infrared Analyzer Builder - Jonas.brm

echo ========================================================
echo   Infrared Analyzer - Flipper Zero App Compiler
echo   made by Jonas.brm
echo ========================================================
echo.

echo [1/3] Pruefe Python und ufbt...
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [FEHLER] Python wurde nicht gefunden! Bitte installiere Python.
    goto ende
)

echo [2/3] Kompiliere FAP-Datei...
python -m ufbt
if %errorlevel% neq 0 (
    echo.
    echo [FEHLER] Fehler beim Kompilieren mit ufbt!
    goto ende
)

echo.
echo [3/3] Kopiere Infrared_Analyzer.fap in diesen Ordner...
if not exist "dist\infrared_analyzer.fap" goto fehler_dist

copy /Y "dist\infrared_analyzer.fap" "Infrared_Analyzer.fap" >nul
echo.
echo ========================================================
echo   [ERFOLG] Die Datei 'Infrared_Analyzer.fap' wurde erfolgreich
echo   erstellt und liegt direkt hier im Ordner!
echo ========================================================
echo.
echo So installierst du die App auf deinem Flipper Zero:
echo 1. Schliesse deinen Flipper per USB an und starte qFlipper,
echo    ODER stecke die microSD-Karte in deinen PC.
echo 2. Kopiere 'Infrared_Analyzer.fap' in den Ordner:
echo       SD-Karte/apps/Infrared  oder  SD-Karte/apps/Tools
echo 3. Oeffne auf dem Flipper das Menue 'Applications' und starte Infrared Analyzer!
echo.
goto ende

:fehler_dist
echo [FEHLER] 'dist\infrared_analyzer.fap' wurde nicht gefunden.

:ende
echo.
pause
