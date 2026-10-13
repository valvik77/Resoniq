# Consolidación y caracterización — 12 de octubre de 2026

Corrección posterior: el rechazo inmediato por cualquier discontinuidad produjo
una regresión reportada por el usuario. Ahora se comparan posiciones de paquetes
para cuantificar pérdida real, se preservan huecos y se rechaza el resultado al
terminar la reproducción si faltan muestras. El primer bloque se prepara antes
de arrancar la salida. Los fallos actualizan el diagnóstico y la reconexión se
informa por separado. La comprobación real posterior detectó 480 muestras
perdidas al inicio. Se añadió un mapa de huecos: se rechaza si el hueco solapa
una ventana medida; si queda fuera, se acepta con aviso. Una segunda medición
autorizada pasó, con hueco en muestra cero, dos bandas fiables y tonos captados.
Datos numéricos en `build/consolidation/measurement-live-result.txt`; audio
descartado y calibración no aplicada. Doce pruebas pasan.

El usuario autorizó consolidar el funcionamiento y seguir mejorando calidad
sonora, conservando el ajuste aprobado. Esta iteración implementa mejoras de
medición y caracteriza el DSP; no modifica su sonido ni adopta una calibración.

## Medición

La alineación anterior dependía del primer sonido que superase cuatro veces
el ruido de sala. Según el ruido podía elegir un transitorio, 60, 120 o 250 Hz;
incluso identificar su banda dejaba el inicio sujeto a ese umbral.

Ahora se busca un retardo común de 0–300 ms, en pasos de 5 ms, maximizando la
energía espectral de los tonos completos de 250 Hz, 500 Hz, 1 kHz y 2 kHz.
Cada banda se normaliza por su máximo y se descartan de la alineación las que
no sobresalen del ruido. No se usa el grave inaudible como ancla temporal.
La lectura de niveles sigue usando la parte central de cada tono.

La validez exige al menos dos tonos medios identificables sobre el ruido y
con suficiente proporción de energía en su frecuencia. Un ruido fuerte durante
el barrido ya no se acepta solo por superar el ruido de la sala previo.
Una discontinuidad de captura durante reproducción provoca error en vez de
concatenar muestras perdidas y proponer una calibración desplazada.

La propuesta de corte usa una transición suave entre déficits de 7 y 13 dB,
en vez de saltar al cruzar −10 dB. Esto elimina el salto de 38 Hz observado
entre 162 y 200. Cambia deliberadamente la propuesta de una nueva medición;
no afecta a perfiles existentes, cuerpo/presencia ni al DSP en directo.

Los tonos se reproducen usando el mismo modo RAW de la sesión. Antes se
forzaba salida con efectos del sistema, aunque la música usase RAW: la medición
podía caracterizar una ruta distinta de la que se estaba escuchando.

`measure-debug.txt` añade las ocho bandas, retardo, cantidad de bandas fiables,
modo RAW, salida y propuesta. Son números; no se conserva grabación salvo
diagnóstico explícito y consentimiento. La última captura de habitación anterior
no se conserva y no se inventa una atribución exacta de su variación.

Regresiones: altavoz plano, portátil, graves ausentes, retardos de 0/10/50/180/
250 ms, ruido impulsivo antes del tono, distintos suelos de ruido, ruido ajeno
durante el barrido, truncación, entradas inválidas y continuidad de propuesta
alrededor de −10 dB. Pasan dentro de la suite de doce pruebas.

La grabación histórica disponible se reanaliza offline con varios suelos de
ruido hipotéticos en `build/consolidation/replay-result.txt`. No es la última
grabación del usuario ni prueba retrospectiva de la causa exacta. La nueva
implementación requiere una nueva prueba de habitación con permiso del usuario.
En ese ensayo, variar el ruido supuesto entre −50 y −35 dB conserva exactamente
la propuesta: retardo 20 ms, corte 192.47 Hz, cuerpo 8.46 y presencia 8.00 dB.
Es estabilidad sobre una captura fija; no acredita repetibilidad acústica.

## Calidad sonora

Nueva herramienta portable `dps_quality PROFILE`: mide seno de 1 kHz y programa
multitono a cuatro niveles y cinco intensidades, tras seis segundos de
estabilización. Informa ganancia RMS, pico, factor de cresta y armónicos 2–5
del seno. No reproduce sonido, no abre micrófono ni escribe perfiles.
CSV en `build/consolidation/quality-favorite.csv`.

Hallazgo prioritario: con el perfil aprobado, seno de 1 kHz de escala 0.02,
intensidad 25 % da −7.35 dB frente al original; 50 % da +5.26 y 100 % +13.36.
La mezcla seca/procesada puede cancelar por la fase de crossover y filtros.
Esto demuestra una respuesta no monótona en ese programa sintético, no una
valoración auditiva general. La mezcla de intensidad necesita una iteración
específica y pruebas de espectro; no basta subir ganancia ni cambiar a mezcla
de potencia constante, que también puede cancelar señales en oposición.

En multitono de escala 0.6, intensidades 75 y 100 % dan +2.96 y +2.95 dB:
el limitador ya limita la ganancia útil. Subir densidad indiscriminadamente
puede comprimir más sin aumentar el nivel útil. Son cifras de este programa,
no un límite físico medido de los altavoces.

No se cambia la mezcla del perfil aprobado en esta iteración. La siguiente
mejora sonora debe evaluar una ruta de intensidad que reduzca la cancelación,
preservando original en bypass/cero, continuidad, techo y referencia al 100 %.
Los ensayos no demuestran ausencia de aliasing, true peak o distorsión en música.

## Estado

Release y doce pruebas CTest pasan; prueba WASAPI real pasa. El favorito y la
sesión conservan sus hashes y el render de referencia permanece idéntico.
La GUI se actualiza conservando su sesión. No se ejecuta una medición de
habitación automáticamente: sigue requiriendo consentimiento por operación.
