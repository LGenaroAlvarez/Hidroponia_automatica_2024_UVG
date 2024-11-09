package com.example.hydromaticmonitor;

import androidx.appcompat.app.AppCompatActivity;
import android.annotation.SuppressLint;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
//import android.widget.Toast;

import com.android.volley.Request;
import com.android.volley.RequestQueue;
import com.android.volley.Response;
import com.android.volley.VolleyError;
import com.android.volley.toolbox.JsonObjectRequest;
import com.android.volley.toolbox.Volley;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

//import java.sql.Time;
import java.util.Timer;
import java.util.TimerTask;

public class Dashboard extends AppCompatActivity implements View.OnClickListener{
    // Declaracion de variables para campo de texto editable
    EditText et_temp1, et_temp2, et_hum1, et_hum2, et_wtemp1, et_wtemp2, et_wtemp3, et_wph, et_time;

    // Declaracion de variable para el boton de solicitud de datos
    Button refresh_btn;

    // Declaracion de variables tipo string a utilizar
    private String date, time;

    // Cracion de objeto para solicitar datos de servidor HTTP
    RequestQueue queue;

    @SuppressLint("MissingInflatedId")
    @Override
    // Aplicacion principal
    protected void onCreate(Bundle savedInstanceState) {
        final Handler handler = new Handler();  // Objeto para correr aplicacion principal
        super.onCreate(savedInstanceState); // Creacion de instancia
        setContentView(R.layout.activity_dashboard);    // Definicion de la interfaz a utilizar
        Timer timer = new Timer();  // Creacion de temporizador para lectura de datos
        //------Enlace entre elementos de la interfaz y variables establecidas en codigo------
        et_temp1 = findViewById(R.id.txt_temp1);
        et_temp2 = findViewById(R.id.txt_temp2);
        et_hum1 = findViewById(R.id.txt_hum1);
        et_hum2 = findViewById(R.id.txt_hum2);
        et_wtemp1 = findViewById(R.id.txt_wtemp1);
        et_wtemp2 = findViewById(R.id.txt_wtemp2);
        et_wtemp3 = findViewById(R.id.txt_wtemp3);
        et_wph = findViewById(R.id.txt_wtpH);
        refresh_btn = findViewById(R.id.button);
        et_time = findViewById(R.id.txt_time);
        //-------------------------------------------------------------------------------------
        queue = Volley.newRequestQueue(this);   // Definiicon de solicitud al servidor HTTP

        //--------Ciclo de lectura de datos--------
        timer.scheduleAtFixedRate(new TimerTask() {
            @Override
            public void run() {
                handler.post(() -> recieve_msg());  // Lectura de datos
            }
        },1000,180000); // delay = retraso para la primera lectura (1 segundo)
                            // period = periodo para lectura recurrente de datos (3 minutos)

        refresh_btn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                recieve_msg();
            }
        });
    }

    public void recieve_msg() {
        String serverURL = "https://dweet.io/get/latest/dweet/for/hydro_test";
        JsonObjectRequest request = new JsonObjectRequest(Request.Method.GET, serverURL, null,
                new Response.Listener<JSONObject>() {
                    @Override
                    public void onResponse(JSONObject response) {
                        try {
                            JSONArray with = response.getJSONArray("with");
                            JSONObject dentro_bracket = with.getJSONObject(0);
                            String date_time = dentro_bracket.getString("created");
                            JSONObject contenido = dentro_bracket.getJSONObject("content");
                            String t_s1 = contenido.getString("t_s1");
                            String h_s1 = contenido.getString("h_s1");
                            String t_s2 = contenido.getString("t_s2");
                            String h_s2 = contenido.getString("h_s2");
                            String wt_s1 = contenido.getString("wt_s1");
                            String wt_s2 = contenido.getString("wt_s2");
                            String wt_s3 = contenido.getString("wt_s3");
                            String wt_ph = contenido.getString("ph_val");
                            String time_rtc = contenido.getString("Time");

                            String[] separated = date_time.split("T");
                            date = separated[0];
                            String[] time_separated = separated[1].split("\\.");
                            time = time_separated[0];

                            // Desplegar texto
                            et_temp1.setText(t_s1);
                            et_temp2.setText(t_s2);
                            et_hum1.setText(h_s1);
                            et_hum2.setText(h_s2);
                            et_wtemp1.setText(wt_s1);
                            et_wtemp2.setText(wt_s2);
                            et_wtemp3.setText(wt_s3);
                            et_wph.setText(wt_ph);
                            et_time.setText(time_rtc);
                        } catch (JSONException e) {
                            e.printStackTrace();
                        }

                    }
                }, new Response.ErrorListener() {
            @Override
            public void onErrorResponse(VolleyError error) {

            }
        });
        queue.add(request);
    }

    @Override
    public void onClick(View v) {

    }
}