<picture>
 <source media="(prefers-color-scheme: dark)" srcset="https://github.com/LGenaroAlvarez/Hidroponia_automatica_2024_UVG/blob/f2462ae6608f58484dead54a124a67f43fbc2c54/Logo%20UVG-08.png">
 <source media="(prefers-color-scheme: light)" srcset="https://github.com/LGenaroAlvarez/Hidroponia_automatica_2024_UVG/blob/f2462ae6608f58484dead54a124a67f43fbc2c54/Logo%20UVG-06.png">
 <img alt="LOGO-UVG" src="https://github.com/LGenaroAlvarez/Hidroponia_automatica_2024_UVG/blob/f2462ae6608f58484dead54a124a67f43fbc2c54/Logo%20UVG-03.png">
</picture>

# Diseño e implementación de un sistema hidropónico automático de técnica NFT para el cultivo de cilantro en un entorno urbano

## Autor
**Nombre:** Luis Genaro Álvarez Sulecio

**Carné:** 20168

**Carrera:** Licenciatura en Ingeniería Mecatrónica

### Información de contacto
**Correo institucional:** alv21068@uvg.edu.gt

**Correo personal:** luis.genaro.alvarez@gmail.com

**LinkedIn:** [L. Genaro A. Sulecio](https://www.linkedin.com/in/l-genaro-a-sulecio-4034471aa?lipi=urn%3Ali%3Apage%3Ad_flagship3_profile_view_base_contact_details%3BWr9K9GOyTMqiwCvlfT2UYQ%3D%3D)

## Asesor
**Nombre:** Ing. Pedro Joaquín Castillo Coronado
### Información de contacto
**Correo institucional:** pjcastillo@uvg.edu.gt

## Contenido
| Ubicación | Descripción de archivos|
|-----------|------------------------|
| Codigo Fuente | Código desarrollado para el moniotoreo y control del sistema (firmware) y código de la aplicación desarrollada en Android Studio |
| Datos | Recopilación de históricos de datos obtenidos durante los períodos de prueba del sistema hidropónico tanto en formato .csv como en formato .xlsx |
| Documentos | Protocolo y trabajo escrito del proyecto de graduación desarrollado en TeXstudio |
| Modelos CAD | Modelos 3D de las piezas diseñadas e utilizadas en el sistema, ensamblaje del sistema completo para visualizaión 3D y planos detallados para cada pieza diseñada |
| Desarrollo PCB | Diagramas de circuitos, archivos de ruteo e imágenes de la placa desarrollada | 
| OldVersions | Versiones previas de código desarrollado |

## Descripción
 Este repositorio cuenta con la documentación del sistema hidropónico automático utilizando técnica de solución nutritiva recirculante (NFT por sus siglas en inglés) desarrollado como trabajo de graduación. El proyecto establece las bases para crear un sistema funcional que permita el monitoreo de diferentes parámetros ambientales que influyen en el crecimiento de las plantas en un entorno hidroónico. El sistema utiliza la placa de desarrollo [ESP-WROOM-32 de HiLetgo](http://www.hiletgo.com/ProductDetail/1906566.html) para recolectar datos de temperatura y humedad ambiental, así como de temperatura y pH de la solución de crecimiento. Para esto se utilizaron una serie de sensores analógicos y digitales los cuales se integraron utilizando diferentes librerías de licencia abierta.

 Este proyecto es el primero en la línea de investigación <!-- Agregar nombre de la nueva línea de investigación -->.

## Desglose del sistema

### Firmware y software

* Sistema físico con lectura de sensores y control de actuadores, programado en la IDE de Arduino.
* Aplicación móvil para la visualización de la información recopilada por el sistema, programado en JAVA y XML.
* Sistema de registro de datos para generación de históricos, programado en Python.

### Controlador y sensores

* Sensores de temperatura y humedad ambiental [DHT11](https://www.adafruit.com/product/386)
* Sensores de temperatura del agua [DS18B20](https://www.adafruit.com/product/381)
* Sensor de pH en el agua [ENV-30-pH](https://atlas-scientific.com/kits/surveyor-analog-ph-kit/)
* Reloj de tiempo real RTC [DS3231](https://laelectronica.com.gt/modulo-de-reloj-rtc-ds3231?srsltid=AfmBOoreVBHK8hEqVxO8LdLH4PKiJ90UkrpBadpoms6Jn7sJ64_w_fGP)
* Relés de 5VDC para control de elementos de 120VAC [Shori S3H-5-1CS](https://tienda.tettsa.gt/producto/relay-5vdc-5-pines-spdt-shori/)
* Placa de desarrollo [ESP WROOM-32](https://laelectronica.com.gt/modulo-wi-fi--bluetooth-esp32?search=esp32&description=true)

### Diseño de estructura física

La estructura física se basó al rededor de una estantería metálica modular. Esta cuenta con 5 repisas, de las cuales se utilizaron dos para los canales de crecimiento. En el ensamblaje del modelo CAD se observa la disposición de las tuberías para los canales de crecimiento, así como la posición de los sensores de temperatura y humedad ambiental.

<picture>
 <img alt="ESTRUCTURA-SISTEMA-HIDROPONICO" src="https://github.com/LGenaroAlvarez/Hidroponia_automatica_2024_UVG/blob/e166b7d0e84ebb052d00660b9c25ded6fa135edc/Visualizacion_ensamble_sistema.png">
</picture>

### Lenguajes de programación

* Python 3.11.9
* Arduino C
* Java
* XML



