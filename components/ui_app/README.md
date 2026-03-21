# UI application component

This component provides the **SquareLine-based UI application** that runs on top of LVGL:

- **squareline** – generated from the SquareLine Studio application (recommended for students)

## Requirements

- ESP-IDF **5.0.x**
- LVGL **8.3.x**
- Byte Lab Development Kit (or a compatible ESP32 board with display + touch, properly configured)

## How the UI app is selected

The entry point is `ui_app_init()` in `ui_app.c`. It calls `ui_init()` from `squareline/project/ui.h`.

Selection is done via **Kconfig**, but in this teaching template **"SquareLine generated example" is already the default**, so students can export their SquareLine project and flash immediately without touching `menuconfig`.

You can still open `idf.py menuconfig → Component config → UI application` if you want to verify the setting.

## SquareLine-based UI (recommended for students)

The `squareline/` folder is designed to be the **export target** for SquareLine Studio:

- `components/ui_app/squareline/` – exported C UI files (`ui.c`, `ui.h`, `ui_helpers.c`, images, screens, etc.)  
- `components/ui_app/squareline/project/` – the SquareLine project files (`.spj`, `.sll`, etc.)

In SquareLine, set:

- **Project Export Root** → `.../components/ui_app/squareline/project/`  
- **UI Files Export Path** → `.../components/ui_app/squareline/`

From ESP-IDF’s point of view, you only need to:

1. Export your UI from SquareLine into this folder.  
2. Build and flash the ESP-IDF project.

For event logic and custom behavior, students are encouraged to work in `squareline/ui_events.c` and `squareline/ui_events.h`. These files are preserved across SquareLine exports.
