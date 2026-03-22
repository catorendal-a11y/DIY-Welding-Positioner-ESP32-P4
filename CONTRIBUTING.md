# Contributing to DIY Welding Positioner

First off, thank you for considering contributing to this open-source project! 🚀

## Development Workflow

1. **Fork** the repository on GitHub.
2. **Clone** your fork locally.
3. **Branch** off `master` for your feature/bugfix (e.g., `git checkout -b feature/wifi-remote`).
4. **Build & Test** locally using PlatformIO.
5. **Commit** your changes with clear, descriptive messages.
6. **Push** your branch to your fork.
7. **Submit a Pull Request** (PR) to the original repository.

## Development Environment
This project runs entirely on **PlatformIO**. 
Please ensure your code builds cleanly against both environments before submitting a PR:
- `pio run -e esp32p4-release`
- `pio run -e esp32p4-debug`

## Coding Standards
- **UI Code:** All LVGL UI code must reside purely within the `src/ui/` isolated directory. Do not mix hardware logic with GUI rendering.
- **Motor Logic:** All stepper timings and FastAccelStepper calls remain in `src/motor/`.
- **Indentation:** 2 spaces. 
- **Comments:** Comment any complex hardware integration logic block.
