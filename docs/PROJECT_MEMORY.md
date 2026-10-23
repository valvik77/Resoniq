# Memoria del proyecto

## Salida propuesta cuando la guardada desaparece — 23 de octubre de 2026, 0.2.17

Petición del usuario tras el informe: «sí, quiero que proponga la
predeterminada». Antes la reconexión esperaba indefinidamente a la salida
guardada y la aplicación se quedaba muda. Cuidado importante: la predeterminada
de Windows en este montaje es el CABLE, así que proponerla sin más crearía un
bucle; la propuesta se hace solo entre las salidas FÍSICAS, es decir las que no
son transporte virtual, y toma la predeterminada del sistema si está entre ellas
o la primera disponible si no.

Se conserva la regla original de no desviar el sonido a otro altavoz, ahora bien
delimitada: se distingue una salida desenchufada de una reemplazada preguntando
a Windows si todavía conoce ese endpoint en cualquier estado, porque la
enumeración solo devuelve los activos. Desenchufada o deshabilitada sigue siendo
conocida y se espera; olvidada por Windows se considera reemplazada y se propone
la predeterminada. Si la consulta falla, se espera, que es lo prudente.
Decisión en una función pura con pruebas; consulta nueva en la capa de audio,
también probada contra un identificador inexistente y uno real del equipo.
Al proponer, la ventana lo dice y queda en el registro de telemetría.

Verificado en el caso real de este equipo (salida guardada
{0.0.0.00000000}.{6e5d6ebf…} ya inexistente): con copia aislada de la
configuración, la aplicación propuso «Altavoces (Realtek(R) Audio)» y el audio
arrancó, running=1, finished=0, volumen leído 0.600074. 19/19 CTest, contrato
de audio clásico incluido; compilación Release/runtime estático sin avisos.
No se cambió el motor de audio ni los perfiles.

Versión 0.2.17 e instalada por continuidad de la misma petición: msiexec salida
0, MSI `dist/RESONIQ-Setup-0.2.17-x64-VBCABLE.msi` SHA256
6D261D64905E4C5ADFD0A578DA2FB71AF6B1ECE33CD5781D699D0773B55B9823,
test_installer -Cable -SkipAudioTest pasa (evidencia
build/installer-test-5f8d3a97066b465f80c871be175d7cd6), exe instalado SHA256
E0CDBE9F9FA49287359F7F163682CBA6246F2405EEEEE02F2F2946A210F185CA,
registro build/install-0.2.17.log. Los dos anclajes de inicio siguen puestos.
La aplicación instalada corre en bandeja procesando con la salida propuesta.
CONSECUENCIA A VIGILAR: al cerrar, la sesión se guardará con la salida nueva,
así que `session.user.dps` dejará de tener el hash 02E32C0F…556F. El favorito
NO se toca y sigue en 5119695B…C3E23E. Sin reinicio de Windows y sin push.

## Auditoría y doble anclaje de inicio — 23 de octubre de 2026, 0.2.16

El fallo SIGUE VIGENTE y se reprodujo hoy: Windows reinició a las 17:20:58,
sesión a las 17:21:21 y RESONIQ no arrancó. La entrada Run restaurada esta
misma mañana a las 09:19 había desaparecido otra vez; la clave Run se escribió
por última vez a las 17:36:49 con tres valores y ninguno RESONIQ. Descartados
como causa: el instalador (no registra ni retira nada en Run), el código de la
aplicación (ningún camino de salida borra el valor; el guardado de sesión de
las 17:19:59 fue el cierre limpio del reinicio), Defender (sin detecciones de
RESONIQ, solo eventos 5007 de configuración), StartupApproved (sin valor de
bloqueo, cero valores) y limpiadores instalados (solo IObit Unlocker, que no
gestiona el arranque). Quién lo borra sigue sin identificar: la marca de hora
de la clave solo conserva la última escritura.

Conclusión de diseño: un único anclaje que solo se repara cuando la aplicación
arranca no puede recuperarse de un borrado ocurrido con la aplicación cerrada.
Ahora se registran DOS anclajes independientes, la entrada Run y un acceso
directo en la carpeta de Inicio, y los dos se comprueban y reponen en cada
arranque. Cada arranque deja una línea en `build/startup-audit.log` con lo que
encontró y si lo arrancó Windows, de modo que una pérdida futura quede fechada.
La política de reparación está en una función pura con pruebas: repone si falta,
reapunta si la ruta ya no existe o le falta `--tray`, no reescribe si ya está
bien ni aunque cambie la caja de las letras, y respeta una entrada de otra copia
que sigue en disco. El arranque se registra SIEMPRE para la copia instalada
(valor nuevo InstallFolder en HKLM, o Program Files/RESONIQ): una compilación
del árbol de fuentes ya no puede quedarse con el arranque de sesión.

Otros defectos corregidos en la misma pasada: no había protección de instancia
única, así que dos copias podían pelearse por el mismo dispositivo (ahora la
segunda muestra la ventana de la primera y sale, salvo en pruebas); `--data-dir`
se ignoraba si el ejecutable no tenía al lado installed.marker, de modo que una
compilación de pruebas leía y escribía la configuración real (ya documentado
como incidente el 14 de octubre y ahora arreglado); y la casilla de ajustes
reflejaba el registro en vez de la preferencia guardada, por lo que guardar
cualquier ajuste con la entrada perdida cancelaba el inicio automático en
silencio. El desinstalador retira ahora los dos anclajes.

Verificado en este equipo: 19/19 CTest, incluido el contrato de audio clásico
y la prueba nueva de arranque; compilación Release/runtime estático sin avisos.
La nueva compilación, ejecutada con configuración aislada y la preferencia
activada, repuso los dos anclajes apuntando a `C:\Program Files\RESONIQ`,
registró `run=restored shortcut=restored`, y en la segunda pasada `run=ok
shortcut=ok` sin reescribir. Dos lanzamientos seguidos dejan un solo proceso.
La aplicación instalada, lanzada con el mismo comando que usa el inicio de
sesión, queda en bandeja sin ventana (PID 8728). Favorito, sesión y ajustes
reales intactos: favorito SHA256 5119695B…C3E23E, sesión 02E32C0F…556F,
ajustes 175A71EF…A1C1. Copia previa en build/startup-audit-20261023.

ERROR PROPIO DETECTADO Y RETIRADO EN LA MISMA SESIÓN: se añadió al instalador
la retirada de los dos anclajes al desinstalar. En WiX, `RemoveRegistryValue`
sin más se aplica AL INSTALAR, de modo que la primera instalación de la 0.2.16
borró la entrada Run del usuario; se vio en el registro inmediatamente después.
Cambiarlo a «al desinstalar» tampoco sirve: esa retirada se ejecuta también
cuando una actualización mayor desinstala el producto anterior, y dejaría la
aplicación sin arrancar en sesión tras cada actualización. Decisión: el
instalador NO toca el registro de inicio; solo lo gestiona la aplicación. Queda
documentado en los dos .wxs. Una entrada huérfana tras desinstalar apunta a un
archivo que ya no está y Windows la ignora, que es el mal menor.

SALIDA DE AUDIO GUARDADA PERDIDA EN EL REINICIO, defecto distinto y vigente:
la sesión guarda `output_id {0.0.0.00000000}.{6e5d6ebf-8251-48db-ba39-e13d55bf8d6a}`
y ese endpoint ya NO existe en el equipo tras el reinicio de las 17:20; los
Realtek presentes son {9e3be449…} activo y {d89d0664…} desconectado. Por eso la
aplicación arranca, se queda en bandeja y NO procesa: telemetría running=0,
finished=1, output_volume=-1, ninguna línea con running=1. No lo causó ningún
cambio de esta sesión: el identificador cambió al reiniciar, probablemente por
actualización del controlador. El comportamiento de esperar la salida guardada
sin sustituirla por otro altavoz es DELIBERADO y está documentado en el README,
así que no se cambió por iniciativa propia. Para volver a oír procesado hay que
abrir RESONIQ y elegir la salida una vez; eso reescribirá la sesión, por lo que
se deja a decisión del usuario. Pendiente de decidir: si la reconexión debe
proponer la salida predeterminada del sistema cuando la guardada desaparece.

Versión subida a 0.2.16 porque cambia el comportamiento del producto.
MSI definitivo `dist/RESONIQ-Setup-0.2.16-x64-VBCABLE.msi`, ya sin la limpieza
equivocada, SHA256
871585537CD2A5169CE1170D9EB7C61BA7039365CDCF61A45BC48EA338BFD2B5;
test_installer -Cable -SkipAudioTest pasa (evidencia
build/installer-test-943e6bcbfdf24c43bb9ab787d9687b50). El hash
E33FF254…E594 corresponde al intento con la limpieza y NO debe usarse.
INSTALADA por autorización expresa del usuario: msiexec salida 0, exe en
Program Files SHA256
6B909B1D7C1078B0B49913FEE696DC61746C5AA3F4FAA909DC544F71ED7874AA, igual al
compilado, 15 archivos, versión registrada en Windows 0.2.16, HKLM
Software\RESONIQ InstallFolder `C:\Program Files\RESONIQ\`. Respaldo de la
instalación anterior en build/install-0.2.16-backup y registros en
build/install-0.2.16.log y build/install-0.2.16-corrected.log.
La aplicación instalada repuso ella sola la entrada que faltaba
(`run=restored shortcut=ok`), en el arranque siguiente no reescribió nada
(`run=ok shortcut=ok`) y reinstalar encima conserva los dos anclajes.
Proceso actual en bandeja desde Program Files.

Al regenerar el instalador se sobrescribió
`dist/RESONIQ-Setup-0.2.15-x64-VBCABLE.msi`: ese archivo ya NO es el MSI del 15
de octubre con SHA256 A1A1424B…9459, que se reconstruye desde el commit e3b6af5
si se necesita. No se reinició Windows ni se hizo push; el commit en main sí fue
autorizado. No se tocó código de audio, perfiles ni volúmenes, y favorito,
sesión y ajustes del usuario siguen con los hashes de arriba. La validación
definitiva del inicio de sesión queda pendiente del próximo arranque real de
Windows, y la escucha del usuario sigue pendiente como antes.

## Corrección permanente de inicio automático — 23 de octubre de 2026

Implementada por petición del usuario para la siguiente entrega. Al abrir
normalmente la aplicación, conserva start_with_windows y repara la entrada
Run si falta o apunta a otra ruta. No modifica StartupApproved (desactivación
externa de Windows). Fallos de reparación se muestran mediante startupWarning.
Guardar ajustes crea la clave si no existe y verifica el comando completo,
con ruta entre comillas y --tray. Ya no basta la mera presencia de RESONIQ.
Lógica en apps/windows_startup.hpp, integrada en gui_main.cpp. Prueba nueva
usa una clave HKCU aislada: entrada ausente, preferencia desactivada, ruta
obsoleta, reparación, comando correcto, idempotencia y eliminación.
Build Release/runtime estático build/classic-ui-release completado sin avisos;
19/19 CTest pasan, incluido contrato de audio clásico y controles GUI.
Favorito repo conserva SHA256 5119695B…C3E23E. No se cambió código de audio.
Ejecutable compilado: build/classic-ui-release/resoniq.exe. Sin generar MSI,
instalar, reiniciar Windows, commit ni push; versión todavía 0.2.15.
La aplicación instalada sigue siendo la anterior, con el registro reparado.
Validación de arranque real pendiente del próximo inicio de sesión.

## Reparación de inicio con Windows — 23 de octubre de 2026

Usuario observa que no arranca pese a tener la opción marcada. Verificado:
LocalAppData/config/settings.user.txt conserva start_with_windows 1, pero
HKCU/Software/Microsoft/Windows/CurrentVersion/Run NO contenía RESONIQ.
Restaurada entrada REG_SZ: `"C:\Program Files\RESONIQ\resoniq.exe" --tray`.
Lectura posterior exacta; exe existe; sin valor RESONIQ de bloqueo en
Explorer/StartupApproved/Run. No se conoce qué eliminó la entrada anterior.
Backup Run y preferencias en build/startup-repair-20261023-091900.
No se reinició Windows ni se simuló inicio lanzando una segunda instancia:
la aplicación instalada sigue PID 25448. Validación real al próximo inicio
de sesión pendiente. Sin cambios de código, audio, volúmenes ni perfiles.
Favorito repo SHA256 5119695BA5D42074679AB8293F12FC44047126F19275C75DAEB57CEA82C3E23E.


## Instalador regenerado y entrega Git — 15 de octubre de 2026

Petición: generar instalador, commit y push. Reconstruida la base actual 0.2.15
sin cambios de código ni versión (el código ya está en a62e492). Instalador:
`dist/RESONIQ-Setup-0.2.15-x64-VBCABLE.msi` (formato Windows Installer).
SHA256 A1A1424BE1681AB593C30898C5CA4C58252EB98A1DFE0E34873607B2D5BA9459.
Exe empaquetado SHA256
9332D01EC30C5E1D1656A091ED5997829B13AC50AD093396E3A1F10F335327D8.
Build Release/runtime estático, 18/18 CTest pasan. test_installer -Cable
-SkipAudioTest pasa: payload, condición OS, prerrequisitos, aislamiento de
controlador y GUI extraída; evidencia build/installer-test-551cf9f10f214765a5b2c84adcec5a65.
No se ejecutó prueba de reproducción ni se reinstaló la aplicación local.
Favorito repo/LocalAppData sigue SHA256 5119695B…C3E23E. Ruta local VB-CABLE
conservada. El MSI no incluye configuraciones privadas ni identificadores de
endpoint de este equipo. dist/ continúa ignorado: commit/push entregan fuentes
y documentación, el instalador se entrega por su ruta local; sin nueva release.

## Cambio solicitado a VB-CABLE — 15 de octubre de 2026

Usuario pide «cambialo»: Windows predeterminado ahora CABLE Input
{0.0.0.00000000}.{66a2c6a8-365f-40e1-b754-e389bbaa8928}, los tres roles.
Preferencia virtual_input_id actualizada al mismo endpoint. RESONIQ instalada
reiniciada --tray, PID en la carpeta indicada por build/switch-vbcable-path.txt.
Telemetría running=1, finished=0, DSP=1, RAW conservado, under=over=0.
Salida sigue Realtek, volumen observado 50 %, cable ~42.4 %; no se tocaron
volúmenes. Sesión actual conserva bass .8 / clarity .5 / dynamics .8 / spatial .2,
intensidad 1 (ajustes del usuario posteriores al cierre anterior); sesión sin
cambios de contenido. Favorito SHA256 5119695B…C3E23E intacto.
Backup config-before y enumeración endpoints-after.txt en esa carpeta.
La ventana del proceso previo no era accesible con FindWindow; reinicio por
Stop-Process, preservando archivos y parámetros concordantes con telemetría.
Sin reinstalación de drivers ni cambios de motor/igualación. Conexión verificada;
no había señal significativa reproduciéndose: escucha del usuario pendiente.

## Interfaz actual con audio restaurado — 14 de octubre de 2026, 0.2.15

ESTADO ACTUAL: 0.2.15 instalada localmente en Program Files, por petición de
incorporar estética, configuración, iconos, inicio con Windows, temas claro y
oscuro, volumen Windows, versión, Acerca de, entrada virtual y minimizado.
Ver `CLASSIC_UI_2026-10-14.md` para implementación, hashes y evidencia.
GUI moderna con MOTOR/BÚFER/BUCLE WASAPI/PERFILES de la 0.2.0 archivada.
No portar ajustes posteriores de sonido ni compensación/piloto del cable.
Contrato de audio en tests protege la base; render favorito bit a bit contra
compilación del archivo. 18/18 CTest, capturas ambos temas y MSI extraído pasan.
MSI instalación salida 0; 15 archivos Program Files iguales al payload;
registro y exe 0.2.15. Sin cambios de controlador/certificados.

Proceso actual: PID en `build/install-0.2.15-process.pid`, aplicación instalada,
arranque oculto en bandeja verificado. Inicio Windows y minimizado activos;
tema sistema, cierre a bandeja, reconexión y DSP como última vez conservados.
Windows: transporte propio predeterminado 20 %, salida física Realtek 24 %.
DSP=1, RAW=1, Laptop Impact+, controles .95/.9/.85/.3/intensidad 1.
Al salir del GUI antiguo, sesión guardó CABLE como salida; se detectó ausencia
de salida física en la nueva GUI, se recuperó sesión EXACTA del backup y se
reinició solo proceso nuevo. Estado final running=1, finished=0, under=over=0,
sin señal reproduciéndose; volumen físico .24. Favorito repo/LocalAppData
5119695B…C3E23E y sesión repo/LocalAppData 5B437712…EC90 intactos.
Backup instalado anterior: build/classic-ui-install-backup-20261014-233637.
Cambios workspace previos: build/before-classic-ui-20261014-232619.

CORRECCIÓN DEL USUARIO: «ahora sí funciona» era para otra conversación;
todavía no había probado la restauración. NO registrar aprobación auditiva
de la 0.2.0 recuperada ni de esta 0.2.15. Se conserva la escucha pendiente.
Cierre solicitado por el usuario: commit y push del código 0.2.15 a main,
memoria guardada y resumen en `CLAUDE_HANDOFF_2026-10-14.md`.
La última release/descarga de GitHub sigue siendo 0.2.13; el push de código
no publica un MSI ni una release 0.2.15. Los estados inferiores son históricos.

## Diagnóstico de distorsión por el cable — 14 de octubre de 2026 (noche)

RETORNO solicitado por el usuario («vuelve entonces»): instalación habitual
restaurada a 0.2.0, desde archives/own-driver-20261012-234107-2b8044,
el cierre anterior a VB-CABLE donde STABILIZATION.md registra sonido aprobado.
97 archivos del archivo verificados contra manifiesto; GUI archivado pasa
autotest aislado. Se retiró MSI 0.2.14 (salida 0) e instaló MSI archivado
0.2.0 Development, SOLO ADDLOCAL=Application (salida 0); ningún driver ni
certificado se reinstaló. Preflight dispositivo antes/después idéntico.
Ejecutable ACTUAL Program Files SHA256
7F299A4FCCE4A3B8F0FFCC6E9C19ED4536213E5A6890ADF1E2B96B24D15F4E9C,
igual al archivo. Registro: RESONIQ - controlador de desarrollo 0.2.0.
Sesión LocalAppData restaurada EXACTA al hash del cierre aprobado
5B437712…EC90, favorito exacto 5119695B…C3E23E; DSP ACTIVO, RAW=1,
Impact+, controles .95/.9/.85/.3. No se reactivó igualación de volumen.
Windows usa transporte propio al 20 %, para recuperar nivel moderado de
entrada (~0.06 documentado en aprobación). El nivel físico histórico NO se
guardó: se conserva el elegido por usuario (22 % al empezar, 24 % al verificar),
sin afirmar reconstrucción exacta de volumen acústico. Favorito repo intacto.
Proceso actual Program Files, PID en build/restore-approved-20261012/process.pid.
0.2.14/config previa respaldados en esa carpeta; MSI 0.2.14 conservado en dist.
No borrados/reset de cambios de código del workspace ni cambios GitHub.
Última lectura: host activo, DSP=1, sin cortes y sin señal reproduciéndose.
Pendiente escucha del usuario; NO afirmar recuperado el punto auditivo exacto.
El MSI antiguo tiene la limitación histórica de condición OS en mantenimiento:
al instalar se pasó WINDOWSBUILD real del registro; no rehacer ni improvisar
el controlador. Este estado sustituye las notas inferiores de 0.2.14 instalada.

ÚLTIMA VALORACIÓN tras instalar 0.2.14: usuario sigue prefiriendo Realtek
directo; dice que se perdió el punto conseguido antes de buscar mejoras y
considera dejar el proyecto. La queja NO está resuelta; la escucha favorable
de la copia de prueba inferior fue provisional y no cierra el resultado actual.
Comprobación solo lectura: sigue enabled=0 (Original, dejado así para diagnóstico),
Realtek ahora 22 %, sin señal en ese instante. Favorito repo conserva SHA256
5119695B…C3E23E. No se cambió audio/perfil/volumen en esta respuesta.
No más ajustes especulativos: si el usuario retoma, reconstruir la versión y
condiciones exactas de la escucha aprobada, no suponer que conservar solo el
perfil equivale a recuperar ese sonido. No hay petición explícita de desinstalar.

INSTALACIÓN HABITUAL ACTUALIZADA por petición expresa del usuario: 0.2.14.
MSI `dist/RESONIQ-Setup-0.2.14-x64-VBCABLE.msi`, SHA256
`BCE968D2E737FA12E556946AF928DE5D1F06AAE0B472F400CAE373E6932BC338`.
Build estático en route-minimal-release (MSVC portable), 18/18 CTest y
test_installer -Cable -SkipAudioTest pasan. Instalación msiexec con elevación,
salida 0, registro de desinstalación y recurso de exe 0.2.14. Los 16 archivos
de Program Files coinciden con payload verificado. Proceso ACTUAL desde
`C:/Program Files/RESONIQ/resoniq.exe` (PID en build/install-0.2.14-process.pid).
GUI/copia de prueba y CLI detenidos. Configuración LocalAppData actual:
misma configuración de sonido aprobada, enabled=0, RAW=1, virtual_input_id
del transporte propio. Preferencias habituales de inicio/tray preservadas.
Windows: transporte propio predeterminado al 100 %, Realtek 28 %; VB-CABLE
permanece instalado sin reinstalar ni retirar su soporte.
Favorito del repo SHA256 5119695B…C3E23E intacto. Configuración habitual previa
y ejecutable 0.2.13 respaldados: ruta en build/install-0.2.14-backup-path.txt.
Última comprobación: aplicación conectada en Original, sin señal reproduciéndose;
sin reducción ni realces. El audio del mismo binario de prueba ya fue aprobado,
pero no se reprodujo nuevo audio durante la verificación postinstalación.
La 0.2.14 todavía SIN commit, push ni publicación; GitHub continúa en 0.2.13.
Este estado sustituye las notas inferiores de «sin instalar» y procesos de prueba.

VALIDACIÓN AUDITIVA FINAL: usuario confirma del Original corregido «es limpio
y suena prácticamente igual a la salida directa por Realtek». Es el GUI de
prueba con transporte propio, RAW=1, DSP apagado, virtual fijado 100 % y
Realtek 28 %. Queda validado por escucha este bypass en este equipo/programa.
No extrapolar al DSP activo ni dar VB-CABLE por probado con la corrección.
Sesión de prueba sigue en build/bypass-fixed-gui, sin instalar/publicar.
Última telemetría: entrada=salida, reducción=0, realces=0; under=2 acumulado,
over=0. Favorito mantiene su SHA256 original. 18/18 pruebas y render favorito
activo bit a bit documentados debajo. Próxima entrega: empaquetar este arreglo
cuando se solicite; la instalación habitual 0.2.13 todavía no lo contiene.

ESTADO MÁS RECIENTE: en comparación con mismo transporte/volúmenes, usuario
dice que Original habitual suena menos limpio que la ruta mínima. Se corrigió
Engine: el limitador de -1 dBFS estaba también después de la mezcla seca y
atenuaba Original. Ahora se cruza suavemente hacia dry DESPUÉS del limitador,
se alcanzan extremos exactos y en bypass asentado la salida es dry, reducción
y realces de medidores cero. Se conserva comparación normalizada solo interna.
18/18 CTest (nuevo tests/bypass.cpp: muestra exacta, picos altos, estéreo,
in-place, particiones, transiciones, reactivación del limitador y no finitos).
Render sintético del favorito ACTIVO antes/después idéntico bit a bit:
SHA256 2CF7A277…A213D. Favorito archivo 5119695B…C3E23E intacto.
Usuario oye el Original corregido «más limpio y más bajo»; el volumen virtual
había cambiado durante la escucha a 52 % (luego 62 %), Realtek seguía 28 %.
Se volvió a fijar SOLO virtual 100 % para comparar, Realtek 28 %. La escucha
posterior confirma limpieza y práctica igualdad con directo, ver arriba.
Proceso ACTUAL: `build/bypass-fixed-gui/resoniq.exe`, configuración aislada
junto a ese exe: perfil original copiado, enabled=0, RAW=1, virtual propio.
Telemetría `build/bypass-fixed-gui/build/gui-telemetry.log`: input=output ~1.0,
reducción/realces 0, sin cortes. PID guardado en esa carpeta. CLI detenido.
La primera apertura del GUI desde build/route-minimal-release ignoró --data-dir
por no llevar installed.marker y leyó config del repo; detectado y detenido,
sin guardar. Corregido copiando exe + profiles + config a carpeta aislada.
La instalación de Program Files sigue 0.2.13 anterior, SIN este arreglo.
No MSI nuevo, commit, push ni release. Escucha del bypass confirmada;
no cerrar la queja del DSP activo ni extrapolar resultado a VB-CABLE todavía.

Última escucha: usuario confirma audio y dice «suena prácticamente igual
realtek o dsp propio» CON RUTA MÍNIMA, virtual 100 % y Realtek 28 %.
No atribuir mejora solo a quitar Engine: también se eliminó la atenuación
virtual. Para aislarlo se cambia al «Original» habitual, mismo CLI/perfil/RAW,
transporte y volúmenes, pero sin --transport-only. Host actual en
`build/route-original-check/host.pid`, telemetría en esa carpeta. Ruta mínima
detenida. Pendiente valorar si Original vuelve a sonar menos limpio o solo
cambia nivel. Favorito conservado; no cambios instalados ni publicados.

Actualización de escucha: usuario respondió «No se oye audio» a la ruta mínima.
Primera lectura posterior: todos los endpoints sin señal y host vivo sin error.
Segunda lectura: música entra y sale por Realtek; `transport_only=1`, `enabled=0`,
reducción cero y input=output, picos ~1.01–1.05 (sin limitador). Realtek muestra
picos ~0.87–0.98; volumen 28 %, transporte propio predeterminado al 100 %.
No se cambió configuración para recuperar señal. Dos underruns acumulados
en pausas/reanudaciones; estables durante la reproducción actual. Esto verifica
funcionamiento digital, no que el usuario oiga audio ni limpieza. Pendiente
confirmación de que oye la prueba; no considerar el silencio una comparación.

ESTADO ACTUAL: usuario también oye peor y más bajo el transporte propio frente
a Realtek directo. Autoriza probar ruta mínima sin motor ni limitador. Se añadió
`dps_live --transport-only` (solo diagnóstico CLI; GUI conserva comportamiento).
Compilación nueva `build/route-minimal-release` porque local-release conserva
rutas de otro equipo; sin avisos, 17/17 CTest y diff revisado.
Proceso activo: dps_live de esa carpeta, PID en `build/route-minimal-check/host.pid`,
`--dps --transport-only --raw --profile restore-original.dps`, telemetría en esa
carpeta. RESONIQ GUI instalada detenida para no duplicar reproducción.
Windows: transporte propio predeterminado, volumen virtual 100 % (antes 40 %),
Realtek conserva 28 % (lo eligió el usuario antes de la prueba). Así se elimina
la atenuación extra del virtual para comparar con directo. Sin motor/limitador,
se conservan FIFO/adaptación de relojes, volumen/mute y rampa de arranque.
Favorito intacto. Inicialmente no entra señal: pendiente reproducción y escucha.
Para volver a directo: seleccionar Realtek en Windows y detener ese dps_live.
Para volver a GUI: detener dps_live y arrancar instalación con restore-original.
Restaurar volumen del transporte a 40 % antes de volver al DSP habitual si
se quiere recuperar el estado previo. No publicar: cambios de diagnóstico locales.

Último estado: usuario autoriza probar el transporte propio antiguo. Windows
usa ahora «DPS Audio Virtual Transport» como salida predeterminada, y RESONIQ
se ejecuta con `--data-dir build/own-transport-check`, DSP apagado y RAW=1,
salida Realtek. Música pasó por esa ruta (input/output ~0.061 con volumen
virtual 20 %). Se subió SOLO el transporte propio a 41 % para acercarlo al
nivel previo del cable al 42 % (~0.218); no se confirmó igualdad porque la
reproducción se detuvo. Realtek permanece al 50 %. VB-CABLE instalado,
configuración habitual sigue apuntándolo, favorito intacto. Sin retirada de
soporte en código: prueba reversible. Pendiente escucha con música y nivel
comparable. Una falta de datos al cesar reproducción dejó `under=1`.

Actualización: repetida la prueba con efectos Windows permitidos, el usuario
dice «yo lo oigo igual». No mejora por escucha. Se restauró RAW=1 y DSP apagado
en la ejecución habitual usando `--profile` a `restore-original.dps` (copia
del perfil original con enabled=0); favorito intacto. La causa sigue abierta.

Usuario confirma distorsión también en «Original», pero Realtek directo limpio.
Sesión actual comprobada: Lenovo 83LU / Realtek, instalada 0.2.13 (NO Conexant).
Ver docs/ROUTE_DIAGNOSTIC_2026-10-14.md para resultados y límites. DSP apagado,
loopbacks de cable y Realtek correlacionan >0.999, ganancia prácticamente 1,
sin muestras perdidas al capturar con búfer amplio. Tonos de 250/1000/4000 Hz
no muestran gran distorsión añadida en la captura digital. Esto NO localiza
ni descarta la distorsión que oye el usuario; no se midió acústicamente.
Prueba temporal aislada con raw=0 y DSP apagado: digitalmente parecida;
valoración del usuario tras repetir: suena igual. Se restauró la ejecución habitual, configuración
original y favorito intactos. Sesiones emisoras silenciadas durante los tonos
y restauradas; no se cambió volumen de endpoints ni se guardó audio.
El cable pasó de 72 % a 22 % durante la sesión; no fue un cambio de Codex.
No hay corrección de código, commit, push ni release en este diagnóstico.
Sigue pendiente localizar la alteración con comparación directa controlada
por nivel/programa y, si procede, comprobar la salida física. No atribuirla
al perfil, al cable o a efectos Windows como causa ya demostrada.

## Volumen físico visible en la ventana — 14 de octubre de 2026

Punto de partida: 0.2.11 ya publicada (etiqueta `v0.2.11`, `origin/main` al día,
árbol limpio). Lo de esta sesión es trabajo nuevo encima de esa base, aún sin
nueva versión, sin MSI nuevo y sin instalar.

Equipo de esta sesión: no es el Lenovo. Tiene `C:/Program Files/CONEXANT`,
salida física «Altavoz (Conexant ISST Audio)» y VB-CABLE instalado. Es decir,
es el equipo del caso pendiente «el cable ignora el volumen», que sigue SIN
comprobarse: en esta sesión no se reprodujo audio ni se movió ningún volumen.
Compilación con `build/tools/portable-env.ps1` y `build/local-release`
(MSVC portable 14.44 + CMake 4.4.4); no hay Visual Studio instalado aquí.

Cambio: la ventana muestra el volumen que Windows aplica a la salida física
elegida, a la derecha sobre la lista de salida, y lo marca en color cálido si
está silenciada. Resuelve el pendiente «volumen de altavoces visible en la
ventana». Es solo lectura: no iguala ni cambia volúmenes.
- `include/dps/endpoint_level.hpp`: `EndpointLevel` y `volumeCaption`, que
  muestra el mismo porcentaje entero que el deslizador de Windows y dice
  «sin dato» cuando el endpoint no respondió, en vez de suponer cero.
- `dps::endpointLevel(id)` en `src/audio/wasapi.cpp`: lectura directa por COM
  desde el hilo de control, sin tocar el hilo de audio.
- GUI: lee al refrescar y una vez por segundo; registra `output_volume` y
  `output_muted` en la telemetría.
- `dps_live --status` imprime la lectura directa junto a la de la enumeración.

Verificación en este equipo: `--status` da 100 % en el altavoz Conexant y 10 %
en el cable, y la lectura directa coincide con la enumeración. La captura del
autotest muestra «Volumen de Windows 100 %» junto a «Altavoz (Conexant ISST
Audio)», sin solaparse con «Entrada virtual». Build sin avisos y 17/17 CTest
pasan. No se ejecutó MSI ni instalador, ni se tocaron perfiles, sesión ni
favorito. README corregido: el instalador actual es el 0.2.11, no el 0.2.9.

Segundo cambio de la sesión: perfil aprobado en el primer arranque. Una
instalación nueva, sin sesión ni ajuste guardados, en un modelo con perfil
aprobado por escucha (hoy solo el Lenovo 83LU, `Basis::Tuned`) carga entero
`profiles/laptop-impact-plus.dps`, que es el favorito salvo nombre y
`output_id`. Antes solo se aplicaba la corrección de tono y los controles
seguían siendo los del preset (0.8/0.5/0.8/0.2), que nunca fueron aprobados.
- `dps::approvedProfileFile` (hardware.hpp) relaciona nombre de dispositivo y
  archivo distribuido; «Measured» y los nombres desconocidos dan vacío.
- `dps::adoptDistributed` (profile.hpp) fija la regla de mezcla: el sonido es
  el del archivo; la salida elegida aquí y el estado inicial del DSP, no.
- En equipos inferidos o genéricos NO se adopta nada; se conserva la
  corrección detectada. Tampoco cambia nada tras el primer arranque.
Pruebas: reglas de mezcla en `tests/profiles.cpp`, tabla y nombres en
`tests/hardware.cpp`, y en el autotest de la ventana que los seis perfiles
distribuidos existen junto a la aplicación y que un equipo no ajustado no
adopta ninguno. 17/17 CTest. LÍMITE: la rama que sí adopta no se pudo ejecutar
aquí porque este equipo no es el modelo conocido; falta comprobarla en el
Lenovo con una instalación nueva (o con LocalAppData\RESONIQ\config vacío).

Tercer paso, a petición del usuario: versiones 0.2.12 y 0.2.13 con MSI.
`dist/RESONIQ-Setup-0.2.13-x64-VBCABLE.msi`, SHA256
`2AC818FCC4BA7EEC8C3270AB6370BDB07EBAB878D1531526A3EC026BDB39F325`.
La 0.2.12 (SHA256 `CEB5A6F8...6B6790`) se retiró de `dist/`: nunca se publicó
y la sustituye la 0.2.13 con el arreglo del aviso. `AllowSameVersionUpgrades`
está activo, pero se subió la versión para no tener dos binarios distintos con
el mismo número durante el diagnóstico. Compilado con runtime estático en
`build/installer-portable` (NMake + MSVC portable), porque la caché de
`build/installer-release` es de Visual Studio 18 y aquí no hay VS instalado.
`test_installer.ps1 -Cable -SkipAudioTest` pasa: incluye el autotest sobre la
distribución extraída, de modo que el MSI lleva de verdad los seis perfiles
que el primer arranque necesita. 17/17 CTest con la versión subida.
En el momento de empaquetarla no estaba publicada ni instalada aquí; se
publicó después, ver el quinto paso. El usuario autorizó sobrescribir lo que
tenga en el Lenovo al instalar esta versión.

Cuarto paso: el usuario instaló la 0.2.12 en el Lenovo y NO vio el mensaje
«Primer arranque: perfil aprobado…». Causa encontrada en el código: 200 ms
después de abrir la ventana, el temporizador 4 llama a `ensureAudio()`, que
reescribe la línea de estado. El mensaje existía pero duraba un repintado.
Esto NO demuestra que la adopción fallara; solo que el aviso era invisible.
Arreglo en 0.2.13: el aviso va por `startupWarning`, que `ensureAudio()`
vuelve a poner tras conectar, y `applyApprovedProfile` registra SIEMPRE su
resultado en la telemetría, también cuando no adopta:
`first_run detected=<perfil> tuned=<0|1> file=<archivo> present=<0|1>` y,
si adopta, `first_run_profile=... name=... device=... bass=...`.
La telemetría se abre ahora antes de decidir el primer arranque.
Comprobado en este equipo con copia aislada (`--data-dir` a carpeta vacía y
entrada virtual inexistente para no abrir audio): sale
`first_run detected=Laptop Focus tuned=0 file=laptop-focus.dps present=1`,
no se adopta nada y `output_volume=1` confirma la lectura del altavoz.
La rama que SÍ adopta sigue sin ejecutarse: requiere el Lenovo.
Diagnóstico pendiente del usuario: en el `build/gui-telemetry.log` de su
LocalAppData, los campos `device=` y `bass=` dicen si la adopción ocurrió
aunque el mensaje no se viera (0.95 = perfil aprobado, 0.8 = preset).

Quinto paso, a petición expresa del usuario: PUBLICACIÓN de la 0.2.13.
Etiqueta `v0.2.13` y release de GitHub con el MSI adjunto; enlace de descarga
del README actualizado a la v0.2.13. Sustituye a cualquier nota anterior de
esta sesión que diga «sin publicar»: la versión distribuida pasa a ser 0.2.13.

Siguen pendientes tras publicar: comprobar en el Lenovo la adopción del perfil
aprobado al primer arranque (la rama `tuned=1` no se ha ejecutado en ningún
equipo, tampoco antes de publicar), VolCtl del cable, «Medir» sobre el perfil,
el caso Conexant con audio real y la escucha del usuario. Publicar no
demuestra mejora acústica ni cierra la queja de sonido.

## Preparación de publicación 0.2.11 — 14 de octubre de 2026

El usuario autoriza explícitamente commit, push y release de los cambios
0.2.10–0.2.11. Los estados «sin publicar» inferiores describen sus respectivas
sesiones históricas. Se actualiza el enlace de descarga a v0.2.11.
Comparación SHA256: los 16 archivos instalados en `C:/Program Files/RESONIQ`
son idénticos al MSI local `dist/RESONIQ-Setup-0.2.11-x64-VBCABLE.msi`,
incluidos ejecutable, perfiles y asistente. El proceso abierto usa esa instalación.
No se reinstala ni se altera audio, sesión o favorito durante la publicación.
La queja de sonido y las comprobaciones pendientes siguen abiertas; publicar
no demuestra una mejora auditiva ni valida el caso Conexant que ignora volumen.
Validación para esta publicación: build completo en `build/local-release`,
17/17 CTest y `test_installer.ps1 -Cable -SkipAudioTest` pasan.
Favorito del repositorio conserva SHA256 `5119695B...C3E23E`.
MSI SHA256: `6CAABE17A1C68A88FEDC2EB5A7D8A8975C47CF5EF9FA94BDEF9525600206E62A`.

## Cierre solicitado: sonido percibido y favorito — 14 de octubre de 2026

Usuario pide guardar el estado. Refiere que con VB-CABLE/RESONIQ el sonido
parece «en una caja», sube de volumen y parece distorsionar; Realtek directo
suena más limpio y hace dos días todo sonaba mejor. No dar por resuelta esta
queja por pasar pruebas ni atribuirla al cable sin comprobar el caso actual.

Lectura de instalación: `C:/Program Files/RESONIQ/resoniq.exe` versión 0.2.11.
Esto sustituye el «sin instalar» de los puntos anteriores respecto al estado
observado actual; Codex no ejecutó la instalación en esta conversación.

Durante el diagnóstico, la sesión LocalAppData se llamaba «Portatil - suave»
pero contenía EQ/densidad de Impact+ y `agc_range_db 0`; posteriormente tenía
controles Max (.8/.5/.8/.2). Era un ajuste mixto, distinto del favorito, no
el perfil distribuido `profiles/laptop-soft.dps`. El nombre del perfil no
garantiza que sus parámetros originales sigan intactos tras editarlo.
Con DSP activo, últimas muestras consultadas: aumento del pico 6–12 dB,
reducción dinámica hasta unos 6 dB y una salida exactamente en el techo
0.891251. Son picos/medidores de bloques, no RMS ni prueba de distorsión
acústica. En bypass, entrada/salida casi iguales; `cable_volume=2` en ambos.
La medida previa entre transportes (<=0.4 dB por banda) reduce la sospecha
sobre VB-CABLE, pero no descarta una regresión en las condiciones actuales.

Se explicó «Original»: DSP desactivado manteniendo cable/host/altavoces,
control de volumen y puerta de silencio. No equivale a Realtek directo.
Se indicó dónde recuperar «Ajuste por defecto» y dónde viven sus archivos.
Codex NO cargó ni sobrescribió perfiles, ni cambió audio/volumen.

AL GUARDAR ESTE CIERRE, una nueva lectura muestra sesión LocalAppData idéntica
al favorito LocalAppData: nombre «Referencia - Portatil impacto», Impact+,
controles .95/.9/.85/.3, AGC 60/900 ms, sin `agc_range_db` (6 histórico).
Ambos hash SHA256:
`5B437712D8E450CAE99F6BB037E8A68388F37F34B4C0012FBEFDFF3AA513EC90`.
Esto indica igualdad de los archivos guardados; no demuestra por sí solo el
perfil activo en RAM ni que el usuario haya aprobado nuevamente el sonido.
La copia original `config/favorite.user.dps` sigue siendo otra referencia:
`5119695BA5D42074679AB8293F12FC44047126F19275C75DAEB57CEA82C3E23E`.
No igualar estos favoritos automáticamente ni sustituir una sesión distinta.

Pendiente: conocer la escucha tras recuperar el favorito y, si persiste la
queja, comparar VB-CABLE/driver propio con el MISMO perfil y volúmenes
controlados, conservando la configuración. No se hizo esa comparación nueva.
No micrófono ni audio guardado en este diagnóstico. Conservar favorito,
Gain retirado y comparación visible DSP activado/desactivado sin compensación.
Estado de código/MSI: cambios locales sin nuevo commit, push ni publicación
por Codex. Auditoría y correcciones posteriores de Claude descritas abajo.

## Punto de continuidad — 14 de octubre de 2026

Estado: 0.2.11 local, SIN commit, push ni release; MSI
dist/RESONIQ-Setup-0.2.11-x64-VBCABLE.msi reconstruido con todo lo siguiente.
Codex auditó 0.2.10–0.2.11 (docs/AUDIT_2026-10-14.md) y corrigió: volumen
seguido durante el piloto, sin caché de resultado entre sesiones, medidas
anuladas por mute/volumen/cortes, historia FIFO al llenarse, StartupGate.
Claude verificó en VB-CABLE real (Lenovo) y corrigió dos regresiones: margen
de «el cable atenúa» de -5/+3 dB (escalón de 8 %: -42.1 frente a -38.08) y la
marca DATA_DISCONTINUITY del primer paquete tras Start ya no anula la medida.
Resultado real: 8–50 % decididos al primer intento, una sola atenuación;
agudos planos (0.0 dB hasta 14 kHz) con procesado apagado. 17 CTest y
test_installer -Cable -SkipAudioTest pasan. Detalle al final del informe.

Siguiente: el usuario instala 0.2.11 y escucha «Portátil - suave» frente a
directo (y Bongiovi si lo reinstala), a igual volumen. Pendientes: volumen de
altavoces visible en la ventana, favorito al primer arranque, VolCtl del
cable, Medir sobre el perfil, caso «cable ignora volumen» en el Conexant.

## Revisión de cambios posteriores de Claude — 14 de octubre de 2026

Leer la última sección de `docs/AUDIT_2026-10-14.md`. Claude corrigió dos
problemas descubiertos al comprobar el Lenovo: margen del piloto insuficiente
al 8 % y marca inicial DATA_DISCONTINUITY que anulaba el primer intento.
Código actual: atenuación entre -5/+3 dB respecto a lo pedido, límites
excluidos; volumen ignorado a menos de 3 dB de 0. La marca del primer paquete
no invalida el piloto; las posteriores sí, y todas conservan su contador.

Claude comunicó detección al primer intento al 8–50 %, arranque/cambio de volumen
y cadena tonal hasta 14 kHz; 17 CTest y MSI extraído pasan en su sesión.
Codex revisó código/documentación: build actual y dos pruebas relevantes pasan,
sin repetir audio real ni MSI. Informe y VB-CABLE_VOLUME sincronizados.
Conexant y la banda 16–20 kHz en la cadena real siguen pendientes. No cambios
de código, favorito, sesión ni publicación en esta revisión documental.

## Auditoría y refactorización de Codex — 14 de octubre de 2026

Informe: `docs/AUDIT_2026-10-14.md`. Sigue siendo 0.2.11 local sin commit,
push ni publicación. MSI reconstruido, sin instalarlo; copia anterior en
`build/audit-20261014/RESONIQ-Setup-0.2.11-before-audit.msi`.

Corregidos: seguimiento del volumen durante el piloto; caché global de modo
del cable retirada (se mide por sesión); clasificación ambigua; invalidación
por cambios observados de volumen/mute y discontinuidad; historia FIFO
sobrescrita al llenarse; pico final de rampa. Refactorización: StartupGate
independiente de COM y publicación atómica conjunta del nivel/ventana del piloto.

Build sin avisos, 17 CTest pasan, dos defectos reproducidos antes/después,
cierre legacy/Restart Manager con copias aisladas pasa. MSI extraído pasa con
`-Cable -SkipAudioTest`. Favorito y sesión conservados en ambas ubicaciones.
No micrófono, cambios de volumen del equipo, instalación ni audio real nuevo.

Cada sesión empieza conservadora: puede haber una pausa breve al reconectar.
Si el modo sigue desconocido, un cable que ya atenúa puede permanecer más bajo;
ver límites del informe. Falta probar ruta real corregida/Conexant y escuchar.
DSP y perfiles sin cambios de algoritmo/valores durante esta auditoría.
Gain retirado; comparación visible DSP activado/desactivado sin compensación.
NO recuperar igualación de volumen: la propuesta de la sección histórica
inferior está sustituida por `AGENTS.md`. Perfil suave no aprobado.

## Comparación real con Bongiovi y versión 0.2.11 local — 13 de octubre de 2026 (noche)

Estado: árbol de trabajo SIN commit ni push; MSI local
dist/RESONIQ-Setup-0.2.11-x64-VBCABLE.msi. No publicar sin petición expresa.

Tras instalar la 0.2.10 el usuario dijo que sonaba mejor con RESONIQ apagado,
luego que los altavoces Realtek directos sonaban mejor, y que «ayer era más
limpio». Hipótesis DESCARTADAS con medida, no repetirlas:
- VB-CABLE frente al dispositivo propio: mismo volumen y perfil, diferencia
  <=0.4 dB en todas las bandas. El transporte no es la causa.
- Nahimic/efectos de Windows: el usuario tiene Nahimic desactivado; por
  loopback, vía normal y RAW son idénticas (0.0 dB por banda). Desmarcar
  «Omitir efectos adicionales» apenas cambia nada al oído.
Causas reales encontradas:
- Volumen de los altavoces Realtek, invisible para la aplicación: pasó de
  100 % a 20 % al usar el usuario la salida directa; luego 100 % saturaba.
  Con el cable como salida, las teclas solo mueven el cable.
- El perfil aprobado se juzgó encendido/apagado sin igualar volumen: procesado
  sonaba 11–13 dB más fuerte que apagado. A igual volumen el usuario prefirió
  el sonido directo. Favorito: 63 Hz -12..-25 dB, 125 Hz +12..-10 dB según
  nivel, medios/presencia +7..+20 dB, carácter muy dependiente del volumen.
- El programa instalado no carga el favorito aprobado al primer arranque:
  usa los controles del preset (0.8/0.5/0.8/0.2, AGC 100/1500).

Bongiovi DPS 2.2.1.1 instalado por el usuario en el Lenovo (dispositivo
«Digital Power Station») y MEDIDO con señales mono, altavoces silenciados:
respuesta casi plana (63–250 Hz -0.6..-0.8 dB, 2–14 kHz -2.0 dB), nivel
-2.6 dB, cresta +3 dB, idéntico al 100 % y al 30 % (lineal, independiente del
volumen); -3.2 dB con música a escala completa. El usuario: «con Bongiovi la
mejora es menor que con Resoniq». Reserva: no se ven efectos estéreo, y es el
perfil que tuviera seleccionado.

Cambios 0.2.11:
1. FIFO adaptativa (stream_buffer.hpp): la interpolación lineal perdía 0.8 dB
   a 8 kHz, 1.8 a 12 kHz y 3 a 16 kHz, y esa pérdida oscilaba varios dB con la
   deriva de reloj. Ahora sinc enventanado de 16 taps y 256 fases: 0.00 dB
   hasta 16 kHz, -0.05 a 18 kHz, -0.5 a 20 kHz, sin oscilación. Fase entera
   sigue siendo paso exacto. Prueba en tests/tests.cpp. Solo medido fuera de
   línea; falta comprobarlo en la cadena real.
2. Parámetro opcional de perfil `agc_range_db` (0..6; ausente = 6 histórico,
   literales 0.5/2 intactos, renders anteriores exactos).
3. profiles/laptop-soft.dps «Portátil - suave»: corte 70 Hz, cuerpo +4 dB a
   160 Hz, presencia +4 dB a 3.2 kHz, densidades 1, agc_range_db 0. Medido:
   nivel +0.0..+0.2 dB respecto a su propia entrada con picos 0.06–0.25,
   bandas entre -1.5 y +2.5 dB, independiente del nivel; -4 dB a escala
   completa por el limitador. Es un PUNTO DE PARTIDA para que el usuario
   compare a igual volumen (botón DSP) contra directo y Bongiovi; no está
   aprobado ni sustituye al favorito.

Pendiente: escucha del usuario; volumen de altavoces físicos visible/avisado
en la ventana; arrancar instalaciones nuevas con el favorito; decidir si se
recupera la igualación de volumen como modo de ajuste; VolCtl del cable.

## Auditoría de Claude y versión 0.2.10 local — 13 de octubre de 2026

Usuario pide a Claude auditar 0.2.5–0.2.9 y después corregir. Estado: cambios
en el árbol de trabajo, SIN commit, SIN push, SIN release. MSI local
dist/RESONIQ-Setup-0.2.10-x64-VBCABLE.msi. No publicar sin petición expresa.
Este equipo es el Lenovo 83LU/Realtek (carpeta Documentos/Desarrollo); el
Conexant usa «Desarrollo 1». Lo «instalado» en un traspaso es del equipo donde
se escribió.

Queja del usuario tras instalar 0.2.9 en el Lenovo: suena mejor con RESONIQ
desactivado. Tenía Gain +12 y lo fijó como ajuste por defecto de este equipo
(LocalAppData, 15:39). No se ha tocado: es suyo. La 0.2.10 lo carga sin Gain.

Fallos confirmados con medida y corregidos en 0.2.10:
1. Volumen doble con VB-CABLE cuando el cable ya atenúa su loopback (Lenovo).
   La 0.2.6 lo supuso siempre sin atenuar. Ahora se mide con piloto y el
   volumen actúa una vez, ANTES del DSP, como en el dispositivo propio con el
   que se aprobó el favorito. Detalle y cifras: docs/VB-CABLE_VOLUME.md.
   src/audio/cable_probe.hpp, endpoint_gain.hpp (antes output_volume.hpp).
2. Prepare-VBCABLE.ps1 no detectaba un cable ya instalado en Windows
   PowerShell 5.1 ((función).Count vacío con un solo CimInstance) y abría el
   instalador de VB-Audio. Corregido con @(); test_vbcable_prepare lo cubre.
   La casilla de preparar VB-CABLE ya no sale marcada al actualizar.

3. Gain RETIRADO a petición del usuario («elimina entonces el gain»): con
   Impact+ la salida ya está en el techo; +3 dB subía 0.5–1.3 dB el nivel medio
   y reducía el contraste de golpes (señal sintética). Sin control, parámetro
   ni telemetría. `gain_db` de perfiles 0.2.9 se lee y se descarta (cargan a
   unidad, no se reescribe); así el ajuste por defecto del usuario con +12 se
   carga sin Gain y sin modificar su archivo. No reintroducirlo sin pedirlo.

Medido, no corregido (decisión del usuario pendiente):
- Medir sustituye la EQ aprobada (cuerpo 11 -> 6.28, presencia 10 -> 0 en el
  Conexant); conserva densidad/armónicos. Posible mejora: corregir sobre el
  perfil en vez de reemplazarlo.
- resoniq_shutdown_tests no está en CTest. Sospecha sin comprobar: Finalizar
  tras «Cambiar -> Quitar» podría intentar abrir el exe eliminado.
- VolCtl=1 activo hace que el cable enmudezca bajo ~6-8 % y baje en escalones.

Validación: 17 CTest (se retira la de Gain), test_installer -Cable -SkipAudioTest y
test_vbcable_prepare pasan con build/lenovo-release (toolchain portable).
Host real con cable al 8–50 %: una sola atenuación. Pendiente: instalación y
escucha del usuario; caso «cable ignora volumen» en el Conexant;
cierre automático en una actualización real con la aplicación abierta.

## Punto de continuidad actualizado — 13 de octubre de 2026, versión 0.2.9

Usuario pide guardar memoria y resumen para Claude. Traspaso completo de los
días 12–13 en `docs/CLAUDE_HANDOFF_2026-10-13.md`; esta sección sustituye los
puntos de retomada antiguos que contradigan el estado actual.

Código publicado: 034688f; README y tag/release v0.2.9: 2c0631f.
Release pública https://github.com/valvik77/Resoniq/releases/tag/v0.2.9,
MSI RESONIQ-Setup-0.2.9-x64-VBCABLE.msi, SHA256
3EDFCB9FADC38C4B53E4C53F7CF6C745DEDD9D417860176DDA052419FFD23B8E.
Main local/remoto coinciden; memoria y traspaso de este cierre quedan locales,
sin nuevo commit/push. No publicar automáticamente en cada continuación.

Ahora se trabaja en otro portátil: Conexant ISST Audio y VB-CABLE 3.3.1.7,
no en el Lenovo Legion/Realtek original. Verificado en este cierre:
C:/Program Files/RESONIQ/resoniq.exe tiene versión 0.2.9 y está activo.
Telemetría: Laptop Impact+, Gain +3 dB, DSP/intensidad 100%, controles
Bass .8 / Clarity .5 / Dynamics .8 / Spatial .2; entrada sin señal.
No afirmar aprobación auditiva a partir de ese silencio.

0.2.6–0.2.9: banner/logo y Acerca de; versión Windows; volumen VB después
del DSP según dB del endpoint (loopback real sin atenuación con VolCtl=1);
captura RAW con fallback y selección por coherencia de canales; cierre de
actualización compatible con versiones anteriores; arranque en Finalizar;
Gain ±12 dB opcional/0 por defecto antes del limitador; Realce/Actividad DSP
antes del volumen; medición tonal conserva la densidad/adaptación/voz musical.

18 CTest pasan; MSI extraído/GUI pasan; helper de cierre probado con copias
aisladas 0.2.7/0.2.8; host VB real probado en bypass y DSP. Gain 0 conserva
renders del favorito y sesión bit a bit; favoritos no modificados por agente.
Favorito original 5119695B...C3E23E; favorito LocalAppData BB43CCE1...6282C1:
son referencias distintas, no igualarlas. Sesión tampoco debe sobrescribirse.

Usuario confirmó que la medición nueva funciona en Conexant. Último registro:
RAW=1, usable=1, margen19.42dB,2 bandas,retardo15ms,sin muestras perdidas;
propuesta167.09Hz/cuerpo6.28dB/presencia0. No nueva captura por agente en esta
continuación. Mantener consentimiento por medición y descarte de audio.

Pendiente: valorar punch/Gain/conservación de voz musical al escuchar 0.2.9;
confirmar cierre y apertura durante actualización MSI real (helper aislado no
equivale a transacción trazada); validar más hardware/transporte/latencia física.
Windows11 22H2+ x64; no prometer cualquier SO/arquitectura ni equivalencia
acústica. Mantener DSP/original sin compensación y favorito aprobado.
Guías: GAIN_AND_METERS, MEASUREMENT_PORTABILITY, INSTALLER_SHUTDOWN,
VB-CABLE_VOLUME, AUDIT y STABILIZATION en docs.

## Cierre y sincronización solicitados — 13 de octubre de 2026

Usuario pide guardar memoria, hacer commit y subir a GitHub. Corrección MSI
ya publicada en main mediante commit533553d y releasev0.2.5:
https://github.com/valvik77/Resoniq/releases/tag/v0.2.5.
Asset: RESONIQ-Setup-0.2.5-x64-VBCABLE.msi. Se conservan identidad Pulso
continuo, asistente VB-CABLE, control de volumen automático y sonido aprobado.
Pendiente al retomar: usuario instala0.2.5 y confirma reparación real; las
pruebas anteriores evaluaron condiciones con motor MSI y layout extraído,
no una transacción de reparación. No afirmar que ya se actualizó ProgramFiles.
No nuevas mediciones de micrófono autorizadas. Repositorio público main/origin
configurados; exclusiones de claves, datos personales y builds siguen vigentes.

## Reparación MSI corregida — 13 de octubre de 2026

Usuario muestra rechazo Windows11 al reparar. Host real CurrentBuildNumber26300,
compatible. MSI anterior WINDOWSBUILD no Secure: se puede perder en elevación;
condición tampoco exceptuaba mantenimiento. CableProduct y Product corregidos:
WINDOWSBUILD Secure=yes; Installed OR (requisitos iniciales). MSI vigente0.2.5.
Prueba con motor MSI real OpenPackage/EvaluateCondition, sin transacción:
Installed1+buildvacío pasa; instalación nueva build19045 rechaza;26300 acepta.
MSI extraído/scripts/GUI/WASAPI propio pasan; evidencia
build/installer-test-c87842f4aa1a408b99c4d3e17bb6a4d2. Logs locales anteriores
MSIe6ce8.LOG/MSIe3cdf.LOG solo muestran aviso, no permiten afirmar trazado
completo de propiedades. No reparación/transacción real hecha por agente.
Publicar corrección y MSI0.2.5 en GitHub; instalar nueva versión para reemplazar
condición del MSI anterior. Audio/perfil/driver conservados, app relanzada.

## Publicación GitHub solicitada — 13 de octubre de 2026

Usuario autoriza subir a valvik77/resoniq. Repositorio remoto canónico público
https://github.com/valvik77/Resoniq inicialmente vacío. Git local inicializado
en main; origin configurado. Primera publicación contiene código, documentación,
iconos y fuentes del driver propio, sin SDK/vendor/build/archives/config personal,
claves/audio ni paquetes locales. MSI0.2.4 se publica como asset de release
v0.2.4, no como binario del historial Git. Mantener estas exclusiones.

## Identidad Pulso continuo integrada — 13 de octubre de 2026

Usuario rechazó propuestas iniciales R/ondas/barras e híbridos, y eligió A
«Pulso continuo» de la última lámina: cinta azul/cian que forma una onda.
Autoriza desarrollar/integrar variantes. Geometría vectorial propia editable,
SVG/PNG16–1024/ICO16–256 en assets/brand; generador scripts/build_brand_icons.py
(Pillow). Lámina final assets/brand/resoniq-variants.png. Colores: activo azul/cian,
bypass gris, sin conexión ámbar; texto de bandeja sigue describiendo estado.
apps/resoniq.rc integra IDs1app/2activo/3bypass/4desconectado; GUI configura
clase/ventana y bandeja LR_SHARED, actualiza estado al cambiar botones.
MSI vigente dist/RESONIQ-Setup-0.2.4-x64-VBCABLE.msi incorpora icono en Inicio,
Programas/Aplicaciones instaladas, asistente VB y banners WiX propios.
Mantiene activación automática Windows Volume Control de versión0.2.3.

StaticRelease y Release habitual compilan. Cuatro recursos cargados vía Win32
en16/24/32/48/256px. MSI extraído pasa scripts/GUI/WASAPI propio; evidencia
build/installer-test-43130488ee52455085ec82fc11f3eb71. Revisión visual lámina.
Solo identidad visual: sin cambios DSP/perfil/volumen/captura/driver.
Aplicación instalada cerrada normalmente para smoke y relanzada; NO sustituida
en Program Files ni ejecutada transacción MSI. Para verla con nuevo icono,
instalar MSI0.2.4. No afirmar actualización de instalación activa ya realizada.

## Control de volumen VB automatizado — 13 de octubre de 2026

Usuario confirma que activar Windows Volume Control resuelve 0%→2% a máximo.
Pide automatizarlo. MSI actual dist/RESONIQ-Setup-0.2.3-x64-VBCABLE.msi incluye
componente x64 HKLM SOFTWARE\VB-Audio\Cable DWORD VBAudioCableWDM_VolCtl=1.
Valor observado activo localmente y nombre/ruta presentes en panel y SYS del
paquete45. Se prepara antes de instalar el driver; si ya existe, requiere reinicio
para releer configuración. Componente permanente para no desactivar el volumen
de VB al retirar RESONIQ. No toca otros ajustes ni ganancia/DSP/perfil.
MSI construido y extraído: test verifica DWORD, vista64, permanencia y feature;
también scripts empaquetados VerifyOnly y GUI/WASAPI propio pasan.
Evidencia build/installer-test-c0b2402bf0124d91bd41422e719289a6. No transacción
MSI nueva ni reinicio/instalación de driver efectuados por el agente. Ruta VB
real pendiente de pruebas ampliadas; volumen sí confirmado por usuario.
App instalada C:/Program Files/RESONIQ/resoniq.exe cerrada normalmente para
smoke y relanzada. No nuevas mediciones de micrófono.

## Incidencia volumen VB-CABLE — 13 de octubre de 2026

Usuario retoma: 0% silencia, 2% suena como 100% con VB-CABLE. Código actual
EndpointSilence solo protege cero/mute; no atenúa niveles positivos, pues espera
que Windows/driver lo haga. Manual oficial página 13 confirma que VB-CABLE
requiere activar Windows Volume Control; sin ello el deslizador no tiene efecto.
Hipótesis inicial: opción desactivada. Aún no comprobada en panel ni corregida.
Panel instalado localmente en C:/Program Files/VB/CABLE/VBCABLE_ControlPanel.exe;
app activa ahora C:/Program Files/RESONIQ/resoniq.exe. No añadir ganancia software
incondicional: con opción activa produciría doble atenuación. Primer paso de
diagnóstico: activar/verificar opción y comprobar 2/10/50% y mute. No nueva
captura, reproducción de prueba ni cambios de volumen realizados por el agente.

## Pausa solicitada por el usuario — 13 de octubre de 2026

El usuario pide guardar en memoria y seguir en otro momento. Desarrollo pausado
tras entregar dist/RESONIQ-Setup-0.2.2-x64-VBCABLE.msi con el asistente de
preparación automática de VB-CABLE. No queda ninguna operación en curso.
Al retomar: leer esta memoria y comprobar el resultado de instalar este MSI en
el otro equipo. Pendiente instalación real de VB-CABLE y validación de audio,
volumen cero/mute/primer paso, continuidad y recuperación con ese transporte.
Las pruebas actuales cubren preparación y ruta WASAPI propia, no ruta VB real.
Conservar el sonido y perfil favorito aprobados; no cambiar driver activo ni
seguridad de arranque. No hay autorización pendiente para nuevas mediciones
de micrófono. Procedimiento en docs/VB-CABLE.txt; driver propio recuperable
según docs/OWN_DRIVER_RECOVERY.md. No continuar tareas hasta que el usuario
retome el trabajo.

## Asistente VB-CABLE automatizado — 13 de octubre de 2026

MSI actual: dist/RESONIQ-Setup-0.2.2-x64-VBCABLE.msi. Sustituye la oferta de
abrir instrucciones por preparar VB-CABLE; Inicio incluye «Instalar VB-CABLE».
Prepare-VBCABLE.ps1 descarga el ZIP oficial, comprueba SHA256 fijado y firmas
de editor/catálogo/SYS, extrae con límites y abre el setup oficial mediante UAC.
Descarga en proceso oculto separado para mantener la interfaz disponible.
Detecta HWID existente y evita abrir un setup que podría ofrecer retirada.
No hay flags silenciosos documentados utilizados, controlador empaquetado,
reinicio automático ni cambios de seguridad. Quedan UAC, Install Driver y
reinicio manual. Discrepancia de licencia de integración sigue documentada.

tests/test_vbcable_prepare.ps1 pasa descarga real, rechazo ZIP alterado,
vista GUI y flujo asíncrono completo sin lanzar setup. Evidencia:
build/vbcable-prepare-test-900a2a5c2ea74bf8a73443ebb28ce0bc.
MSI reconstruido con scripts corregidos; test_installer -Cable verifica hashes
de scripts empaquetados, ejecuta VerifyOnly extraído y pasa GUI/WASAPI propio.
Evidencia: build/installer-test-c99b34d7d6194d9598f0c3fe88a3c872.
No se ha instalado VB-CABLE ni probado su ruta real. App habitual relanzada;
DSP y favorito conservados. Mantener esta distinción en respuestas al usuario.

## Compatibilidad GUI VB-CABLE implementada

Verificacion final: Release habitual tambien17/17; nuevoMSI extraido vuelve a
pasar GUI/WASAPIown; favorito/sesion/renderexactos, appactualizada activa.
Procedimiento docs/VB-CABLE.txt y accesosInicio/checkboxfinMSI. VB siguependiente
de instalar/probar en equipo remoto. No declarar transporteVBvalidado por smokeOwn.

Usuario pide continuar, autoriza "si adelante". Añadido Entrada virtual enGUI,
listaOwn/VB, persistencia virtual_input_id en prefs(no perfiles), guardadoopcional
sin cambiar serializationexistente cuando vacio. Mantiene rutaOwn por defecto
si no hay virtualpredeterminada; defaultWindows si existe; variosVB sin default
requieren elegir. FaltaIDespera no sustituye. Virtuales reconocidos fuera salidas
fisicas y microfonosmedicion. IDseleccionado conserva clasificacion tras renombre.
Clasificacion inicial por nombre/DeviceInterface metadata: no prometer topologia
completa. No se altera motorDSPni perfil favorito.
MSI actual dist/RESONIQ-Setup-0.2.1-x64-VBCABLE.msi: app, perfiles, guiaVB-CABLE.txt,
URLoficial enInicio, opcionfinalAbrirInstrucciones; SIN driverpropio, niVBbundled,
ni accionesdecrear/quitar dispositivos. VB oficialseparado por discrepancialicencia.
17CTest pasan StaticRelease; tests/test_installer.ps1 -Cable verifica contenido,
ausenciadriver/acciones, GUI y WASAPIown enlayoutinstalado condataaislada. FixtureGUI
pruebaInputpersist/missing; render visual inspeccionado, Inputchrome corregido.
NO prueba de transporte VB real: no esta instalado. GUIprincipalya soporta
seleccion una vezinstalado. Instrucciones requierenadminreiniciomanual y
comprobarLoopback/WindowsVolumeControlVB: si opcionvolumendesactivada nongains.

## Vía propia conservada; investigación VB-CABLE

Usuario pide guardar todo el driver propio e investigar VB-CABLE. Fuentes y
paquetes siguen en sitio; instantanea scripts/Archive-OwnDriver.ps1 en archives,
sin PFX/claves/audio/configpersonal. Recuperacion docs/OWN_DRIVER_RECOVERY.md.
Prioridad de distribucion pasa a investigar VB-CABLE, NO desinstalar ruta actual.
Reporte docs/VBCABLE_RESEARCH.md: paquete45 descargado/inspeccionado no ejecutado,
firmaMicrosoftCAT y signtool/kp SYS x64 + INFmembership + setupAuthenticode pasan.
Volumen VB es opcional y dos renders posibles: investigar seleccion/latencia/mute
antes de adoptar. CLI loopback-id viable; GUI aun soloDPS. Licencia web permite
bundle condicionado pero readmeZIPexigeacuerdo: primera prueba instalador separado,
aclarar antes de integrar. Usoempresa/profesional requierelicencia, no "gratis todo".
Sin instalarVB/reiniciar/alterarTestSigning/driver/volumen/favorito ni microfono.
Instantanea verificada: archives/own-driver-20261012-234107-2b8044.zip,97archivos.

## Error MSI en otro equipo - revision

Usuario muestra error generico de programa WindowsInstaller y confirma otro
equipo. No hay logs locales de ese equipo: NO afirmar causa exacta. Posible
rechazo de drivertest. MSI Development revisado: featureVirtualDevice Level0
oculta, acciones instalar/rollback SOLO DpsSigningModeproduction. Se instala
app sin intentar driver; creador separado conserva comprobacion y mensaje.
No resuelve firma general ni demuestra transaccion MSI real en equipo remoto.

## Instalador MSI y creador de dispositivo DSP

Usuario pide instalador para cualquier equipo con dispositivo virtual incluido.
Preparado dist/RESONIQ-Setup-0.2.0-x64-Development.msi (WiX6.0.2 local build/tools).
Incluye GUI, perfiles, helper nativo SetupAPI/newdev, Manage-Device.ps1, SYS/INF/CAT,
licenciaMS-PL y CER PUBLICO. Nunca PFX, claves, sesiones personales ni audio.
Windows11 22H2+ x64 (INF minimo22621), no ARM/x86/Server. StaticCRT elimina
redist/.NET destino. installed.marker redirige config a LocalAppData/RESONIQ;
--data-dir permite aislamiento de pruebas. Workspace habitual sin marker no cambia.

Driver development opcional/no seleccionado por defecto; solo TESTSIGNING ya
efectivo y SecureBootoff, sin modificar arranque. --Production exige catalogo
Microsoft y signtool/kp/membership. CuentaEV/PartnerCenter/firmaHLK externos
NO disponibles: no afirmar instalacion general segura lista. Documentado en
docs/INSTALLATION.md con fuentes Microsoft actuales. CAB de envio sin firma
dist/RESONIQ-Driver-Submission-Unsigned.cab (SYS/INF/CAT/PDB en subcarpeta)
preparado, sin compra ni envio. Requiere firma cuenta antes de portal.

16/16 CTest StaticRelease pasan. tests/test_installer.ps1 extrae MSI, verifica
payload, acciones elevadas/rollback/upgrade, firmas offline, GUI/WASAPI en
layout instalado con datos aislados, y rechazo produccion de drivertest.
Helper --check ve1 device, adminfalse, modified_systemfalse. NO instalar,
retirar ni actualizar driver actual para probar, ni modificar registro de arranque.
Creacion/remocion y MSI transaccion real siguen pendientes de VM limpia.
Favorito/sesion y render StaticCRT conservan hashes originales. App habitual activa.
Verificacion final tambien en Release habitual:16/16, MSI extraido GUI/audio
correctos; sourcefavorite/sessionSHA intactos. Produccion SignTool usa paquete
SDK CPP sin sufijo.x64 (el binario esta en c/bin/.../x64). No nuevo driver creado.

## Cierre de mejora sonora y consolidacion local

Usuario dice obtener buenos resultados y autoriza "hazlo todo lo que puedas".
Sonido aprobado queda estable: NO adoptar experimentos ni mas efectos.
Cambios/evidencia en docs/STABILIZATION.md; uso en docs/STABLE_LOCAL.md.
Salida desaparecida conserva ID y espera, nunca fallback automatico a otro
altavoz. Cargar perfil respeta salida nueva; suspend/resume libera/reabre;
render sin progreso5s termina para retry. Guardado fallido no cambia metadata.
Inicio corrupto deja copias y recupera respaldo/preferencias con aviso.
15/15 CTest, GUI selectores reales/sinteticoMeasured, WASAPI reinicio con
mensajes de energia simulados, paquete portable y corruptconfig en copia
aislada pasan; test driveroffline tambien. NO suspension/desconexion fisica
ni nueva medida de microfono. Favorito/sesion/reference hashes conservados.
Paquete dist/RESONIQ-0.2.0-local-20261012-230500-c4a18e.zip; sin driver,
claves/audio/experimentos, manifiesto y acceso directo opcional. Driver/runtime
preexistentes; no afirmar instalador o firma produccion. App habitual relanzada.

## Investigacion documental Bongiovi DPS

Usuario solicita fuentes web para valorar cercania. Contraste y fuentes en
docs/BONGIOVI_RESEARCH.md (2026-10-12). Coincidencia de familia: EQ/dinamica,
AGC, graves armonicos, perfiles, estereo; NO equivalencia sonora ni porcentaje.
Distinguir comunicado plugin2015 y blog2012 de suite actual y chip2018.
Pistas pendientes: calibracion mas rica, equilibrio segun volumen, coordinacion
dinamica/ataques. No adoptar cambios por analogia, preservar favorito.

## Pico al salir de mute o volumen cero

Usuario confirma funcionamiento correcto tras limitar la pausa a mute/cero:
"ahora si, gracias". Validacion auditiva favorable del arreglo; conservar
pasos entre volumenes no nulos sin pausas y proteccion al reactivar sonido.

Revision posterior: usuario rechaza pausa en cada paso. Callback incrementa
revision SOLO cuando cambia estado silencio/activo; pasos entre volumenes
no nulos no vacian FIFO, no reinician DSP y no renuevan espera. Conserva
proteccion 180 ms para mute/cero, incluso mute/unmute entre dos ciclos audio.
Pruebas verifican pasos pequenos/grandes sin alterar muestras y toggle rapido.

Usuario comunica rafaga a volumen alto al reactivar sonido. La puerta anterior
abria inmediatamente con el callback, pudiendo pasar paquetes de loopback sin
atenuar acumulados durante mute/cero. Correccion: cada revision de volumen vacia
FIFO, reinicia DSP y rampa de entrada; puerta mantiene 180 ms de silencio a
48 kHz tras cambio, renovados durante mute. No multiplica volumen no nulo.
Esto introduce una pausa breve tambien al cambiar volumen. Pruebas cubren
supresion de muestras residuales al abrir y retorno a ganancia intacta tras
espera. Falta confirmar desaparicion del pico en uso real del equipo.

## Ensayo de ataques y cuerpo

Usuario autorizo preparar variante. Ralentizar compresion y reducir densidad o
detalle no mejoro suficientemente contraste: se descartaron esos cambios y el
parametro band_attack_ms se retiro. No repetir afirmacion de que ralentizar
compresion mejora necesariamente los ataques: el limitador anulaba el efecto.

Nueva variante config/attack-body.user.dps: favorito mas transient_accent_db4,
opcionaldefault0. Acento corto activado por eventos, puerta/periodo minimo y
menor cantidad cuando bandafuerte; sin cambiarEQ/AGC/densidad/RAW. Ensayo y
limites en docs/ATTACK_EXPERIMENT.md. NO adoptar ni activar sin escucha.
Ataque relativo +.566/.284/.123dB para escalas.08/.16/.32; cuerpo -.585/-.348/
-.261dB; RMS+.048/-.496/-.318dB. Mejora pequena y compromiso de cuerpo,
no prometer diferencia perceptible. Perfil no incluye Intensidad alineada.

Verificacion final: Release sin avisos, 14/14 CTest y prueba WASAPI correctos.
Render del favorito identico a la referencia; hashes de favorito y sesion
conservados. Aplicacion relanzada en bandeja con la sesion habitual. Ensayo
preparado para escucha manual, sin activar automaticamente ni medir microfono.

## Escucha de Intensidad alineada: sin mejora perceptible

El usuario comparo intensity-aligned.user.dps con su ajuste por defecto y no
distinguio diferencia. Tras explicar que 0/100 son iguales y como comparar a
25/50, reafirmo: "no noto absolutamente nada". La variante NO tiene validacion
auditiva favorable y no se adopta como defecto. No presentar la mejora del tono
1k como progreso perceptivo ni seguir pidiendo escucharla. Conservar archivo
experimental como ensayo tecnico, favorito aprobado intacto; no cambiar la
sesion activa automaticamente a partir de este comentario.

## Espacio del boton Sonido Windows

Usuario mostro icono/texto demasiado justos. Route pasa de146 a170 de ancho;
fila redistribuida conservando separaciones8 y borde final840. SetDefault116,
Detect114 y Measure recolocados. Cambio soloGUI; Release y gui_controls pasan.
Aplicacion relanzada en bandeja con sesion guardada. Captura build/button-spacing.png.

## Variante experimental de Intensidad alineada

Usuario autorizo prepararla. Archivo config/intensity-aligned.user.dps, copia
del favorito con campo opcional aligned_intensity1 (por defectofalse). Motor
usa bancos allpass preparados para mover fase seca hacia crossover en posiciones
intermedias. Sin cambios de AGC/densidad, sin compensacion de volumen visible.
No activar automaticamente ni sustituir favorito/sesion. Pruebas y limites en
docs/INTENSITY_EXPERIMENT.md. Escucha del usuario pendiente.

Seno1k/escala.02/intensidad25: de-7.35 a+3.92dB; mejoras tambien en250,650,2800,
6000Hz ensayados. Extremos0/100 exactos; renderfull igual a8D00DBCE...97DC1EA.
Trece pruebas pasan en build de auditoria, incluida automatizacion, bypass,
estabilidad16/48/192k, bloques, serializacion y ausencia de asignaciones.
P99local48k/960frames .54ms frente20ms. No prometer monotonia universal ni
validacion acustica. Favorito intacto; variante requiere evaluacion auditiva.

Verificacion final: Release habitual sin avisos; trece CTest pasan y --audio-test
WASAPI pasa. Render Release del experimental al100 igual8D00DBCE...97DC1EA.
Favorito5119695B...C3E23E y sesion5B437712...13EC90 conservados; aplicacion
relanzada en bandeja con sesion habitual, NO con experimental. Para probar:
Cargar perfil config/intensity-aligned.user.dps, escuchar25/50/75%; Ajuste por
defecto vuelve al aprobado. No hay permiso para fijarlo automaticamente.

## Selector de guardado tras medir - correccion

El usuario mostro "No se pudo abrir el selector de archivos" al guardar el
perfil recien medido. El dialogo recibia profilePath completo, que al arranque
puede ser relativo "config/session.user.dps" con barras /. Se cambia la semilla
a SOLO filename; directorio inicial va en lpstrInitialDir. Para Measured se
propone Medido.dps y al aplicar medicion se limpia profilePath para no tratar
la medicion como una edicion del archivo cargado. Errores incluyen codigo de
CommDlgExtendedError y se registran. El codigo original no permitia confirmar
el codigo concreto del fallo mostrado; no atribuirlo a calibracion invalida.

Release y doce CTest pasan, aplicacion actualizada y relanzada en bandeja.
Favorito conservado. Al cerrar normalmente para actualizar, la sesion guardada
seguia siendo Laptop Impact+, no Measured; no hay evidencia de una medicion
aplicada en esa sesion. No inventar valores ni reemplazarla por propuesta de
diagnostico. Falta confirmar que Guardar perfil abre el dialogo en uso real.

## Verificacion real de la regresion de medicion - resuelta en captura

Con consentimiento por operacion se realizaron dos pruebas. Primera detecto
480 muestras perdidas (10 ms) y rechazo. Se implemento mapa AudioGap, relleno
temporal con ceros y comprobacion de solape con las ventanas reales de analisis:
solo rechazar si un hueco afecta a un tono medido, no por flag ni por perdida
fuera del tramo analizado. Pruebas de hueco inicial aceptado y hueco central
rechazado pasan. Doce CTest pasan en Release, sin avisos.

Segunda medicion real autorizada: valida (usable=1), frames246240, dos avisos de
discontinuidad, missing480, gap_begin=0. El hueco esta al INICIO, fuera de las
ventanas medidas; esa era la causa del rechazo general. El microfono capto
tonos; reliableBands2, ruido -31.503 y senal -4.914 dB. No declarar que el usuario
confirmo oirlos: la captura si los confirma. Niveles relativos:
60:-54.109 /120:-27.757 /250:-11.838 /500:6.414 /1k:5.424 /
2k:-9.902 /4k:-18.138 /8k:2.532 dB. Numeros en
build/consolidation/measurement-live-result.txt; audio descartado, nada aplicado.
Aplicacion relanzada en bandeja con la sesion y volumen previo conservados.
No repetir mas capturas sin nuevo permiso por operacion.

## Regresion de medicion: discontinuidad y siguientes intentos sin tonos

El usuario reporto rechazo por perdida de muestras y que tras el primer intento
los siguientes no sonaban. El rechazo inmediato por cualquier flag de
discontinuidad se introdujo en la consolidacion y podia abortar antes de oir los
tonos. No equivalia a una comprobacion cuantificada de muestras perdidas.

Correccion: comparar posiciones de los paquetes con CaptureTimeline; flag sin
salto temporal no aborta. Huecos reales conservan su posicion con ceros y hacen
rechazar el resultado tras completar la reproduccion; no se concatenan como si
no faltase nada. Timestamps marcados invalidos no producen huecos ficticios.
Reset de reloj invalida captura. Primer bloque de tonos preparado ANTES de Start.
Fallo de captura escribe measure-debug.txt (antes quedaba un diagnostico viejo),
y fallo de reconexion se comunica por separado sin ocultar el fallo de medida.

Doce pruebas pasan en build de auditoria, incluidas continuidad, hueco real,
reset de reloj y timestamp invalido. La prueba real con microfono requiere
permiso por operacion: se pidio para una medicion, sin aplicar ajuste ni guardar
audio. No declarar la medicion de habitacion resuelta solo por pruebas unitarias.

Verificacion de esta correccion: Release sin avisos, doce CTest y --audio-test
WASAPI pasan. Aplicacion actualizada y relanzada en bandeja con la sesion.

## Consolidacion de medicion y revision sonora

El usuario autorizo consolidar y mejorar calidad sonora. Cambios en
docs/CONSOLIDATION.md: alineacion conjunta por energia de tonos medios, ya no
por primer sonido sobre umbral; prueba de coherencia de al menos dos tonos;
error ante discontinuidad de captura; corte con transicion suave 7-13 dB;
tonos con el mismo RAW de la sesion; diagnostico numerico con bandas, retardo,
RAW y propuesta. Esto cambia deliberadamente propuestas nuevas, no perfiles.

Nueva herramienta dps_quality: caracterizacion offline sin tocar sonido ni
microfono. Hallazgo concreto: intensidad25 a1kHz/escala.02 da -7.35dB mientras
50 da+5.26 y100 da+13.36. Mezcla seca/procesada cancela por fase. Es prioridad
para una iteracion sonora, manteniendo bypass/cerooriginal y100aprobado.
No corregir con mas ganancia ni asumir que potencia constante elimina la fase.
CSV build/consolidation/quality-favorite.csv. Perfil aprobado sin cambios.

Verificacion: Release sin avisos, doce pruebas CTest y --audio-test WASAPI pasan.
Favorito 5119695B...C3E23E y sesion 5B437712...13EC90 conservados; render favorito
8D00DBCE...97DC1EA identico al de referencia. Aplicacion relanzada en bandeja.
Reanalisis del WAV historico (NO la ultima captura): con ruido supuesto -50 a
-35 dB conserva retardo20 ms, corte192.47/cuerpo8.46/presencia8.00. Esto confirma
estabilidad sobre la misma grabacion, no repetibilidad de la habitacion.

## Variacion de medicion tras auditoria - pendiente de atribuir

El usuario repitio la medicion: corte 200 Hz, cuerpo 9.4 dB, presencia 6.2 dB;
antes 162 / 7.3 / 10. Las formulas de calibrationFrom NO cambiaron; si se
modificaron alineacion y captura durante la auditoria. No afirmar que fueron
neutras para medicion por conservar un render DSP identico.

La regla de corte es discontinua: si 250 Hz baja de -10 dB relativos, salta
de 162 al tope 200. Con referencia media de bandas 250/500/1000, cuerpo no
saturado = -.75*level250: 7.3 implica -9.73 dB y 9.4 implica -12.53. Basta
una variacion de unos 2.8 dB en esa banda para provocar este salto grande.
200 es un limite de propuesta, no un corte acustico medido exactamente.

Registro nuevo (20:26) tiene margen 39.29 dB y pico 1.18927; no contiene niveles
por banda ni alineacion. WAV disponible es de 19:42, NO la ultima captura.
Analisis offline de ese WAV historico con el mismo suelo de ruido sintetico
para ambos algoritmos confirma que la alineacion puede cambiar las propuestas;
no demuestra la causa de la diferencia de la ultima medicion. El WAV disponible
tampoco reproduce los valores 162/7.3/10 documentados: no tratarlo como la
grabacion original de aquellos numeros. Scripts en build/audit/compare_measurement.py.
Conservar perfil aprovado y sesion; no aplicar una propuesta por ser mas nueva.

## Volumen Windows cero / mute subia el sonido - corregido

El usuario reporto que al llegar a 0 el volumen de Windows el sonido subia.
Confirmo que era Windows, no Intensidad. Se reprodujo con la ruta fisica parada:
loopback del endpoint virtual seguia entregando la mezcla al activar mute, sin
la atenuacion que tenia antes. Probe real: volumen .2, peak .0208; volumen 0
sin mute, .0000053; mute a .2, .3375; mute a 0, .3711. Hay musica del usuario
en esa mezcla, asi que esos picos no son una medicion de ganancia de un tono
aislado. Demuestran que mute no silencia la captura. La salida fisica estuvo
detenida durante la reproduccion del fallo para evitar un salto audible.

Arreglo en host, sin modificar DSP ni driver: IAudioEndpointVolumeCallback
observa el endpoint capturado por loopback, publica mute/volumen cero en un
atomico, y el render aplica silencio DESPUES del DSP, incluidas colas de filtros
y bypass. No se escala el volumen no nulo de nuevo. Snapshot versionado evita
que una lectura inicial antigua sobrescriba una notificacion de mute nueva.
Callback sin asignaciones, esperas ni llamadas COM. Inicio cerrado hasta leer
estado, y desregistro RAII. Medidor de salida refleja el silencio aplicado.

Doce pruebas CTest pasan, incluida endpoint_silence_validation: mute, cero sin
mute, salida exactamente cero, retorno a normal sin cambiar muestras, carrera
de lectura inicial y ausencia de asignaciones. Probe con notificaciones reales
confirma protected_peak=0 en los tres casos de silencio. Release sin avisos y
--audio-test real pasa. Volumen virtual restaurado a su .2 previo, mute false;
salida fisica y perfiles no tocados. Aplicacion relanzada en bandeja.
Favorito hash 5119695B...C3E23E y sesion 5B437712...13EC90 conservados.

## Auditoria y refactorizacion - 12 de octubre de 2026

El usuario pidio auditar todo el proyecto y refactorizar. Informe en docs/AUDIT.md.
Cambios: preferencias portables separadas; paleta, iconos y sesion WASAPI fuera
de gui_main; macros de colores eliminadas; adaptador Windows compilado una sola
vez; sugerencias de calibracion separadas de lectura BIOS y probadas sin Windows.
Guardado de perfiles/preferencias/WAV mediante temporal y reemplazo, validacion
de nombre de dispositivo y formato decimal independiente de la configuracion
regional. No modificar favorito ni sesion por el hecho de que difieran.

Captura de medicion: clientes detenidos por RAII, listener jthread con excepciones
propagadas tras join y coordinador MTA para evitar compartir COM STA con MTA.
Entradas invalidas/no finitas se rechazan antes de analizar. La alineacion ahora
identifica la banda del primer tono audible: buscar el tercer tono sin descontar
dos pasos desplazaba todas las bandas. Regresion con graves ausentes y 50 ms.

Privacidad: el WAV se guardaba pese al aviso de descarte. Ahora solo se guarda
con --measurement-diagnostics y el consentimiento lo dice. El WAV duplica mono
como estereo para conservar duracion; antes se etiquetaba mono como estereo.
Resumen numerico measure-debug.txt conservado; WAV historicos no borrados.

Correccion de documentacion: latencyMs suma informes de endpoints, capacidad
de render y FIFO. Los 22 ms historicos eran una estimacion de software, NO una
medicion fisica de ida y vuelta. La tabla de modelos contiene ajustes aprobados
por escucha (Basis::Tuned), no calibraciones de laboratorio.

Once pruebas CTest pasan en la compilacion de auditoria; paquete de controlador
verificado offline con rechazo de INF/SYS alterados. Render de favorito y sesion
identico bit a bit al anterior sobre programa sintetico de 3 segundos; hash de
ambos archivos al inicio 5119695BA5D42074679AB8293F12FC44047126F19275C75DAEB57CEA82C3E23E.
La medicion real corregida no se lanza automaticamente: requiere consentimiento
explicito para el microfono. Ver el informe para la verificacion final.

Verificacion final: Release habitual compila sin avisos; once pruebas pasan y
--audio-test pasa con WASAPI real. Renders Release de favorito y sesion siguen
identicos bit a bit (8D00DBCE...97DC1EA). Favorito conserva hash 5119695B...C3E23E;
sesion guardada por la aplicacion pasa a LF, hash 5B437712...13EC90, mismos campos
y sonido. Aplicacion relanzada en bandeja con su sesion. No reinstalar driver.

## Confirmacion del usuario: medicion por microfono funciona - 12 de octubre de 2026

El usuario confirma que la medicion a traves del microfono funciona tras la
correccion de captura en hilo propio. Queda cerrada la comprobacion pendiente
del flujo de medicion real. Esto confirma su funcionamiento en el equipo del
usuario, no la precision acustica ni la aprobacion del ajuste que propone.
Conservar el ajuste por defecto aprobado y la sesion del usuario por separado.

Actualizado: 2026-10-11.

- Aplicación original C++20 para mejorar audio del sistema mediante nuestro
  dispositivo virtual y WASAPI, con salida física Realtek del Lenovo Legion
  Pro 5 83LU. Debe admitir otros equipos y perfiles.
- El usuario autorizó instalar el controlador de desarrollo en este portátil
  y activar firma de pruebas. Reinició manualmente. No reiniciar automáticamente.
- FxSound está desinstalado: no depender de él.
- El usuario aprecia la mejora sin compensación, especialmente a volumen
  moderado. A 100% es menor. Con igualado de volumen declara no apreciar
  diferencia entre A y B, incluso con el ajuste Laptop Focus.
- La telemetría confirma conmutación enabled 0/1 con señal y comparación activa.
  La compensación atenúa el procesado aproximadamente 7–9 dB durante su música.
  Esto indica una contribución considerable de ganancia; no demuestra que toda
  la mejora sea ganancia ni que exista una limitación física determinada.
- Pruebas sintéticas de timbre pasan, pero no acreditan mejora audible en los
  altavoces. No presentar resultados técnicos como aprobación perceptiva.
- Prioridad: cuerpo, articulación y claridad que sobrevivan A/B a igual volumen,
  con protección de picos, continuidad y cero asignaciones/bloqueos en audio.
- Conservar `config/favorite.user.dps`: referencia aprobada. Los ajustes nuevos
  son provisionales hasta que el usuario los valore.
- El usuario pide continuar autónomamente y guardar contexto. No terminar una
  iteración solo con un diagnóstico o una propuesta si hay trabajo realizable.
- Esta memoria es local al proyecto; no afirmar memoria global de la cuenta.

Estado: seis pruebas CTest pasan. Interfaz Win32, perfiles v1/v2, comparador RMS
temporal, salida RAW para evitar el fallo de efectos OEM, sin calibración acústica
medida. Limitador de pico de muestra a -1 dBFS, sin garantía true peak.

## Nueva iteración: articulación temporal

Se añadió `TransientShaper`, detector de ataques enlazado en estéreo, en bandas
180–900, 900–3500 y >3500 Hz (mitad de intensidad en la última). Se activa con
`detail_db > 0`; su cantidad sigue Clarity. Detector rápido 1 ms, lento 80 ms,
puerta 0.003–0.020, suavizado de ganancia 2/35 ms. El limitador final sigue activo.
Perfiles con detail_db=0 conservan la ruta anterior. En perfiles v2 que lo usan,
detail_db ahora controla tanto el detalle sostenido existente como los ataques.
No se alteraron los valores del perfil favorito ni se incrementó el AGC.

Prueba sintética de ráfagas a 1800 Hz sobre fondo a 90 Hz: contraste energético
del ataque +1.41 dB con diferencia RMS total de 0.0015 dB usando el comparador
real. Es evidencia técnica de efecto temporal, no validación auditiva en Lenovo.
Se añadió una sexta prueba, incluyendo puerta, señal sostenida sin ganancia
constante, compatibilidad mono, estabilidad y continuidad entre bloques.
GUI permite --compare para arrancar la evaluación a volumen igualado.

Verificación final: compilación Release correcta; seis pruebas pasan incluyendo
ausencia de asignaciones en el DSP y automatización de controles con Laptop Focus.
Prueba WASAPI real de inicio/parada y ruta virtual a Realtek pasa. Hash del perfil
favorito conservado; WAV de referencia renderizado con él es idéntico bit a bit
al anterior. GUI relanzada con Focus y --compare, sin modificar volúmenes Windows.
Registro previo conservado en build/gui-telemetry-before-transient.log.

## Comparación estable

El usuario volvió a pedir acción autónoma después de dos respuestas que solo
explicaban el problema. Reafirmó que no quiere tener que pedir continuamente
seguir. Completar implementación, pruebas y puesta en marcha antes de responder.

Se sustituyó el seguimiento continuo de la relación RMS por una compensación
fija por fragmento: 5 segundos con señal para estabilizar DSP y 3 para medir.
La atenuación se aplica suavizada; A/B, silencios y cambios del programa conservan
el valor. Cambiar controles/intensidad o desactivar/reactivar el igualado inicia
una medición nueva. Es RMS, no LUFS, y otro tema musical puede requerir recalibrar.
No prometer idéntico nivel en cada pasaje: preservar dinámica exige no seguirlo
instantáneamente. El control de igualación de la GUI se conserva entre sesiones
en config/comparison.user.txt, separado de los perfiles de sonido.

Se añadió regresión para comprobar que una subida del programa o silencio no
altera la compensación fijada. Las pruebas tonales ahora esperan 12 segundos
para medir después de calibración y transición, manteniendo sus tolerancias.
La mejora perceptiva en altavoces reales continúa pendiente de la escucha del
usuario; no declarar resuelto todo el objetivo a partir de pruebas sintéticas.

Verificación de la comparación fija: seis pruebas pasan; error RMS estacionario
0.147 dB, prueba de ataques 1.395 dB con diferencia RMS global -0.0067 dB.
Render del perfil favorito sigue idéntico bit a bit. Inicio/parada WASAPI real
pasa; GUI actualizada y relanzada con Focus y A/B activo. La diferencia audible
entre igualado activado y desactivado no puede desaparecer por completo mientras
el DSP aporte ganancia: no ocultarla ni aumentar volumen para simular progreso.

## Perfil provisional: cuerpo y claridad, revisión 2

El usuario escucha apenas diferencia A/B tras el cambio del comparador. Esta
escucha invalida considerar suficiente el ajuste anterior. Se preparó una
variante de Laptop Focus con corte 130 Hz, cuerpo 320 Hz / 11 dB, presencia
2800 Hz / 11 dB, recorte 650 Hz / -7 dB, armonicos .9, detalle 6 y suelo .85;
controles Bass .9, Clarity .9, Dynamics .65, Spatial .2. No aumentar AGC.
Config local nueva: config/body-clarity.user.dps; config/focus.user.dps anterior
y config/favorite.user.dps conservados. El botón de cuerpo/detalle y el perfil
distribuido Laptop Focus seleccionan ahora la revisión 2.

Prueba multitono con el comparador real: respecto al bypass a igual RMS,
270 Hz +1.51 a +2.39 dB; 2800 Hz +4.54 a +4.85 dB. Diferencia global RMS
0.007 a 0.012 dB. 90 Hz se reduce 12.5–14.6 dB: vigilar pérdida de cuerpo
en fuentes o dispositivos que sí reproduzcan ese grave. Sigue sin existir una
medición acústica del portátil. No extrapolar estas cifras a calidad percibida.
Se reforzaron las pruebas para exigir contraste contra bypass real, además
de comparar contra otro preset. La evaluación auditiva de la variante está pendiente.

Revisión 2 compilada y ejecutada: seis pruebas pasan, test WASAPI real pasa,
referencia favorita idéntica en render binario. GUI activa con perfil local
body-clarity.user.dps, A/B igualado activo y controles .9/.9/.65/.2 verificados
en telemetría. En la última comprobación no había entrada de audio: la nueva
calibración de nivel espera música, no presentar silencio como una prueba de sonido.

## Decisión actual del usuario: retirar igualación

El usuario considera que el igualado atenúa demasiado y no le aporta valor.
Pidió eliminar esa opción y comparar únicamente con DSP activado/desactivado.
Esta decisión sustituye el criterio anterior de exigir A/B igualado en la GUI.
Se eliminaron checkbox, indicadores, opción --compare, preferencia persistente
y controles/medidores de compensación del host WASAPI. El host en directo no
activa la comparación compensada. No restaurar la opción sin petición expresa.
Las pruebas internas del núcleo conservan mediciones normalizadas para analizar
algoritmos; no representan una opción accesible de la aplicación.

## Identidad y ventana principal

El usuario eligió RESONIQ (aplicación), RESONIQ DSP Engine (motor), RESONIQ Enhance
y RESONIQ Max (perfiles). Azul eléctrico, grafito oscuro, minimalismo y ondas.
Disponibilidad comercial/marca pendiente antes de publicar: no afirmar exclusividad.
No usar Bongiovi en la identidad pública; mantener originalidad y notas técnicas.

GUI renombrada a resoniq.exe, título RESONIQ, nuevo Start-RESONIQ.cmd y lanzador
anterior compatible. Rediseño con gran interruptor, intensidad destacada y
envolvente real del historial de picos de salida dibujada en UI. Clarity/Bass/
Dynamics/Spatial conservan sus controles y procesamiento. Presets comerciales
tienen alias compatibles con los cuatro nombres antiguos. Favorito y sesión
conservan sus parámetros; no reinstalar controlador por cambiar marca.

Verificación: Release compila; seis pruebas pasan, incluyendo roundtrip exacto
con 32 combinaciones de dispositivos/presets antiguos y RESONIQ, controles GUI
y bypass. Vista propia renderizada y revisada en build/resoniq-preview.png.
Prueba WASAPI real pasa. Se abrió resoniq.exe --start con la sesión del usuario,
sin sustituirla por un preset de demostración. Hash del favorito sigue intacto.

## Estilo Windows 11

El usuario aprueba el rediseño pero pide una estética Windows 11 moderna y actual.
Se sustituyó el aspecto clásico de combos/deslizadores/checkbox por dibujo Fluent
con GDI+: tarjetas y botones redondeados, bordes suaves, sliders con tramo activo
azul y tiradores circulares, selectores oscuros con chevron, interruptor pill,
Segoe UI Variable Text/Display, estados hover/foco y esquinas de ventana DWM.
Se solicitó backdrop de sistema Windows 11 (visible en marco; cliente opaco).
Win32 conserva selección/teclado/estado de controles; gráficos solo en hilo UI.
No se cambiaron DSP, volumen Windows ni parámetros guardados del usuario.
Captura propia revisada: build/resoniq-fluent-preview.png. DWM confirma soporte
de ambas opciones en este equipo. No afirmar migración a WinUI 3: sigue Win32/GDI+.

Verificación final: seis pruebas pasan tras introducir el nuevo dibujo. Prueba
GUI ampliada confirma que VK_RIGHT mueve el slider y publica al motor, que el
interruptor conserva clic/estado, y que presets/bypass/perfiles funcionan. Último
retoque de icono y estado deshabilitado vuelve a pasar GUI. WASAPI real pasa y
resoniq.exe --start queda abierto con la sesión guardada del usuario.

Corrección del interruptor: el usuario mostró un cuadrado de checkbox clásico
sobre el toggle moderno. El control nativo dibujaba durante cambios de estado,
foco y hover fuera de WM_PAINT. Se fuerza repintado propio después de esos
mensajes, conservando semántica de checkbox y teclado; no modificar audio.

## Interruptor único

El usuario pidió unificar Iniciar/Detener audio con el interruptor principal.
Se retiró el botón de iniciar/parar. Toda apertura normal conecta automáticamente
la ruta; DSP activado/desactivado alterna procesado/original manteniendo el mismo
trabajador WASAPI. Cerrar la aplicación sí termina la ruta. Si la conexión falla,
el interruptor permite reintentar y muestra Conectar audio.
Dispositivo, salida, RAW y carga de perfil quedan disponibles y reinician la
sesión automáticamente cuando se necesita preparar otra configuración. Presets
y controles normales siguen aplicándose en directo. La prueba WASAPI real
comprueba arranque automático y que conmutar el DSP conserva el mismo trabajador.

Verificación: seis pruebas pasan; prueba WASAPI real confirma arranque automático
y conmutación DSP/original sin sustituir el trabajador. Se abrió resoniq.exe sin
argumentos y telemetría confirma running=1, finished=0, con los ajustes de sesión.

Claridad de acciones: el usuario pidió renombrar los tres botones inferiores a
Aplicar ajuste portátil, Recuperar favorito y Sonido de Windows. Nombres aplicados
y distribución ampliada para evitar solapamientos. Recuperar favorito conserva
la misma función: carga config/favorite.user.dps y activa DSP, no es bypass.

El usuario pidió simplificar aún más: perfil de dispositivo aplicado al
seleccionarlo, sin botón Aplicar ajuste. Se retiró ese botón. La selección cambia
la calibración y reconecta automáticamente si hay audio activo; conserva los
controles de escucha y el estado DSP. Recuperar favorito y Sonido de Windows
permanecen. GUI prueba aplicación automática y conservación de controles.

El usuario mostró restos de tiradores clásicos sobre los sliders Fluent. Se
amplió el repintado posterior al manejador nativo para TBM_SETPOS/rangos, rueda,
temporizadores de arrastre, tamaño y cancelación. Compilación y prueba GUI pasan;
aplicación reiniciada con sus valores guardados. No tocar procesamiento de audio.


## Continuidad - 12 de octubre de 2026

Guardado por peticion expresa del usuario. Memoria local del proyecto.
- Continuar trabajo rutinario autorizado sin pedir confirmacion en cada paso.
- Identidad RESONIQ, estetica Windows 11 azul/grafito; GUI actual Win32/GDI+.
  Comprobar disponibilidad de marca antes de distribuir.
- Comparacion DSP procesado/original sin igualar volumen; no restaurar igualado.
- Audio automatico; interruptor DSP mantiene ruta WASAPI. Cerrar termina ruta.
- Perfil de dispositivo se aplica al seleccionarlo y conserva controles y DSP.
  No hay boton Aplicar. Recuperar favorito y Sonido de Windows siguen disponibles.
- Preservar favorito aprobado y sesion guardada; no imponer presets de muestra.
- Ultimo cambio: correccion de repintado de sliders. Release y gui_controls pasan;
  aplicacion reiniciada. Falta confirmacion visual del usuario en uso normal.
- Mejora apreciada a volumen moderado, menos perceptible al 100 %. No dar por
  resuelto ese punto ni presentar pruebas sinteticas como mediciones acusticas.

## Tiradores clasicos: correccion de raiz - 12 de octubre de 2026

El repintado posterior por lista de mensajes no bastaba: las cinco barras no
tienen estilo de dibujo propio, asi que el control con tema visual siempre
pintaba su tirador clasico y la lista solo tapaba los casos enumerados. Ahora
se retira el tema visual de las barras (SetWindowTheme con nombre vacio), de
modo que el control ya no tiene con que dibujarlo; se enlaza uxtheme. El dibujo
propio de los controles pasa por un mapa de bits intermedio para no parpadear,
y la lista de mensajes se amplio con los que provocan dibujo nativo
(WM_SETTEXT, cambios de tema/color/DPI, posicion, teclas de sistema, scroll).

Se probo un filtro amplio de tipo "todo menos consultas" y se descarto: dejaba
pasar mensajes que la interfaz consulta en cada tick, cada uno encolaba otro
repintado y gui_controls pasaba de 0.14 s a mas de 7 minutos. Tambien se probo
WS_EX_COMPOSITED en los hijos y se retiro: con WS_CLIPCHILDREN en la ventana
padre los controles no se dibujaban y la prueba fallaba. Ambos intentos quedan
anotados para no repetirlos.

Verificacion: Release compila sin avisos; seis pruebas pasan y gui_controls
vuelve a 0.62 s. Prueba WASAPI real pasa. Captura propia revisada en
build/resoniq-sliders-after.png: tiradores circulares azules, sin restos
clasicos. Esa captura usa WM_PRINT y no sustituye la confirmacion visual del
usuario arrastrando con el raton. No se toco audio: engine, wasapi, profile y
transient conservan fecha anterior; el render del favorito es determinista y
su hash sigue siendo a5d2d70c...4ce2.

Correccion sobre favorito y sesion: al ver que no coincidian se restauro el
favorito sobre config/session.user.dps. Fue un error y se deshizo. El favorito
es la referencia guardada y la sesion es lo que el usuario esta usando; que
difieran es correcto y no autoriza a igualarlos. El usuario tenia cargado a
proposito el perfil experimental de cuerpo y detalle (Laptop Focus / RESONIQ
Max, corte 130 Hz, cuerpo y presencia 11 dB, mud -7, detalle 6, raw 0) y ese
estado quedo restaurado. Conservar ambos y preguntar antes de decidir.

## Objetivo declarado: sustituir a Bongiovi DPS

El usuario quiere que el efecto se parezca lo maximo posible al de Bongiovi DPS,
que realzaba notablemente el sonido de portatiles. Ese es el criterio de exito.

Medicion con senal tipo programa (pasaje bajo 6 s, alto 7 s, bajo 7 s) en vez de
tonos estables: los tonos fijos no revelan el comportamiento dinamico y llevaron
a una primera conclusion equivocada.

Hallazgo que corrige el diagnostico inicial: la "caida del efecto a volumen alto"
es en gran parte ganancia del AGC que desaparece, no coloracion que se pierda.
Igualando nivel en el pasaje alto, el favorito apenas cambia el sonido (<1 dB en
todas las bandas) mientras que el perfil de cuerpo y detalle que el usuario tiene
cargado si lo moldea: -3 dB de grave inutil, +2 dB en medios, +1.5 en presencia.
El perfil del usuario ya era bueno; no sustituirlo a la ligera.

Dos parametros de calibracion nuevos, ambos opcionales y con valor por defecto
igual al comportamiento historico, de modo que los perfiles existentes no cambian:
- adaptive_depth (0..0.9, por defecto 0.7): cuanto se retira el realce con el
  nivel. 0.7 es la curva historica; valores bajos mantienen el efecto con
  material fuerte, que es lo que necesita la musica comercial ya comprimida.
- loud_density (1..2.5, por defecto 1): empuja el objetivo del AGC por encima
  del nivel de entrada en material fuerte, lo que hace trabajar mas al compresor
  de bandas en vez de limitarse a subir volumen. El limitador final sigue
  gobernando la seguridad de pico.

Probado solo adaptive_depth bajo: empeoraba la caida (de -9.71 a -11.25 dB)
porque no ataca la causa. Con loud_density 1.45 el efecto en pasaje alto sube
de +1.18 a +3.12 dB y el margen de pico baja de 11.85 a 11.34 dB (mas denso).

Perfil provisional config/bongiovi-style.user.dps: corte 175 Hz, cuerpo 300/11,
presencia 3000/10, armonicos 1.6 con foco .85, mud -6, adaptive_depth .15,
loud_density 1.45. Pendiente de escucha del usuario; no es calibracion medida.
Seis pruebas pasan; favorito identico bit a bit; sesion del usuario sin tocar.

## Punto de retomada - 12 de octubre de 2026

Pausa pedida por el usuario. Al volver, empezar por aqui.

Estado al pausar:
- Aplicacion abierta con la sesion del usuario (Laptop Focus / RESONIQ Max,
  corte 130 Hz, cuerpo y presencia 11 dB, raw 0). Favorito con su hash de
  siempre, a5d2d70c...4ce2. Seis pruebas pasan. Nada a medio hacer.
- Compila y pasa todo; no queda codigo en estado intermedio.

Lo unico pendiente de respuesta del usuario:
1. Escuchar config/bongiovi-style.user.dps (boton Cargar perfil) y decir si va
   en la direccion correcta, si queda corto o si resulta agobiante. Segun eso,
   subir o bajar loud_density. Sin esa escucha no hay forma de seguir ajustando:
   las medidas orientan pero no sustituyen su oido.
2. Su sesion tiene raw 0 y el perfil nuevo raw 1. Se le sugirio activar
   "Omitir efectos adicionales de Windows" tambien en el suyo, para que Windows
   no procese encima. Sin confirmar.
3. Queda sin confirmar visualmente que no reaparecen los tiradores clasicos
   durante un arrastre real con el raton.

Siguientes temas ya acordados, para cuando se cierre lo anterior: robustez de
reconexion de dispositivos y medicion de latencia real.

Dos errores cometidos en esta sesion, anotados para no repetirlos:
- Se restauro el favorito sobre la sesion del usuario al ver que diferian.
  Favorito y sesion son cosas distintas a proposito. Conservar ambos y
  preguntar; no decidir por el.
- Se diagnostico la perdida de efecto a volumen alto con tonos estables y se
  concluyo mal. Para cualquier cuestion dinamica, medir con senal tipo programa
  que tenga pasajes bajos y altos.

Trato con el usuario: quiere avance autonomo y resultados concretos, pero no
decisiones sobre sus ajustes sin consultarle. Codex es quien ha construido este
proyecto; su trabajo no es un añadido reciente sino la base, y conviene
contrastar con el en vez de dar por hecho que falta algo.

## Fallo COM 0x80010106 al cargar perfil - corregido

El usuario mostro el error "COM: HRESULT 0x80010106" al cargar un perfil desde
la GUI. No era culpa del perfil: este se cargaba bien (la sesion guardada al
cerrar ya contenia el perfil nuevo). El error aparecia despues, al releer los
dispositivos de audio.

Causa: el selector de archivos de Windows deja el hilo de interfaz inicializado
como STA. A continuacion la enumeracion de dispositivos pedia COM multihilo, y
Windows devuelve RPC_E_CHANGED_MODE (0x80010106). Ese codigo no es un fallo:
COM esta disponible tal cual esta. El envoltorio Com de src/audio/wasapi.cpp lo
trataba como error.

Arreglo: el envoltorio acepta RPC_E_CHANGED_MODE y solo llama a CoUninitialize
cuando fue el quien inicializo COM, para no descompensar el contador del hilo.
Afecta a los tres usos (enumerar, volumen y host en directo).

Es un fallo preexistente, no introducido por el perfil nuevo: ocurria al cargar
cualquier perfil desde la GUI con audio activo.

Verificacion: se construyo una prueba aparte que pone el hilo en STA y llama a
dps::endpoints(). Con el codigo anterior reproduce "COM: HRESULT 0x80010106";
con el arreglo enumera 3 dispositivos. Ademas, seis pruebas CTest pasan y la
prueba WASAPI real pasa.

## El usuario aprueba el ajuste estilo DPS - 12 de octubre de 2026

Tras escucharlo en sus altavoces, el usuario dice que le gusta. Es la primera
validacion auditiva favorable de un ajuste nuevo en el proyecto; hasta ahora
solo habia medidas sinteticas y una queja recurrente de poca diferencia.

Guardado como config/approved-dps.user.dps, hash 577c06be...b59e1, con copia
fuera del proyecto. Render identico bit a bit al perfil que escucho. Conserva
los dos parametros nuevos: adaptive_depth 0.15 y loud_density 1.45.

El usuario tenia activado raw 1 ("Omitir efectos adicionales de Windows"),
siguiendo la sugerencia. Forma parte de lo que aprobo: al comparar o ajustar
mas adelante, mantenerlo activado o decirlo explicitamente si se cambia.

config/favorite.user.dps sigue intacto (hash a5d2d70c...4ce2) y es un ajuste
distinto, mucho mas suave: a igual volumen apenas colorea (<1 dB por banda).
No sustituir uno por otro sin preguntar: son dos referencias validas y es el
usuario quien decide cual es su favorito.

Siguiente paso natural, cuando el usuario quiera: preguntarle si este pasa a
ser su favorito, y despues seguir acercandose a Bongiovi subiendo densidad por
pasos cortos, verificando en cada uno que no se vuelve cansado o plano.

Integrado como opcion por defecto a peticion del usuario: nuevo perfil de
dispositivo "Laptop Impact", visible en la GUI como "Portatil: impacto", quinta
entrada del desplegable; "Personalizado" pasa a ser la sexta. Calibracion en
deviceCalibration() y perfil distribuido profiles/laptop-impact.dps. El
indicador de detalle/ataque de la ventana ahora aparece tambien con el.

Verificacion: seis pruebas pasan, prueba GUI y WASAPI real pasan, y el render
de profiles/laptop-impact.dps es identico bit a bit al del ajuste que el usuario
aprobo. Los cinco perfiles distribuidos cargan y dan comportamientos distintos
(reduccion maxima 5.6 / 7.9 / 8.4 / 2.9 / 2.9 dB).

Nota: el usuario no ha dicho que esto sustituya a config/favorite.user.dps.
Preguntar antes de cambiar su favorito.

## Nuevo favorito: Portatil impacto - 12 de octubre de 2026

El usuario pidio expresamente que el ajuste aprobado pase a ser su favorito.
config/favorite.user.dps es ahora "Referencia - Portatil impacto", dispositivo
Laptop Impact, raw 1, hash a91ecb7a...9590d. El boton Recuperar favorito
devuelve exactamente el sonido que aprobo (render identico bit a bit).

El favorito anterior (hash a5d2d70c...4ce2, mucho mas suave) se conserva en
config/favorite-previous.user.dps y con copia fuera del proyecto. No borrarlo:
fue la referencia aprobada durante toda la fase anterior y sirve de contraste.

Las menciones de a5d2d70c en secciones anteriores de esta memoria son historia
de aquel momento y siguen siendo correctas en su contexto; el hash vigente del
favorito es el nuevo.

Verificacion: seis pruebas pasan; el favorito nuevo valida y carga; render
identico al ajuste aprobado.

## Icono de bandeja y segundo plano - 12 de octubre de 2026

El usuario pidio que la aplicacion quede en segundo plano con icono junto al
reloj, como hacia Bongiovi. Implementado en apps/gui_main.cpp:

- Cerrar la ventana ya no termina el programa: oculta la ventana, anade el icono
  de bandeja y guarda la sesion. El audio sigue procesando.
- Clic izquierdo en el icono muestra la ventana; clic derecho abre un menu con
  Mostrar, Activar/Desactivar DSP, Recuperar favorito y Salir.
- El texto del icono indica el estado (procesado / original / sin conectar).
- Salir de verdad se hace desde el menu del icono; retira el icono y para el
  audio. Los modos --self-test y --audio-test no entran en bandeja, para que
  las pruebas sigan terminando solas.

Verificacion: compila sin avisos, seis pruebas pasan, prueba WASAPI real pasa, y
se comprobo en ejecucion que tras cerrar la ventana el proceso sigue vivo sin
ventana y la telemetria mantiene running=1 con los parametros del usuario.

Aclaracion por si se repite la duda: el ejecutable build/Release/resoniq.exe
siempre ha existido y es lo que se ejecuta; no hay nada interpretado. Lo que no
existe todavia es una instalacion fuera de la carpeta de compilacion (acceso
directo, inicio con Windows, instalador). Eso sigue pendiente.

## Densidad real: el AGC cancelaba la compresion - 12 de octubre de 2026

Objetivo: acercarse mas a Bongiovi. Hallazgo principal de esta iteracion.

Primero se comprobo que loud_density estaba agotado: su ganancia topa en el
limite de 2 del AGC, y con 1.45 ya lo alcanza. Subirlo mas no cambia nada.

El compresor de bandas solo actuaba por encima de 0.16 (unos -16 dBFS por
banda) y hasta 4:1, asi que el cuerpo de la musica pasaba sin comprimir. Se
anadio band_density, que baja el umbral y sube la relacion a la vez.

Primer intento equivocado: comprimir sin ganancia de compensacion. Solo bajaba
los picos, de modo que subir el parametro REDUCIA el efecto (+3.12 -> +2.48 dB).
Se anadio la compensacion que recupera lo que el umbral pierde.

Seguia sin funcionar, y la causa real estaba despues: el AGC mide el pico de la
senal YA procesada, detecta la compensacion y baja su propia ganancia en la
misma proporcion. Se cancelaban. Se verifico empiricamente congelando el AGC en
una copia del motor: con el congelado el compresor daba +4.38 dB y margen de
pico 10.10; con el activo, nada. Arreglo: el AGC descuenta la compensacion de
bandas de lo que mide (bandMakeup_), de modo que ya no la deshace.

Resultado medido en pasaje alto, con band_density 1.8 sobre el favorito:
ganancia +3.12 -> +4.71 dB; margen de pico 11.34 -> 9.77 (mas denso); la
variacion corta baja de 0.64 a 0.50, senal de que se pierde algo de vida. Por
eso se eligio 1.8 y no 2.6: a partir de ahi la ganancia crece poco y la musica
se aplana. Rango permitido 1..4.

Ningun perfil existente cambia: con band_density=1 la compensacion vale 1 y el
descuento no hace nada. Verificado que el favorito y el favorito anterior
renderizan identicos bit a bit tras el cambio.

Nuevo perfil "Laptop Impact+" ("Portatil: impacto maximo"), sexta entrada del
desplegable; Personalizado pasa a la septima. Tambien en
profiles/laptop-impact-plus.dps y config/impact-density.user.dps. Pendiente de
que el usuario lo escuche: si le resulta plano o cansado, bajar band_density
hacia 1.4; si quiere mas, subir con cuidado hasta 2.2.

Seis pruebas pasan, prueba WASAPI real pasa, compila sin avisos.

## El medidor "Reduccion" no servia - 12 de octubre de 2026

El usuario observo que el campo Reduccion nunca se movia. Tenia razon y habia
dos cosas distintas.

1. Medicion correcta pero inutil en la practica. Mostraba cuanto bajaba la
   ganancia el compresor o el limitador. Al nivel al que el usuario escucha
   (pico de entrada ~0.06) ninguno de los dos actua: el limitador necesita
   picos cercanos a 0.89 y el compresor del favorito un umbral de 0.16 por
   banda. Verificado con senal a su nivel real: favorito 0 dB de reduccion,
   impacto maximo 4.19 dB. El medidor decia la verdad, pero de algo que casi
   nunca ocurria.

2. Fallo introducido en esta sesion: la ganancia de compensacion del compresor
   entraba en el mismo minimo, y al ser mayor que 1 impedia que el medidor
   registrara compresion con band_density alto. Corregido midiendo la ganancia
   de banda relativa a su propia compensacion.

El medidor pasa a mostrar el realce real (salida/entrada en dB) con la barra, e
indica entre parentesis la compresion cuando la hay; sin audio dice "sin senal".
Es informacion que se mueve con la musica y explica lo que hace el motor.

Seis pruebas pasan, prueba WASAPI real pasa, captura revisada.

## "Favorito" pasa a ser "Ajuste por defecto" - 12 de octubre de 2026

El usuario pidio el cambio de nombre: "favorito" describe su relacion personal
con el ajuste y resulta subjetivo para alguien ajeno, mientras que "por defecto"
se entiende sin explicacion. Textos cambiados en el boton, el menu del icono de
bandeja, el mensaje de error y README.md.

El archivo sigue siendo config/favorite.user.dps y en codigo se conserva
recoverFavorite(): renombrar el archivo invalidaria los ajustes ya guardados del
usuario y las rutas documentadas. Si alguna vez se renombra, migrar el archivo
existente, no abandonarlo.

Nuevo en el menu del icono: "Fijar lo actual como ajuste por defecto". Pide
confirmacion y copia el ajuste anterior a config/favorite-previous.user.dps
antes de sustituirlo, de modo que un clic no pueda hacer perder el ajuste
vigente. Hasta ahora esto solo se podia hacer copiando archivos a mano.

Las menciones de "favorito" en secciones anteriores de esta memoria describen
lo que se llamaba asi entonces y siguen siendo correctas en su contexto.

Correccion: "Fijar lo actual como ajuste por defecto" se habia puesto solo en el
menu del icono de bandeja, que solo aparece tras cerrar la ventana y hacer clic
derecho. El usuario no lo encontraba, con razon. Ahora hay un boton
"Fijar como defecto" en la ventana principal, entre "Ajuste por defecto" y
"Sonido de Windows" (tres botones de 260 de ancho en 30/305/580). La entrada
del menu de bandeja se conserva. Captura revisada: los tres caben sin solapar.

## Ventana de ajustes - 12 de octubre de 2026

El usuario pidio poder configurar el arranque automatico y lo que hiciera falta.
Boton "Ajustes" (cuarto de la fila; los cuatro pasan a 196 de ancho) que abre una
ventana aparte con controles estandar, para que respete Tab, Esc y Enter sin
dibujo propio. Opciones acordadas con el:

- Iniciar RESONIQ con Windows. Escribe en HKCU Run (por usuario, sin permisos de
  administrador) la ruta entre comillas mas --tray. Al abrir los ajustes se lee
  el registro, no el archivo: el usuario puede haberlo quitado desde Windows.
- Arrancar minimizado en la bandeja. Tambien por argumento --tray.
- Al cerrar la ventana, salir del todo. Antes cerrar siempre ocultaba, sin
  avisar, y alguien podia creer que habia cerrado el programa.
- Reconectar el audio automaticamente. Si el trabajador termina (cambio de
  auriculares, reinicio del controlador) reintenta a los 3 s; si falla, espacia
  a 5 s para no insistir sobre un dispositivo que ya no esta.
- Al arrancar, el DSP: como estaba, siempre activado o siempre desactivado.

Preferencias en config/settings.user.txt, separadas de los perfiles de sonido:
describen comportamiento, no sonido. Formato clave/valor, se crea al guardar.

Verificacion: compila sin avisos, seis pruebas pasan, prueba WASAPI real pasa,
captura revisada con los cuatro botones sin solapar, y se comprobo en ejecucion
que --tray arranca sin ventana y procesa audio (entrada 0.021 -> salida 0.205,
unos +20 dB, con realce de ataques y detalle activos).

Pendiente de uso real: el usuario aun no ha activado el inicio con Windows.
Conviene que lo pruebe reiniciando. La clave de registro es RESONIQ en
HKCU\Software\Microsoft\Windows\CurrentVersion\Run.

Correccion del estilo: la primera version de la ventana de ajustes usaba
controles del sistema con fondo gris, con el argumento de respetar las
convenciones de dialogo. El usuario señalo que desentonaba con la interfaz
Fluent oscura, y tenia razon. Rehecha reutilizando el dibujo propio que ya
existia: los controles de ajustes tienen su propio rango de identificadores
(SetStart=400..) y el pintado los reconoce, de modo que las casillas salen como
interruptores pill y la lista como selector oscuro. Botones con dibujo propio
(Guardar en azul de acento), fondo oscuro con tarjeta redondeada, marco oscuro
y esquinas redondeadas por DWM, igual que la ventana principal.

La prueba gui_controls cubre ahora la ventana de ajustes: que abra, que tenga
sus siete controles, que la lista tenga tres opciones, que cierre y que Cancelar
no altere las preferencias. Genera build/settings-preview.bmp para revisarla.

Nota de metodo: intentar abrir la ventana desde fuera con clics simulados fallo
y ademas interfirio con el escritorio del usuario (una captura recogio su
navegador). La via correcta es la prueba automatica dentro de la aplicacion.

## Temas claro, oscuro y del sistema - 12 de octubre de 2026

El usuario pregunto si la aplicacion respetaba los modos claro/oscuro/sistema.
No lo hacia: estaba fija en oscuro con los colores escritos en el codigo.

Se introdujo una paleta (struct Palette) con 28 colores y dos instancias,
darkPalette y lightPalette. Los nombres historicos background, card, text,
muted y accent quedan como macros que apuntan a la paleta activa, de modo que
el codigo de dibujo no cambio. Los tonos sueltos que habia repartidos pasaron
a entradas con nombre (rail, border, meter, surface, button, toggleOn, etc.).

Cuidado con las macros: escribir theme.card dentro del codigo se expande a
theme.theme.card y no compila. Dentro del dibujo hay que usar el nombre corto.

Preferencia theme_mode en config/settings.user.txt: 0 sigue a Windows (por
defecto), 1 siempre oscura, 2 siempre clara. Selector "Apariencia" en la ventana
de ajustes. Se lee AppsUseLightTheme del registro; si falta, se asume oscuro.
Con WM_SETTINGCHANGE e ImmersiveColorSet la aplicacion cambia en caliente cuando
Windows cambia de modo, solo si la preferencia es seguir al sistema. applyTheme
tambien actualiza el marco por DWM para que la barra de titulo acompañe.

El tema se aplica tambien en modo prueba, a proposito, para poder revisar
capturas de ambas apariencias. El resto de preferencias siguen sin cargarse en
las pruebas.

Fallo corregido en la ventana de ajustes: las esquinas de los botones salian
blancas. La clase de ventana no definia hbrBackground, asi que Windows pintaba
blanco debajo, y el dibujo redondeado deja las esquinas sin cubrir. Se define
el pincel de la clase y ademas cada boton limpia su rectangulo antes de dibujar,
igual que ya hacia la ventana principal.

Tiradores: el circulo del interruptor y el del slider usaban el color de texto,
que en tema claro es oscuro y quedaba feo sobre el azul. Ahora usan onAccent y
card respectivamente.

Verificacion: compila sin avisos, seis pruebas pasan, prueba WASAPI real pasa.
Capturas revisadas de las dos apariencias en build/light-main2.png y
build/dark-check.png: el tema oscuro queda identico al anterior y el claro es
legible en botones, interruptores, tarjetas y medidores.

Desplegables de la ventana de ajustes: el usuario mostro que la lista abierta se
veia mal (texto centrado, bordes blancos por fila, recuadro de foco). La causa
es que WM_MEASUREITEM y el dibujo de ODT_COMBOBOX solo existian en la ventana
principal; sin ellos Windows dibuja los elementos a su manera aunque el control
sea owner-draw. Añadidos a settingsProc, con la fila bajo el cursor en
theme.surfaceHot. Ademas se retira el tema visual de la ventana de la lista
(GetComboBoxInfo + SetWindowTheme sobre hwndList) para quitar su borde claro.

La prueba gui_controls despliega ahora la lista y guarda build/settings-open.bmp
capturando desde pantalla, porque la lista es una ventana aparte y WM_PRINT
sobre el dialogo no la incluye.

## Iconos - 12 de octubre de 2026

El usuario pidio iconos para hacer la interfaz mas atractiva. Se dibujan con
GDI+ mediante drawIcon(), con trazo redondeado y en el color que se le pase, de
modo que siguen la paleta y no hacen falta imagenes ni un juego por tema.

Doce iconos: Intensity (barras), Bass (onda lenta), Clarity (destello),
Dynamics (señal entre dos limites), Spatial (flecha doble), Restore, Pin,
Speaker, Gear, Folder, Save y Refresh. Colocados en las cinco filas de
controles (en muted) y en los siete botones (en accent, con el icono y el texto
centrados juntos: se mide el texto con DT_CALCRECT para repartir el espacio).

Tres se rehicieron tras revisar la captura: Spatial salia pequeño y apretado,
Dynamics se veia cortado como un arco a medias, y el engranaje parecia un sol.
Dynamics paso a ser una señal contenida entre dos limites y el engranaje un
poligono de radio alternante con su eje. No basta con que el icono compile:
hay que mirarlo.

Verificacion: compila sin avisos, seis pruebas pasan, prueba WASAPI real pasa,
y se revisaron capturas en ambos temas (build/icons3.png y build/icons-light.png).
Los iconos se leen bien sobre fondo oscuro y claro.

## Ventana sin marco y tamano fijo - 12 de octubre de 2026

El usuario pidio quitar la barra de titulo del sistema con sus botones y no
poder redimensionar, con botones propios de minimizar y cerrar.

La ventana pasa de WS_OVERLAPPEDWINDOW a WS_POPUP|WS_MINIMIZEBOX|WS_CLIPCHILDREN
con WS_EX_APPWINDOW. WS_MINIMIZEBOX se conserva aunque no haya marco: sin el, la
entrada en la barra de tareas y la animacion de minimizar se comportan mal. Se
retiro WM_GETMINMAXINFO, que ya no aplica. El tamano del cliente se fija al
crear, sin AdjustWindowRect, porque ya no hay marco que descontar.

Botones Minimise y Close dibujados con el resto: planos, cuadrados, con realce
al pasar el raton y el de cerrar en rojo, como Windows. Colocados en 560 y 608
dentro de la tarjeta de cabecera; cuidado al moverlos porque el interruptor DSP
ocupa de 670 a 850 en horizontal y de 30 a 170 en vertical. En un primer intento
quedaron encima de el y hubo que reubicarlos.

Arrastre: WM_NCHITTEST devuelve HTCAPTION en la franja superior (hasta 46) y
HTCLIENT en el resto, de modo que la cabecera mueve la ventana y los controles
siguen funcionando. Los botones son ventanas hijas, asi que reciben sus clics
antes que la franja.

La prueba gui_controls comprueba ahora que no hay WS_CAPTION ni WS_THICKFRAME,
que existen los dos botones, que la franja superior devuelve HTCAPTION y que la
zona de los sliders sigue siendo HTCLIENT. Verificado tambien en ejecucion real
sobre la ventana abierta.

Ajustes sin marco y botones en la esquina - 12 de octubre de 2026.
La ventana de ajustes pasa tambien a WS_POPUP: titulo "Ajustes de RESONIQ"
dibujado por nosotros, boton de cerrar propio (SetClose) en su esquina y
arrastre por la franja superior (HTCAPTION hasta 50). Su panel y sus controles
bajaron 38 para dejar sitio al titulo, y la ventana crecio a 388 de alto.

El interruptor DSP se redujo de 180x140 en 670,30 a 152x118 en 688,64, con su
dibujo interior proporcional (radio 19, trazo a 24, texto a 70). Asi los botones
de minimizar y cerrar ocupan la esquina superior derecha (768 y 812, en 24).

Error propio corregido: las comprobaciones de arrastre enviaban WM_NCHITTEST con
coordenadas de cliente, cuando ese mensaje las espera de pantalla. La de la
ventana principal pasaba por casualidad, porque la ventana caia cerca del origen
de la pantalla; la de ajustes fallo y destapo el fallo. Ambas usan ahora
GetWindowRect. Una prueba que acierta por azar no vale.

La prueba gui_controls comprueba ademas que la ventana de ajustes no tiene
WS_CAPTION, que su franja superior arrastra y que estan sus nueve controles.

## Limitador con anticipacion: probado y descartado - 12 de octubre de 2026

Se implemento un limitador con anticipacion (1.5 ms, buffer fijo de 192
muestras, sin reservas en el hilo de audio) para aprovechar los 9 dB de margen
de pico que quedaban sin usar a volumen alto.

No funciono y se revirtio. Medido sobre la misma musica: el limitador de
muestra da RMS -12.36 dB con margen de pico 11.34; el de anticipacion, RMS
-20.64 con margen 12.70. Es decir, 8.3 dB menos de nivel y peor margen: recorta
de mas. Se probaron dos variantes (maximo plano de la ventana, y rampa de
ataque a lo largo de ella) y ninguna mejoro. tonal_validation lo detecto.

Motivo probable, no confirmado: la ganancia se calcula sobre el pico maximo de
toda la ventana y se aplica a muestras que no lo necesitan. Un limitador con
anticipacion correcto necesita una envolvente que llegue a su minimo justo en
el pico y no antes, y eso requiere mas trabajo del que se hizo aqui. Si se
retoma, medir RMS y margen de pico antes de darlo por bueno; la reduccion
reportada por si sola engaña, porque mas reduccion puede significar peor
resultado.

Motor restaurado y verificado identico bit a bit al render conocido del perfil
denso. Seis pruebas pasan.

## Latencia real medida - 12 de octubre de 2026

Se añadio medicion real en lugar de estimaciones: GetStreamLatency de los dos
endpoints, mas el buffer de render, mas la ocupacion instantanea del FIFO.
Campo latencyMs en LiveState y en la telemetria.

Medida en este Lenovo con la ruta completa: 22 ms. Es latencia de ida y vuelta
de la ruta virtual a fisica. Por debajo del umbral en el que el desfase se nota
viendo video. No es latencia de instrumento en directo, pero la aplicacion no
esta pensada para eso.

## Reconocimiento automatico de equipo - 12 de octubre de 2026

Objetivo del usuario: que el efecto funcione en cualquier equipo, como Bongiovi.
Bongiovi no tenia un algoritmo generico: tenia una base de datos de perfiles
medidos por modelo. Se implementan los dos caminos acordados.

Modulo nuevo include/dps/hardware.hpp y src/io/hardware.cpp, compilado solo en
el ejecutable (usa API de Windows; la biblioteca dps sigue limpia). Lee
fabricante, modelo y familia del registro de BIOS en lugar de WMI: sin COM, sin
depender del servicio, y es el mismo dato. Portatil se deduce de la presencia de
bateria, mas fiable que el tipo de chasis, que muchos firmwares rellenan mal.

suggestCalibration decide en tres niveles, y dice cual aplica:
- Measured: tabla de equipos con perfil preparado por alguien que los escucho.
  De momento solo el Lenovo Legion Pro 5 83LU, con Laptop Impact+.
- Inferred: no esta medido, pero sabemos si es portatil o si la salida parece
  auriculares. Los altavoces de portatil comparten el mismo problema, asi que
  se aplica una version moderada de la correccion de portatil.
- Generic: nada util conocido, ajuste neutro.

Los auriculares se comprueban primero: aplicarles un corte de graves pensado
para altavoces pequenos seria peor que no hacer nada.

Boton "Detectar" en la GUI, y aplicacion automatica en el primer arranque
(cuando no hay sesion guardada), para no imponer un perfil de otro portatil.

Verificado en este equipo: detecta LENOVO / 83LU / Legion Pro 5 16IAX10H,
portatil, y elige Laptop Impact+ como medido. La prueba gui_controls comprueba
la deteccion y, sobre todo, que una salida de auriculares nunca recibe un perfil
de altavoz ni un corte de graves. Informe en build/hardware-detect.txt.

Limitacion honesta: la tabla de equipos medidos solo crece con equipos reales
delante. Eso es justo lo que Bongiovi tenia y este proyecto no. Lo inferido es
una conjetura razonable, no una medicion, y asi se le dice al usuario.

## Medicion acustica por microfono - 12 de octubre de 2026

El usuario quiere un sistema que recoja datos para crear perfiles. Se le explico
que la telemetria por software describe la senal, no el altavoz, y que la unica
via que crea perfiles de verdad es medir con microfono. Acepto, con la condicion
expresa de pedir permiso para usar el microfono y para tomar la medicion.

Hecho en esta iteracion (nucleo, probado):
- include/dps/measure.hpp y src/dsp/measure.cpp. Senal de 8 tonos (60 Hz a
  8 kHz) con desvanecido en cada extremo, para no medir el chasquido. Analisis
  por Goertzel sobre la parte estable de cada tono.
- La referencia es la zona media (250 Hz a 1 kHz), no la media de todas las
  bandas. Con la media, los graves ausentes de un portatil la hunden tanto que
  todo lo demas parece realce y la calibracion sale al reves. Este fallo lo
  destapo la prueba, no el razonamiento.
- El cuerpo se mide comparando 120-250 Hz contra la zona media alta; compararlo
  contra los medios seria compararlo consigo mismo y daria siempre cero.
- Una medicion con menos de 12 dB sobre el ruido de sala se rechaza y no
  produce calibracion. Entre 12 y 20 dB se acepta pero se avisa.
- captureMeasurement en src/audio/wasapi.cpp: reproduce los tonos en la salida
  mientras graba del microfono, con un segundo de sala antes para el suelo de
  ruido. Devuelve ambas grabaciones, sin analizar nada.
- Prueba nueva measure_validation (septima): altavoz plano mide plano (<0.07 dB
  de error), un portatil simulado da corte 162 Hz, cuerpo 4.0 y presencia 4.2
  (cerca del perfil que el usuario aprobo a oido), y se rechazan tanto el exceso
  de ruido como una grabacion truncada.

Pendiente, acordado con el usuario:
- Dialogo de permiso ANTES de tocar el microfono, explicando que se graba, que
  se analiza en local, que no se envia nada y que la grabacion se descarta.
  Preguntar en cada medicion, sin recordar la respuesta.
- Ventana de resultado con las ocho bandas y la calibracion propuesta, y que
  sea el usuario quien decida aplicarla. Nada se aplica solo.
- Boton para lanzar la medicion y seleccion de microfono.

Advertencia a conservar: el microfono de un portatil mide con sesgo y desde una
posicion mala. El resultado es un punto de partida para escuchar, no una
calibracion de laboratorio, y asi debe presentarse al usuario.

Medicion completada y usable - 12 de octubre de 2026. Boton "Medir" en la fila
inferior (seis botones). El flujo respeta lo que pidio el usuario:

1. Busca un microfono entre las entradas, descartando las virtuales. Si no hay,
   lo dice y no hace nada.
2. Pide permiso ANTES de tocar el microfono, con un aviso que nombra el
   microfono concreto, dice que suena durante unos 5 segundos, que se analiza en
   el equipo, que no se envia nada y que la grabacion se descarta. El boton por
   defecto es Cancelar. Se pregunta en cada medicion, sin recordar la respuesta.
3. Detiene la ruta en directo para liberar la salida, mide y la restaura.
4. Muestra las ocho bandas con una barra de texto, la calibracion propuesta y el
   aviso de que el microfono del portatil no es de medicion. El boton por
   defecto es Cancelar: no se aplica nada sin decirlo expresamente.
5. Solo al aceptar se aplica, y se avisa de escucharla antes de fijarla.

Detalle a conservar: la calibracion medida se guarda bajo el nombre de
dispositivo "Laptop Impact", no "Personalizado". Ese ultimo es solo una etiqueta
del desplegable y no lo acepta el formato de perfil: usarlo haria fallar el
guardado de sesion al cerrar.

Otro detalle: en el dibujo de la GUI no se puede declarar una variable llamada
text, porque text es la macro del color de la paleta. Paso en showMeasurement y
rompio la compilacion.

Siete pruebas pasan, prueba WASAPI real pasa, captura revisada con los seis
botones sin solapar. La medicion real con microfono queda pendiente de que el
usuario la ejecute: no se ha probado con sonido real en la habitacion.

Iconos repetidos y confusos - correccion. El usuario señalo que "Detectar" y
"Actualizar" compartian icono y que ademas no se entendia que representaba. Las
dos cosas eran ciertas: la flecha circular estaba mal dibujada (la punta no
caia en el extremo abierto del arco, asi que no se leia como flecha) y Detect
reutilizaba ese mismo glifo.

Flecha circular rehecha: arco de 300 grados con la punta sobre el extremo
abierto. Dos iconos nuevos: Chip (un procesador con sus patillas) para Detectar,
y Mic (microfono con pie) para Medir. "Ajuste por defecto" conserva la flecha
de deshacer, que es distinta y significa volver atras.

Leccion repetida: los iconos hay que mirarlos, no basta con que compilen. Es la
segunda vez en el proyecto que un icono dibujado "correctamente" no se entendia.

## Por que la medicion rechazaba siempre - corregido

El usuario midio tres veces en silencio y las tres se rechazaron por ruido de
fondo. Era un fallo propio, con dos causas, y ninguna era el ruido.

1. Desalineacion. El analisis asumia que el primer tono empieza en la muestra 0
   de la grabacion. No es cierto: el audio tarda en llegar al altavoz y volver
   por el microfono. Cada banda se medía en el sitio equivocado, caía sobre un
   silencio y el conjunto parecia ruido. Ahora se busca el inicio real del
   primer tono (ventana de 10 ms sobre cuatro veces el suelo de ruido) y todas
   las bandas se desplazan con el.
2. El margen de ruido se calculaba con el RMS de toda la grabacion, que incluye
   los huecos entre tonos y la cola de silencio. Ahora se mide sobre las
   muestras de cada tono y se toma la banda mas debil, que es la que decide si
   la medicion vale.

Dos intentos equivocados por el camino, ambos destapados por la prueba: medir
la banda mas fuerte (aceptaba ruido) y sumar la referencia a los niveles de
banda para obtener un absoluto (los niveles son relativos, el resultado daba
signal=29 dB, un valor sin sentido).

Descartado tras comprobarlo: el microfono del Lenovo es estereo 48 kHz 32 bits,
asi que la lectura de dos canales era correcta; y la dilucion del RMS por los
huecos era de solo 1.7 dB, insuficiente para explicar el rechazo.

Diagnostico anadido para la proxima vez: build/measure-debug.txt con niveles,
margen y microfono usado, y build/measure-recorded.wav con lo grabado.

## DESCARTADO: "el microfono estaba silenciado" (era falso)

La seccion siguiente se escribio creyendo que el microfono estaba silenciado.
NO LO ESTABA: la lectura COM que lo "demostro" estaba mal declarada y devolvio
basura. El usuario lo desmintio y tenia razon. Se conserva como registro del
error, no como diagnostico. La causa real esta mas abajo.

El usuario seguia sin poder medir. El diagnostico que se habia añadido lo
resolvio en una lectura: recorded_peak=0 y silence_peak=0. No era ruido de
fondo: el microfono no captaba absolutamente nada.

Se comprobo con un programa aparte, fuera de RESONIQ: grabando dos segundos del
microfono por defecto, pico 0.000000 con 96000 muestras. Y leyendo el volumen
del endpoint: silenciado=True, volumen=82%. El microfono del Lenovo estaba
silenciado en Windows, probablemente por la tecla de funcion del portatil.

Correcciones:
- Antes de medir se comprueba si el microfono esta silenciado o con el volumen
  por debajo del 20%, y se dice exactamente eso, mencionando la tecla de
  funcion. Antes el usuario recibia "demasiado ruido de fondo" y buscaba un
  problema que no existia.
- analyseSweep distingue una grabacion vacia (señal y ruido por debajo de
  -110 dB) de una sala ruidosa, con mensajes distintos.

Leccion: un mensaje de error que describe la causa equivocada cuesta mas tiempo
que no tener mensaje. El diagnostico volcado a build/measure-debug.txt fue lo
que permitio verlo; sin el se habria seguido ajustando umbrales a ciegas.

Detalle de compilacion: en apps/gui_main.cpp hay macros de paleta llamadas
muted, text, card y accent. Cualquier campo con uno de esos nombres (como
Endpoint::muted) se expande mal. Se lee con la funcion auxiliar isMuted().

## Microfono: diagnostico completo y correccion

Correccion importante: en una iteracion anterior se afirmo que el microfono
estaba silenciado. Era FALSO. La comprobacion se hizo con una interfaz COM mal
declarada desde PowerShell y devolvio un valor basura. El usuario lo desmintio y
tenia razon. No dar por buena una lectura COM improvisada sin contrastarla.

Lo que si estaba activo: procesado de voz sobre el microfono (Realtek ELEVOCAPO
y Nahimic, visibles en FxProperties del endpoint de captura). Con el activo el
microfono captaba 0.0003 de pico; tras desactivar "mejoras de audio" en
mmsys.cpl paso a 0.04. En Windows 11 esa opcion no esta en Configuracion:
hay que abrir el panel clasico con mmsys.cpl, pestaña Grabar.

Pero el fallo principal era propio, no del equipo. Se comprobo con un programa
aparte que reproduce un tono de 1 kHz mientras graba: pico 2.33 en el microfono,
es decir, el camino altavoz-microfono funciona perfectamente.

Causa real: captureMeasurement leia el microfono desde el bucle del render,
esperando el evento de la SALIDA. El microfono tiene su propio ritmo y su propio
evento; cuando el bucle llegaba a leerlo, sus paquetes ya se habian descartado.
Por eso solo quedaban los transitorios de arranque de cada tono (9 ventanas de
100 ms en toda la grabacion) y no los tonos sostenidos.

Arreglo: el microfono se lee en un hilo propio con su evento, separando el
segundo de sala de la parte con tonos mediante una bandera. El bucle de
reproduccion solo reproduce. Pendiente de que el usuario lo pruebe.

Otros fallos corregidos en el camino, todos destapados midiendo:
- El analisis asumia que el primer tono empieza en la muestra 0. No es cierto
  por el retardo del camino; ahora se busca el inicio real hasta step*4, porque
  un portatil no reproduce 60 ni 120 Hz y el primer tono audible puede ser el
  tercero.
- El margen de ruido se juzgaba con el RMS de toda la grabacion (incluidos los
  huecos) y luego con la banda mas debil (que en un portatil son los graves
  inaudibles). Ahora se juzga con los medios, bandas 2 a 5.
- Un intento sumaba la referencia a los niveles relativos de banda para obtener
  un absoluto: daba signal=29 dB, un valor imposible que delato el error.

Herramientas de diagnostico que quedan: build/measure-debug.txt (niveles,
margen, microfono) y build/measure-recorded.wav (lo grabado). Sin ellas se
habrian seguido ajustando umbrales a ciegas.

## Primera medicion valida y perfil "Medido" - 12 de octubre de 2026

La medicion funciono tras corregir la captura. Margen 38.5 dB sobre ruido.
Respuesta de los altavoces del Legion, relativa a los medios:
60 Hz -50.5 / 120 Hz -26.9 / 250 Hz -9.7 / 500 Hz +7.8 / 1 kHz +1.9 /
2 kHz -13.4 / 4 kHz -23.4 / 8 kHz -3.2.

Coherente con altavoz de portatil: nada abajo, pico en 500 Hz (caja rigida),
hundimiento en 2-4 kHz. Se verifico la alineacion probando desfases: la mejor
es 50 ms, o sea estaba bien alineada; una sospecha de desalineacion resulto
infundada.

El repunte de 8 kHz se considera artefacto de posicion: el microfono esta en la
tapa y la pantalla refleja agudos hacia el. Por eso calibrationFrom lo excluye.

Fallo de guardar perfil, causa real: al aplicar una medicion se ponia
deviceName="Laptop Impact" para que el formato lo aceptara, pero entonces el
desplegable mostraba "Portatil: impacto" mientras sonaba la calibracion medida.
La interfaz mentia, y tocar el desplegable borraba la medicion sin aviso.

Arreglo: nombre de dispositivo "Measured" ("Medido en este equipo", septima
entrada del desplegable). deviceCalibration lo acepta devolviendo los valores
de Laptop Impact solo como respaldo; los valores reales viven en el perfil.
Reseleccionarlo no sobrescribe la medicion.

calibrationFrom afinado tras ver la medicion real:
- Presencia: dos tercios de la caida medida, tope 10 dB. Corregir del todo
  sobrepasa, porque el microfono colorea esa zona.
- Cuerpo: 250 Hz contra 500 Hz y 1 kHz (los medios que el altavoz si da), la
  mitad de la caida. Antes se comparaba contra 4 kHz, que esta muy hundido, y
  salia 0; y contra 120 Hz salia 23 dB, imposible de entregar.

Resultado con la medicion del usuario: corte 162 Hz, cuerpo 7.3, presencia 10.
Su perfil ajustado a oido tiene 175 / 11 / 10. Practicamente lo mismo: la
medicion confirma lo que escucho.

## Cierre de la sesion del 12 de octubre de 2026

Estado: compila sin avisos, siete pruebas CTest pasan, prueba WASAPI real pasa.
Ajuste por defecto del usuario intacto (hash 5119695b...), sesion con
Laptop Impact+. Nada a medio hacer.

Lo que el usuario tiene pendiente de decidir, no de arreglar:
- Si se queda con su perfil ajustado a oido o prueba el medido. Son casi
  iguales (175/11/10 frente a 162/7.3/10), asi que no hay urgencia.
- Confirmacion visual de que no reaparecen tiradores clasicos al arrastrar.

Siguiente trabajo acordado pero sin empezar: instalacion de verdad (acceso
directo, instalador; el ejecutable vive en build/Release) y, solo si se
distribuye, telemetria con envio opcional y consentimiento.

Resumen para Codex en docs/CODEX_HANDOFF.md, actualizado al cerrar.

## Verificacion independiente de la auditoria

Comprobado despues de la auditoria de Codex, sin fiarse del informe:
once pruebas CTest pasan, prueba WASAPI real pasa, ajuste por defecto con su
hash de siempre (5119695b...) y render del perfil denso identico bit a bit al
anterior. La aplicacion arranca y se relanzo con la sesion del usuario.

Los cuatro hallazgos eran fallos reales, y tres los habia introducido Claude en
esta misma sesion:

1. El dialogo de medicion prometia descartar la grabacion y a la vez escribia un
   WAV en disco. En una funcion donde se pide permiso expreso al usuario, el
   programa hacia algo distinto de lo que decia. Es el hallazgo mas importante.
2. saveProfile escribia directamente sobre el archivo anterior: un corte a mitad
   dejaba el perfil truncado. Ademas no fijaba la configuracion regional, asi
   que en un Windows con coma decimal los perfiles saldrian ilegibles.
3. Si algo fallaba tras crear el hilo de captura, el hilo quedaba sin recoger y
   el proceso terminaba de golpe.
4. La alineacion asumia que el primer sonido detectado era el tono de 60 Hz. En
   un portatil ese tono no se reproduce, asi que el primero audible es el de
   250 Hz y todas las bandas quedaban desplazadas. En la medicion del usuario
   acerto por casualidad (desfase optimo 50 ms); en otro equipo habria dado una
   respuesta falsa.

Dos correcciones de documentacion que afectan a afirmaciones hechas al usuario:
los 22 ms de latencia son una estimacion por tamaño de buffers, no una medicion
del recorrido real, y se le presentaron como medicion; y la tabla de modelos
contiene ajustes aprobados escuchando, no calibraciones de laboratorio.

Leccion: el patron comun de los cuatro es afirmar mas certeza de la que habia,
o asumir un caso que no se comprobo. Es el mismo patron que ya costo tiempo con
el microfono. Ver [[resoniq-medir-antes-de-afirmar]] en la memoria personal.

## Punto de retomada tras la auditoria - 12 de octubre de 2026

Informe completo en docs/AUDIT.md. Veinte hallazgos corregidos, refactorizacion
en modulos, once pruebas CTest. Verificado de forma independiente: sonido
identico bit a bit, favorito con su hash, ruta WASAPI pasa.

Dos hallazgos del informe que conviene no perder de vista, porque afectan a lo
que el usuario ya hizo:

- El WAV de diagnostico etiquetaba mono como estereo, asi que duraba la mitad.
  La grabacion que se analizo para darle la respuesta de sus altavoces estaba
  mal formada; los numeros salieron bien porque el analisis lee las muestras
  directamente, no por rigor.
- La opcion "Medido" podia aplicar una calibracion de respaldo sin haber medido.
  Se habia añadido en esta misma sesion para arreglar el guardado.

PRIMERO AL RETOMAR: pedir al usuario que repita la medicion de su habitacion.
La alineacion, el manejo de errores y el COM cambiaron tras su unica medicion
valida, y eso solo tiene cobertura sintetica. Comparar con lo anterior:
corte 162 Hz, cuerpo 7.3 dB, presencia 10 dB. Si sale parecido, confirma que la
refactorizacion no rompio nada.

Limites conocidos que el informe documenta y conviene recordar antes de
prometer nada al usuario:
- La medicion bloquea la GUI unos segundos. Deberia moverse a una tarea con
  cancelacion.
- La deteccion por modelo puede confundir unos altavoces externos conectados a
  un portatil conocido con los internos. Falta topologia de endpoint.
- No hay restauracion automatica de la salida fisica si desaparece: el
  transporte virtual puede quedar sin sonido.
- Limitador sin true peak, armonicos sin sobremuestreo, diferencias de fase
  entre ruta seca y procesada. Tocarlo afecta al sonido aprobado.
- Falta medicion fisica de latencia, instalador y firma de produccion.
- build/ contiene material de desarrollo y una clave privada de firma de
  pruebas: NO es un paquete publicable.

Nada de esto es trabajo urgente. El usuario tiene un sonido que le gusta y el
proyecto esta estable.
