# RayaTune — corrector de afinación VST3

Plugin de corrección de tono en tiempo real (monofónico, para voz e instrumentos de una sola línea),
con interfaz basada en el escudo de referencia (azul marino / dorado / blanco).

## Controles
| Control | Función |
|---|---|
| **Key / Scale** | Tonalidad y escala (Cromática, Mayor, Menor, Menor armónica, Pentatónicas, Blues, Dórica, Mixolidia) |
| **Retune Speed** | 0 ms = efecto "robot" instantáneo · 20–50 ms = natural · 200+ ms = corrección suave |
| **Amount** | Cuánto de la desviación se corrige (0–100 %) |
| **Detune** | Desplaza el tono objetivo ±100 cents |
| **Reference A** | Afinación de referencia (415–466 Hz) |
| **Mix / Output** | Mezcla seca/húmeda y ganancia de salida |
| **Bypass** | Puentea el efecto manteniendo la latencia |

La pantalla muestra la nota objetivo, el teclado de 12 notas (las notas de la escala se iluminan),
el tono detectado en tiempo real y el medidor de desviación en cents.

## Cómo obtener el .vst3

### Opción A — sin instalar nada (recomendada): GitHub Actions
1. Crea un repositorio en GitHub y sube el contenido de esta carpeta.
2. Pestaña **Actions** → se ejecuta *Build RayaTune* (o dale a *Run workflow*). Tarda ~10 min.
3. Al terminar, descarga el artefacto **RayaTune-VST3-Windows** (o macOS / Linux).
4. Copia `RayaTune.vst3` a:
   - Windows: `C:\Program Files\Common Files\VST3\`
   - macOS: `~/Library/Audio/Plug-Ins/VST3/` (si macOS lo bloquea: `xattr -cr RayaTune.vst3`)
   - Linux: `~/.vst3/`
5. Reescanea plugins en tu DAW.

### Opción B — compilar en tu equipo
- **Windows:** instala Visual Studio 2022 (carga de trabajo "Desarrollo para el escritorio con C++") y CMake; ejecuta `build_windows.bat` como administrador.
- **macOS:** `xcode-select --install`, `brew install cmake`, luego `./build_mac.sh`.

La primera compilación descarga JUCE 8.0.6 automáticamente (requiere internet).

## Notas técnicas
- Detección de tono: YIN con interpolación parabólica y filtro de mediana. Cambio de tono: dos cabezales
  con ventana Hann sincronizada al periodo (suman en fase, sin efecto flanger). Latencia reportada: 20 ms.
- Probado offline (`tests/dsp_test.cpp`): error de afinación < 1 cent a 44.1/48/96 kHz, sin clics, ~9 % de un núcleo.
- Mejor resultado con voz seca, sin reverb y sin coros/armonías (es monofónico).

## Licencias
- JUCE 8 se distribuye bajo AGPLv3 o licencia comercial. Para uso personal no hay problema; si vas a
  distribuir RayaTune públicamente o venderlo, necesitas publicar el código (AGPL) o una licencia de JUCE.
- El escudo es una marca del club; úsalo solo para uso personal/no comercial.
