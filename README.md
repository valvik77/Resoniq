# RESONIQ

![Icono de RESONIQ](assets/brand/resoniq-app-128.png)

[Descargar el instalador 0.2.11 para Windows x64](https://github.com/valvik77/Resoniq/releases/tag/v0.2.11).
Instala RESONIQ y usa el asistente de VB-CABLE incluido al finalizar.
El instalador activa el control de volumen de Windows del cable. Tras instalar
VB-CABLE, reinicia Windows y selecciona CABLE Input como salida de Windows
y entrada virtual de RESONIQ; elige tus altavoces como salida física.
Consulta el [procedimiento completo](docs/VB-CABLE.txt).

Aplicación C++20 para mejorar el audio del sistema, con transporte virtual
VB-CABLE o controlador propio, RESONIQ DSP Engine y salida física por WASAPI. El objetivo del proyecto
es ofrecer la mejora de altavoces de portátil que busca el usuario. El perfil
Laptop Impact+ fue aprobado por escucha en el Lenovo Legion Pro 5 83LU.

## Uso en este equipo

Abre **Start-RESONIQ.cmd** y selecciona **Altavoces (DPS Audio Virtual Transport)**
como salida de Windows. El volumen físico se conserva. La ruta comienza al abrir
la aplicación; el interruptor DSP alterna original/procesado sin detenerla ni
compensar volumen. La igualación solo existe en pruebas internas. El host respeta
«Silencio» y volumen cero del endpoint virtual aunque su captura loopback siga
entregando audio: silencia la salida después del DSP, incluido el bypass.

La ventana usa Win32/GDI+, temas claro/oscuro/sistema y controles propios.
Al cerrarla se oculta en la bandeja y continúa procesando, salvo que se configure
«Al cerrar la ventana, salir del todo». El menú de bandeja permite salir.
Los ajustes incluyen inicio con Windows, arranque minimizado, reconexión del
audio y estado inicial del DSP. La reconexión espera a la salida guardada,
sin sustituirla automáticamente por otro altavoz. Suspender Windows detiene
la ruta y reanudarla permite reconectar según preferencias. Un render sin
progreso durante cinco segundos se considera detenido. No garantiza continuidad
ni cambia la salida predeterminada de Windows.

Los presets y deslizadores se aplican en directo. Cambiar salida, calibración,
RAW o cargar un perfil completo detiene y prepara el trabajador; puede haber
una breve pausa. RAW solicita omitir los efectos de Windows para la salida de
esta aplicación, sin cambiar el volumen ni los ajustes globales.

## Perfiles y preferencias

- `config/favorite.user.dps`: ajuste por defecto aprobado. «Ajuste por defecto»
  lo recupera. «Fijar actual» pide confirmación y conserva una copia anterior.
- `config/session.user.dps`: sesión actual. Es independiente del ajuste por
  defecto; que difieran es correcto. Los modos de prueba no la guardan.
- `config/settings.user.txt`: preferencias de comportamiento y apariencia,
  separadas del sonido. La entrada de inicio se consulta en HKCU Run.
- `profiles/`: calibraciones distribuidas, incluidos Laptop Impact e Impact+.

Los perfiles v1/v2 contienen controles, calibración, RAW, etiquetas e ID de salida.
Los v1 conservan los valores neutros de las funciones posteriores. V2 requiere
`mud_hz`, `mud_db`, `detail_db`, `harmonic_focus`, `body_floor`; son opcionales
`adaptive_depth`, `loud_density`, `band_density`, con valores históricos por
defecto. Los nombres antiguos de presets siguen siendo compatibles.

«Detectar» consulta fabricante/modelo en el registro de BIOS y distingue un
perfil ajustado para un modelo conocido, una inferencia y un genérico. Los
auriculares tienen prioridad. El reconocimiento por nombre y modelo no
identifica con certeza todos los dispositivos externos conectados al portátil.

«Medir» pide permiso cada vez antes de usar el micrófono y vuelve a pedirlo antes
de aplicar una propuesta. Reproduce ocho tonos y analiza la captura localmente.
Un perfil aplicado se identifica como **Medido en este equipo** (`Measured`);
sus valores reales se guardan en el perfil. El micrófono del portátil y su
posición colorean la respuesta: la propuesta requiere escucha.

La grabación se descarta por defecto. Solo `resoniq --measurement-diagnostics`
permite guardar `build/measure-recorded.wav`, y el diálogo avisa de ello antes
de grabar. El WAV duplica el canal mono como estéreo, conservando duración y
frecuencia. `build/measure-debug.txt` conserva cifras de diagnóstico, sin audio.
Los WAV de diagnóstico históricos no se borran automáticamente.

La medición alinea conjuntamente los tonos medios y comprueba que al menos dos
son identificables. Usa el RAW de la sesión; las discontinuidades se rechazan.
La propuesta de corte cambia de forma continua y el diagnóstico registra bandas
y desfase. Ver [consolidación](docs/CONSOLIDATION.md) para cambios y límites.

## Compilar y verificar

Requisitos: CMake ≥3.20, compilador C++20; para GUI/WASAPI, Windows y Windows SDK.
En Visual Studio, CMake puede estar disponible solo desde su consola de desarrollo.

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tests/test_driver_package.py
```

Hay diecisiete pruebas en Windows: DSP, perfiles, transporte virtual, recuperación, comparación interna, timbre,
transitorios, medición, preferencias, elección de calibración, WAV, GUI y errores
WASAPI, silencio del endpoint e intensidad experimental. Las pruebas WASAPI de error comprueban entradas
inválidas y COM STA sin abrir el micrófono; la de silencio comprueba mute, cero
y notificaciones sin asignaciones.
La prueba de ataques evalúa el ensayo temporal, cuerpo y techo de salida.
La comprobación del dispositivo enumera únicamente nuestro Hardware ID, sin instalarlo.
La prueba de paquete requiere el controlador de desarrollo ya construido y
firmado; es offline y no cambia almacenes de certificados ni instala nada.

`resoniq --self-test` comprueba controles y genera capturas. `--audio-test`
comprueba la ruta real y la conmutación DSP sin sustituir el trabajador;
ejecutarlo con la aplicación habitual detenida. No forma parte de CTest.
Incluye mensajes simulados de suspensión/reanudación y reapertura real de streams.
`-DBUILD_TESTING=OFF` omite los ejecutables de prueba.

## Paquete portable local

`scripts/Package-RESONIQ.ps1` prepara un ZIP por lista explícita de archivos,
con favorito, sesión, perfiles estables y manifiesto SHA-256. Excluye experimentos,
grabaciones, claves y controladores. El controlador y runtime deben estar ya
disponibles. Uso y recuperación en [versión local](docs/STABLE_LOCAL.md).
No sustituye un instalador ni la firma de producción.

## Instalador y dispositivo virtual

El instalador actual es `dist/RESONIQ-Setup-0.2.9-x64-VBCABLE.msi`,
generada con `installer/Build-CableInstaller.ps1`. Instala RESONIQ y añade
un asistente «Instalar VB-CABLE» al menú Inicio; al finalizar ofrece abrirlo.
Descarga, verifica las firmas y extrae el paquete oficial, y abre su instalador
con elevación. Quedan la autorización de administrador, «Install Driver» y el
reinicio manual. Si el dispositivo existe, evita abrir el instalador de nuevo.
El MSI activa el ajuste persistente Windows Volume Control de VB-CABLE;
un cable ya instalado requiere reiniciar Windows para leer esa configuración.
La versión 0.2.6 añade el banner «Resoniq by Valvik», «Acerca de» con versión
y créditos. Desde la 0.2.10 el volumen de Windows del cable actúa una sola vez
y antes del DSP: el host mide con un tono piloto breve si el cable ya atenúa su
loopback y solo aplica él la curva en dB del endpoint cuando el cable la ignora.
Validación y límites en [VB-CABLE_VOLUME.md](docs/VB-CABLE_VOLUME.md).
La versión 0.2.7 abre RESONIQ al pulsar Finalizar en el asistente de instalación
o actualización, con permisos normales de usuario. Ajusta los créditos de Acerca
de y prepara captura RAW del micrófono para medir; si el controlador la rechaza,
usa un cliente nuevo en modo normal y avisa. Los canales se analizan por separado
para evitar cancelación de fase. Detalles en [MEASUREMENT_PORTABILITY.md](docs/MEASUREMENT_PORTABILITY.md).
La 0.2.8 solicita una salida normal a la aplicación instalada antes de comprobar
archivos en uso, incluyendo versiones anteriores que se ocultan al recibir Cerrar.
La GUI responde además al cierre de Restart Manager. Se guarda la sesión y se
detiene el audio; el instalador vuelve a abrir RESONIQ al finalizar.
Ver [INSTALLER_SHUTDOWN.md](docs/INSTALLER_SHUTDOWN.md).
La 0.2.9 añadió un control Gain que la 0.2.10 retira: con el perfil ya en el
techo del limitador solo reducía el contraste de los golpes. Los perfiles que
lo guardaron se siguen cargando, sin él. Actividad DSP y Realce DSP muestran la etapa
del motor; el medidor Salida muestra lo que se envía al dispositivo físico.
La medición ajusta el tono conservando densidad, adaptación, ataques y demás
ajustes musicales del perfil elegido. Ver [GAIN_AND_METERS.md](docs/GAIN_AND_METERS.md).
Identidad «Pulso continuo» y variantes en [assets/brand](assets/brand/README.md):
icono de aplicación, bandeja por estado, accesos directos y arte del instalador.
Procedimiento en [VB-CABLE.txt](docs/VB-CABLE.txt). La GUI permite elegir la
entrada virtual y guarda su ID en preferencias; todos los endpoints propios
o VB-CABLE reconocidos se excluyen de la lista de salidas físicas.
Las pruebas automatizadas cubren detección, elección, persistencia y coexistencia;
la ruta real VB-CABLE todavía requiere validación. El DSP aprobado no cambia.

El controlador propio está conservado en una [instantánea](docs/OWN_DRIVER_RECOVERY.md).
Los siguientes comandos corresponden a esa vía anterior:

`installer/Build-Installer.ps1` genera un MSI x64 con aplicación, perfiles,
accesos y creación/retirada del dispositivo DSP mediante APIs Windows.
La aplicación instalada guarda sus datos por usuario en LocalAppData.
La compilación estática evita instalar Visual C++ aparte.

El MSI disponible incluye **firma de desarrollo**: la creación del dispositivo
requiere un equipo de pruebas ya preparado. Para Windows normal con Secure Boot
falta el paquete firmado por Microsoft; `-Production` rechaza el driver de pruebas.
Objetivo del INF actual: Windows 11 22H2+, x64. Ver [instalación](docs/INSTALLATION.md).
`installer/Prepare-Driver-Submission.ps1` prepara el CAB para la tramitación externa.

## Herramientas de consola

```powershell
.\build\Release\dps_live.exe --list
.\build\Release\dps_live.exe --dps --render-id "ID_SALIDA_FISICA" --raw --profile config\favorite.user.dps
.\build\Release\dps_wav.exe entrada.wav salida.wav --profile profiles\laptop-impact-plus.dps
.\scripts\Start-DPS.ps1 -Profile .\profiles\headphones.dps -ChooseOutput
```

GUI y consola comparten el control de instancia única. **B** cambia DSP,
**+/-** intensidad, **S** guarda y **Q/Esc** sale. El volumen físico solo cambia
con `--output-volume` o `-OutputVolume`. Los índices de dispositivos pueden
cambiar: preferir IDs persistentes. No usar captura y reproducción sobre la
misma salida, ni el micrófono como entrada de la ruta de música.

La herramienta offline lee RIFF WAV estéreo PCM16/24/32 o float32, 16–192 kHz,
con un máximo de datos de 512 MiB. Rechaza mono, extensible, RF64 y muestras no
finitas. Escribe float32 con `fact`, conserva duración/frecuencia y no agrega
cola de filtros. Evita sobrescribir la entrada; sustituye la salida indicada.
Perfiles, preferencias y WAV se escriben primero en un temporal y se sustituyen
solo después de cerrar correctamente el archivo. No es una garantía frente a
fallos de energía ni un sistema de copias de seguridad.

`dps_quality PERFIL [FRECUENCIA_HZ]` caracteriza señales eléctricas offline;
`dps_benchmark [PERFIL]` mide coste del motor. La variante de Intensidad está en
`config/intensity-aligned.user.dps`; es optativa y conserva cero/100 %. Consultar
[resultados y límites](docs/INTENSITY_EXPERIMENT.md) antes de adoptarla.
El usuario no apreció mejora audible en esa variante de Intensidad; no se adopta.
El nuevo ensayo `config/attack-body.user.dps` añade un acento temporal breve:
ver [resultados y límites](docs/ATTACK_EXPERIMENT.md). Ningún ensayo sustituye
automáticamente el ajuste aprobado.

## Arquitectura y límites

```text
include/dps/       API portable y adaptadores
src/dsp/           filtros, cadena DSP y análisis de medición
src/io/            WAV, perfiles, preferencias y elección de calibración
src/audio/         WASAPI y captura de medición
apps/              GUI, consola, render offline y benchmark
tests/             regresiones
drivers/virtual/   transporte de desarrollo basado en SysVAD
docs/              arquitectura, memoria, traspaso y auditoría
```

`Engine` es portable y no comparte estado directamente con la UI. Preparación
y reset requieren audio detenido; el trabajador aplica controles suavizados y
publica medidores atómicos. `process` no asigna memoria ni bloquea. La cadena
incluye crossover LR4, compresión enlazada, detalle/ataques, armónicos, EQ,
AGC, Mid/Side, mezcla y limitador de muestra a −1 dBFS.

Ese limitador no garantiza true peak ni protección térmica/mecánica del
altavoz. Los armónicos no usan sobremuestreo. La latencia mostrada se calcula
con latencias notificadas por los endpoints, capacidad del render y ocupación
FIFO: es una **estimación**, no una medición de ida y vuelta física.
Las pruebas eléctricas no equivalen a validación acústica o auditiva.

El controlador está instalado y firmado para desarrollo en este Lenovo. No es
un paquete de distribución. Compilar la aplicación no reinstala el controlador.
FxSound está desinstalado y no es una dependencia. Detener la ruta con Windows
dirigido al transporte virtual puede dejar el audio sin salida: seleccionar la
salida física. La firma de producción, instalación de la aplicación y pruebas
prolongadas de suspensión/reconexión siguen pendientes.

Ver [arquitectura](docs/architecture.md), [controlador](drivers/virtual/README.md),
[auditoría](docs/AUDIT.md) y [memoria del proyecto](docs/PROJECT_MEMORY.md).
