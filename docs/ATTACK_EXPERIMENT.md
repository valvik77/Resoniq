# Ataques y cuerpo — ensayo experimental

Perfil separado: `config/attack-body.user.dps`. Copia del favorito aprobado
con `transient_accent_db 4`, sin modificar EQ, AGC, densidad, intensidad o RAW.
No se carga automáticamente ni se adopta como defecto. Campo opcional 0..6 dB,
por defecto cero; los perfiles existentes conservan su procesamiento.

## Ensayos descartados

Ralentizar compresión de 8 a 16/24/30 ms no mejoró el contraste de ataques en
el programa de prueba. Con 30 ms, ráfagas fuertes perdían unos 0.99 dB de
contraste y ganaban 1.91 dB de RMS: el limitador absorbía los picos extra.
Reducir densidad y detalle tampoco produjo una mejora suficiente.
El parámetro de ataque del compresor probado se retiró del código y del perfil.

## Variante preparada

Un detector enlazado en estéreo añade un pulso breve al comienzo de un evento:
envolventes rápida/lenta 1/40 ms, caída de pulso 6 ms, suavizado de ganancia 1/8
ms y periodo mínimo entre disparos 120 ms. Hay puerta para ruido y reducción
del acento conforme aumenta el nivel de la banda. No levanta de forma permanente
la señal sostenida y conserva el limitador. Se aplica en medios/agudos, con mitad
de cantidad en la banda superior; Clarity escala el efecto.

Se probó cantidad 6 dB y se descartó por perder más cuerpo en el ensayo. La
variante moderada 4 dB permite comparar el cambio temporal sin alterar el
refuerzo de detalle existente. Es cantidad máxima del detector; no implica
que todo ataque reciba 4 dB ni que sea audible en cualquier música.

## Resultados

Ráfagas a 1800 Hz sobre fondo a 90 Hz; medida en los últimos dos segundos,
respecto al favorito, normalizando energías por el total del programa:

| Escala de ráfaga | Contraste de ataque | Cuerpo relativo | Cambio RMS total |
|---|---:|---:|---:|
| 0.08 | +0.566 dB | −0.585 dB | +0.048 dB |
| 0.16 | +0.284 dB | −0.348 dB | −0.496 dB |
| 0.32 | +0.123 dB | −0.261 dB | −0.318 dB |

Es una mejora técnica modesta y con una pequeña pérdida de energía relativa
en el cuerpo de la ráfaga. No representa una aprobación auditiva ni demuestra
mejora en altavoces. Las cifras no se extrapolan a canciones enteras.

Pruebas: contraste, cuerpo, nivel total, techo, mono, independencia de bloques,
puerta de ruido, retorno a unidad en señal sostenida a cinco frecuencias de
muestreo, reset, serialización y ausencia de asignaciones. Suite: catorce CTest.
Renders del favorito conservan la referencia binaria `8D00DBCE…97DC1EA`.

## Escucha

Cargar `attack-body.user.dps` y comparar con «Ajuste por defecto», manteniendo
Intensidad 100 % y el mismo fragmento con percusión/voz. Evaluar definición,
cuerpo y fatiga. Si no aporta valor perceptible o pierde cuerpo, no adoptarlo.
Este ensayo no incluye la variante de Intensidad alineada, que el usuario no
apreció. Favorito y sesión permanecen independientes.
