# Volumen VB-CABLE

## Auditoría de la 0.2.11 — 14 de octubre de 2026

Esta sección sustituye las referencias inferiores al resultado recordado entre
sesiones. Se mide de nuevo en cada apertura: un endpoint o estado del controlador
distinto no puede heredar una decisión global. Puede haber una pausa breve al
reconectar. Las esperas del piloto siguen actualizando la ganancia cada 10 ms
y son cancelables; cambios observados de volumen, revisiones de mute y huecos
de captura invalidan la medida. Se comprueba también volumen/mute de sesión al
finalizar. Tras la corrección de Claude, la hipótesis «el cable atenúa» admite
una pérdida entre 5 dB más y 3 dB menos que la pedida (límites excluidos), por
los escalones medidos al 8 %. «Ignora el volumen» mantiene menos de 3 dB respecto
a 0 dB. Las pérdidas intermedias siguen desconocidas. La ventana del medidor
se reinicia tras un hueco; la marca del primer paquete al arrancar no invalida
el piloto, pero las posteriores sí.

La rampa de salida se separa en StartupGate y la telemetría publica su pico
final. Si no se consigue medir, permanece la atenuación conservadora: puede
ser doble con un cable que ya atenúa hasta obtener una decisión. No se promete
atenuación única en modo desconocido. Ver AUDIT_2026-10-14.md para pruebas y
límites: Claude comunicó posteriormente verificación física en el Lenovo al
8–50 %, arranque en silencio, cambio 100 % -> 50 % y respuesta hasta 14 kHz.
Codex contrastó después el código y las regresiones, sin repetir esas medidas.
El caso Conexant que ignora el volumen sigue pendiente.

## Corrección de la 0.2.10 (13 de octubre de 2026) — sustituye el diseño 0.2.6

La 0.2.6 aplicaba el volumen del endpoint después del DSP, suponiendo que el
loopback de VB-CABLE nunca llega atenuado. Esa suposición venía de un único
equipo y resultó depender del estado del controlador:

- Lenovo 83LU, VB-CABLE 3.3.1.7, Windows Volume Control activo en vivo: el
  loopback ya llega atenuado. Tono 0.02: 0 / -10.1 / -18.1 / -36.1 dB en
  100 / 50 / 30 / 10 %, silencio total en 5 % y 2 %, y cerca de un segundo
  para asentarse en cada escalón. En uso real al 30 %: navegador -4.9 dBFS,
  loopback -22.9 dBFS, y la 0.2.9 restaba otros 18.24 dB tras el DSP.
- Conexant (medida de la 0.2.6): mismo controlador y mismo valor de registro,
  loopback sin atenuar. Ambos declaran `QueryHardwareSupport=7`.

Consecuencias de la 0.2.9: volumen aplicado dos veces donde el cable atenúa,
y DSP trabajando siempre a nivel pleno donde no. El perfil favorito se aprobó
con el dispositivo propio, cuyo loopback llega ya atenuado (medido: sigue la
curva exacta, soporte hardware 0): es decir, con el volumen antes del motor.
Con música sintética de pico 0.57 y volumen -18.2 dB, Impact+: volumen antes
del motor, cresta 11.3 dB y 0 dB de compresión/limitador; después, cresta
9.4 dB y hasta 8.5 dB. Doble atenuación con Gain +12 a -10.5 dB: cresta 7.1.

Diseño actual, solo para VB-CABLE:

- El volumen actúa una sola vez y antes del DSP, como en el dispositivo propio.
- Al abrir cada sesión de audio, un observador fuera del hilo de audio reproduce
  en el cable un piloto de 15 kHz a -42 dBFS durante 0,6 s y mide su nivel en el
  loopback con un Goertzel de 100 ms que el hilo de audio alimenta sin COM,
  bloqueos ni asignaciones. Compara con el volumen en dB: si el piloto llega
  atenuado, el cable ya aplica el volumen y el host no añade nada; si llega
  íntegro, el host aplica la curva en dB delante del motor, suavizada 5 ms.
- Solo decide entre -6 y -40 dB, con volumen estable 1,5 s, piloto al menos
  4 veces por encima de la energía de programa en su banda (leída al terminar
  el tono) y resultado coherente; si no, reintenta a los 3 s, hasta 6 veces.
- Sin medir, atenúa: equivocarse hacia el silencio es breve y seguro; hacia el
  nivel pleno no. En la primera sesión del proceso la salida espera en silencio
  la medida (menos de 1,5 s) y abre con rampa; las siguientes parten del
  resultado recordado. Cero y mute conservan su puerta posterior al DSP.
- Telemetría `cable_volume`: 0 no es cable, 1 sin medir, 2 el cable atenúa,
  3 el cable ignora el volumen y lo aplica el host.

Verificación real en el Lenovo con el host de consola, tono 0.3 más ruido de
banda ancha de 0 a 0.1, salida al dispositivo propio (nada audible) y volumen
restaurado: 8, 10, 15, 20, 30, 40 y 50 % decididos al primer intento como
«el cable atenúa», salida 0.0023 / 0.0047 / 0.0125 / 0.0213 / 0.0375 / 0.0654 /
0.0990, una sola atenuación. Arranque en silencio y cambio 100 % -> 30 % en
marcha también correctos. Pendiente: el caso «el cable ignora el volumen» solo
está cubierto por pruebas unitarias; falta verlo en el equipo Conexant, y la
escucha del usuario en ambos. El límite inferior del cable (mudo bajo ~6-8 %
con su control activo) no se puede compensar desde el host.

El MSI sigue fijando `VBAudioCableWDM_VolCtl=1`. Con el cable ignorando el
volumen el host daría resolución completa y sin escalones; cambiar ese ajuste
es una decisión pendiente del usuario, no adoptada aquí.

# Histórico: volumen VB-CABLE y presentación, versión 0.2.6


## Problema reproducido el 13 de octubre de 2026

Equipo con salida Conexant ISST Audio, VB-CABLE 3.3.1.7 y RESONIQ instalado
0.2.5. `HKLM\SOFTWARE\VB-Audio\Cable\VBAudioCableWDM_VolCtl` ya era 1.
Con una señal de prueba de pico 0.02, el loopback de CABLE Input entregaba
0.0200005 en 0, 2, 10, 50 y 100 por ciento. El controlador declara soporte
de volumen, mute y medidor hardware (`QueryHardwareSupport=7`). El ajuste
persistente no implica que el loopback entregue muestras atenuadas.

La protección existente solo cerraba la salida en cero y mute. Por ello, los
pasos positivos de volumen no llegaban al audio que RESONIQ enviaba al altavoz.

## Corrección

Solo en el transporte identificado como VB-CABLE, el host lee el volumen real
en dB del endpoint en un observador COM independiente cada 20 ms. Convierte
ese valor a amplitud y lo aplica después del DSP, suavizado durante unos 5 ms
a 48 kHz. No interpreta el porcentaje como ganancia lineal. La curva observada
fue -58.1738 dB en 2 %, -34.7547 en 10 %, -10.5086 en 50 % y 0 en 100 %.

El hilo de audio solo lee un atómico y multiplica muestras: no hace consultas
COM, bloqueos ni asignaciones para ajustar el volumen. Las variaciones entre
volúmenes positivos no vacían FIFO ni reinician DSP. Cero y mute mantienen la
protección previa y su espera al reactivar. Fallos del observador silencian y
terminan la ruta para que el mecanismo de reconexión la vuelva a preparar.
El medidor de salida refleja la señal final. La ruta del driver propio conserva
su comportamiento. El motor DSP y los perfiles favoritos no se modifican.

## Interfaz y empaquetado

Banner con el logo Pulso continuo, «Resoniq» en Segoe UI Bold y «by Valvik».
Botón «Acerca de» con versión 0.2.6, autoría de Víctor Manuel Valenzuela aka
Valvik, copyright y créditos de VB-CABLE a VB-Audio Software, Vincent Burel
y Jean-Sylvain Loezic, y enlace a su web oficial. La aplicación, el recurso
de versión de Windows y el MSI deben coincidir antes de empaquetar.

## Verificación y límites

17 pruebas CTest pasan, incluida curva en dB, ganancia progresiva, volumen
máximo sin alterar muestras, invalidación segura y ausencia de asignaciones.
Capturas del banner y Acerca de revisadas visualmente. MSI extraído y verificado:
versión, contenido, créditos, condiciones de Windows, configuración persistente,
asistente firmado y GUI con datos aislados.

Prueba real del host WASAPI, con salida física silenciada temporalmente, sin
micrófono ni audio guardado: en bypass, picos 0 / 0.000024637 / 0.000364381 /
0.00595857 / 0.0199421 para 0 / 2 / 10 / 50 / 100 %. Se restauraron volumen
y mute previos. Evidencia local: `build/vbcable-volume-probe/host-result.txt`.
Con DSP activado, una vez estabilizada su dinámica tras salir de cero, picos
0 / 0.000051781 / 0.000765329 / 0.0125418 / 0.0419367 en los mismos pasos.
La curva se conserva después del procesado. Evidencia:
`build/vbcable-volume-probe/host-dsp-result.txt`. El primer ensayo de esta
comparación fue demasiado corto para estabilizar el AGC tras reactivar el
audio; se corrigió la duración del ensayo, sin cambiar el DSP.

La prueba de audio de la GUI empaquetada se omitió para conservar el motor
instalado abierto. El transporte se comprobó por separado con el host real.
La escucha perceptiva del usuario tras instalar 0.2.6 queda pendiente. No se
ha realizado una transacción MSI ni publicado esta versión en GitHub.

Fuentes de créditos y configuración:
https://vb-audio.com/Cable/VBCABLE_ReferenceManual.pdf y
https://vb-audio.com/Voicemeeter/Voicemeeter_UserManual.pdf.
