# Investigación de VB-CABLE — 12/10/2026

Conclusión: vía técnicamente viable y prioritaria para distribución personal.
VB-CABLE todavía no está instalado; la compatibilidad de la GUI está implementada
y la ruta propia del Lenovo sigue activa.

Actualización de implementación: la GUI ya incluye selector Entrada virtual,
preferencia virtual_input_id y detección propia/VB-CABLE. El MSI 0.2.1 incluye
guía, enlaces y apertura opcional de instrucciones al terminar. No incorpora
drivers ni acciones de instalación de dispositivo. La detección inicial usa
nombres/metadatos de interfaz; un ID seleccionado se conserva incluso si cambia
el nombre. No equivale a identificar todo hardware virtual ni prueba de ruta VB
real. Sigue sin instalarse VB-CABLE en el Lenovo.

## Paquete inspeccionado

Descarga oficial `VBCABLE_Driver_Pack45.zip` en `build/vbcable-research/`.
SHA-256: `B950E39F01AF1D04EA623C8F6D8EB9B6EA5C477C637295FABF20631C85116BFB`.
INF Windows moderno: versión 3.3.1.7, 07/10/2024, x64/ARM64, mínimo 16299.
Esto no amplía automáticamente el soporte de RESONIQ, cuyo MSI sigue Win11 x64.

Comprobaciones locales, sin ejecutar instalador:

- `signtool verify /kp /c CAT SYS`: correcto para el SYS x64 moderno.
- `signtool verify /pa /c CAT INF`: correcto.
- CAT: firma válida de Microsoft Windows Hardware Compatibility Publisher.
- Setup x64: firma Authenticode válida del editor BUREL VINCENT.
- En este equipo solo aparecen DPS propio/Realtek: VB-CABLE no está instalado.

La firma válida resuelve el requisito de firma del proveedor; no demuestra por
sí sola compatibilidad HVCI ni funcionamiento de nuestra ruta en otra máquina.

## Licencia

La [web del editor](https://vb-audio.com/Services/licensing.htm) permite integración
del cable básico respetando identificación, origen y modelo donationware. El
uso profesional/organizaciones tiene requisitos de pago. A+B/C+D no se incluyen.
El README del ZIP exige acuerdo del autor para integrarlo en otra instalación.
Hay discrepancia documental: primera validación con instalador oficial separado;
confirmar las condiciones antes de distribuir una instalación integrada.
No se ha contactado al editor ni aceptado una licencia mediante instalación.

## Transporte propuesto y cambios locales

Ruta inicial: aplicaciones → CABLE Input (render) → loopback WASAPI de ese
endpoint → motor RESONIQ actual → salida física. El [manual](https://vb-audio.com/Cable/VBCABLE_ReferenceManual.pdf)
documenta loopback y control de volumen opcional; la instalación necesita admin
y reinicio, y puede alterar las salidas/entrada predeterminadas de Windows.
Conviene probar 48 kHz estéreo y verificar cero/mute y primer paso antes de
adoptar la ruta. No asumir el comportamiento del driver propio.

La consola ya permite `--loopback-id`, así que se puede validar el transporte
sin modificar DSP. La GUI dispone de selección explícita de transporte y guarda
IDs persistentes. La clasificación inicial usa nombre/metadatos; una identificación
completa mediante topología y la validación real del cable siguen pendientes.

El paquete moderno expone más de un render: no elegir el primero por texto.
Excluir todos los renders virtuales de la lista física, impedir realimentación
y distinguir coexistencia con DPS propio. No usar CABLE Output como micrófono
de medición. Si se optase por captura de ese endpoint, también habría que
observar silencio del render asociado; no basta leer volumen del capture.

## Instalación y validación siguiente

1. Compatibilidad y nuevo MSI preparados, sin cambiar el favorito ni la ruta activa.
2. Instalar el paquete oficial por separado en un equipo de prueba, conservar
   los IDs predeterminados previos y reiniciar manualmente.
3. Enumerar sus endpoints y confirmar cuál corresponde al render seleccionado.
4. Probar música/tono conocido sin micrófono: bypass, intensidad, volumen normal,
   cero/mute/reapertura, continuidad y ausencia de reproducción doble.
5. Probar suspensión, reconexión y cierre del host. Recuperar salida física
   cuando RESONIQ no esté activo; un cable virtual no reproduce solo en altavoces.
6. Revisar condiciones de distribución y empaquetar una versión sin driver propio.

No modificar volumen, políticas de arranque, instalar, desinstalar o reiniciar
este equipo como parte de la investigación. No prometer instalación silenciosa
ni cero reinicios: la documentación consultada no permite afirmarlo.

Fuente de descarga/manual: [VB-CABLE oficial](https://vb-audio.com/Cable/).
