# Diagnóstico de distorsión por la ruta virtual — 14 de octubre de 2026

El usuario oye distorsión por VB-CABLE, también en «Original», y dice que
Realtek directo suena limpio. Equipo comprobado en esta sesión: Lenovo 83LU,
Realtek; no es el Conexant de la sesión anterior. Instalada 0.2.13.

## Medidas sobre la reproducción actual

- DSP desactivado confirmado por `enabled=0`; sin underruns ni overflows.
- Cable al 72 %, Realtek al 50 % al principio. Más tarde el cable estaba al
  22 % y Realtek seguía al 50 %; Codex no cambió volúmenes de endpoints.
- Ambos endpoints: mezcla nativa estéreo float32 a 48 kHz.
- Sesiones activas: Brave y RESONIQ en cable; solo RESONIQ en Realtek.
  Volúmenes de sesión 1 y mute desactivado tras las pruebas.
- Captura simultánea de loopback de cable y Realtek durante ocho segundos,
  usando búfer de un segundo: cero muestras perdidas, una marca inicial de
  discontinuidad en cada endpoint. Correlación en seis ventanas de 250 ms
  0.999626–0.999950; ganancia de ajuste 0.999364–0.999727.
  Se alinearon muestras enteras; queda residuo de fase/reloj y no es una
  medida de distorsión acústica ni una comparación con Realtek directo.
- El primer intento con búfer de 20 ms perdía muestras EN LA HERRAMIENTA;
  esos resultados no demuestran un fallo de RESONIQ ni del cable.

## Señales conocidas

Tonos sintetizados por una herramienta independiente, DSP apagado; sesiones
emisoras silenciadas temporalmente y restauradas mediante RAII. No micrófono,
ni WAV guardado: muestras solo en memoria; resultados numéricos en `build/`.

| Frecuencia | Amplitud capturada cable / Realtek | Estimación armónica cable / Realtek |
|---|---|---|
| 1 kHz | 0.018750 / 0.018742 | 0.00849 % / 0.00840 % |
| 250 Hz | 0.015625 / 0.015621 | 0.00394 % / 0.00394 % |
| 4 kHz | 0.015625 / 0.015621 | 0.01892 % / 0.01282 % |

Estimaciones con ventana Hann de un segundo y ajuste de frecuencia.
La cifra de 4 kHz incluye evaluaciones por encima de Nyquist: no usarla
como THD calibrada. Solo apoya que no se observó una gran alteración digital
del tono. El usuario no pudo distinguir si los tonos sonaban distorsionados.
La primera prueba de 1 kHz silenció también la sesión de RESONIQ y no produjo
salida: inválida para la comparación; se repitió excluyendo ese proceso.

## Límite y siguiente comprobación

No se ha identificado la causa de lo que oye el usuario. Estas pruebas no
descartan distorsión dependiente del programa/nivel, procesamiento posterior
al punto de loopback ni comportamiento del amplificador/altavoz.

Se preparó `build/route-effects-check` con copia aislada de la configuración,
`raw 0` y `enabled 0`; se reinició temporalmente RESONIQ con `--data-dir` a
esa carpeta para comparar por escucha efectos de Windows permitidos frente
al RAW original. En ese modo la captura de música dio correlación
0.999269–0.999847 y ganancia 0.999259–0.999857, sin muestras perdidas.
No se recibió una valoración auditiva de ese modo durante la prueba.
Configuración original y favorito preservados. Se restauró la ejecución
habitual al finalizar; no se adoptó `raw 0` ni la configuración temporal.
Al arrancar sin argumentos se activó el DSP porque la sesión guardada tenía
`enabled 1` y `dsp_at_start 0` significa recordar perfil, no forzar apagado.
Se corrigió lanzando la instalación habitual con `--profile` a una copia del
perfil original con `enabled 0`: mismo sonido y RAW, «Original» restaurado.
El archivo original de sesión permanece intacto durante esta ejecución;
un cierre normal posterior guardará el modo elegido en la sesión habitual.
No publicar ni atribuir una causa basándose solo en esta prueba.

El usuario pidió repetir la comparación. Se volvió a lanzar la instalación
con `--data-dir build/route-effects-check`: actualmente prueba aislada activa,
`raw 0`, `enabled 0`, confirmada por perfil y telemetría. Se deja sonando para
su valoración. El usuario respondió «yo lo oigo igual». Se restauró RAW=1
con DSP apagado, ejecutando la instalación habitual con el perfil de
restauración. Permitir efectos de Windows no mejoró la distorsión percibida.

## Prueba del transporte propio solicitada por el usuario

Windows usa ahora DPS Audio Virtual Transport como salida predeterminada.
RESONIQ ejecuta copia aislada `build/own-transport-check`: virtual_input_id
del transporte propio, RAW=1, DSP apagado, salida Realtek al 50 %.
La música pasó por ese transporte, `cable_volume=0`, entrada=salida ~0.061
con volumen propio al 20 %. Se aumentó solo ese volumen a 41 % para acercar
el nivel al del cable al 42 %; la reproducción cesó y no se verificó igualdad.
Queda listo para escucha. No se desinstaló VB-CABLE ni se retiró soporte de
código. Configuración habitual y favorito preservados. Para volver: reiniciar
con el perfil restore-original y seleccionar CABLE Input como predeterminado.

## Ruta mínima sin motor ni limitador

El usuario aclaró que ambas rutas virtuales suenan menos limpias y más bajas
que Realtek directo, y autorizó aislar el motor. Opción CLI `--transport-only`:
no prepara ni procesa Engine; medidores calculados directamente sobre las
muestras, reducción cero. GUI y modos existentes no cambian. Mantiene FIFO,
adaptación de relojes, lectura de volumen/mute y rampa para aislar solo Engine.
Controles de DSP/intensidad del CLI deshabilitados en ese modo.

Build nuevo `build/route-minimal-release`: 17/17 CTest, sin avisos. Caché de
local-release no utilizable por rutas antiguas de otro equipo.
Host CLI activo con transporte propio, RAW, configuración de sonido copiada.
Windows usa el transporte propio al 100 % (antes 40 %); Realtek permanece al
28 % que tenía con reproducción directa. Se elimina atenuación extra del
virtual; falta verificar niveles con música y resultado audible.
GUI detenida, perfil original y favorito intactos, sin instalador ni release.
Telemetría y PID en `build/route-minimal-check`. Inicialmente entrada y salida
cero, sin underruns/overflows ni error. Pendiente escucha del usuario.

Usuario respondió «No se oye audio». Primera lectura posterior: todos los
endpoints sin señal. Después empezó a entrar música sin cambios de Codex:
input=output ~1.01–1.05, reducción 0; Realtek medidor ~0.87–0.98, sesiones
sin mute y volumen 1. Host activo, error.log vacío; dos underruns acumulados
de pausas/reanudaciones, sin crecimiento durante esta reproducción.
Picos por encima de 0.891251 confirman que se omite el limitador del motor;
pueden incluir sobrepicos de la adaptación sinc. No prueban saturación acústica.
Pendiente confirmar audio audible y comparación con directo.

El usuario confirmó después que oye audio y que «suena prácticamente igual
realtek o dsp propio». Pista favorable para la ruta mínima, con la confusión
de haber cambiado también volumen virtual de 40 a 100 %.
Se inició comparación controlada con Original habitual (mismo CLI, perfil,
RAW, transporte propio al 100 %, Realtek 28 %), quitando --transport-only.
Host/log en `build/route-original-check`; mínimo detenido. Pendiente escucha.

## Corrección del bypass

Usuario respondió que Original habitual suena menos limpio que la ruta mínima
con mismo transporte y volúmenes. El código limitaba también la mezcla seca
a -1 dBFS; telemetría Original previo: input~1.00 / output=0.891251 y en colas
input=0.676 / output=0.608. No era un bypass transparente.

Engine ahora cruza hacia la muestra seca después del limitador; extremos
exactos del fundido y métricas de reducción/realce cero en bypass asentado.
DSP activo conserva algoritmo y techo; modo de igualación solo interno sigue
separado. 18/18 CTest: nuevo bypass_validation cubre exactitud de muestras,
sobrepicos, estéreo, particiones, in-place, transiciones, reactivar DSP y NaN.
Favorito procesado sobre señal sintética antes/después idéntico bit a bit:
2CF7A2770CEA171CFD6F36FBD47376C074BFF2006CD5DA46BAAA9B70582A213D.

Usuario oye corregido «más limpio y más bajo», pero volumen virtual había
pasado a 52 % (después 62 %) durante pruebas. Se fijó de nuevo 100 %, Realtek
28 %, y se deja ejecutando GUI corregido aislado en build/bypass-fixed-gui.
Telemetría: input=output~1.0, reducción=0 y realces=0, sin cortes. Favorito
original intacto; instalación habitual anterior y sin MSI/publicación nuevos.
Escucha posterior: «es limpio y suena prácticamente igual a la salida directa
por Realtek». Validado por el usuario el Original corregido en este equipo
con transporte propio. Continúa en ejecución la copia aislada. No afirmar que la queja de DSP
activo está resuelta ni extrapolar mejora acústica a VB-CABLE sin comprobarlo.

## Aplicación a la instalación habitual

El usuario pidió instalar la corrección. Versión subida a 0.2.14 y MSI local
generado con runtime estático; 18/18 CTest y verificación del MSI extraído
pasan. Instalación elevada con salida 0. Registro y versión del exe 0.2.14;
los 16 archivos de Program Files coinciden por SHA256 con el payload verificado.
MSI SHA256 BCE968D2E737FA12E556946AF928DE5D1F06AAE0B472F400CAE373E6932BC338.

Proceso actual desde Program Files, copias de prueba detenidas. Configuración
habitual conserva parámetros, RAW=1 y Original; selecciona transporte propio.
Preferencias de inicio conservadas, favorito original intacto. Backup de
configuración y exe anterior indicado en build/install-0.2.14-backup-path.txt.
Windows mantiene virtual 100 % y Realtek 28 %. En la comprobación final no
reproduciéndose audio, aplicación conectada sin errores ni procesado aplicado.
No nuevo commit, push ni publicación. GitHub continúa distribuyendo 0.2.13.

## Valoración posterior a la instalación

El usuario sigue oyendo mejor Realtek directo y considera dejar el proyecto;
refiere que el sonido aprobado anterior a las mejoras se perdió. La queja
continúa abierta, pese a la aprobación provisional de la copia de prueba.
Lectura actual: Original (enabled=0), salida física 22 %, sin señal en ese
instante; favorito original íntegro. Solo lectura, sin cambios de audio.
Si retoma, reconstruir versión, ruta y niveles del punto aprobado antes de
proponer más mejoras. No dar el problema por resuelto por pruebas sintéticas.

## Retorno a la versión archivada solicitado por el usuario

Archivo own-driver-20261012-234107-2b8044: 97 archivos coinciden con manifiesto.
Documenta cierre satisfactorio y sonido conservado antes de integrar VB-CABLE.
GUI pasa autotest aislado. Se retiró 0.2.14 y se instaló el MSI 0.2.0 archivado,
solo Application: ambas salidas 0, preflight driver antes/después idéntico.
Ejecutable instalado coincide con hash original 7F299A4F…D15F4E9C.
La sesión activa y favorito LocalAppData coinciden exactamente con hashes del
cierre aprobado, DSP activo y RAW. Transporte propio predeterminado al 20 %,
para nivel de entrada moderado documentado; volumen físico histórico no se
conservó, por lo que no se puede afirmar reconstrucción acústica exacta.
Se respetó nivel físico del usuario, 22 % al iniciar y 24 % en última lectura.
App conectada sin señal en comprobación final; pendiente valoración auditiva.
Backup 0.2.14/config y logs en build/restore-approved-20261012; workspace y
GitHub conservados, sin nuevas mejoras ni cambios de perfil inventados.
