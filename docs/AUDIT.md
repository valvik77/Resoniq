# Auditoría y refactorización — 12 de octubre de 2026

Auditoría del código propio: motor y cabeceras DSP, medición, WASAPI, perfiles,
WAV, detección de hardware, GUI, consola, herramientas, pruebas, CMake,
lanzadores y scripts/overlay/INF del controlador. Se revisaron los límites de
integración con SysVAD y SDK/WDK; sus árboles externos no se refactorizaron ni
se presenta esta revisión como una auditoría completa del código de Microsoft.

No hay repositorio Git en la raíz actual. Antes de editar se guardó una copia
del código/documentación en `build/audit/before`. Los cambios y evidencias
locales quedan en `build/audit`; el informe no sustituye un historial Git.

## Hallazgos corregidos

| Prioridad | Hallazgo | Corrección |
|---|---|---|
| Alta | Perfiles y preferencias truncaban el destino antes de terminar de escribir. | Escritura temporal exclusiva, flush/cierre comprobados y reemplazo. La misma utilidad protege WAV. |
| Alta | Un fallo del coordinador de medición con un `std::thread` activo podía ejecutar `std::terminate`; las excepciones del listener escapaban. | `std::jthread`, parada/join automática, captura y propagación de excepciones y clientes detenidos por RAII. |
| Alta | La captura creaba interfaces en la UI, potencialmente STA, y las compartía con un listener MTA. | Coordinador dedicado MTA; ambos hilos de audio comparten ese apartamento. |
| Alta | El aviso decía que la grabación se descartaba, pero se guardaba siempre un WAV. | Descarte por defecto; guardado solo con `--measurement-diagnostics`, anunciado antes del consentimiento. |
| Media | El WAV de diagnóstico etiquetaba muestras mono como estéreo y reducía a la mitad su duración. | Duplicación de mono en dos canales, con duración y frecuencia correctas. |
| Media | Buscar hasta el tercer tono no descontaba su índice: graves inaudibles podían desplazar las bandas. | Identificación de la primera banda audible y descuento de sus pasos, con regresión de 50 ms y dos tonos inaudibles. |
| Media | Frecuencias de muestreo no finitas y punteros/datos inválidos podían llegar a conversiones a enteros o al análisis. | Validación previa de rate, amplitud, grabaciones y niveles antes de generar/calibrar. |
| Media | La opción «Medido» podía aplicar una calibración fija de respaldo sin haber medido. | La GUI exige una medición aplicada o un perfil medido cargado; reseleccionar conserva sus valores. |
| Media | Los nombres de dispositivo no se validaban al guardar/cargar perfiles. | Validación contra los nombres admitidos, incluido `Measured`. |
| Media | La configuración regional podía cambiar el separador decimal del perfil. | Lectura/escritura con locale clásico; regresión con decimal de coma. |
| Media | Cancelar ajustes podía cambiar preferencias; un guardado fallido dejaba cambios activos en memoria. | Candidato local, commit tras persistencia y restauración del inicio de Windows si falla el archivo. |
| Media | Las macros `text/card/muted/accent/background` expandían variables y miembros ajenos. | Eliminadas; acceso explícito a la paleta, separada de la GUI. |
| Media | El bucle de mensajes no atendía explícitamente la navegación del diálogo de ajustes. | `IsDialogMessage` para ajustes antes de la ventana principal. |
| Baja | Pincel de tarjeta desactualizado tras cambio de tema. | Recreación del pincel al aplicar la paleta. |
| Baja | `StreamBuffer::tune(0)` dividía por cero y podía contaminar la fase. | Objetivo cero ignorado, con regresión. |
| Baja | CLI aceptaba índices/segundos con basura final mediante `stoi`. | Parseo completo con `from_chars` y validación de rango. |
| Media | El lanzador de consola podía fijar volumen físico al 100 % implícitamente. | Solo modifica volumen si se solicita `-OutputVolume`. |
| Documentación | README afirmaba que no existían GUI/perfiles/bandeja/reconexión y mostraba FxSound como ruta actual. | README y arquitectura reescritos conforme al código actual. |
| Documentación | Se describía `latencyMs` como medición física real. | Identificado como estimación basada en informes de endpoints y buffers. |
| Documentación | La tabla de perfiles por modelo se llamaba «medida» aunque su ajuste fue por escucha. | `Basis::Tuned` y comentarios corregidos; alias `Measured` preservado en la API. |

## Refactorización

- `apps/gui_theme.*` y `apps/gui_icons.*`: paleta, lectura de tema e iconos.
- `apps/audio_session.hpp`: vida del trabajador WASAPI y mutex de instancia.
- `include/dps/preferences.hpp` y `src/io/preferences.cpp`: preferencias
  independientes de Windows y del sonido, con pruebas propias.
- `src/io/atomic_file.hpp`: reemplazo tras escritura completa, compartido por
  perfiles, preferencias y WAV. No requiere cambios de formato ni migración.
- `src/io/calibration_suggestion.cpp`: elección de calibración sin acceder a
  Windows. `hardware.cpp` queda como adaptador de BIOS/endpoint.
- `dps_windows`: WASAPI se compila una vez para GUI y consola. Diagnósticos de
  compilación coherentes para todos los targets, CTest con timeout y
  `BUILD_TESTING` opcional; ya no se pasan flags MSVC a otro compilador Windows.
- Compresor: cálculo de densidad/ratio/umbral/makeup común a las cuatro bandas
  una vez por muestra; se elimina el miembro temporal `bandMakeup_`. No se
  cambian fórmulas, orden de filtros ni controles.
- Raíz de recursos localizada por ancestros con `config` y `profiles`, en vez
  de asumir siempre tres niveles. Funciona en una compilación anidada.

## Verificación

La suite inicial tenía siete pruebas y pasaba. La suite ampliada tiene once:
DSP, perfiles (56 combinaciones, incluida calibración medida personalizada),
comparación interna, timbre, transitorios, medición, preferencias, hardware,
WAV, GUI y errores WASAPI. Incluye guardado fallido con el archivo bloqueado,
formatos inválidos, grabaciones no finitas, graves ausentes, retardo, COM STA,
conservación de «Medido» y rechazo de selección medida ficticia.

Compilación Release de auditoría y compilación habitual verificadas sin avisos.
Once pruebas CTest pasan. Verificación offline del paquete de controlador pasa;
INF y SYS alterados se rechazan sin modificar certificados. Los scripts
PowerShell se comprobaron con su parser. La prueba WASAPI real comprueba
arranque y conmutación DSP sin cambiar trabajador. No reinstala controladores.

En programa sintético estéreo de tres segundos, render del ajuste aprobado y
de la sesión antes/después es idéntico bit a bit:
`8D00DBCED7758AE61A37A7B8DD43A2ECAE958C21033F2C407C31787EA97DC1EA`.
El archivo favorito conserva
`5119695BA5D42074679AB8293F12FC44047126F19275C75DAEB57CEA82C3E23E`.
La sesión guardada normalmente por la aplicación usa ahora finales de línea LF
y su hash de archivo es
`5B437712D8E450CAE99F6BB037E8A68388F37F34B4C0012FBEFDFF3AA513EC90`;
sus campos y el render permanecen iguales. No se restauró sobre ella el favorito.
Esto acredita conservación en esa señal y equipo; no es una prueba auditiva
universal ni una medición acústica.

Se revisaron capturas de ventana principal y ajustes. El benchmark local queda
en `build/audit/benchmark-before.csv` y `benchmark-after.csv`; es una muestra
breve y no incluye WASAPI ni garantiza plazos bajo cualquier carga.

La aplicación se cerró por su comando normal para guardar la sesión y se
relanzó en bandeja, como estaba. No se sustituyó la sesión por el favorito,
no se cambiaron controles ni volumen de Windows y no se activó igualación.

## Trabajo y límites restantes

- La medición real funcionaba según el usuario antes de la auditoría. No se
  abrió el micrófono automáticamente después: el permiso debe solicitarse por
  operación. La nueva gestión de errores/COM y alineación tiene cobertura
  sintética, pero falta una nueva medición de habitación tras esos cambios.
- No se simularon todos los fallos de dispositivos después de arrancar el
  listener. RAII y propagación se revisaron, pero las pruebas WASAPI de error
  no sustituyen desconexiones físicas ni pruebas prolongadas.
- La medición continúa bloqueando la GUI durante unos segundos. Conviene
  mover el flujo completo a una tarea con cancelación y resultado en UI.
- Reconexión, suspensión/reanudación y continuidad prolongada requieren una
  matriz de equipos y pruebas reales. El host no restaura automáticamente la
  salida física si desaparece; el transporte virtual puede quedar sin sonido.
- La detección por nombre/modelo puede confundir una salida externa conectada
  a un portátil conocido con sus altavoces internos. Falta topología/metadatos
  de endpoint para distinguirlos con fiabilidad.
- Límite de muestra sin true peak, generador armónico sin sobremuestreo y
  diferencias de fase entre ruta seca y procesada continúan. Cambiarlos afecta
  al sonido aprobado y requiere una iteración de DSP y escucha aparte.
- Falta medición física de latencia, instalador de aplicación y firma de
  producción. No hay servidor de telemetría ni envío de mediciones.
- Una pérdida de energía puede dejar temporales o perder una escritura no
  persistida por el sistema. El reemplazo no equivale a un backup transaccional.
- Los WAV de diagnóstico anteriores se conservan; el nuevo modo normal no
  crea más. La carpeta de build también contiene material de desarrollo y una
  clave privada de firma de pruebas: no es un paquete publicable.

No se considera cerrado el objetivo perceptivo por pasar las pruebas ni se
añadieron parámetros de sonido al perfil aprobado.
