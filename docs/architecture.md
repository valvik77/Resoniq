# Arquitectura actual de RESONIQ

Ruta de sistema: aplicaciones → endpoint virtual DPS → loopback WASAPI →
motor DSP → salida física. El transporte de desarrollo consume audio sin
enviarlo a hardware; la salida audible depende del host. GUI y consola comparten
el mutex `Local\DpsAudioEngine.LiveHost`. FxSound no participa.

## Núcleo portable

La biblioteca `dps` contiene filtros/crossover LR4, compresión enlazada, detalle
dinámico y ataques, armónicos, EQ de cuerpo/presencia, AGC, Mid/Side, mezcla de
intensidad/bypass y limitador. `prepare/reset` pertenecen al control con audio
detenido. `process`, setters de controles y lectura de medidores pertenecen al
trabajador. No hay asignaciones ni bloqueos en `process`.

La compensación RMS sigue disponible solo para pruebas internas. La aplicación
compara original/procesado sin compensar. El techo es de muestras, −1 dBFS,
sin garantía true peak. La mezcla seca/húmeda conserva las diferencias de fase
históricas. No se alteró el sonido aprobado durante la auditoría.

`DeviceCalibration` contiene el ajuste de dispositivo; `Parameters` añade los
controles. `Profile` serializa ambos, etiquetas, RAW e ID de salida. V1/v2
mantienen compatibilidad; los parámetros nuevos opcionales conservan valores
históricos. `Measured` identifica una calibración cuyos valores viven en el
archivo, no en una tabla fija. Favorito y sesión son estados independientes.

WAV, perfiles, preferencias y sugerencias de calibración también son portables.
Las escrituras usan un directorio temporal exclusivo junto al destino y
reemplazan el archivo después de cerrar. Un error antes del reemplazo conserva
el original; no garantiza persistencia ante pérdida de energía.

## Adaptadores Windows

`dps_windows` compila una sola vez WASAPI y lectura de BIOS. Negocia estéreo
float32 a 48 kHz, usa eventos y MMCSS. La ruta de música tiene FIFO fija con
resampling adaptativo de deriva. Underrun/overflow reinician el cebado y su
desvanecido. La configuración y los buffers se preparan antes del bucle.

La GUI Win32/GDI+ publica controles y lee medidores por atómicos cada 100 ms.
Solo el trabajador toca Engine. Cambiar calibración/salida/RAW recrea la sesión;
los controles normales conservan estado de filtros. Si el dispositivo falla,
la GUI reintenta según preferencias; la consola informa y termina.

La latencia de telemetría suma las latencias reportadas, capacidad de render y
ocupación FIFO. Es una estimación de software. Una latencia real de extremo a
extremo necesita una señal conocida y captura/loopback con marcas de tiempo;
esa medición no está implementada.

La medición acústica requiere consentimiento por operación y por aplicación.
El coordinador y el listener se ejecutan en MTA, incluso si la UI quedó STA.
El micrófono tiene su propio hilo/evento. Excepciones se propagan tras parar y
unir el listener; RAII detiene los clientes. Los tonos se analizan por Goertzel
y se normalizan contra 250 Hz–1 kHz. Se identifica qué banda produce el primer
tono audible para compensar retardo sin desplazar bandas enteras.

La propuesta es orientativa por el sesgo del micrófono y su posición. El modo
normal descarta audio; `--measurement-diagnostics` lo guarda con aviso explícito.
La medición todavía bloquea la ventana durante unos segundos.

## Controlador y distribución

`drivers/virtual` mantiene un transporte propio sobre una revisión fijada de
SysVAD, con overlay, INF, construcción y verificación offline de firmas/hashes.
No publica captura hardware ni carga APOs del sample. Construcción, firma de
desarrollo e instalación son operaciones separadas. No reiniciar automáticamente.

No hay instalador de producción ni supervisor que restaure la salida física
si muere el host. Quedan validación prolongada de suspensión/reconexión,
evaluación de aliasing/true peak y ampliación de perfiles con equipos reales.
La instalación de desarrollo existente no se modifica por refactorizar la GUI.
