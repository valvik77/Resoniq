# RESONIQ

Aplicación: **RESONIQ**. Motor: **RESONIQ DSP Engine**.
Perfil inicial: **RESONIQ Enhance**. Perfil de máxima intensidad: **RESONIQ Max**.
Variantes adicionales: RESONIQ Natural y RESONIQ Power.

Azul eléctrico `#2D81FF`, fondo grafito `#0D1016`, superficies `#171C25`.
Tipografía Segoe UI Variable Text/Display, contraste claro y espacio entre secciones.
Estilo Fluent: tarjetas y controles redondeados con antialias, deslizadores azules
con tiradores circulares, selectores con chevron e interruptor de opción tipo pill.
Botones y selectores muestran hover/foco. Los controles mantienen su interacción
nativa de teclado y sus nombres/estados accesibles de Win32.
La pantalla prioriza el interruptor DSP, los perfiles y la intensidad global.
La onda muestra una envolvente simétrica del historial de picos de salida,
actualizado cada 100 ms; no es un osciloscopio ni una animación ficticia.

Los perfiles anteriores siguen cargando sin alterar sus parámetros. Sus nombres
de preset se presentan con la nueva identidad. Los nuevos archivos pueden usar
los nombres RESONIQ. Los ejecutables auxiliares y el núcleo conservan nombres
técnicos internos para evitar cambios innecesarios en scripts y herramientas.
El controlador virtual ya instalado conserva su nombre de endpoint de Windows.

La aplicación se inicia con `Start-RESONIQ.cmd`. `Start-DPS.cmd` queda como acceso
compatible. La igualación de volumen permanece retirada de la interfaz.

La disponibilidad del nombre como marca y su uso por otros productos **no se
han comprobado**. Verificarlos antes de publicación o distribución comercial.
La identidad pública no utiliza referencias a Bongiovi; la procedencia de
dependencias y las notas históricas de investigación deben conservarse.

La ventana solicita esquinas redondeadas y material de sistema a DWM mediante
las [API documentadas de Windows 11](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute).
El material se ve en el marco; las tarjetas del cliente siguen opacas. La UI usa
Win32/GDI+ de Windows, separada del hilo WASAPI y del DSP.
