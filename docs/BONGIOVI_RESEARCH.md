# Contraste documental con Bongiovi DPS

Investigacion: 12 de octubre de 2026. Sin cambios al DSP ni a perfiles.

## Fuentes y alcance

- [Comunicado de Bongiovi para Windows/Mac, 2015](https://www.prnewswire.com/news-releases/experience-audio-like-never-before-with-digital-power-station-dps-technology-300187778.html): perfiles con mas de 120 puntos, control dinamico, claridad, graves y escena estereo. Es descripcion comercial, no especificacion reproducible.
- [Blog del fabricante, perfiles, 2012](https://bongiovidps.blogspot.com/2012/06/why-city-profiles.html): perfiles realizados manualmente por ingenieros para dispositivos concretos o familias; perfiles City como alternativas generales.
- [Patente US8160274B2](https://patents.google.com/patent/US8160274B2/en): ejemplo de cadena con filtros shelving, compresor, mas shelving, ecualizador y segundo compresor; coeficientes preparados para dispositivo/entorno. Describe un embodiment, no prueba de cadena exacta del programa Windows.
- [Ficha SPHE8107, documento fechado 27/03/2018](https://sdk.bongiovidps.com/wp-content/uploads/2020/01/Sunplus-SPHE8107-Onesheet.pdf): AGC, EQ 10+2 bandas, graves y agudos dinamicos, proteccion multibanda y estereo dinamico para chip/karaoke/soundbar. No atribuir todo esto al plugin antiguo.
- [Tecnologias actuales del fabricante](https://bongiovidps.com/tech-apps/): VSUB armonico, EQ dinamica y vinculada a volumen, AGC/multibanda, V3D HRTF. Suite actual, no inventario demostrado del antiguo DPS Windows.

## Contraste con codigo local

`src/dsp/engine.cpp` implementa compresion en cuatro bandas con ataques/releases
comunes, detalle ascendente con puerta, refuerzo transitorio, graves armonicos
con envolvente, EQ de cuerpo/presencia/barro, adaptacion segun nivel de entrada,
AGC, ampliacion mid/side y limitador de pico de muestra a -1 dBFS.

Coincidencias de familia funcional: EQ + dinamica, AGC, graves virtuales,
perfiles y ampliacion estereo. No implican algoritmos ni calidad equivalentes.

Diferencias verificables:

- EQ local deliberadamente reducida frente a EQ de tuning mas amplia documentada.
- Adaptacion local por nivel de senal no equivale a conocer volumen/SPL del usuario.
  El callback Windows actual solo controla silencio y transiciones, no EQ.
- Mid/side local no demuestra escena espacial dinamica o localizacion HRTF.
- Highpass y limitador local no constituyen modelo de excursion/temperatura ni
  proteccion multibanda calibrada del altavoz.
- Medicion local propone corte, cuerpo, presencia y armonicos desde barrido;
  no reemplaza tuning manual ni caracterizacion no lineal del dispositivo.
- Los 120 puntos publicados no significan 120 bandas, ni permiten deducir
  superioridad, valores, numero de etapas o porcentaje de equivalencia.

## Valoracion y posibles mejoras

Inferencia: RESONIQ pertenece a una familia de procesamiento muy similar a la
que Bongiovi presenta para mejorar pequenos altavoces. Hay fundamento tecnico
para llamarlo alternativa con enfoque parecido; no para declararlo clon,
igual calidad, ni porcentaje de cercania. Faltan parametros y salidas de
referencia del producto Windows usado por el usuario.

Pistas utiles: calibracion mas rica (resonancias, recortes, repetibilidad y
confianza), evaluar equilibrio tonal a volumen bajo/medio/alto y coordinar
dinamica para conservar ataques sin bombear ni acumular trabajo del limitador.
Toda variante debe ser independiente del favorito y demostrar valor auditivo.
No activar EQ ligada al volumen sin diseno y comprobacion: el volumen virtual
no mide SPL y la ruta loopback ya contiene cambios de nivel.

La documentacion comercial de restauracion/decompresion no demuestra recuperar
informacion original perdida ni proporciona criterio medible para copiarla.
No usar esas promesas como requisitos ni extrapolar tecnologias actuales al
plugin discontinuado. La investigacion no verifica el estado comercial exacto
de cada version del plugin; la tecnologia del fabricante sigue documentada.
