# TECKIO Carriers

Repositorio principal de las **carriers TECKIO**: placas base que amplían las capacidades de una tarjeta de desarrollo mediante interfaces y periféricos orientados a una aplicación.

Este repositorio reúne la documentación de cada carrier, el código fuente de su firmware y los binarios disponibles para programación. También es el canal de distribución de **Carrier Hub**, la aplicación Android del ecosistema, mediante [GitHub Releases](https://github.com/Dar-cpu/carriers/releases).

[Explorar Access-Control](dsPIC33/Access-Control/Readme.md) · [Descargar la aplicación](https://github.com/Dar-cpu/carriers/releases) · [TECKIO](https://teckio.pe)

## Carriers disponibles

| Carrier | Plataforma compatible | Aplicación | Recursos |
| --- | --- | --- | --- |
| **TK LINK ACCESS-01 · Rev A** | **TECKIO dsPIC33FJ Red Edition — dsPIC33FJ32MC204** | Control de acceso con teclado, relé, LCD y comunicación Bluetooth; funciones auxiliares configurables | [Documentación, fuentes y firmware HEX](dsPIC33/Access-Control/Readme.md) |

**TK LINK ACCESS-01 Rev A es compatible únicamente con dsPIC33FJ32MC204.** La variante dsPIC33EP32MC204 queda excluida por la compatibilidad con el módulo de relé de esta carrier. Esta restricción corresponde a Access-Control Rev A; cada nueva carrier tendrá su propia especificación de compatibilidad.

## TK LINK ACCESS-01

| Carrier base | Carrier con TECKIO dsPIC33FJ Red Edition |
| --- | --- |
| ![TK LINK ACCESS-01 sin tarjeta de desarrollo](img/only_carrier.jpeg) | ![TK LINK ACCESS-01 con TECKIO dsPIC33FJ Red Edition instalada](img/carrier_with_dspic33fj_red_edition.jpeg) |

La carrier incorpora la base de conexión para la tarjeta dsPIC33, interfaces para LCD 16×2, teclado y módulos externos, buzzer, botones de usuario y alimentación. Las fotografías muestran la placa base y el montaje con la tarjeta de desarrollo; el LCD, el teclado y los módulos de relé y Bluetooth no están instalados en estas imágenes.

## Organización del repositorio

| Ubicación | Contenido |
| --- | --- |
| [`dsPIC33/Access-Control/`](dsPIC33/Access-Control/) | Firmware y documentación de TK LINK ACCESS-01. |
| [`img/`](img/) | Fotografías y recursos gráficos utilizados en la documentación. |
| [Releases](https://github.com/Dar-cpu/carriers/releases) | Distribuciones de las aplicaciones, notas de versión y archivos asociados a cada publicación. |

Las futuras carriers se incorporarán en directorios propios, agrupados por plataforma y aplicación. Cada una documentará su hardware compatible, funciones, conexiones y procedimiento de programación.

## Carrier Hub para Android

**Carrier Hub** es la aplicación común para configurar y utilizar las carriers TECKIO compatibles. Las funciones disponibles dependen del modelo conectado, de su firmware y de las capacidades que este anuncia a la aplicación.

1. Abrir [Releases](https://github.com/Dar-cpu/carriers/releases).
2. Seleccionar la versión de Carrier Hub adecuada y consultar sus notas de compatibilidad.
3. Descargar el archivo **`.apk`** de los adjuntos de esa publicación.
4. Instalarlo en Android y seguir el procedimiento de conexión de la carrier correspondiente.

Cuando una publicación incluya un archivo `.sha256`, este permite comprobar la integridad de la descarga. Los archivos automáticos **Source code (zip/tar.gz)** corresponden al contenido de este repositorio y no son el instalador Android.

**El código fuente compartido en este repositorio corresponde al firmware de las carriers. El código fuente de la aplicación Android no se publica aquí; la app se distribuye como APK compilada.**

## Firmware y versiones

El firmware se encuentra en la carpeta de cada carrier. Cuando existe un archivo `.hex`, puede programarse directamente en el microcontrolador especificado sin compilar los fuentes.

Las versiones de **Carrier Hub**, del **firmware** y de la **revisión de hardware** se identifican por separado. Una actualización de la aplicación no implica que deba reprogramarse la placa: la compatibilidad y los requisitos se indican en la documentación de la carrier y en las notas de cada release.

Para comenzar con el modelo actual, consultar la [guía de TK LINK ACCESS-01](dsPIC33/Access-Control/Readme.md).

## Soporte

Para reportar incidencias, utilizar [Issues](https://github.com/Dar-cpu/carriers/issues) e indicar el modelo de carrier, revisión de hardware, microcontrolador, versión del firmware, versión de Carrier Hub y pasos para reproducir el problema.

**TECKIO** · [teckio.pe](https://teckio.pe)
