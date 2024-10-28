## README
### Medidas Electrónicas 1 - Curso R4053 2024
Proyecto Integrador - Grupo 1

### Balanza contabilizadora
La utilidad del proyecto es poder medir el peso de un conjunto de elementos y poder determinar el peso del conjunto.

### Requisistos
* Se debe tener instalada la extensión PlatformIO para VScode.

### ¿Cómo correr el proyecto?

Se debe realizar el siguiente proceso en la tab de PlatformIO (es importante respetar el orden)

1. Bajo la seccion *Platform* seleccionar `Build Filesystem Image`.
2. Bajo la seccion *Platform* seleccionar `Upload Filesystem Image`.
3. Bajo la sección *General* seleccionar `Upload and Monitor`.

Para los pasos 2 y 3, se debe estar atento a la consola. Si queda esperando en `Connecting....` luego de haber impreso `Serial port COM3`, es necesario presionar el boton `boot` de la placa del ESP-32.
