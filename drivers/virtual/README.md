# DPS Audio Virtual Transport — development driver

Estado: driver x64 compilado, INF verificado con `InfVerif /w`, catálogo generado con `Inf2Cat`, firma de desarrollo aplicada a SYS/CAT y hashes verificados offline. **Instalado y probado en el Lenovo:** dispositivo `ROOT\MEDIA\0000`, código de error PnP 0. Tras el reinicio autorizado, TESTSIGNING está activo. La ruta señal generada → endpoint DPS → loopback de software → DSP → Realtek funcionó durante diez segundos, con 37 mediciones de entrada no nula, cero underruns/overflows y una discontinuidad notificada al inicio de señal. El funcionamiento prolongado y la evaluación auditiva quedan pendientes. No se cambió Secure Boot (ya estaba desactivado).

## Construcción reproducible

```powershell
python drivers/virtual/build_driver.py
# Si ya tienes los paquetes y la revisión fijada descargados:
python drivers/virtual/build_driver.py --no-restore
.\drivers\virtual\sign_test_package.ps1
.\drivers\virtual\verify_package.ps1
python tests/test_driver_package.py
```

El script restaura WDK/SDK `10.0.28000.2526` desde NuGet y la revisión fija de SysVAD indicada en THIRD_PARTY_NOTICES. Usa directamente MSVC y herramientas del WDK; no requiere modificar la instalación global de Visual Studio. Compila C++17 de kernel con `/kernel`, CRT de kernel, `/W4 /WX`, Spectre mitigations y KMDF 1.15. La infraestructura del controlador es MS-PL; el DSP sigue siendo original y MIT.

`overlay/minipairs.h` y `overlay/dpswavtable.h` definen un único endpoint render, dos pins (host/bridge), PCM32 estéreo 48 kHz, modos default/raw y sin hardware loopback/offload/APO. La captura prevista es el loopback de software de Windows. Esto debe confirmarse con señal real tras la instalación; la compilación no prueba los relojes WaveRT ni el enrutamiento. El transporte virtual consume el flujo sin enviarlo a hardware; el host externo produce la salida Realtek.

Las fuentes públicas se descargan en `third_party`, se preparan en `vendor` y se excluyen de Git. Las transformaciones específicas del sample están versionadas en `build_driver.py`: eliminar override de grabación de demostración y ramas sideband. Código dormant de tono/grabación permanece en la infraestructura, pero ningún pin de captura se publica y no se habilita grabación. No se incluyen DLL APO ni adaptadores keyword del sample.

## Prueba en el Lenovo

Primero guarda el trabajo y **reinicia manualmente**. Después abre PowerShell como administrador en la raíz del proyecto:

Esta preparación ya se completó en el Lenovo. Para usar el motor, puedes abrir `Start-DPS.cmd` desde la raíz y seleccionar **Altavoces (DPS Audio Virtual Transport)** como salida de Windows. El lanzador lee el ID persistente Realtek de `config/local-routing.json`, un archivo local excluido de Git. **B** cambia bypass, **+/-** intensidad y **Q** detiene el host; al detenerlo vuelve a seleccionar Realtek para escuchar el audio directamente.

```powershell
.\drivers\virtual\install_test_driver.ps1 -Install
```

El instalador verifica el modo TESTSIGNING efectivo del kernel (no solo la configuración BCD), valida firmas y hashes sin confiar inicialmente en el certificado, importa el certificado de desarrollo en Root/TrustedPublisher y usa DevCon para crear/actualizar únicamente `ROOT\DPSAUDIOVIRTUAL`. No cambia el dispositivo predeterminado ni reinicia. Guarda registro en `build/install-state.json`. La instalación aún debe validar el estado del dispositivo: si devuelve error, consultar `%SystemRoot%\INF\setupapi.dev.log`.

Comprobar después:

```powershell
.\build\Release\dps_live.exe --list
.\build\Release\dps_live.exe --dps --render-id "ID_REALTEK_DE_LA_LISTA" --raw --preset Enhanced
```

Envía una aplicación a DPS Audio Speakers, empezando con volumen moderado, y confirma niveles de entrada/salida. Antes de hacerlo salida predeterminada habitual, probar señal conocida, bypass, continuidad, duración prolongada, suspensión/reanudación y quitar/reponer endpoints. El host todavía termina ante invalidación en vez de reconectar automáticamente. Al detenerlo, seleccionar Realtek para volver a escuchar las aplicaciones que estaban dirigidas al endpoint virtual.

Para probar con señal generada sin WAV, mantener el host anterior ejecutándose y abrir otra consola:

```powershell
.\build\Release\dps_live.exe --tone --render INDICE_DPS --seconds 5
```

La señal de esa prueba ya pasa por DSP en el generador además del host; valida transporte, no compara la calidad de un único procesamiento. Para una evaluación auditiva usar música del reproductor.

## Reversión

Selecciona Realtek como salida si habías elegido el endpoint virtual y detén el host. Desde PowerShell administrador:

```powershell
.\drivers\virtual\remove_test_driver.ps1 -Remove
# Al terminar las pruebas de desarrollo, desactivar TESTSIGNING y reiniciar:
bcdedit /set testsigning off
```

El desinstalador elimina solo dispositivos con nuestro Hardware ID, el INF publicado asociado a ese proveedor y los certificados que nuestro instalador añadió. No modifica BCD automáticamente. El cambio de firma de pruebas afecta a Windows y puede mostrar la marca de modo de prueba tras reiniciar. El paquete tiene un certificado local de pruebas; no es firma de producción ni certificación de compatibilidad.

No distribuir `build/DpsAudioDevelopment.pfx`: contiene la clave privada local sin contraseña para automatización de desarrollo. No se importa en almacenes durante la creación del paquete. No publicar logs con rutas personales. Los paquetes de distribución deberán usar un proceso de firma admitido por Windows y superar pruebas de driver.
