# Traspaso a Claude: RESONIQ, 12–13 de octubre de 2026

Este documento resume el trabajo de los dos días y el estado confirmado al
cerrar. Para el día 12 se apoya en la memoria y los informes del proyecto;
para el día 13, en los cambios, pruebas y publicaciones de esta conversación.
Leer primero la sección nueva de PROJECT_MEMORY.md. Los puntos de retomada
anteriores contienen decisiones históricas que ya fueron sustituidas.

## 1. Objetivo y reglas del usuario

Aplicación de mejora sonora para Windows, con cuerpo, claridad y punch, inspirada
en la experiencia del usuario con Bongiovi DPS. Código propio: no afirmar
equivalencia sonora ni un porcentaje de similitud. Debe adaptarse a otros equipos,
sin depender del portátil original. Las medidas y pruebas ayudan, pero la escucha
del usuario decide si una variante mejora el sonido.

Continuar trabajo rutinario autorizado sin preguntar en cada iteración. Preservar
el favorito aprobado y la sesión del usuario como cosas distintas. No copiar uno
sobre otro porque difieran. No activar experimentos automáticamente.

El usuario retiró expresamente la igualación de volumen: la comparación visible
es DSP activado/desactivado, sin compensación. Las pruebas internas pueden
normalizar niveles. Ganancia manual no equivale a restaurar la compensación A/B.
Una medición requiere consentimiento; el audio se analiza localmente y se descarta
salvo modo diagnóstico consentido. No lanzar nuevas capturas automáticamente.

## 2. Trabajo registrado el día 12

- Identidad RESONIQ y GUI Win32/GDI+ de estilo Windows 11: azul/grafito,
  controles propios, sliders, selectores, bandeja, ventana de ajustes y temas.
  No es una migración a WinUI 3. Se corrigieron fugas del dibujo nativo de los
  sliders retirando su tema visual y usando un bitmap intermedio.
- Audio automático al abrir. El interruptor DSP alterna procesado/original
  manteniendo la ruta; cerrar normalmente deja la aplicación en la bandeja.
  El cambio de dispositivo aplica su calibración y conserva controles.
- Perfiles Laptop Impact/Impact+ aprobados por escucha en el Lenovo original:
  más cuerpo, armónicos, densidad y persistencia del efecto con material fuerte.
  adaptive_depth, loud_density y band_density permiten ajustar ese carácter.
- Auditoría del código propio y refactorización: preferencias, paleta, iconos,
  sesión WASAPI y sugerencias de calibración separados en módulos; adaptador
  Windows compartido; guardado atómico y formato decimal independiente de locale.
  Informe: docs/AUDIT.md. No presenta una auditoría de todo SysVAD/Microsoft.
- Captura de medición en hilo independiente, coordinación COM MTA, RAII y manejo
  de errores. Alineación conjunta con tonos medios; coherencia de tonos;
  propuestas de corte continuas. Los huecos reales se rellenan para preservar
  tiempo y solo invalidan cuando solapan ventanas analizadas. Un flag de
  discontinuidad por sí solo no demuestra pérdida de muestras.
- Corrección de privacidad: antes se escribía WAV pese al mensaje de descarte.
  Ahora solo se escribe con diagnóstico consentido. Corrección del etiquetado
  estéreo/mono del WAV y del selector al guardar un perfil medido.
- Mute/cero: el loopback podía entregar audio sin silenciar. Protección después
  del DSP, incluyendo colas y bypass, con 180 ms de espera al reactivar. Los
  cambios entre volúmenes positivos no vacían FIFO ni reinician el DSP.
- Recuperación: salida ausente conserva ID y espera; suspensión/reanudación
  libera/reabre; falta de progreso de render durante 5 s permite reconectar;
  archivos corruptos se conservan y se intenta recuperar respaldo.
- Investigación documental de Bongiovi, sin adoptar efectos por analogía.
  Variantes de Intensidad alineada y acento de ataques quedaron separadas.
  El usuario no oyó mejora con Intensidad alineada: no adoptarla. Los ensayos de
  ataques dieron una mejora técnica pequeña con compromisos, no aprobación sonora.
- Paquete portable local y preparación del MSI/driver propio. El driver de
  desarrollo no equivale a distribución general con firma Microsoft. Sus fuentes
  y recuperación se conservaron; se investigó VB-CABLE como transporte de distribución.

Advertencias históricas importantes: una lectura COM improvisada afirmó
incorrectamente que el micrófono estaba silenciado. La memoria conserva esa
hipótesis como descartada. La latencia calculada con informes/buffers/FIFO es una
estimación de software, no una medida física de ida y vuelta. Los perfiles por
modelo son ajustes escuchados, no calibraciones de laboratorio.

## 3. Distribución y marca el día 13

Se pasó a MSI con VB-CABLE instalado por separado. El asistente descarga el ZIP
oficial, verifica SHA256 y firmas, extrae con límites y abre el instalador oficial
mediante UAC. No empaqueta el driver de VB, no altera Secure Boot/TESTSIGNING ni
reinicia Windows. Si detecta el cable existente, evita abrir un setup que podría
retirarlo. La licencia de VB-Audio sigue siendo independiente.

Logo elegido por el usuario: «Pulso continuo», cinta azul/cian formando una onda.
SVG/PNG/ICO, variantes de bandeja activo/bypass/desconectado, iconos MSI y accesos.
Integración visual original; motor y favorito conservados.

Se corrigió la condición MSI de Windows: WINDOWSBUILD es Secure y el mantenimiento
se exceptúa mediante Installed OR requisitos iniciales. Publicaciones anteriores:
0.2.4 y 0.2.5. La corrección del MSI se verificó con el motor MSI y extracción,
sin confundirlo con una reparación real trazada en el equipo del usuario.

## 4. Volumen de VB-CABLE: 0.2.6

El usuario volvió a reportar 0 % silencio y 2 % como máximo. El registro ya
contenía VBAudioCableWDM_VolCtl=1. Una prueba real con VB-CABLE 3.3.1.7 mostró
que su loopback entregaba el mismo pico (~0.0200005) en 0/2/10/50/100 %.
Activar la opción persistente no atenúa esa ruta de captura.

Corrección específica para VB-CABLE: observer COM independiente lee el dB real
del endpoint cada 20 ms; el audio aplica su amplitud después del DSP, suavizada.
No se usa el porcentaje como multiplicador lineal. Mute/cero mantienen la puerta
previa. Si falla el observer, se cierra la ruta para reconectar. La ruta propia
no recibe esa atenuación extra. Ver docs/VB-CABLE_VOLUME.md y output_volume.hpp.

Prueba del host real, salida física silenciada temporalmente y estado restaurado:
picos en bypass 0 / 0.000024637 / 0.000364381 / 0.00595857 / 0.0199421 para
0/2/10/50/100 %. Con DSP también sigue la curva. Sin micrófono ni audio guardado.
Un ensayo DSP inicial no dejó estabilizar el AGC: se amplió la duración de la
prueba, no se cambió el motor para hacerla pasar.

Banner «Resoniq by Valvik», Resoniq en negrita, botón Acerca de, versión en recursos
Windows y MSI, autoría de Víctor Manuel Valenzuela, copyright y créditos VB-CABLE.
Acerca de se refinó en 0.2.7: by Valvik más cerca, sin aka duplicado, licencia MIT
y repositorio. Se revisaron capturas reales de la GUI generadas por su fixture.

## 5. Medición en el segundo portátil: 0.2.7

El equipo actual usa Conexant ISST Audio; el original era Lenovo Legion Pro 5
83LU/Realtek. El usuario oía los tonos claramente, pero el análisis rechazaba.
El diagnóstico no acreditaba ruido de sala: había pico captado, pero pocos tonos
sostenidos coherentes. La captura usaba procesado predeterminado y promediaba
canales, riesgos de supresión de voz/eco y cancelación de fase.

Se intenta RAW por stream, con cliente nuevo normal si falla. Se prefiere el
micrófono físico predeterminado, se excluyen transportes virtuales, se analizan
los dos canales por separado y se elige por fiabilidad/coherencia, no por pico.
No se bajaron los umbrales. Los avisos ya no culpan automáticamente al ruido.
Diagnóstico: input_raw, capture_channel y motivo de fallback.

El usuario confirmó que la medición ya funciona. Registro actual:
RAW=1, usable=1, margen 19.42 dB, dos bandas fiables, retardo 15 ms y cero muestras
perdidas. Propuesta: corte 167.09 Hz, cuerpo 6.28 dB, presencia 0. Estas cifras
incluyen micrófono/sala/posición; no son una respuesta de altavoz de laboratorio.
Ver docs/MEASUREMENT_PORTABILITY.md. No repetir capturas sin nuevo consentimiento.

## 6. Inicio y cierre del instalador: 0.2.7–0.2.8

Finalizar abre el programa mediante WixUnelevatedShellExec, también al actualizar
o reparar, independientemente del checkbox VB-CABLE. No abre al desinstalar.
Ese arranque requiere UI de MSI; la instalación silenciosa no tiene Finalizar.

El usuario reportó que el cierre automático de la instalación no terminaba el
proceso. Cerrar solo mandaba la GUI a la bandeja y faltaba WM_ENDSESSION.
La GUI ahora acepta WM_QUERYENDSESSION y sale/guarda/detiene solo al confirmarse
WM_ENDSESSION(TRUE); una cancelación no altera la sesión.

Para versiones ya instaladas, DLL estática de cierre antes de InstallValidate:
comprueba clase y ruta completa INSTALLFOLDER/resoniq.exe, solicita el comando
normal Salir (304) y espera hasta 15 s. No mata procesos por nombre ni fuerza
TerminateProcess. No cierra copias de otras carpetas. Si falla, aborta con error.
El cierre se aplica también en instalación silenciosa; el arranque final no.

Prueba nativa: copia aislada 0.2.7 que se queda en bandeja, helper que logra salir
y guardar, otra copia en otra ruta que permanece viva y nueva GUI que responde a
confirmación/cancelación de cierre. tests/shutdown.cpp. No se interrumpió la
aplicación instalada del usuario. No equivale a una transacción MSI completa trazada.

## 7. Punch, Gain e indicadores: 0.2.9

El usuario no percibía el punch del primer portátil ni barra amarilla de Realce.
Había diferencias de hardware/perfiles y dos defectos comprobables:

1. Realce comparaba entrada anterior al volumen con salida posterior a VB-CABLE.
   El volumen de escucha ocultaba el realce. La onda usaba esa salida atenuada.
2. Aplicar una medición reemplazaba densidad/adaptación por defaults, aunque los
   tonos acústicos no miden esas preferencias musicales.

Ahora Actividad DSP y Realce DSP usan la salida del motor antes del volumen.
Salida conserva el pico posterior al volumen del host; el hardware físico aún
puede atenuar después. Realce es una relación de picos, no LUFS ni calidad.
Compresión se muestra aparte. Nueva telemetría incluye perfil/calibración activos,
densidades, Gain y pico anterior al volumen.

Gain: −12 a +12 dB, pasos 0.5 dB, 0 por defecto. Se suaviza y actúa sobre el audio
procesado siguiendo Intensidad, antes del limitador de pico de muestra −1 dBFS.
DSP desactivado e Intensidad cero conservan original. La comparación interna
compensada considera Gain; no hay compensación visible. gain_db es opcional y
solo se escribe si no es cero. Más Gain puede hacer trabajar más el limitador;
no genera graves físicamente ausentes ni garantiza punch universal.

Medir adapta EQ conservando armónicos, estéreo, detalle, body_floor, densidades,
adaptación y acentos del perfil elegido. Respeta auriculares/salida externa; si
ya es Medido usa la detección disponible, que no garantiza toda topología externa.
No reconstruye automáticamente perfiles medidos anteriormente. Se sugirió elegir
«Portátil: impacto máximo», medir de nuevo y probar Gain +3 dB; no se impuso.

## 8. Validación y preservación

18 CTest pasan. Incluyen Gain en dB, original/Intensidad cero, limitador,
rangos/NaN, perfiles compatibles, canales opuestos, rechazo de ruido/muestras
incompletas, medición que conserva voz musical y controles GUI. Se comprobó
automatización Gain sin asignaciones de memoria en audio.

Renders sintéticos de seis segundos del favorito y sesión son idénticos bit a
bit antes/después con Gain 0. Los archivos favoritos conservan hashes:

- Original del workspace: 5119695BA5D42074679AB8293F12FC44047126F19275C75DAEB57CEA82C3E23E.
- Favorito de LocalAppData: BB43CCE1A83EFD581F890CA691DCBDBE9830D11B239FDBC7D32CDF10AF6282C1.

No son el mismo archivo ni deben igualarse. El segundo fue guardado por el usuario
con controles distintos. No confundir la sesión guardada con todos los ajustes
en directo: sus valores pueden cambiar mientras la aplicación permanece abierta.

MSI extraído/verificado: payload, privacidad, versiones, condiciones Windows,
acciones de cierre/arranque y GUI con datos aislados. El smoke de audio de la
GUI empaquetada se omitió para no cerrar el motor instalado; el host de audio
VB se verificó por separado. La cobertura no demuestra precisión acústica ni
una transacción completa de actualización en todos los equipos.

## 9. GitHub e instalación confirmados al cerrar

Repositorio público: https://github.com/valvik77/Resoniq
Commit de código 034688f; documentación de descarga 2c0631f. Antes de guardar este
traspaso, main local y remoto estaban limpios y sincronizados en
2c0631f450eb5040f94da4024dfa1513b795983f; tag v0.2.9 coincide.

Release pública y marcada última versión:
https://github.com/valvik77/Resoniq/releases/tag/v0.2.9

Asset RESONIQ-Setup-0.2.9-x64-VBCABLE.msi, 1597440 bytes, SHA256 verificado local
y remoto: 3EDFCB9FADC38C4B53E4C53F7CF6C745DEDD9D417860176DDA052419FFD23B8E.
build/dist/config, audio, claves, vendor y archivos personales no se publicaron
en el historial. El MSI se subió como asset, no como binario del repositorio.

Verificación nueva de este cierre: Program Files/RESONIQ/resoniq.exe tiene versión
0.2.9 y está ejecutándose. El usuario ya la instaló; no consta traza de esa
transacción para atribuir comportamiento a todas sus fases.
Telemetría: Laptop Impact+, Gain +3 dB, DSP habilitado, Intensidad 100%, Bass .8,
Clarity .5, Dynamics .8, Spatial .2, densidades 1.45/1.8. En la lectura no había
señal: no es evidencia de éxito auditivo ni de punch.

Esta memoria y este traspaso se guardan localmente, sin nuevo commit/push.
Las autorizaciones de publicación anteriores ya se completaron; no seguir
publicando cambios futuros sin una petición que lo autorice.

## 10. Entorno y próximos pasos

Workspace actual: D:/OneDrive/Desarrollo 1/VibeCoding/Resoniq. Es otro ordenador:
los caches antiguos referencian rutas y Visual Studio del equipo anterior.
Se prepararon herramientas portables oficiales, con hashes verificados, en
build/tools/portable-msvc: MSVC 14.44 y CMake 4.4.4. SDK local 10.0.28000.0.
No se instalaron herramientas globales ni se cambiaron drivers para compilar.

En este workspace se puede cargar build/tools/portable-env.ps1 y compilar
build/local-release con NMake, Release y RESONIQ_STATIC_RUNTIME=ON.
Build-CableInstaller.ps1 acepta -BinaryPath build/local-release/resoniq.exe;
busca resoniq_close.dll a su lado y exige coherencia de versión. En un clon
nuevo esas herramientas ignoradas no existen: configurar un toolchain disponible.

Datos instalados: %LOCALAPPDATA%/RESONIQ; diagnósticos en su build y perfiles en
config. Los perfiles originales del workspace también son locales/ignorados.
Plataforma soportada: Windows 11 22H2+ x64, no cualquier SO/arquitectura.

Siguiente tarea: pedir valoración auditiva de 0.2.9 con señal real. Si sigue sin
punch, inspeccionar la telemetría nueva y comparar mismo fragmento/perfil con
volumen virtual/físico conocido; no concluir que falta Gain solo por la onda.
Confirmar cierre y reapertura en una actualización MSI real. Ampliar pruebas
de otros altavoces, transporte, continuidad y recuperación; medir latencia
física si se necesita una cifra acústica. No cambiar el favorito para simular progreso.

Informes clave: AUDIT.md, STABILIZATION.md, BONGIOVI_RESEARCH.md,
VB-CABLE_VOLUME.md, MEASUREMENT_PORTABILITY.md, INSTALLER_SHUTDOWN.md,
GAIN_AND_METERS.md. Experimentos: INTENSITY_EXPERIMENT.md y ATTACK_EXPERIMENT.md.
