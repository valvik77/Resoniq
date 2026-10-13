# Volumen VB-CABLE y presentación, versión 0.2.6

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
