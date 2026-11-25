# CrazyHead VST

Instrumento **VST3 / AU / Standalone** de **Sampled Head**: la versión básica (una pista) de [CrazyHead](https://github.com/sampledhead-dot/CrazyHead).
Cargas un loop, lo troceas, fijas golpes y el generador toca un patrón al azar sincronizado con tu DAW, con efectos aleatorios.

- Descargas: pestaña **Releases** (`vst-vX.Y.Z`, zips para Windows y Mac) o desde el portal y la app de plataforma de [SampledHead](https://github.com/sampledhead-dot/SH-apps).
- Instalación y uso: `vst/LEEME.txt`. Detalles técnicos: `HOJA_DE_PROYECTO.md`.
- Compilar: `cmake -S vst -B build && cmake --build build --config Release` (descarga JUCE 9.0.5).
