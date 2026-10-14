# Interfaz actual sobre el audio archivado — 0.2.15 local

Petición: incorporar estética, configuración, iconos, inicio con Windows,
temas claro/oscuro, volumen de Windows, versión, Acerca de, entrada virtual y
arranque minimizado a la versión recuperada. Instalada en Program Files el
14 de octubre de 2026. El usuario solicitó después commit y push del código a
main, guardar la memoria y preparar el traspaso a Claude. La última release
publicada y descargable sigue siendo 0.2.13.

## Implementación

- Interfaz actual: logotipo y controles con iconos, iconos de bandeja por
  estado, temas sistema/oscuro/claro, Ajustes y Acerca de con versión 0.2.15.
- Preferencias independientes del perfil: inicio con Windows, minimizado en
  bandeja, cierre, reconexión, DSP al arrancar, apariencia e ID de entrada.
- Entrada virtual seleccionable: transporte propio y VB-CABLE. La selección
  guardada tiene prioridad; si falta el dispositivo, no se sustituye por otro.
- Volumen físico de Windows visible y actualizado cada segundo, solo lectura.
  «Sonido Windows» abre su configuración. Sin igualación ni nuevo control Gain.
- Audio, perfiles y medición recuperados de
  `archives/own-driver-20261012-234107-2b8044` (base 0.2.0).
  No se portaron la compensación/piloto VB-CABLE, el remuestreador posterior,
  el cambio de bypass de 0.2.14 ni la adopción automática de perfiles.

La API de lectura del volumen y los nombres de interfaz/defaultCapture son
metadatos fuera del trabajador de audio. Los medidores de la GUI usan los
picos del motor archivado; no aplican procesamiento adicional.

## Verificación

- Build Release MSVC 19.44, runtime estático: `build/classic-ui-release`.
- 18/18 CTest. Tras los retoques visuales, GUI y contrato de audio revalidados.
- Capturas revisadas de los temas claro/oscuro en ventana, Ajustes y Acerca de.
  Corregidos el solapamiento junto al logo y los fondos de filas de Ajustes.
- `tests/classic_audio_contract.py` protege DSP, búfer, silencio, semántica de
  perfiles, formato, inicialización y bucle WASAPI. La referencia de texto
  normalizado está en `tests/fixtures/classic_audio_baseline.json`, sin depender
  del archivo histórico local ni de los finales de línea de Git.
- Render estéreo sintético de tres segundos con el favorito: binario nuevo y
  compilación del archivo generan WAV idénticos, SHA256
  `2CF7A2770CEA171CFD6F36FBD47376C074BFF2006CD5DA46BAAA9B70582A213D`.
  Esto demuestra equivalencia en esa prueba, no una aprobación auditiva.
- MSI extraído: GUI aislado, prerrequisitos, versión, condición OS y aislamiento
  de driver pasan. No se ejecutó la prueba que reproduce audio.
  Evidencia: `build/installer-test-ff51e89b352d476c8b86020ec8c9ca33`.

## Instalación y estado final

MSI: `dist/RESONIQ-Setup-0.2.15-x64-VBCABLE.msi`, SHA256
`ADAE49B6334AA4E23555A76345B343ECA59EA2A5F4A1F96240CF416224E0F301`.
Exe instalado SHA256
`2B5281BFDB3C90221108CC08E0571A560E4DA6219A8173E67F23FB5BC1A6A653`.
msiexec salida 0, registro 0.2.15; 15 archivos instalados iguales al MSI
verificado. El upgrade omite `RemoveDeviceOwned`; preflight antes/después
mantiene un dispositivo propio, sin modificaciones de driver/certificados.

Backup del ejecutable 0.2.0 y configuración anterior:
`build/classic-ui-install-backup-20261014-233637`.
El trabajo previo del workspace está respaldado en
`build/before-classic-ui-20261014-232619`.

Durante el cierre de la versión antigua, el ID de salida guardado pasó de
Realtek a CABLE Input. Detectado en la comprobación posterior: no había salida
física seleccionada. Se recuperó la sesión exacta del backup y se reinició
únicamente el proceso nuevo. Estado final: transporte propio predeterminado
20 %, Realtek 24 %, DSP activo, RAW=1, sesión y favorito intactos.

Preferencias conservadas: inicio con Windows activo, minimizado activo,
cierre a bandeja, reconexión activa, DSP como la última vez, tema del sistema.
Run de HKCU: `"C:\Program Files\RESONIQ\resoniq.exe" --tray`.
Proceso instalado en `build/install-0.2.15-process.pid`; ventana principal
oculta al arrancar. Telemetría: running=1, finished=0, volumen físico .24,
sin cortes ni señal reproduciéndose durante la comprobación.

Favorito repo/LocalAppData SHA256
`5119695BA5D42074679AB8293F12FC44047126F19275C75DAEB57CEA82C3E23E`.
Sesión repo/LocalAppData SHA256
`5B437712D8E450CAE99F6BB037E8A68388F37F34B4C0012FBEFDFF3AA513EC90`.
No sobrescribir estos archivos al continuar. La escucha del usuario sigue
pendiente: su «ahora sí funciona» era para otra conversación y lo corrigió.
