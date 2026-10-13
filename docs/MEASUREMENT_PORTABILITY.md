# Medición en otros equipos y cierre de instalación — 0.2.7

## Evidencia disponible

El equipo actual usa Conexant ISST Audio, distinto del Lenovo/Realtek anterior.
El registro del intento del usuario muestra 249120 muestras, ruido de captura
-66.11 dBFS, tono reconocido -58.25 dBFS, margen 7.86 dB y cero bandas fiables.
Hubo una notificación de discontinuidad, pero ninguna muestra perdida. El pico
captado fue 0.2077. Estos datos no prueban que la sala sea ruidosa: muestran que
los tonos sostenidos no se reconocen con margen suficiente.

La captura anterior abría el micrófono con el procesado predeterminado y sumaba
sus dos canales. La supresión de ruido/eco puede eliminar tonos sostenidos;
canales de distinta fase pueden cancelarse al promediarlos. Son riesgos del
código, no causas acústicas confirmadas por el registro. La inspección local,
sin iniciar captura, confirma formato estéreo float32/48 kHz del micrófono,
`RawProcessingSupported=true` y aceptación de SetClientProperties RAW.

## Cambios

- Preferir el micrófono físico predeterminado de Windows, excluyendo transportes
  virtuales. No fijar un nombre ni un ID del equipo de desarrollo.
- Intentar captura RAW por sesión con AudioCategory_Other. Si la preparación
  falla, abrir un cliente nuevo en modo normal. No cambiar ajustes globales de
  privacidad, volumen, mejoras de audio ni configuración del controlador.
- Conservar los dos canales durante la captura y analizarlos independientemente.
  Elegir primero un canal utilizable, después el de más bandas coherentes y,
  como desempate, el de mayor margen de tonos sobre el ruido. No elegir por pico.
- Mantener los umbrales de fiabilidad y no generar calibración de una medida
  rechazada. El aviso ya no atribuye un margen bajo a ruido de sala como certeza.
- Registrar `input_raw`, `capture_channel` y motivo de fallback RAW junto con
  los valores y dispositivos del diagnóstico. El audio se sigue descartando,
  salvo el modo diagnóstico que requiere consentimiento en la aplicación.

La comparación sigue siendo DSP activado/desactivado, sin compensación. No se
cambian el motor de sonido, el favorito ni la sesión del usuario.

## Instalador y Acerca de

El botón Finalizar del MSI lanza el ejecutable instalado mediante
WixUnelevatedShellExec antes de cerrar el diálogo. También lo hace al actualizar
o reparar, independientemente del checkbox del asistente VB-CABLE. No se lanza
al desinstalar ni desde la transacción elevada. Es el flujo del asistente con UI;
una instalación silenciosa no tiene ese botón ni abre una ventana al usuario.

Acerca de aproxima «by Valvik» según el ancho real de Resoniq, elimina «aka Valvik»
del texto de autoría y añade licencia MIT y dirección del repositorio.

## Validación y pendientes

17 pruebas CTest pasan. Nuevas regresiones cubren canales opuestos y selección de
tonos coherentes frente a un canal más ruidoso, conservando rechazo de ruido real,
huecos, entradas inválidas y capturas truncadas. Acerca de se revisó visualmente.
MSI extraído: versión 0.2.7 y contenido correctos; motor MSI evalúa que el arranque
se habilita al instalar/reparar y se deshabilita en desinstalación. La acción está
en Finalizar, antes de EndDialog, y utiliza el ejecutable incluido en el paquete.

No se ha ejecutado una transacción MSI ni una nueva medición acústica. El usuario
debe instalar 0.2.7 y repetir una medición para confirmar el resultado real de
esta captura en Conexant. Los requisitos siguen siendo Windows 11 22H2+ x64;
no se promete compatibilidad con cualquier arquitectura o sistema operativo.

Fuentes:
https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-signal-processing-modes
y https://docs.firegiant.com/wix/tools/wixext/util/.
