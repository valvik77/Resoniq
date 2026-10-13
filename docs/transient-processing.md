# Articulación temporal

El módulo original `TransientShaper` detecta la diferencia entre una envolvente
rápida y otra lenta de cada banda. Durante ataques añade una ganancia breve;
con señal sostenida converge a unidad. Tiene puerta para no amplificar señales
por debajo del umbral y enlaza ambos canales para conservar posición estéreo.

Engine lo aplica antes de sumar bandas, únicamente si `detail_db > 0`, en medios
y agudos. Clarity controla la cantidad; la banda superior recibe la mitad. Los
controles conservan el suavizado de 20 ms. No necesita memoria dinámica,
bloqueos, lookahead ni cambios de coeficientes en el hilo de audio.

La función detail_db de perfiles v2 se amplía: controla la elevación de detalles
existente y este nuevo realce de ataques. Los perfiles con valor cero no cambian.
No se modifica la ganancia global ni el techo del limitador de muestras.

`transient_validation` usa ráfagas sobre un fondo grave y compara la proporción
de energía durante los ataques con el RMS del programa completo. Ejecuta el
comparador RMS real, para detectar cambios que desaparezcan al igualar nivel.
Las cifras resultantes no predicen calidad acústica ni preferencia auditiva.
La evaluación con música en los altavoces reales sigue pendiente.
