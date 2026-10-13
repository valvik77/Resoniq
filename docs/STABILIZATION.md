# Consolidación local — 12 de octubre de 2026

El usuario considera satisfactorio el sonido actual y autoriza cerrar todo lo
posible de fiabilidad y distribución. No se modifica el DSP ni el favorito.

## Cambios

- Selección de salida por ID: si falta la elegida, esperar a que vuelva; elegir
  otro dispositivo exige selección explícita. Primera ejecución sin ID usa la
  salida física predeterminada si existe. Cargar perfil respeta su salida,
  en lugar de dar prioridad a la selección del perfil anterior.
- Suspensión: detener trabajador y liberar streams; reanudar permite reconectar
  según preferencias. Un render sin progreso durante cinco segundos termina
  con error recuperable. No se cambian volúmenes ni salida Windows.
- Guardado: actualizar nombre/ruta solo después de escribir correctamente.
  Apertura/cancelación real de selectores probada en GUI, incluida semilla
  relativa y perfil medido. Datos sintéticos guardados y recargados sin alterar
  calibración. No se hace otra grabación.
- Inicio con configuración dañada: copias `.invalid-*`, respaldo de perfil y
  preferencias por defecto; aviso y registro de recuperación. Favorito intacto.
- Paquete local por lista explícita, manifiesto, lanzador portable y script de
  acceso directo opcional. Excluye experimentos, audio, claves y controlador.
  No instala ni modifica certificados, registro o controlador al empaquetar.

## Evidencia

Release sin avisos; 15/15 CTest correctos. Prueba WASAPI de toggles y reinicio
correcta. Mensajes de suspensión/reanudación simulados en la aplicación, con
streams WASAPI reales; no se suspende físicamente Windows.

`tests/test_portable.ps1` ejecutado sobre paquete: GUI, audio, detección de
alteración y arranque normal con sesión/preferencias corruptas en copia temporal.
Se conservaron dos copias inválidas y se comprobó trabajador activo. Fuente
y configuración del usuario no modificadas por esa prueba.

`tests/test_driver_package.py`: paquete firmado de desarrollo aceptado;
INF/SYS alterados rechazados offline. No se reinstala ni se confía otro certificado.

Favorito SHA-256:
`5119695BA5D42074679AB8293F12FC44047126F19275C75DAEB57CEA82C3E23E`.
Sesión SHA-256:
`5B437712D8E450CAE99F6BB037E8A68388F37F34B4C0012FBEFDFF3AA513EC90`.
Render favorito sobre señal de referencia:
`8D00DBCED7758AE61A37A7B8DD43A2ECAE958C21033F2C407C31787EA97DC1EA`.
Son los hashes previos. Aplicación habitual relanzada con su sesión.

## Pendiente externo

Desconexión física USB/Bluetooth y suspensión real de Windows requieren una
prueba de uso que no interrumpa el trabajo del usuario. La selección/reconexión
tiene pruebas, pero no afirmar que esos dispositivos se probaron físicamente.
Una nueva medición real con guardado sigue requiriendo permiso por operación;
la persistencia de los valores medidos está probada con datos sintéticos.

El paquete es para este equipo, con driver/runtime existentes. Distribución
general requiere instalador, firma de producción y validación en otras máquinas.
Medición con cancelación, topología del hardware y latencia física siguen como
posibles mejoras futuras, sin cambiar el sonido satisfecho del usuario.
