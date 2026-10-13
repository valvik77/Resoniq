# Instalación de RESONIQ y dispositivo virtual DSP

Objetivo actual: Windows 11 22H2 o posterior **x64**. No se admite todavía ARM64,
Windows de 32 bits ni Windows Server. El INF actual establece el mínimo 22621.
El MSI instala aplicación, perfiles y herramientas; no necesita .NET ni Visual
C++ Redistributable en el equipo destino. Solicita administrador para Program
Files y el dispositivo de sistema.

## Instalador disponible

`RESONIQ-Setup-0.2.0-x64-Development.msi` incluye el controlador firmado para
pruebas. El MSI instala la aplicación y las herramientas; la creación automática
del dispositivo está deshabilitada y no aparece en el árbol de componentes.
Se realiza desde el acceso «Crear dispositivo virtual DSP» y requiere un equipo
que YA tenga TESTSIGNING efectivo y Secure Boot desactivado.
El instalador no modifica esas opciones ni reinicia automáticamente.

En un equipo normal la aplicación se puede instalar, pero el controlador actual
no puede crear una ruta DSP utilizable hasta disponer de firma Microsoft.
No confundir este paquete de desarrollo con un instalador de distribución general.

Desde Inicio → RESONIQ:

- **RESONIQ** abre la aplicación.
- **Crear dispositivo virtual DSP** solicita elevación, comprueba requisitos y
  crea solo `ROOT\DPSAUDIOVIRTUAL`, evitando duplicarlo. Usa APIs Windows, sin
  DevCon ni WDK en destino. Si ya existe, Windows conserva la mejor versión;
  no se fuerza instalar un controlador más antiguo.
- **Retirar dispositivo virtual DSP** elimina exclusivamente esos nodos; deja
  el paquete en Driver Store para permitir reinstalación y conserva perfiles.

Una vez instalado el dispositivo, abre RESONIQ, selecciona la salida física y
elige **DPS Audio Virtual Transport** como salida de Windows. No se cambia la
salida predeterminada ni el volumen físico durante la instalación.
Si el controlador solicita reinicio, hazlo manualmente antes de usar la ruta.

## Datos, actualización y desinstalación

Los binarios viven en Program Files; cada usuario conserva su configuración en
`%LOCALAPPDATA%\RESONIQ`. No se instala la sesión ni los IDs del portátil del
desarrollador en otros equipos. La primera ejecución propone un ajuste según
el hardware; tu favorito del proyecto original permanece intacto.

La actualización conserva configuración. El desinstalador retira el dispositivo
solo si el instalador registró que lo creó; no elimina un dispositivo previo.
No lo retira durante una actualización mayor. Selecciona una salida física en
Windows antes de retirar DSP. Los datos del usuario no se borran al desinstalar.
Sal de la aplicación desde la bandeja antes de actualizar o desinstalar.
Desactiva «Inicio con Windows» en cada usuario que lo tenga activado antes de
desinstalar; esa entrada de usuario la gestiona la aplicación, no el MSI.
La reversión de una instalación fallida retira el nodo creado en ese intento;
no restaura una versión previa si Windows llegó a actualizar un driver existente.

Diagnósticos: `%PROGRAMDATA%\RESONIQ\driver-install.log`, `driver-state.json` y
`%SystemRoot%\INF\setupapi.dev.log`. La aplicación registra cifras, sin audio,
en su carpeta de usuario. No se graba el micrófono durante la instalación.

## Distribución general pendiente de firma

Microsoft exige firma admitida por el Hardware Dev Center para nuevos drivers
kernel en Windows moderno. La firma local de pruebas no la sustituye.

1. Registrar una cuenta Hardware Developer/Partner Center con certificado EV.
2. Preparar y probar el driver, incluyendo HLK para distribución de producción.
   `installer/Prepare-Driver-Submission.ps1` crea un CAB con SYS/INF/CAT/PDB en
   una subcarpeta, sin claves. Hay que firmar el CAB con el certificado asociado
   a la cuenta antes de enviarlo; no es un paquete ya aprobado por Microsoft.
3. Obtener el paquete firmado por Microsoft.
4. Compilar con `installer/Build-Installer.ps1 -Production -DriverDirectory RUTA`.
   El script rechaza un catálogo de desarrollo y verifica política kernel y
   pertenencia de SYS/INF. La opción de dispositivo quedará seleccionada.
5. Firmar el MSI con la identidad de publicación y verificar en máquinas limpias.

La firma de atestación está documentada para escenarios de pruebas y no acredita
certificación HLK. No afirmar compatibilidad universal por disponer de esa firma.
La cuenta, identidad/certificado y validación externa aún no están disponibles
en este proyecto. No se han comprado certificados ni realizado un envío.

Fuentes oficiales:

- [Política de firma de drivers](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/kernel-mode-code-signing-policy--windows-vista-and-later-)
- [Opciones de firma y HLK](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/driver-signing-offerings)
- [Requisitos del certificado](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/code-signing-reqs)

## Compilar

```powershell
dotnet tool install wix --version 6.0.2 --tool-path build/tools/wix
build/tools/wix/wix.exe extension add WixToolset.UI.wixext/6.0.2
cmake -S . -B build/installer-release -DRESONIQ_STATIC_RUNTIME=ON
cmake --build build/installer-release --config Release
ctest --test-dir build/installer-release -C Release --output-on-failure
./installer/Build-Installer.ps1
```

El empaquetado usa una lista explícita: nunca incluye PFX, claves privadas,
grabaciones, sesiones personales ni directorios completos de desarrollo.
