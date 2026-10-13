# Traspaso a Codex — sesión del 12 de octubre de 2026

**Ensayo temporal nuevo:** `config/attack-body.user.dps`, favorito más acento
corto `transient_accent_db 4`. Conserva EQ/densidad/AGC/RAW. Mejora técnica modesta
de contraste, con pequeña pérdida relativa de cuerpo; escucha pendiente.
No cargarlo como sesión ni adoptar como defecto. Ver `docs/ATTACK_EXPERIMENT.md`.
Los ensayos de ralentizar compresión no funcionaron y se retiraron.

**Evaluación de Intensidad alineada:** el usuario no nota absolutamente ninguna
diferencia. No adoptar como defecto ni considerar la correccion sintetica una
mejora audible. Ensayo conservado como referencia tecnica; favorito intacto.

**Intensidad experimental:** ver `docs/INTENSITY_EXPERIMENT.md`. Perfil separado
`config/intensity-aligned.user.dps`, campo opcional `aligned_intensity`. Reduce
cancelación en las pruebas, conserva extremos y favorito. Trece pruebas.
No activar como sesión/defecto sin que el usuario la escuche y decida.

**Medición comprobada otra vez en este equipo:** el rechazo por discontinuidad
era excesivo. Dos capturas autorizadas identificaron un hueco de 480 muestras al
inicio. Ahora los huecos se rellenan para preservar tiempo y solo invalidan si
solapan ventanas de análisis. La segunda captura fue válida; no se guardó audio
ni se aplicó calibración. Doce pruebas pasan. Ver PROJECT_MEMORY.

**Consolidación posterior:** ver `docs/CONSOLIDATION.md`. Medición con alineación
conjunta de tonos medios, coherencia, corte continuo, rechazo de discontinuidad,
RAW de la sesión y diagnóstico de las ocho bandas. Requiere nueva comprobación
de habitación, con permiso. Nueva herramienta `dps_quality` ha cuantificado
cancelación por fase en intensidades intermedias; el perfil aprobado no cambia.

**Corrección posterior de volumen cero:** al silenciar Windows, el loopback
virtual seguía entregando mezcla sin su atenuación previa. El host ahora observa
mute/volumen cero y silencia después del DSP, también en bypass. El DSP y el
volumen no nulo no se modifican. Doce pruebas pasan y la prueba con notificaciones
reales confirma salida protegida exactamente cero. Detalles en PROJECT_MEMORY.

> **Posterior a este documento:** Codex auditó y refactorizó el proyecto. Informe
> en `docs/AUDIT.md`. Encontró cuatro fallos reales, tres introducidos en la
> sesión que aquí se describe, y corrigió dos afirmaciones demasiado seguras
> (la latencia es una estimación; los perfiles por modelo están aprobados por
> escucha, no medidos en laboratorio). Verificado de forma independiente: once
> pruebas pasan, el sonido es idéntico bit a bit y el ajuste del usuario conserva
> su hash. Lo de abajo sigue siendo válido salvo donde el informe lo corrige.

**Auditoría y refactorización posteriores:** consultar `docs/AUDIT.md` para el
estado actual y sus verificaciones. Preferencias, sesión de audio, paleta e
iconos se separaron de la GUI; se eliminaron las macros de colores. El guardado
de perfiles/preferencias/WAV usa reemplazo tras escritura completa. Hay once
pruebas CTest. La captura maneja excepciones y COM MTA; la alineación distingue
el primer tono audible de la primera banda. La grabación se descarta salvo
`--measurement-diagnostics`, con aviso en el consentimiento. La latencia de
telemetría es una estimación de software, no una medición física de extremo a
extremo. Estas notas sustituyen las descripciones históricas correspondientes.

**Actualizacion posterior, 12 de octubre de 2026:** el usuario confirma que la
medicion por microfono funciona con la captura corregida. La comprobacion real
indicada como pendiente en las secciones 4 y 7 queda cerrada. No implica una
validacion de precision acustica ni la aprobacion de la calibracion propuesta.

Escrito para Codex, que conoce este proyecto desde su inicio. Resume qué cambió
en esta sesión, qué está sin verificar y dónde hay trampas.

Estado al cerrar: compila sin avisos, **siete** pruebas CTest pasan (antes seis),
prueba WASAPI real pasa. Ajuste por defecto del usuario intacto, hash
`5119695b…`. Nada a medio hacer en el árbol.

---

## 1. El objetivo cambió de naturaleza

El usuario declaró explícitamente que RESONIQ debe ser **un sustituto de Bongiovi
DPS**, no una aplicación de mejora genérica. Ese es ahora el criterio de éxito,
por encima de que las pruebas pasen.

Conviene saber, porque orienta las decisiones: Bongiovi no tenía un algoritmo
mágico, tenía una **base de datos de perfiles medidos por modelo**. Su ventaja
estaba en el laboratorio, no en el código.

**Hito: el usuario aprobó un ajuste por primera vez en el proyecto.** Tras muchas
iteraciones de «apenas noto diferencia», dijo que el perfil denso le convence y
pidió fijarlo como su ajuste por defecto. Ese perfil es la referencia a conservar.

---

## 2. Motor DSP

### Tres parámetros nuevos de calibración

Todos opcionales en el formato de perfil, con el valor por defecto igual al
comportamiento histórico: **ningún perfil existente cambia de sonido**. Verificado
con render idéntico bit a bit del favorito anterior y del nuevo.

| Parámetro | Rango | Qué hace |
|---|---|---|
| `adaptive_depth` | 0–0.9 (def. 0.7) | Cuánto se retira el realce con el nivel. Bajo = mantiene el efecto con material fuerte. |
| `loud_density` | 1–2.5 (def. 1) | Empuja el objetivo del AGC por encima de la entrada en material fuerte. |
| `band_density` | 1–4 (def. 1) | Baja el umbral del compresor de bandas y sube la relación a la vez. |

### El hallazgo que importa: el AGC cancelaba la compresión

El compresor de bandas solo actuaba por encima de 0.16 (≈ −16 dBFS por banda) y
hasta 4:1, así que el cuerpo de la música pasaba sin comprimir. Al bajar el
umbral hizo falta añadir ganancia de compensación… y seguía sin notarse.

La causa real estaba después: **el AGC mide el pico de la señal ya procesada,
detecta la compensación y baja su ganancia en la misma proporción**. Se
cancelaban. Verificado congelando el AGC en una copia: con él congelado el
compresor daba +4.38 dB y margen de pico 10.10; con él activo, nada.

Arreglo: el AGC descuenta la compensación de bandas (`bandMakeup_`) de lo que
mide. Resultado con `band_density 1.8`: efecto en pasaje alto de +3.12 a
+4.71 dB, margen de pico de 11.34 a 9.77.

**Aviso:** la variación corta baja de 0.64 a 0.50 al subir ese parámetro. Por eso
se eligió 1.8 y no 2.6: a partir de ahí la ganancia crece poco y la música se
aplana.

### Limitador con anticipación: probado y descartado

Implementado (1.5 ms, buffer fijo, sin reservas en el hilo de audio) y revertido
tras medirlo. Dos variantes, las dos peores:

| Limitador | RMS | Margen de pico |
|---|---|---|
| De muestra (actual) | −12.36 dB | 11.34 |
| Con anticipación | −20.64 dB | 12.70 |

Pierde 8.3 dB de nivel y empeora el margen. `tonal_validation` lo detectó. El
motor quedó restaurado y verificado idéntico bit a bit.

Si se retoma: la envolvente debe llegar a su mínimo **justo** en el pico, no
antes. Y medir RMS y margen, nunca «cuánto reduce»: más reducción puede
significar peor resultado.

---

## 3. Perfiles y reconocimiento de equipo

### Perfiles nuevos

- `Laptop Impact` — el ajuste que el usuario aprobó. Corte 175 Hz, cuerpo 300/11,
  presencia 3000/10, armónicos 1.6 con foco .85, mud −6, `adaptive_depth` .15,
  `loud_density` 1.45.
- `Laptop Impact+` — igual más `band_density` 1.8. Es el que está fijado como
  ajuste por defecto del usuario.

Ambos en `deviceCalibration()`, en el desplegable de la GUI y como perfil
distribuido en `profiles/`.

### Detección automática (`hardware.hpp` / `hardware.cpp`)

Compilado solo en el ejecutable: usa API de Windows, la biblioteca `dps` sigue
limpia. Lee fabricante/modelo/familia del registro de BIOS (sin COM, sin WMI).
Portátil se deduce de la presencia de batería, más fiable que el tipo de chasis.

Tres niveles, y dice cuál aplica: **medido** (tabla de equipos; de momento solo
el Legion Pro 5 83LU), **inferido** (portátil o auriculares), **genérico**.

Los auriculares se comprueban **primero**: aplicarles el corte de graves de
altavoces pequeños sería peor que no hacer nada. Cubierto por la prueba.

---

## 4. Medición acústica por micrófono

Módulo nuevo `measure.hpp` / `measure.cpp`, con prueba propia
(`measure_validation`, la séptima).

Ocho tonos de 60 Hz a 8 kHz con desvanecido en los extremos; análisis por
Goertzel sobre la parte estable. El usuario **exigió permiso explícito** antes de
tocar el micrófono y antes de aplicar nada: el diálogo nombra el micrófono, dice
que no se envía nada, y el botón por defecto es Cancelar en los dos pasos.

### Cuatro errores de diseño propios, todos destapados midiendo

1. **Referencia equivocada.** Usaba la media de todas las bandas; en un portátil
   los graves ausentes la hunden tanto que todo lo demás parece realce y la
   calibración sale al revés. Ahora la referencia son los medios (250 Hz–1 kHz).
2. **Cuerpo medido contra sí mismo.** Se comparaba con las bandas de referencia,
   lo que siempre da cero. Ahora 120–250 Hz contra la zona media alta.
3. **Alineación asumida.** El análisis daba por hecho que el primer tono empieza
   en la muestra 0. No es cierto por el retardo del camino, y además un portátil
   no reproduce 60 ni 120 Hz, así que el primer tono audible puede ser el
   tercero. Ahora se busca el inicio real hasta `step*4`.
4. **Margen de ruido mal juzgado.** Primero con el RMS de toda la grabación
   (diluido por los huecos), luego con la banda más débil (los graves inaudibles
   del portátil). Ahora con los medios, bandas 2–5.

### El fallo de captura, que es el relevante

`captureMeasurement` leía el micrófono **desde el bucle del render**, esperando
el evento de la salida. El micrófono tiene su propio ritmo y su propio evento:
cuando el bucle llegaba a leerlo, sus paquetes ya se habían descartado. Solo
quedaban los transitorios de arranque de cada tono — 9 ventanas de 100 ms en toda
la grabación.

Arreglo: el micrófono se lee en **hilo propio con su evento**, y una bandera
separa el segundo de sala de la parte con tonos.

**Verificado:** el usuario midió con la captura corregida y funcionó. Margen de
38.5 dB sobre el ruido.

### La medición de su Legion Pro 5

Relativa a los medios:

| Banda | Nivel | Lectura |
|---|---|---|
| 60 Hz | −50.5 | ausente, el altavoz no lo da |
| 120 Hz | −26.9 | casi ausente |
| 250 Hz | −9.7 | zona de cuerpo, hundida |
| 500 Hz | +7.8 | pico de caja rígida |
| 1 kHz | +1.9 | referencia |
| 2 kHz | −13.4 | presencia hundida |
| 4 kHz | −23.4 | presencia hundida |
| 8 kHz | −3.2 | **artefacto, ver abajo** |

Se comprobó la alineación probando desfases: la mejor es 50 ms, o sea estaba
bien alineada. Una sospecha de desalineación resultó infundada.

El repunte de 8 kHz se considera artefacto de posición: el micrófono está en la
tapa y la pantalla refleja agudos hacia él. `calibrationFrom` lo excluye.

### Lo que valida el trabajo anterior

| | Corte | Cuerpo | Presencia |
|---|---|---|---|
| Perfil del usuario, ajustado a oído | 175 Hz | 11 dB | 10 dB |
| Propuesta de la medición | 162 Hz | 7.3 dB | 10 dB |

Prácticamente lo mismo. El perfil que el usuario ajustó escuchando coincide con
lo que mide el micrófono. Es la mejor validación que ha tenido el proyecto.

### Problema del equipo del usuario, ya resuelto

Su micrófono tenía procesado de voz activo (Realtek `ELEVOCAPO` y Nahimic). Con
él captaba 0.0003 de pico; al desactivar «mejoras de audio» pasó a 0.04 y luego
midió bien. En Windows 11 esa opción **no está en Configuración**: hay que abrir
`mmsys.cpl`, pestaña Grabar. Si otro equipo falla igual, mirar eso primero.

### Perfil de dispositivo «Measured»

Al aplicar una medición se ponía `deviceName="Laptop Impact"` para que el formato
lo aceptara. Consecuencia: el desplegable mostraba «Portátil: impacto» mientras
sonaba la calibración medida — **la interfaz mentía** — y tocar el desplegable
borraba la medición sin aviso. Eso era el «error al guardar perfil» que reportó
el usuario.

Arreglado con un nombre propio, `Measured` («Medido en este equipo», séptima
entrada). `deviceCalibration()` lo acepta devolviendo Laptop Impact solo como
respaldo; los valores reales viven en el perfil. Reseleccionarlo no los
sobrescribe.

### `calibrationFrom` afinado con la medición real

- **Presencia**: dos tercios de la caída medida, tope 10 dB. Corregir del todo
  sobrepasa, porque el micrófono colorea esa zona.
- **Cuerpo**: 250 Hz contra 500 Hz y 1 kHz (los medios que el altavoz sí da), la
  mitad de la caída. Comparar contra 4 kHz daba 0 (está muy hundido) y contra
  120 Hz daba 23 dB, imposible de entregar.

## 5. Interfaz

Cambios acumulados, todos a petición del usuario:

- **Icono de bandeja**: cerrar la ventana esconde la aplicación y **sigue
  procesando**; se sale desde el menú del icono. Los modos de prueba no entran en
  bandeja, para que las pruebas terminen solas.
- **Ventana de ajustes**: inicio con Windows, arranque minimizado, cerrar = salir,
  reconexión automática de audio, estado del DSP al arrancar, apariencia.
  Preferencias en `config/settings.user.txt`, separadas de los perfiles.
- **Temas claro / oscuro / sistema**: paleta de 28 colores con dos instancias.
  Sigue a Windows por defecto y cambia en caliente.
- **Ventanas sin marco**: ambas ventanas sin barra de título del sistema, tamaño
  fijo, botones propios, arrastre por la franja superior.
- **Iconos** dibujados con líneas, que siguen la paleta.
- **Medidor «Realce»** sustituye al de «Reducción», que marcaba 0.0 permanente
  porque a volumen normal ni el compresor ni el limitador actúan.
- **Renombrado**: «favorito» → «Ajuste por defecto» en todo lo visible. El archivo
  sigue siendo `favorite.user.dps` y la función `recoverFavorite()`: **no
  renombrar el archivo sin migrar el existente**.
- **Latencia real medida**: 22 ms en este equipo, en la telemetría.

---

## 6. Trampas de este código

Cosas que han roto la compilación o el comportamiento más de una vez:

- En `apps/gui_main.cpp` hay **macros de paleta** llamadas `muted`, `text`,
  `card`, `accent`, `background`. Cualquier variable o campo con esos nombres se
  expande mal. `Endpoint::muted` se lee con la función `isMuted()`. Y dentro del
  dibujo hay que escribir `card`, no `theme.card`, o sale `theme.theme.card`.
- Los `\n` dentro de cadenas C++ se convierten en saltos reales si el script que
  edita el archivo no los escapa bien. Ha roto la compilación unas seis veces.
  Conviene verificar la línea después de editarla.
- `WM_NCHITTEST` espera **coordenadas de pantalla**. Una comprobación con
  coordenadas de cliente pasaba por casualidad porque la ventana caía cerca del
  origen; al replicarla en la ventana de ajustes falló y destapó el error.
- El perfil de dispositivo guardado debe ser un nombre que acepte
  `deviceCalibration()`. «Personalizado» es solo una etiqueta del desplegable:
  usarlo haría fallar el guardado de sesión al cerrar.
- Los iconos hay que **mirarlos**. Dos veces se dibujaron iconos correctos que no
  se entendían (un engranaje que parecía un sol, una flecha circular que parecía
  un arco cortado).

---

## 7. Pendiente

**Decisión del usuario, no trabajo pendiente:**
1. Si se queda con su perfil ajustado a oído o adopta el medido. Son casi
   iguales, así que no hay urgencia ni una respuesta mejor que otra.
2. Confirmación visual de que no reaparecen tiradores clásicos al arrastrar.

Si una medición vuelve a fallar en otro equipo, mirar `build/measure-debug.txt`
y `build/measure-recorded.wav` **antes** de tocar umbrales. Esos dos archivos
resolvieron en una lectura lo que varias hipótesis razonadas no.

**Acordado, sin empezar:**
- Instalación de verdad: acceso directo, inicio con Windows ya existe como
  preferencia pero el ejecutable vive en `build/Release`.
- Telemetría con envío opcional para construir la base de perfiles. Solo tiene
  sentido si se distribuye, y requiere servidor y consentimiento explícito.

**Limitación estructural que conviene no olvidar:** la tabla de equipos medidos
solo crece con equipos reales delante. Es exactamente lo que Bongiovi tenía y
este proyecto no. Lo inferido es una conjetura razonable, no una medición, y así
se le presenta al usuario.

---

## 8. Dos errores de criterio, no de código

Anotados porque se repitieron:

- **Se restauró el favorito sobre la sesión del usuario** al ver que diferían.
  Fue un error: son cosas distintas a propósito, y eso le quitó el sonido que
  estaba usando. Conservar ambos y preguntar.
- **Se afirmó que su micrófono estaba silenciado** basándose en una lectura COM
  improvisada que devolvió un valor basura. El usuario lo desmintió y tenía
  razón, tras haber perdido tiempo buscando un problema inexistente. Cuando una
  comprobación rápida contradiga al usuario, sospechar de la comprobación.

El detalle completo de todo esto está en `docs/PROJECT_MEMORY.md`.
# Consolidación local completada (12/10/2026)

Sonido aprobado cerrado por satisfacción del usuario. Ver `STABILIZATION.md`
y `PROJECT_MEMORY.md`: recuperación segura de salida, suspensión/reanudación,
persistencia GUI y arranque con configuración dañada; 15 pruebas y paquete
portable local verificados. No cambiar favorito ni adoptar experimentos.
Pruebas físicas de desconexión/suspensión e instalación firmada general quedan
fuera de lo confirmado. Aplicación habitual activa, hashes conservados.

# Instalador preparado; firma de producción externa pendiente

Ver `PROJECT_MEMORY.md` e `INSTALLATION.md`. MSI Development incluye el driver
y helper para crear/retirar el dispositivo; objetivo Windows11 22H2+ x64.
StaticCRT y datos por usuario. 16 pruebas y extracción/smoke MSI pasan.
No llamarlo universal: el driver sigue test-signed. El build de producción
rechaza ese paquete; falta cuenta/certificado y firma Microsoft/validación HLK.
CAB de envío preparado, sin firma de envío. No reinstalar el driver del usuario
para una prueba; creación/remoción real y transacción MSI necesitan VM.

# Cambio de vía de distribución: investigar VB-CABLE

Usuario autoriza conservar el driver propio e investigar VB-CABLE. Instantánea
de recuperación verificada en archives/own-driver-20261012-234107-2b8044.zip;
97 archivos, sin clave privada ni datos personales. Originales siguen en sitio.
Ver OWN_DRIVER_RECOVERY.md y VBCABLE_RESEARCH.md. VB paquete45 verificado offline
(firmaMicrosoft kernel/CAT/INF y editorSetup). No está instalado; GUI aún no lo
reconoce. Priorizar prueba separada de transporte/volumen, sin cambiar favorito.
Discrepancia licencia web/README sobre integración: aclarar antes de bundling.
No sustituir/desinstalar el driver activo ni cambiar arranque como investigación.

# GUI e instalador VB-CABLE preparados

Entrada virtual admite propio/VB y persiste ID; preserva ruta y favorito.
MSI nuevo 0.2.1-x64-VBCABLE: guía, enlace en Inicio y apertura de instrucciones
al acabar, sin driver propio ni instalación automática de VB. Instalador oficial
de VB separado. 17 pruebas y MSI extraído GUI/WASAPI propio pasan; falta ruta VB
real (aún no instalado). Ver PROJECT_MEMORY.md y VB-CABLE.txt para continuar.

# Preparación automática de VB-CABLE — 13 de octubre de 2026

Último estado publicado: MSI0.2.5 y commit533553d corrigen mantenimiento:
WINDOWSBUILD Secure=yes y condición Installed OR requisitos iniciales.
Release https://github.com/valvik77/Resoniq/releases/tag/v0.2.5.
Usuario pide guardar, commit y subir. Próxima verificación: instalación0.2.5
y reparación real confirmadas por usuario; no efectuadas por agente.
Leer PROJECT_MEMORY.md para estado actual, que sustituye versiones anteriores.

Actualización identidad: usuario elige «Pulso continuo» (cinta onda azul/cian).
Integrada en ejecutable, bandeja con3estados, accesos y arteMSI0.2.4.
Originales/variantes assets/brand, generador scripts/build_brand_icons.py.
Builds y MSIextraído GUI/WASAPIpropio pasan, recursosWin32verificados.
Instalación activa ProgramFiles aún anterior; aplicar MSI0.2.4 para actualizar.
No cambios de sonido/perfil/driver. Estado completo en PROJECT_MEMORY.md.

Actualización posterior: MSI vigente 0.2.3 activa automáticamente el DWORD
Windows Volume Control de VB-CABLE (HKLM SOFTWARE\VB-Audio\Cable,
VBAudioCableWDM_VolCtl=1, x64, permanente). Reinicio manual si driver ya instalado.
Usuario confirmó que activar la opción manualmente arregla el volumen.
Pruebas del MSI extraído/configuración/scripts/GUI/WASAPI propio pasan;
transacción MSI nueva y reinicio no efectuados. Ver PROJECT_MEMORY.md.

Distribución actual: dist/RESONIQ-Setup-0.2.2-x64-VBCABLE.msi. Al finalizar y
en Inicio ofrece «Instalar VB-CABLE». El asistente descarga, verifica hash y
firmas, extrae el paquete oficial y abre el setup con UAC; evita abrirlo si ya
detecta el dispositivo. El usuario confirma Install Driver y reinicia manualmente.
Pruebas de descarga real, ZIP alterado, interfaz y preparación asíncrona pasan;
también scripts extraídos del MSI y GUI/WASAPI propio. No instalación real de
VB ni validación de volumen/ruta VB. No cambios de DSP, favorito o arranque.

Usuario solicita pausa y guardar memoria. Retomar desde PROJECT_MEMORY.md;
siguiente paso: comprobar instalación del MSI 0.2.2 y transporte VB-CABLE en
el otro equipo, conservando el audio aprobado. Sin operaciones pendientes en
ejecución ni permiso para nuevas mediciones de micrófono.

