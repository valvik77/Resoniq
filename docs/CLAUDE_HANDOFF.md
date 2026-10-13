# Traspaso de RESONIQ a Claude

Estado: 12 de octubre de 2026. Lee primero `AGENTS.md`, este documento y
`docs/PROJECT_MEMORY.md`. La memoria contiene historia: las decisiones recientes
sustituyen a las antiguas. Inspecciona el código antes de modificarlo.

## Objetivo y preferencias del usuario

Aplicación Windows 11 de mejora perceptiva del audio del sistema: sonido con
cuerpo, profundidad, claridad y potencia, especialmente a volumen moderado.
Primer equipo: Lenovo Legion Pro 5 83LU, Realtek y altavoces HARMAN. Debe admitir
otros equipos mediante perfiles. No es simplemente un ecualizador.

Identidad: **RESONIQ**, **RESONIQ DSP Engine**, **RESONIQ Enhance / Max**.
Azul eléctrico, grafito oscuro, interfaz minimalista moderna Windows 11.
Bongiovi fue inspiración; no copiar código propietario ni usar su marca en la
identidad pública. Disponibilidad de RESONIQ como marca todavía no comprobada.
Código propio MIT; consultar `THIRD_PARTY_NOTICES.md` para componentes externos.

El usuario quiere desarrollo autónomo y resultados concretos: no detener cada
iteración para pedir permiso para trabajo rutinario ya autorizado. Comunicar en
español, compilar, verificar y ejecutar lo modificado cuando corresponda.
Conservar sus ajustes: no sustituir la sesión por una demostración.

**Decisión firme: eliminar la igualación de volumen A/B.** Atenuaba demasiado y
el usuario no veía beneficio. Comparación visible exclusivamente DSP activado
(procesado) / desactivado (original), sin compensación de volumen. El núcleo
conserva comparación normalizada para pruebas internas; no volver a exponerla
en GUI o WASAPI salvo petición expresa.

## Implementación actual

- C++20, estéreo float32; motor preparado fuera del hilo de audio. Buffers
  preasignados y controles atómicos; ninguna asignación o bloqueo propios en
  procesamiento. Probar continuidad entre bloques y estabilidad numérica.
- DSP: biquads, compresor de cuatro bandas con crossover Linkwitz-Riley,
  armónicos graves, AGC, cuerpo/presencia y corte de barro, Mid/Side,
  limitador de picos a aproximadamente -1 dBFS, intensidad y bypass suavizados.
- Adaptación a nivel de entrada y realce de transitorios original con puerta
  para evitar amplificar ruido. Ver `docs/transient-processing.md`.
- Limitador actual es de muestras: no afirmar protección true-peak, lookahead
  o protección física de altavoces que no se haya implementado y medido.
- GUI independiente del DSP, pero **Win32/GDI+**, no WinUI 3. Dibujo propio de
  sliders, combos y toggle con semántica nativa de teclado; Segoe UI Variable,
  tarjetas redondeadas y atributos DWM. Cliente opaco: no afirmar Mica integral.
- Visualización es historial real de picos de salida, no osciloscopio de muestras.
- Archivos WAV offline disponibles; usuario no tiene WAV, las pruebas generan
  señales sintéticas. La ruta de tiempo real ya funciona.

## Ruta de audio y controlador

Ruta: sonido Windows → dispositivo virtual **DPS Audio Virtual Transport** →
captura WASAPI loopback → DSP en proceso de usuario → salida física Realtek.
Loopback solo no sustituye el audio del sistema. FxSound está desinstalado y no
es dependencia. No elegir FxSound Speakers ni capturar y reproducir en el mismo
endpoint creando realimentación.

Controlador virtual de desarrollo basado en Microsoft SysVAD, con documentación
y scripts en `drivers/virtual/` y `docs/virtual-device.md`. El DSP no reside en el
controlador. Un APO nativo es estudio futuro, no implementación terminada.

El usuario autorizó instalar el controlador en este Lenovo y activar firma de
pruebas; ya reinició. Según la comprobación previa Secure Boot estaba desactivado.
No reinstalar el controlador para cambiar el nombre comercial ni reiniciar
Windows automáticamente. Si necesitas estado actual, compruébalo.

Realtek presentó error 0x80040154 con efectos OEM en modo compartido normal;
RAW funciona. Toggle «Omitir efectos adicionales de Windows» controla esa ruta.
No imponer cambios en volumen de Windows. IDs locales históricos (enumerar si
han cambiado):

```text
Realtek: {0.0.0.00000000}.{6e5d6ebf-8251-48db-ba39-e13d55bf8d6a}
Virtual: {0.0.0.00000000}.{d89cf3d8-6a73-4992-b1ac-cf1ca440d9d9}
```

## Comportamiento vigente de la interfaz

- Abrir normalmente inicia automáticamente la ruta. No hay botón Iniciar/Detener.
- Interruptor DSP alterna procesado/original manteniendo el trabajador WASAPI.
  Cerrar la app termina la ruta; fallo de conexión permite reintentar.
- Intensidad, Bass, Clarity, Dynamics y Spatial aplican cambios en directo.
- Seleccionar perfil de dispositivo aplica automáticamente su calibración,
  conservando controles y estado DSP. **No hay botón Aplicar ajuste.** Cambios
  que requieren preparación reinician la sesión automáticamente.
- «Recuperar favorito» carga el ajuste aprobado y activa DSP; no equivale a
  original/bypass. Conserva la salida física seleccionada.
- «Sonido de Windows» abre `ms-settings:sound`.
- Guardar/cargar perfiles y selección de salida física disponibles.
- Presets Natural, Enhance, Power y Max admiten aliases históricos.

## Archivos importantes

| Archivo/directorio | Función |
| --- | --- |
| `apps/gui_main.cpp` | GUI, sesión y controles; cambios UI recientes |
| `src/audio/wasapi.cpp`, `include/dps/wasapi.hpp` | Transporte de tiempo real |
| `src/dsp/engine.cpp`, `include/dps/engine.hpp` | Cadena DSP |
| `include/dps/transient.hpp` | Realce de transitorios |
| `include/dps/stream_buffer.hpp` | Buffers de transporte |
| `src/io/profile.cpp`, `include/dps/profile.hpp` | Perfiles v1/v2 y validación |
| `apps/live_main.cpp`, `apps/wav_main.cpp` | CLI tiempo real y WAV |
| `tests/` | Pruebas DSP, perfiles, comparación, tono y transitorios |
| `config/session.user.dps` | Sesión local guardada automáticamente |
| `config/favorite.user.dps` | Favorito aprobado: preservar, no sobrescribir |
| `config/body-clarity.user.dps` | Ajuste experimental cuerpo/detalle revisión 2 |
| `config/focus.user.dps` | Revisión anterior conservada |
| `profiles/` | Perfiles distribuidos |
| `docs/architecture.md`, `README.md` | Arquitectura y uso |

Favorito vigente: `A91ECB7A6158F17517361790B52CA0B1E6EE3960F608404FD12FC0AA7509590D`
(«Referencia - Portatil impacto»). El anterior, `A5D2D70C...4CE2`, se conserva
en `config/favorite-previous.user.dps`.
No interpretar perfiles experimentales como calibración acústica validada.

## Compilar, comprobar y ejecutar

Workspace: `D:\OneDrive\Documentos\Desarrollo\VibeCoding\Resoniq`.
PowerShell, Visual Studio 18 2026; CMake ya configurado en `build/`.
El target interno `dps_gui` genera **resoniq.exe**; los nombres internos `dps`
siguen por compatibilidad y no significan que el rebranding esté pendiente.

```powershell
$taskCmakeBin = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
& "$taskCmakeBin\cmake.exe" --build build --config Release
& "$taskCmakeBin\ctest.exe" --test-dir build -C Release --output-on-failure
& "$taskCmakeBin\ctest.exe" --test-dir build -C Release -R gui_controls --output-on-failure
.\build\Release\dps_live.exe --status
# Prueba de ruta real: cerrar antes la instancia normal, no ejecutar dos hosts.
.\build\Release\resoniq.exe --audio-test
Start-Process .\build\Release\resoniq.exe -WindowStyle Normal
```

Antes de recompilar el ejecutable abierto, cerrar su ventana de forma normal
para guardar sesión y esperar a que termine. Mutex compartido
`Local\DpsAudioEngine.LiveHost` impide hosts duplicados. Los modos de prueba GUI
no guardan sesión. Lanzador normal: `Start-RESONIQ.cmd`; `Start-DPS.cmd` compatible.

CTest contiene seis pruebas: `dsp_validation`, `profile_validation`,
`gui_controls`, `comparison_validation`, `tonal_validation`,
`transient_validation`. Cubren NaN/Inf, límites de muestras, varias tasas,
continuidad exacta entre bloques, ausencia de asignaciones, perfiles y controles.
Pruebas sintéticas no equivalen a mediciones acústicas ni a mejora audible.

Artefactos útiles: `build/gui-test-result.txt`, `build/gui-preview.bmp`,
`build/gui-platform-result.txt`, `build/gui-telemetry.log` y resultados de audio
test. Telemetría: nivel entrada/salida, estado, parámetros, GR y fallos de buffers.
Evitar búsquedas recursivas en `drivers/virtual/packages`, `vendor` y `build`:
contienen miles de archivos SDK. Leer/escribir fuentes como UTF-8 explícito.

> **Al retomar, lo primero:** pedir al usuario que repita la medicion de su
> habitacion. Su unica medicion valida es anterior a la auditoria, que cambio
> alineacion, manejo de errores y COM. Comparar con: corte 162 Hz, cuerpo
> 7.3 dB, presencia 10 dB. No es urgente: su sonido actual le gusta.
>
> Auditoria completa en `docs/AUDIT.md` (veinte hallazgos, once pruebas).
> Limites conocidos al final de ese informe: leerlos antes de prometer nada.
>
> Resumen de la sesion del 12 de octubre de 2026 para Codex:
> **`docs/CODEX_HANDOFF.md`**. Incluye los cambios, lo que quedo sin verificar y
> las trampas de este codigo (macros de paleta, escapes en cadenas, coordenadas
> de WM_NCHITTEST).

## Al retomar: leer esto primero

**Objetivo declarado del usuario: que RESONIQ sustituya a Bongiovi DPS.** El
efecto debe parecerse lo maximo posible al de aquella aplicacion, que realzaba
notablemente el sonido en portatiles. Ese es el criterio de exito, por encima
de que las pruebas pasen.

Sesion pausada el 12 de octubre de 2026 sin trabajo a medias: compila, seis
pruebas pasan, favorito intacto, sesion del usuario sin tocar.

**El usuario escucho el ajuste estilo DPS y le gusta.** Es la primera
aprobacion auditiva del proyecto. Quedo integrado como opcion por defecto:
perfil de dispositivo «Portatil: impacto» (`Laptop Impact`), quinta entrada del
desplegable, mas `profiles/laptop-impact.dps` y `config/approved-dps.user.dps`.
Ya activo «Omitir efectos adicionales de Windows»; forma parte de lo aprobado.

A peticion suya, ese ajuste es ya **su favorito**: `config/favorite.user.dps`
= «Referencia - Portatil impacto», dispositivo `Laptop Impact`, raw 1, hash
a91ecb7a...9590d. El favorito anterior (a5d2d70c...4ce2, mas suave) se conserva
en `config/favorite-previous.user.dps`; no borrarlo.

Pendiente de el:
1. Si quiere acercarse aun mas a Bongiovi: subir `loud_density` por pasos
   cortos, comprobando en cada uno que no se vuelve cansado o plano.
2. Confirmar visualmente que no reaparecen tiradores clasicos al arrastrar.

Despues, temas ya acordados: reconexion de dispositivos y latencia real.

Dos cosas que no repetir, ambas falladas en esa sesion: no igualar favorito y
sesion por iniciativa propia (son distintos a proposito, preguntar antes), y no
diagnosticar comportamiento dinamico con tonos estables (usar senal con pasajes
bajos y altos). Detalle completo en `docs/PROJECT_MEMORY.md`.

Codex es quien ha construido este proyecto. Su trabajo es la base, no un
añadido reciente; contrastar con el antes de suponer que algo falta.

## Último cambio y pendientes

Fragmentos blancos del tirador clásico sobre los sliders: corregido en el
origen. Las barras no admiten dibujo propio por estilo, así que el control con
tema visual siempre pintaba su tirador y el repintado posterior solo tapaba los
mensajes enumerados. Ahora se les retira el tema visual (`SetWindowTheme` con
nombre vacío, enlazando `uxtheme`), el dibujo propio pasa por un mapa de bits
intermedio para no parpadear, y la lista de mensajes se amplió con los que
provocan dibujo nativo. Seis pruebas pasan, `gui_controls` en 0.62 s, WASAPI
real pasa, app reiniciada con la sesión del usuario.

No repetir dos intentos ya descartados: un filtro amplio «todo menos consultas»
dispara repintados en cada tick y bloquea `gui_controls` (de 0.14 s a +7 min);
`WS_EX_COMPOSITED` en los hijos choca con `WS_CLIPCHILDREN` del padre y deja
los controles sin dibujar.

**Pendiente confirmar visualmente que no reaparece durante uso real.** La
captura `build/resoniq-sliders-after.png` se revisó y está limpia, pero usa
WM_PRINT y puede no reproducir el dibujo nativo directo durante un arrastre con
el ratón. No dar por verificado solo porque pasa el test.

**Favorito y sesión son cosas distintas: no igualarlas.** El favorito es la
referencia guardada; la sesión es lo que el usuario está usando. Que difieran es
el comportamiento correcto, no un fallo. En esta iteración se restauró el
favorito sobre la sesión al ver que no coincidían: fue un error, sustituía el
sonido que el usuario tenía en uso. Se deshizo y la sesión volvió a su estado
real (Laptop Focus / RESONIQ Max, corte 130 Hz, cuerpo y presencia 11 dB,
raw 0). Conservar ambos y preguntar antes de decidir cuál vale.

Al cerrar, la aplicación guarda sobre `config/session.user.dps` lo que tenga en
pantalla. Eso es normal; tenerlo en cuenta antes de cerrar una instancia ajena.

Las seis pruebas completas y la ruta WASAPI pasaron en iteraciones anteriores;
el último cambio de repintado repitió la prueba GUI, no toda la batería ni una
nueva evaluación acústica. Este traspaso es documentación, sin cambios DSP.

El usuario aprecia la mejora a volúmenes moderados; a 100 % es menos perceptible.
No dar por resuelto ese objetivo. Continuar ajuste mediante mediciones y escucha,
preservando favorito y evitando promesas basadas solo en aumento de nivel.
Después: robustez de dispositivo/reconexión, evaluación de latencia real,
perfiles para otros equipos y estudio APO/distribución cuando se solicite.

Para empezar: revisar memoria y código, comprobar estado de ejecución y ajustes,
atender el siguiente pedido del usuario sin repetir cambios ya completados.

## Nombre del ajuste de referencia

Lo que antes se llamaba «favorito» se presenta al usuario como **«Ajuste por
defecto»** (boton, menu del icono de bandeja, mensajes y README). El archivo
sigue siendo `config/favorite.user.dps` y la funcion sigue siendo
`recoverFavorite()`: no renombrar el archivo sin migrar el existente.

El menu del icono incluye «Fijar lo actual como ajuste por defecto», que pide
confirmacion y conserva el anterior en `config/favorite-previous.user.dps`.

