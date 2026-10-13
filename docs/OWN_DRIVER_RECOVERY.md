# Recuperar la vía del controlador propio

Decisión del usuario, 12/10/2026: conservar esta vía e investigar VB-CABLE para
distribución. El controlador instalado en el Lenovo permanece activo; no se
desinstala, no se cambia TESTSIGNING y el sonido aprobado continúa igual.

## Qué se conserva

- `drivers/virtual/`: build reproducible, revisión SysVAD fijada, overlay,
  INF/recursos, firma de pruebas, comprobaciones e instalación/desinstalación.
- `installer/`: helper SetupAPI/newdev, MSI WiX, gestión de dispositivo y CAB
  para el proceso de firma externa. El MSI Development no crea el dispositivo
  automáticamente; su creador separado exige un equipo ya preparado.
- `docs/virtual-device.md`, `drivers/virtual/README.md`, `INSTALLATION.md`,
  `PROJECT_MEMORY.md` y `CODEX_HANDOFF.md`: diseño, problemas y estado verificado.
- Bibliotecas, host, perfiles estables, pruebas y binarios de recuperación.

`scripts/Archive-OwnDriver.ps1` crea una instantánea en `archives/`, con manifiesto
SHA-256 y ZIP. Es material técnico de recuperación, no un instalador para terceros.
No incluye paquetes WDK/SDK descargables, vendor regenerable, logs, grabaciones,
sesiones personales ni claves privadas. Preserva el build firmado público y PDB.

La clave de desarrollo original permanece exclusivamente en su ubicación local
`drivers/virtual/build/DpsAudioDevelopment.pfx`; no se copia al archivo. La copia
permite recuperar código y paquete existente. Para volver a firmar con la misma
identidad hay que conservar esa clave aparte, de forma privada. No publicarla.

## Retomar

1. Extraer el ZIP en una carpeta de trabajo y verificar su manifiesto.
2. Consultar `drivers/virtual/README.md` y restaurar la revisión/SDK indicados.
3. Ejecutar `build_driver.py`; comprobar INF, catálogo y firmas.
4. Usar un equipo de pruebas ya preparado para instalar el driver Development.
5. Para distribución general, obtener la firma Microsoft y validar en máquinas
   limpias. No etiquetar el paquete de pruebas como producción.

El DSP vive en modo usuario; el driver solo transporta audio. Cambiar de transporte
no exige reemplazar el algoritmo ni el perfil favorito. La compatibilidad con
VB-CABLE aún está pendiente de implementación y prueba real.
