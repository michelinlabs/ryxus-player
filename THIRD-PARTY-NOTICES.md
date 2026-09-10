# Componentes de terceros

El código propio de Ryxus Player está bajo licencia MIT (ver `LICENSE`). Los
componentes de terceros que usa conservan la suya:

## Qt 6

- Licencia: **LGPLv3**
- Sitio: https://www.qt.io — código: https://code.qt.io
- Uso: enlazado **dinámicamente**. Las bibliotecas de Qt se distribuyen como
  DLL independientes junto al ejecutable, de modo que se pueden reemplazar por
  otra compilación de Qt de la misma versión sin recompilar Ryxus Player, tal
  como exige la LGPL.
- Los binarios publicados en *Releases* incluyen Qt 6.8.3. El código fuente de
  esa versión está disponible en https://download.qt.io/archive/qt/6.8/6.8.3/

## TagLib

- Licencia: **LGPL 2.1** o **MPL 1.1**, a elección
- Sitio: https://taglib.org
- Uso: enlazado dinámicamente (`tag.dll`), igualmente reemplazable.

## miniaudio

- Licencia: **dominio público (Unlicense)** o **MIT-0**, a elección
- Sitio: https://miniaud.io
- Uso: incluido como cabecera única en `third_party/miniaudio.h`.

## zlib

- Licencia: **zlib**
- Se distribuye como dependencia de TagLib (`z.dll`).

---

## Sobre el diseño de la interfaz

La disposición y la paleta se inspiran en los reproductores de escritorio
clásicos de estilo oscuro. **No se ha utilizado ningún código, recurso gráfico,
skin ni marca de AIMP ni de ningún otro reproductor**: todos los iconos están
dibujados por código con `QPainter`, la hoja de estilos es propia y el motor de
audio se escribió desde cero sobre miniaudio.

AIMP es una marca de sus respectivos titulares y este proyecto no está
asociado, respaldado ni patrocinado por ellos.
