"""
Version 1.2
Autor: Ing. Luis Genaro Alvarez Sulecio
Descripcion: Este codigo utiliza la libreria "requests" para
realizar una solicitud de datos al servidor de Dweet.io. Los
datos solicitados son serializados y almacenados en un archivo
.csv con el fin de registrar los parametros enviados a la nube
por el sistema hidroponico automatico. 
Comentarios:
    - 
"""
#%%
import requests
from csv import writer
from csv import DictWriter
import time

# Direccion URL del servidor del cual se estaran leyendo los datos
server_url = "https://dweet.io/get/latest/dweet/for/hydro_test"

# Nombres de las columnas para cada dato recibido
column_names = ['Date', 'Time', 't_s1', 'h_s1', 
                't_s2', 'h_s2', 'wt_s1', 'wt_s2', 'wt_s3', 'ph_val', 'fan_state']

# Recopilacion de hora y fecha para detectar desconexion del sistema al servidor
last_time = '00:00:00' # Inicializacion de hora registrada
last_date = '2000/01/1' # Inicializacion de la fecha registrada

#---------------Banderas y contadores---------------
cont = 0
i = 0
date_err = 0
flag = 0    # Bandera para determinar si el servidor cuenta con datos

#----------------Ciclo principal----------------
while(True):
    if(flag == 0):  
        # Intento de lectura de datos
        try:
            http_content = requests.get(server_url) # Solicitar contenido del servidor HTTP

            with_content = http_content.json()['with'][0]['content']    # Obtener el contenido principal de la respuesta del servidor
       
        except ConnectionError: # Si fue imposible conectar al servidor
            print("Http Connetion Sync Error")
            time.sleep(5)   # Esperar 5 segundos y volver a intentarlo
        
        # Revisar que el contenido recibido sea diferente al ultimo contenido registrado (asegurando datos nuevos)
        if (with_content['Time'] != last_time):
            # Intento de lectura y formateo de datos
            try:
                last_time = with_content['Time']    # Actualizar hora
                last_date = with_content['Date']    # Actualizar fecha

                #-----------------------Diccionario para almacenar datos recibidos del servidor-----------------------
                data_buffer = {'Date':last_date,'Time':last_time,'t_s1': float(with_content['t_s1'][0:5]), 't_s2' : float(with_content['t_s2'][0:5]),
                            'h_s1' : float(with_content['h_s1'][0:5]), 'h_s2' : float(with_content['h_s2'][0:5]),
                            'wt_s1' : float(with_content['wt_s1'][0:5]), 'wt_s2' : float(with_content['wt_s2'][0:5]),
                            'wt_s3' : float(with_content['wt_s3'][0:5]), 'ph_val' : float(with_content['ph_val']),
                            'fan_state':int(with_content['Fan_State'])}
                flag = 0    # Setear bandera a 0 para asegurar que se pueda volver a intentar una lectura
                if (i == 0):
                    # Determinar si el archivo .csv existe o debe ser creado
                    try:
                        # Lectura de archivo .csv
                        with open('temperature_history.csv', 'r') as csv_read:
                            i = 1   # Cambiar estado de bandera "i" a 1 para indicar que existe el archivo
                            csv_read.close()    # Cerrar archivo
                    # Si no existe el archivo .csv
                    except FileNotFoundError:
                        # Crear archivo .csv
                        with open('temperature_history.csv', 'w', newline='') as csvfile:
                            writer = DictWriter(csvfile, fieldnames=column_names)   # Definir objeto para escritura de nombres de columnas
                            writer.writeheader()    # Agregar nombres de columnas al archivo
                            i = 1   # Cambiar estado de bandera "i" a 1 para indicar que ya existe el archivo
                            csvfile.close() # Cerrar archivo
                # Si el archivo ya existe
                else:
                    with open('temperature_history.csv', 'a') as csv_file:
                            #--------------------Cargar datos almacenados en buffer al archivo--------------------
                            dictwriter_object = DictWriter(csv_file, fieldnames=column_names, lineterminator='\n')
                            dictwriter_object.writerow(data_buffer)
                            csv_file.close()

            # Si el servidor esta vacio     
            except TypeError:
                print("No data in server")
                flag = 1    # Indicar que no existe datos en el servidor
       # Si los datos en el servidor no han sido refrescados
        else:
            cont += 1   # Aumentar contador
            if (cont > 5):  # Si han pasado 30 minutos
                cont = 0    # Reiniciar contador
                print('ESP32 Disconected from server')  # Indicar que el sistema se ha desconectado del servidor
        time.sleep(360) # Esperar 6 minutos antes de proxima lectura (valor elegido para evitar interferencia con envio de daots)
    # Si el sistema hidroponico esta desconectado de la red WiFi esperar a que este se vuelva a conectar y enviar datos
    else:
         time.sleep(900)    # Esperar 15 minutos antes de nueva solicitud de datos
         flag = 0   # Habilitar intento de lectura de datos

# %%