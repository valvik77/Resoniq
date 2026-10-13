# RESONIQ 0.2.0 — versión portable para este equipo

El sonido aprobado se conserva. La versión incluye el perfil favorito, una
copia de la sesión y preferencias al empaquetar. Los perfiles experimentales
no se distribuyen en este paquete.

## Uso

1. Extrae el ZIP en una carpeta propia donde puedas escribir.
2. Sal de la otra instancia de RESONIQ desde su menú de bandeja.
3. Abre `Start-RESONIQ.cmd`. El controlador virtual ya debe estar instalado.
4. Selecciona DPS Audio Virtual Transport como salida de Windows y la salida
   física en RESONIQ. Si la salida guardada falta, elige otra explícitamente.

Opcional: `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/Create-Shortcut.ps1`
crea un acceso directo al ejecutable de esta carpeta. No mueve archivos ni
modifica el inicio con Windows. Para arrancar automáticamente, usa Ajustes en
la instancia que quieras conservar; la carpeta deberá permanecer en su sitio.

El paquete se prepara para el Windows x64 de este equipo, con el controlador
virtual y el runtime de Visual C++ ya disponibles. No instala ni reinstala el
controlador. No es un instalador firmado para distribuir en otros equipos.

## Perfiles y recuperación

«Guardar perfil» crea un archivo `.dps`; «Cargar perfil» recupera controles,
calibración, RAW y salida. Las medidas aplicadas se guardan como «Measured».
El guardado sustituye el archivo solo cuando la escritura terminó correctamente.
Si falla, los metadatos del perfil activo se conservan.

«Ajuste por defecto» recupera el favorito aprobado. «Fijar actual» reemplaza
ese favorito tras confirmación; no lo uses solo para guardar un perfil nuevo.
Haz copias de `config` para conservar tu configuración.

Con reconexión activada, la aplicación espera a que vuelva la salida guardada;
no envía automáticamente el sonido a otros altavoces. Tras suspensión reinicia
la ruta. Un render sin progreso durante cinco segundos provoca recuperación.
Puede haber una pausa de reconexión. Los pasos entre volúmenes no nulos siguen
sin pausa; al salir de mute/cero permanece la protección de 180 ms aprobada.

Si una sesión no se puede cargar, se conserva una copia `.invalid-*` y se
intenta el siguiente perfil de respaldo. Una salida inexistente se comunica en
la ventana. Nunca se cambia automáticamente la salida predeterminada de Windows.
Las preferencias ilegibles también conservan copia y usan valores por defecto;
la recuperación se comunica en la ventana y en el diagnóstico de arranque.

## Comprobaciones y límites

Quince pruebas automatizadas: DSP, perfiles, recuperación de selección, interfaz,
medición sintética y otras comprobaciones de estabilidad. La interfaz prueba
apertura/cancelación real de los selectores y guardado/carga de un perfil medido
sintético. La prueba WASAPI comprueba reinicio de streams y mensajes simulados
de suspensión/reanudación. No equivale a una suspensión real de Windows ni a
desconectar físicamente USB/Bluetooth; esas comprobaciones quedan pendientes.

No se usa el micrófono en estas verificaciones. No cambia el sonido aprobado.
El limitador protege picos de muestra; no es un modelo físico de altavoz ni un
limitador true peak. La latencia mostrada es una estimación de software.

`Verify-Portable.ps1` verifica el manifiesto SHA-256 del paquete original.
Tras usarlo, los archivos de sesión/preferencias pueden cambiar legítimamente.
El manifiesto detecta alteraciones, pero no autentica al editor como una firma.
El paquete excluye controladores, claves privadas, grabaciones y diagnósticos.
