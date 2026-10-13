# Dispositivo virtual propio: DPS Audio Speakers

> Estado actualizado 12/10/2026: vía conservada para retomarla, mientras se
> investiga VB-CABLE para distribución. Recuperación en OWN_DRIVER_RECOVERY.md;
> estudio actual en VBCABLE_RESEARCH.md. El driver del Lenovo sigue instalado.

Decisión: reemplazar el endpoint virtual de terceros por uno propio. La ruta objetivo es:

```text
Aplicaciones (salida DPS Audio Speakers)
              ↓
Windows Audio Engine → driver virtual WaveRT/PortCls
              ↓ captura loopback del endpoint virtual
Host DPS en modo usuario → Engine C++20
              ↓ WASAPI render
Altavoces Realtek
```

El dispositivo recibe el sonido antes de llegar a Realtek. No captura la salida física ni debe enviarla de vuelta al mismo endpoint. El DSP continúa en modo usuario para reutilizar el motor, facilitar actualizaciones y mantener las operaciones matemáticas fuera del kernel. El driver se limita a exponer y mantener el transporte del audio virtual.

## Contrato de la primera versión

- Endpoint de reproducción `DPS Audio Speakers`, sin dependencia de FxSound.
- Windows 11 x64, WaveRT/PortCls, modo compartido estéreo PCM a 48 kHz inicialmente. El motor procesa float32; los formatos del endpoint y su conversión se verifican al desarrollar el controlador.
- Sin offload ni modo exclusivo en la primera entrega; loopback tiene que capturar el flujo mezclado recibido por el endpoint.
- Implementar posiciones de búfer, temporización, notificaciones, transiciones STOP/ACQUIRE/PAUSE/RUN y limpieza PnP/power. Un dispositivo que aparece en Sonido pero no mueve correctamente su reloj no cumple el contrato.
- El host abre loopback sobre el ID persistente del endpoint DPS y reproduce sobre el ID persistente Realtek. No seleccionar por índices como configuración permanente.
- Inicio automático del host y recuperación frente a invalidación, suspensión y cambio de frecuencia antes de instalarlo como salida predeterminada habitual.
- Bypass conserva la ruta y manda audio sin mejora a Realtek. Si el host termina, el endpoint virtual queda sin salida audible: desarrollar un supervisor y una restauración explícita de la salida anterior. El driver por sí solo no puede garantizar el bypass a Realtek.

## Implementación y validación

1. Instalar/configurar WDK compatible con Visual Studio y SDK. Usar SysVAD como referencia pública de interfaces y ciclo de vida; su muestra completa no equivale a un cable de audio listo para este producto. Registrar licencias si se reutiliza código abierto de Microsoft; DSP original.
2. Crear proyecto de driver, INF, identidad propia y paquete de prueba, sin asociaciones ni CLSID heredados de muestras. Implementar primero endpoint render virtual y reloj WaveRT.
3. Probar en un entorno Windows de desarrollo separado con controladores firmados para pruebas. No cambiar Secure Boot ni la política de firma del portátil como efecto secundario de la compilación.
4. Validar apertura WASAPI, captura loopback real con señal conocida, continuidad, tiempos/posición, silencio y múltiples aplicaciones. Confirmar ausencia de doble reproducción.
5. Conectar al host existente, ampliar pruebas a larga duración y reconexión. Validar Driver Verifier, suspensión/reanudación, quitar dispositivo y cerrar host.
6. Preparar instalación/desinstalación y firma admitida por Windows para distribución. La compilación gratuita no elimina los requisitos ni los posibles costes de firma/distribución.

## Estado real

Existe motor DSP y host WASAPI. Se ha adquirido WDK/SDK localmente por NuGet y desarrollado una variante de transporte basada en infraestructura pública SysVAD, con endpoint y paquete propios. El driver está compilado, firmado para desarrollo e instalado en el Lenovo sin error PnP. INF, catálogo, firmas criptográficas y hashes se han verificado. La prueba inicial del endpoint propio → DSP → Realtek funcionó con señal y sin underruns/overflows. TESTSIGNING está activo tras el reinicio autorizado. Ver [instrucciones y límites](../drivers/virtual/README.md). Quedan pruebas prolongadas, suspensión y evaluación auditiva.

Referencias oficiales:
- https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/sysvad-virtual-audio-device-driver-sample/
- https://learn.microsoft.com/windows/win32/coreaudio/loopback-recording
