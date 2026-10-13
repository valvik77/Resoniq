# Intensidad alineada — variante experimental

Resultado de escucha: el usuario afirma no notar ninguna diferencia frente al
perfil por defecto, incluso tras explicar la comparacion en posiciones medias.
No se adopta: la correccion electrica no aporto una mejora perceptible en esta
evaluacion. Las instrucciones de escucha siguientes quedan como documentacion
del ensayo, no como una peticion de repetirlo.

El usuario autorizó preparar una variante para reducir la cancelación de fase
en intensidades intermedias, conservando el ajuste aprobado.

Archivo para escuchar: `config/intensity-aligned.user.dps`. Es una copia del
ajuste aprobado con `aligned_intensity 1`; no reemplaza favorito ni sesión.
El campo es opcional y por defecto falso. Las versiones anteriores rechazan
este campo: cargar la variante solo en la versión actualizada.

## Implementación

La ruta seca adopta gradualmente la fase del crossover mediante 33 bancos de
allpass preparados antes del procesamiento. Los polos se interpolan desde la
identidad al allpass LR de cada cruce, dentro del círculo unidad. Se mantienen
todos los estados y se interpolan bancos contiguos para variar intensidad sin
recalcular coeficientes, asignar memoria ni bloquear en audio.

El recorrido de fase usa una curva de raíz octava para aproximarse pronto al
crossover y evitar el valle de la mezcla histórica. Es una elección experimental,
no una garantía de respuesta monótona para cualquier señal. Los filtros de EQ,
protección y procesamiento no lineal todavía añaden sus propias diferencias.

En cero y al 100 % se usa la expresión histórica exacta; las comprobaciones
de extremos parten de estado preparado con esos valores. Las transiciones
mantienen suavizado y el limitador final sigue activo. No se introduce igualación
de volumen ni se incrementan AGC, densidad o parámetros de la calibración.

## Resultados eléctricos

Seno de escala 0.02, tras seis segundos de estabilización, intensidad 25 %:

| Frecuencia | Mezcla anterior | Experimental |
|---|---:|---:|
| 250 Hz | +7.36 dB | +8.86 dB |
| 650 Hz | +1.61 dB | +4.58 dB |
| 1000 Hz | −7.35 dB | +3.92 dB |
| 2800 Hz | +10.51 dB | +10.97 dB |
| 6000 Hz | +2.55 dB | +5.95 dB |

Son ganancias RMS respecto al original, no mediciones de altavoces ni aprobación
auditiva. La recuperación de nivel procede de reducir cancelación en la mezcla;
puede hacer que una posición intermedia suene más fuerte y requiere escucha.

En el render de referencia de tres segundos, mezcla anterior y experimental
al 100 % son idénticas bit a bit: `8D00DBCE…97DC1EA`. El favorito conserva
su hash `5119695B…C3E23E`.

Pruebas: equivalencia exacta de extremos, mejora del caso de 1 kHz/25 %,
independencia de bloques, automatización sin salto abrupto en el programa de
prueba, retorno a bypass, estabilidad a 16/48/192 kHz, serialización del campo
y ausencia de asignaciones. La suite completa tiene trece pruebas.

Benchmark local a 48 kHz, 960 frames: p99 0.54 ms frente a 20 ms de bloque.
La variante cuesta más CPU; el benchmark no incluye WASAPI/GUI ni garantiza
plazos bajo cualquier carga. CSV de señales y rendimiento en `build/intensity`.

## Escucha pendiente

Cargar el perfil experimental y comparar intensidades 25, 50 y 75 % sobre la
misma música. Cero y 100 % conservan los extremos. Evaluar cuerpo, claridad,
transitorios y fatiga, sin atribuir toda mejora a calidad solo porque suba nivel.
«Ajuste por defecto» vuelve al aprobado. No fijar la variante como defecto sin
decisión expresa del usuario.
