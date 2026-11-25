# CrazyHead VST — Hoja de proyecto

Familia de apps de **Sampled Head**. **Versión actual:** 1.0.0 (25-11-2026) · **Repositorio:** https://github.com/sampledhead-dot/CrazyHead-VST

## Descripción
Versión plugin (instrumento VST3 / AU / Standalone, JUCE 9.0.5) de **CrazyHead**, en versión básica y de **una sola pista**:
cargas un loop, eliges la región, se trocea (por golpes, por rejilla o a mano), se pueden **fijar golpes** en pasos concretos
y el plugin toca un patrón al azar con el mismo generador que la app, **sincronizado con el tempo y el compás del DAW**.
Lleva los **efectos aleatorios** de la app (filtro, delay, reverb, bit, drive, pan, stutter) y su estética (violeta, amarillo, grafiti).
**No** incluye los efectos de GlitchHead (tienen su propio plugin), ni las 4 pistas, ni compresor/dub/sidechain/escenas/exportaciones.

## Arquitectura
```
vst/Source/core/CrazyGen.h     Generador (SIN JUCE): mulberry32, genCell, pinCell, regen, dado, mutación, detección de golpes, troceado.
                               Port 1:1 de CrazyHead (index.html). Probado contra la app: tests/parity.js
vst/Source/core/CrazyEngine.h  Motor de audio (SIN JUCE): loop con trozos, voces, secuenciador por posición (ppq) del DAW,
                               notas MIDI, filtro, bitcrush, drive, delay, reverb, efectos aleatorios (rfx), limitador suave
vst/Source/PluginProcessor.*   Parámetros (APVTS), transporte del DAW, carga de audio, región/marcas, estado (audio dentro del proyecto)
vst/Source/PluginEditor.*      Ventana: vista general + región, trozos, patrón, golpes fijos, mandos, efectos, aleatorios
vst/Source/UpdateBanner.h      Aviso de versión nueva (común a los plugins de Sampled Head)
tests/parity.cpp + parity.js   Paridad JS ↔ C++ del generador (40 casos: regen + mutación + dado) contra CrazyHead/index.html
tests/core_test.cpp            Pruebas del motor (sincronía, bloques variables, MIDI, golpes fijos, aleatorios, estabilidad, CPU)
vst/tests/TestMain.cpp         Pruebas del plugin (carga, troceado, reproducción, MIDI, estado, captura de la ventana)
.github/workflows/build-vst.yml  Pruebas en Linux + compilación Windows y Mac universal; release `vst-vX.Y.Z`
```
Hilos: el hilo de audio es el dueño del patrón y del motor; la interfaz le habla con atómicos (`pinsUI`, `poolUI`, órdenes de
regenerar/dado/audición) y entrega el loop nuevo con un `shared_ptr` bajo SpinLock (los loops viejos se guardan en `keepAlive` para
no liberarlos en el hilo de audio).

## Cómo suena (decisiones)
- Cada paso de 1/16 se calcula desde la posición en negras (ppq) del DAW: salto, bucle o arranque a mitad de compás funcionan.
  Sin DAW (Standalone) usa un transporte interno (Play/Stop) y el mando «Tempo».
- `Seguir tempo`: velocidad = tempo del proyecto / tempo original del loop (compases × 4 × 60 / duración), como `follow` de la app (cambia el tono).
- Notas MIDI C1 (36)–D#2 (51) = trozos 1–16 (como el modo Slice del Simpler). Un instrumento no puede escribir MIDI en la pista del DAW.
- Reverb: red de 4 retardos con matriz de Hadamard (la app usa convolución con ruido): parecido, no idéntico. Sin delay dub ni compresor.
- Estado: «CHVST1» + XML (mandos, semilla, región, marcas, golpes fijos) + audio PCM 16 bits estéreo (máx. 60 s).

## Reglas permanentes
- Versionado semántico `MAYOR.MENOR.PARCHE`: `project(... VERSION)` en `vst/CMakeLists.txt` **y** `kVersion` en `PluginProcessor.h`.
- **Paridad:** cualquier cambio en `CrazyGen.h` exige el mismo cambio en CrazyHead (index.html) y pasar `tests/parity.js`
  (el workflow descarga CrazyHead y lo comprueba). Los parámetros de sonido del generador no se cambian solo aquí.
- Entrega numerada (`CrazyHead-VST-X.Y.Z-Windows.zip` / `-Mac.zip` en la release `vst-vX.Y.Z`), hoja de proyecto y LEEME actualizados.
- Probar antes de entregar: `tests/parity.js`, `tests/core_test.cpp`, `CrazyHeadTests` y decir con honestidad qué no se pudo probar.
- Registro: portal SH-apps (`app.js` → `vst` en la tarjeta de CrazyHead) y plataforma (`desktop/apps.json` → `plugins`).

## Historial
- **1.0.0** (25-11-2026): primera versión. Instrumento VST3/AU/Standalone de una pista; generador con paridad exacta con la app;
  región, troceado automático/rejilla/manual, golpes fijos (con probabilidad), mutación, humanize, swing; efectos aleatorios; MIDI; aviso de versión.

## Límites conocidos
- **No probado en un DAW real, ni en Windows/Mac reales** (compilado y probado en Linux solo el motor y las partes sin ventana; la
  ventana, el enlazado del plugin y los formatos se compilan en GitHub Actions). Sin firma de Apple (firma ad-hoc).
- **La ventana no se ha visto con los ojos** (la captura `crazyhead-ventana.png` sale de la prueba en GitHub): revisar maquetación.
- El troceado automático y los efectos aleatorios están portados de la app, pero no comparados a oído con ella.
- Audio máximo 60 s (va dentro del proyecto del DAW). Sin cambio de velocidad sin cambio de tono (como la app).
- La app tiene más cosas (4 pistas, sidechain, escenas, exportar a Live/Move/MPC/SP-404, grabador): no están aquí.
