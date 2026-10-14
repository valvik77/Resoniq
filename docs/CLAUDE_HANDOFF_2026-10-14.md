# Traspaso a Claude — RESONIQ 0.2.15, 14 de octubre de 2026

Lee `AGENTS.md`, la primera sección de `docs/PROJECT_MEMORY.md` y este resumen.
Las notas anteriores de memoria e informes son históricas cuando contradicen
este estado. El usuario ha pedido commit/push de estos cambios y guardar el
traspaso; no ha pedido otra investigación ni nuevos ajustes de sonido.

## Resultado y contexto

Está instalada **0.2.15** en `C:\Program Files\RESONIQ\resoniq.exe`:
**interfaz actual sobre el motor, búfer, WASAPI y perfiles de la 0.2.0 archivada**.
El código se entrega en `main`; la última release descargable sigue siendo
**v0.2.13**. El MSI 0.2.15 está en `dist/`, ignorado por Git, sin nueva release.

El usuario percibía distorsión/pérdida de limpieza y menor nivel por la ruta DSP
frente a Realtek directo. Las capturas digitales no localizaron una causa
acústica. Una ruta mínima y luego el bypass corregido de 0.2.14 recibieron
valoraciones favorables provisionales; tras instalarlo, volvió a preferir
Realtek y pidió recuperar el punto del día 12. Se instaló entonces la 0.2.0 del
archivo `archives/own-driver-20261012-234107-2b8044`, con 97 archivos verificados.

Después pidió reincorporar los cambios de interfaz/configuración a esa base:
estética, iconos, inicio Windows, temas claro/oscuro, volumen Windows visible,
versión, Acerca de, entrada virtual y arranque minimizado. Esa es la 0.2.15.
**«Ahora sí funciona» era un mensaje para otra conversación y el usuario lo
corrigió: aún no había probado la restauración. No hay aprobación auditiva
de la 0.2.0 recuperada ni de la 0.2.15. La queja sonora permanece pendiente.**

## Cambios y reglas al continuar

- GUI Win32/GDI+ moderna, logotipo e iconos de aplicación/bandeja, temas del
  sistema/oscuro/claro, Ajustes y Acerca de con versión. Corregidos solapamiento
  de texto junto al logo y fondos inconsistentes en las filas de Ajustes.
- Preferencias separadas del sonido: inicio Windows, minimizado, cerrar del
  todo/bandeja, reconexión, DSP inicial, tema e ID de entrada virtual.
- Selector de transporte propio/VB-CABLE. Conserva el ID elegido; una entrada
  o salida ausente no se sustituye automáticamente por otro dispositivo.
- Volumen de la salida física de Windows leído cada segundo; indicador
  informativo y botón «Sonido Windows». No cambia ni iguala volúmenes.
- El audio archivado conserva su limitador y bypass histórico. **El bypass
  corregido de 0.2.14 no se incluye**, por la vuelta explícita a la base anterior.
- No se portaron el remuestreador posterior, compensación/piloto del volumen
  del cable, ampliación de rango AGC, medición posterior ni adopción automática
  del perfil completo al primer arranque. No reintroducirlos por iniciativa.
- No exponer comparación normalizada ni Gain. Conservar el favorito aprobado
  y la sesión como archivos independientes. No copiar uno sobre otro.
- El MSI contiene la aplicación y el asistente de VB-CABLE; la ruta activa
  usa el controlador propio ya existente. No reinstalar drivers/certificados.

## Estado local confirmado al cerrar

Transporte propio predeterminado al **20 %**, salida física **Realtek al 24 %**;
VB-CABLE sigue instalado y no es la ruta activa. DSP=1, RAW=1, Laptop Impact+,
Bass .95 / Clarity .9 / Dynamics .85 / Spatial .3 / Intensidad 1.
AGC 60/900 ms; highpass 175 Hz, body 300 Hz/+11 dB, presence 3 kHz/+10 dB,
mud 600 Hz/-6 dB, detail 6, harmonic_gain 1.6, adaptive_depth .15,
loud_density 1.45 y band_density 1.8.

Preferencias: inicio con Windows y minimizado activos, cierre a bandeja,
reconexión activa, DSP como última vez y tema del sistema. HKCU Run:
`"C:\Program Files\RESONIQ\resoniq.exe" --tray`.
Arranque oculto comprobado; PID en `build/install-0.2.15-process.pid`.
Última telemetría: running=1, finished=0, under=over=0, volumen físico .24,
sin señal reproduciéndose. No se generó audio en esta comprobación final.

En el cierre de la GUI antigua, el ID guardado de salida cambió a CABLE Input;
la nueva GUI lo excluyó de las salidas físicas y quedó sin ruta. Se detectó y
se recuperó la sesión exacta de la copia previa; la nueva aplicación quedó
conectada a Realtek. No atribuir ese incidente a una causa de la distorsión.

Favorito repo y LocalAppData:
`5119695BA5D42074679AB8293F12FC44047126F19275C75DAEB57CEA82C3E23E`.
Sesión repo y LocalAppData:
`5B437712D8E450CAE99F6BB037E8A68388F37F34B4C0012FBEFDFF3AA513EC90`.
Datos instalados: `%LOCALAPPDATA%\RESONIQ\config`.
Reenumerar dispositivos antes de actuar; los volúmenes son el estado observado,
no una instrucción para sobrescribir cambios posteriores del usuario.

## Evidencia y comandos

**18/18 CTest pasan**, además del GUI del MSI extraído y sus comprobaciones de
versión, mantenimiento OS, prerrequisitos y aislamiento del driver.
Capturas claro/oscuro revisadas en ventana, Ajustes y Acerca de.
Contrato `tests/classic_audio_contract.py` protege el código archivado mediante
`tests/fixtures/classic_audio_baseline.json`; no requiere la carpeta `archives/`.
El archivo histórico, perfiles privados, builds, WAV y MSI no se suben a Git.

Render sintético estéreo de tres segundos con favorito, comparado con un binario
compilado del archivo: WAV idénticos, SHA256
`2CF7A2770CEA171CFD6F36FBD47376C074BFF2006CD5DA46BAAA9B70582A213D`.
Es equivalencia digital de esa prueba, no validación acústica ni de VB-CABLE.

```powershell
. ./build/tools/portable-env.ps1
cmake --build build/classic-ui-release
ctest --test-dir build/classic-ui-release --output-on-failure
python tests/classic_audio_contract.py
```

Compilación local MSVC 19.44/CMake 4.4.4, Release con runtime estático.
La caché de otros builds puede contener rutas de otro equipo: crear una carpeta
nueva si falla. Python 3 es requisito de las pruebas, no del producto instalado.
No ejecutar `--audio-test` con la aplicación habitual usando la ruta de audio.

MSI local: `dist/RESONIQ-Setup-0.2.15-x64-VBCABLE.msi`, SHA256
`ADAE49B6334AA4E23555A76345B343ECA59EA2A5F4A1F96240CF416224E0F301`.
Exe instalado SHA256
`2B5281BFDB3C90221108CC08E0571A560E4DA6219A8173E67F23FB5BC1A6A653`.
Instalación salida 0, 15 archivos iguales al MSI. Upgrade omitió la retirada
del dispositivo; preflight antes/después mantuvo un dispositivo propio.

Backups locales:
`build/classic-ui-install-backup-20261014-233637` (0.2.0/config anterior),
`build/before-classic-ui-20261014-232619` (workspace previo),
`build/restore-approved-20261012/backup-0.2.14` (versión intermedia).
Detalle de entrega en `docs/CLASSIC_UI_2026-10-14.md`; diagnóstico histórico en
`docs/ROUTE_DIAGNOSTIC_2026-10-14.md`.

## Siguiente paso

Esperar la escucha del usuario de esta versión y conservar este punto.
Si pide diagnóstico, partir de la versión/ruta/perfil/volúmenes reales, sin
suponer que el perfil por sí solo reconstruye el sonido que aprobó el día 12.
No afirmar resuelta la queja ni hacer nuevas mejoras sonoras especulativas.
