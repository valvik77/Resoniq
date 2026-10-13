# Cierre durante actualización — 0.2.8

El instalador no conseguía cerrar RESONIQ: el cierre normal enviaba la ventana
a la bandeja y la GUI no procesaba WM_ENDSESSION. Aceptar el cierre automático
de Windows no detenía el proceso ni liberaba el ejecutable.

La aplicación ahora acepta WM_QUERYENDSESSION y, solo cuando recibe
WM_ENDSESSION con TRUE, guarda la sesión, retira la bandeja, detiene WASAPI y
sale. Una consulta cancelada no cierra ni altera la sesión.

Para poder actualizar versiones ya instaladas que carecen de esta corrección,
el MSI incluye un helper nativo estático. Se ejecuta antes de InstallValidate:
enumera ventanas de clase Resoniq.Gui, comprueba la ruta completa del proceso
contra INSTALLFOLDER/resoniq.exe y solicita el comando normal Salir (304), que
ya existe en las versiones 0.2.x. Espera hasta 15 segundos en total. No usa
TerminateProcess, taskkill ni cierres por nombre de proceso. No cierra copias
portables situadas en otra carpeta. Si no consigue salir, devuelve un error
para que el usuario cierre la aplicación manualmente; no sigue ignorando el fallo.

Se programa para instalar/actualizar/reparar, incluyendo instalación silenciosa;
no para desinstalar. El arranque al terminar sigue en el botón Finalizar de la
interfaz MSI. Cerrar para actualizar interrumpe temporalmente el audio procesado.

## Verificación

GUI y paquete MSI extraído pasan. La base MSI verifica que CloseRunningResoniq
se ejecuta antes de InstallValidate. Prueba funcional nativa con dos copias
aisladas y perfiles cuya salida inexistente evita abrir audio:

- Una copia del binario 0.2.7 reproduce el cierre a bandeja.
- El helper del MSI la cierra de forma normal y guarda su sesión de prueba.
- La otra copia, situada en una carpeta diferente, permanece abierta.
- En 0.2.8, consulta aceptada y cancelación dejan la ventana viva; cierre
  confirmado de Restart Manager termina el proceso y guarda la sesión.

Ejecutable de prueba: resoniq_shutdown_tests MSI close.dll new.exe legacy.exe
work-directory. Código en tests/shutdown.cpp. Datos exclusivamente en build.
La aplicación instalada del usuario no se cerró durante estas pruebas. No se
ha realizado una transacción MSI real de actualización; queda su confirmación.

Fuente del protocolo:
https://learn.microsoft.com/en-us/windows/win32/shutdown/wm-queryendsession
y https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/RstMgr/guidelines-for-applications.md.
