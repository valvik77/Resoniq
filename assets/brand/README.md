# RESONIQ — Pulso continuo

Identidad elegida por el usuario: una cinta azul/cian que forma una onda.
Los SVG son originales vectoriales editables; no recortes de las propuestas.

- `resoniq-app`: aplicación, accesos directos, instalador y barra de tareas.
- `resoniq-tray-active`: procesamiento activo, azul y cian.
- `resoniq-tray-bypass`: sonido original, gris.
- `resoniq-tray-disconnected`: falta de conexión, ámbar.
- `resoniq-mark`: símbolo de color transparente.
- `resoniq-white` / `resoniq-black`: monocromáticas para fondos oscuro/claro.

Cada variante incluye SVG, PNG de 16 a 1024 px e ICO multirresolución
(16, 20, 24, 32, 40, 48, 64, 128, 256 px). Estado también indicado en el texto
de la bandeja; el color no es la única forma de conocerlo.
`resoniq-variants.png` es la lámina de revisión con tamaños pequeños reales.
`installer-banner.bmp` y `installer-dialog.bmp` son las piezas del asistente MSI.

Regeneración: `python scripts/build_brand_icons.py` (requiere Pillow).
Paleta: fondo #0B1B31, azul #087FFF, cian #00D9EB,
gris #B4C1D2, ámbar #F5B75D. Mantener proporciones y espacio del símbolo.
