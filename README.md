# 📡 Infrared Analyzer für Flipper Zero

**made by Jonas.brm**

Eine vollwertige Infrarot-Analyse- und Capture-App für den Flipper Zero mit automatischer Geräteerkennung, Signalanalyse, Speichern von Captures auf SD-Karte und Signal-Wiedergabe (Replay/TX) im originalen Flipper-Design.

---

## ✨ Features & Übersichtliches UI

- **🚀 3-Sekunden Start-Animation:**
  - *"made by Jonas.brm"* mit animiertem Ladebalken und pulsierender Fernbedienungs-Grafik.
- **🔍 Tab 1: Analyse:**
  - Echtzeit-Infrarot-Empfang.
  - Zeigt automatisch Hersteller & Gerätetyp (*Samsung TV, Sony Bravia, Apple TV, LG TV, etc.*), Signal-Taste (*Power, Lautstärke +, Mute, etc.*) und alle technischen Daten (*Protokoll, Adresse, Command, kHz, Duty Cycle*).
  - **Übersichtliche 2-Button-Leiste:**
    - **[◄] Links:** `◄ Löschen` (Verwirft das gefangene Signal und scannt neu).
    - **[►] Rechts / [OK]:** `Speichern ►` (Speichert das Signal auf die SD-Karte & in die Captures-Liste).
- **💾 Tab 2: Saved Captures:**
  - Liste aller gespeicherten Signale mit Gerät, Taste und Hex-Codes.
  - **[▲] Oben / [▼] Unten:** Durch gespeicherte Signale scrollen.
  - **Übersichtliche 3-Button-Leiste (wie im Original):**
    - **[◄] Links:** `◄ Lösch` (Löscht das ausgewählte Signal sofort).
    - **[OK] Mitte:** `OK: Senden` (Sendet das Signal per Infrarot-LED aus).
    - **[►] Rechts:** `Info ►` (Öffnet die detaillierte Signal-Info-Ansicht).
- **📋 Detail-Ansicht (Info):**
  - Zeigt die vollständigen Signal-Parameter an.
  - **[◄] / [Zurück]:** Zurück zur Liste.
  - **[OK]:** Signal direkt aus der Info-Ansicht senden.

---

## 🕹️ Tastenbelegung im Überblick

### Im Tab `Analyse`:
| Taste | Wenn Signal gefangen wurde | Im Scan-Modus (Warten) |
|---|---|---|
| **[◄] Links** | **◄ Löschen** (Verwerfen & neu scannen) | - |
| **[►] Rechts** | **Speichern ►** (In Captures ablegen) | Zu *"Saved Captures"* wechseln |
| **[OK]** | **Speichern** | - |
| **[Zurück]** | Verwerfen / Reset | App beenden |

### Im Tab `Saved Captures` (Liste):
| Taste | Aktion |
|---|---|
| **[▲] Oben** | Vorheriges Capture auswählen |
| **[▼] Unten** | Nächstes Capture auswählen |
| **[OK]** | **Signal senden (TX / Wiedergeben)** |
| **[►] Rechts** | **Info ►** (Detailansicht öffnen) |
| **[◄] Links** | **◄ Lösch** (Ausgewähltes Capture löschen) |
| **[Zurück]** | Zurück zum *Analyse*-Tab |

---

## 📲 Installation

1. Kopiere die Datei **`Infrared_Analyzer.fap`** per qFlipper auf deine SD-Karte:
   ```
   SD-Karte / apps / Infrared
   ```
2. Auf dem Flipper öffnen unter:
   **Applications ➔ Infrared ➔ IR Analyzer**
