# Gain, indicadores y carácter entre equipos — 0.2.9

## Hallazgos

El usuario confirma que la medición funciona en Conexant, pero no percibe el
mismo punch que en el portátil original. No atribuirlo a una causa acústica
única: el micrófono recoge altavoz, posición, sala y su propia respuesta.

La medición actual (margen 19.42 dB, dos bandas coherentes) propone corte 167 Hz,
cuerpo 6.28 dB y presencia 0. El favorito original tenía corte 175 Hz, cuerpo
11 dB y presencia 10, además de band_density=1.8 y loud_density=1.45. El perfil
genérico Laptop Focus tiene esas densidades en 1 y adaptive_depth=0.7; el
favorito usa adaptive_depth=0.15. La sesión guardada no prueba qué calibración
estaba activa en ese instante; los controles en telemetría pueden diferir.

Dos errores de software contribuían a la confusión:

- Realce comparaba el mix antes de volumen con la salida después del volumen
  VB-CABLE. Una reducción de escucha hacía desaparecer la barra aunque el DSP
  realzara. La onda usaba también la salida atenuada.
- Aplicar una medición reemplazaba los parámetros musicales por defaults,
  incluyendo densidades/adaptación que no se miden con tonos acústicos.

## Cambios

El host publica processedPeak y dspGainDb antes de aplicar el volumen VB-CABLE.
Actividad DSP usa ese pico y Realce DSP muestra la relación de picos del mismo
bloque entre entrada y salida del motor. No es LUFS, RMS ni calidad percibida.
Salida sigue mostrando el pico final tras el volumen aplicado por el host;
el volumen/atenuación del hardware físico posterior también afecta la escucha.
Mute/cero mantienen los indicadores de salida en cero. Compresión aparece por
separado, sin confundirla con el realce ni esconder el control Gain.

Gain funciona de -12 a +12 dB, en pasos de 0.5 dB. El motor suaviza sus cambios,
lo aplica al procesado siguiendo Intensidad y conserva el limitador de pico
de muestra a -1 dBFS. DSP desactivado e Intensidad cero mantienen la ruta
original. Más ganancia puede aumentar la limitación; no crea graves que los
altavoces no entreguen ni garantiza el mismo carácter en cualquier equipo.
La comparación interna compensada también tiene en cuenta Gain; no se añade
compensación a la aplicación visible.

gain_db es opcional en los perfiles, validado y persistido solo si difiere de 0.
Los perfiles anteriores mantienen Gain 0, su serialización y su sonido.
No se cambia el favorito ni la sesión guardada automáticamente.

La GUI pide una calibración tonal que conserva del perfil elegido armónicos,
estéreo, detalle, body_floor, adaptive_depth, loud_density, band_density y
acentos de ataque. Los tonos ajustan corte, cuerpo, presencia y recorte de
medios; no deciden cuánto efecto musical prefiere el usuario. Esta regla se
aplica a mediciones futuras, no reconstruye perfiles ya medidos/guardados.
También respeta la elección explícita de auriculares/salida externa al preparar
la propuesta, en vez de tratar toda medición como altavoces de portátil. Si el
perfil ya es Medido, usa la detección disponible; no promete identificar toda
topología de altavoces externos conectados a un portátil.

Telemetría local ampliada con calibración activa, densidades, Gain y salida
anterior al volumen, para que las siguientes comparaciones sean verificables.

## Verificación

18 pruebas pasan: ganancia en dB antes de límite, original e Intensidad cero,
serialización compatible, rangos/NaN, controles GUI, medición que conserva la
voz musical y estabilidad. La automatización de Gain no asigna memoria en audio.
Renders sintéticos de seis segundos del favorito original y sesión guardada
resultan idénticos bit a bit con Gain 0 frente al ejecutable anterior.
Evidencia en build/gain-verification. GUI revisada visualmente, MSI extraído.

No se cambió el volumen del equipo ni se hicieron nuevas capturas de micrófono.
La escucha del usuario con 0.2.9 queda pendiente. Antes de sacar conclusiones
entre equipos, usar el mismo fragmento y perfil, y comparar volumen físico y
virtual. El objetivo es portabilidad del procesado y controles ajustables;
no se promete equivalencia acústica universal ni soporte fuera de Windows 11 x64.
